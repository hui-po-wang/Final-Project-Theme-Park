#include "fbxloader.h"
#include <glew.h>
#include <algorithm>

using namespace std;
using namespace tinyobj;

#ifdef IOS_REF
    #undef  IOS_REF
    #define IOS_REF (*(pManager->GetIOSettings()))
#endif

typedef struct
{
    std::vector<int> vertexControlIndices; // translate gl_VertexID to control point index
    std::vector<int> vertexJointIndices; // 4 joints for one vertex
    std::vector<float> vertexJointWeights; // 4 joint weights for one vertex
    std::vector<std::vector<float> > jointTransformMatrices; // 16 float column major matrix for each joint for each frame
    int num_frames;
} animation_t;

vector<shape_t> gShapes;
vector<material_t> gMaterials;
vector<animation_t> gAnimations;
vector<shape_t> gShapeAnim;

bool LoadScene(FbxManager* pManager, FbxScene* pScene, FbxArray<FbxString*> pAnimStackNameArray, const char* pFilename);
void LoadCacheRecursive(FbxScene* pScene, FbxNode * pNode, FbxAMatrix& pParentGlobalPosition, FbxTime& pTime);
void LoadAnimationRecursive(FbxScene* pScene, FbxNode * pNode, FbxAMatrix& pParentGlobalPosition, FbxTime& pTime);
void InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene);
void DestroySdkObjects(FbxManager* pManager, bool pExitStatus);

void DisplayMetaData(FbxScene* pScene);
void DisplayHierarchy(FbxScene* pScene);
void DisplayHierarchy(FbxNode* pNode, int pDepth);

FbxAMatrix GetGlobalPosition(FbxNode* pNode, const FbxTime& pTime, FbxPose* pPose = NULL, FbxAMatrix* pParentGlobalPosition = NULL);
FbxAMatrix GetPoseMatrix(FbxPose* pPose, int pNodeIndex);
FbxAMatrix GetGeometry(FbxNode* pNode);

void MatrixScale(FbxAMatrix& pMatrix, double pValue);
void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue);
void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix);

void ComputeSkinDeformation(FbxAMatrix& pGlobalPosition, FbxMesh* pMesh, FbxTime& pTime, FbxVector4* pVertexArray, FbxPose* pPose);
void ComputeShapeDeformation(FbxMesh* pMesh, FbxTime& pTime, FbxAnimLayer * pAnimLayer, FbxVector4* pVertexArray);
void ComputeClusterDeformation(FbxAMatrix& pGlobalPosition, FbxMesh* pMesh, FbxCluster* pCluster, FbxAMatrix& pVertexTransformMatrix, FbxTime pTime, FbxPose* pPose);

void GetFbxAnimation(fbx_handles &handles, std::vector<tinyobj::shape_t> &shapes, float frame)
{
    if (handles.lScene != 0)
    {
        frame = min(max(frame, 0.0f), 1.0f);
        FbxTimeSpan lTimeLineTimeSpan;
        handles.lScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
        FbxTime lTime = lTimeLineTimeSpan.GetStart() + ((lTimeLineTimeSpan.GetStop() - lTimeLineTimeSpan.GetStart()) / 10000) * (10000 * frame);
        gShapeAnim.clear();
        FbxAMatrix lDummyGlobalPosition;
        LoadAnimationRecursive(handles.lScene, handles.lScene->GetRootNode(), lDummyGlobalPosition, lTime);
        shapes = gShapeAnim;
    }
}

bool LoadFbx(fbx_handles &handles, vector<shape_t> &shapes, vector<material_t> &materials, std::string err, const char* pFileName)
{
    gShapes.clear();
    gMaterials.clear();
    gAnimations.clear();

    bool lResult;
    InitializeSdkObjects(handles.lSdkManager, handles.lScene);
    lResult = LoadScene(handles.lSdkManager, handles.lScene, handles.lAnimStackNameArray, pFileName);

    if (lResult == false)
    {
        FBXSDK_printf("\n\nAn error occurred while loading the scene...");
        DestroySdkObjects(handles.lSdkManager, lResult);
        return false;
    }
    else
    {
        // Display the scene.
        // DisplayMetaData(handles.lScene);
        // FBXSDK_printf("\n\n---------\nHierarchy\n---------\n\n");
        // DisplayHierarchy(handles.lScene);

        // Load data.
        FbxAMatrix lDummyGlobalPosition;
        FbxTimeSpan lTimeLineTimeSpan;
        handles.lScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
        FbxTime lTime = lTimeLineTimeSpan.GetStart();
        LoadCacheRecursive(handles.lScene, handles.lScene->GetRootNode(), lDummyGlobalPosition, lTime);
    }

    shapes = gShapes;
    materials = gMaterials;
    return true;
}

void ReleaseFbx(fbx_handles &handles)
{
    if (handles.lScene)
    {
        bool lResult;
        DestroySdkObjects(handles.lSdkManager, lResult);
        FbxArrayDelete(handles.lAnimStackNameArray);
        handles.lSdkManager = 0;
        handles.lScene = 0;
    }
}

void DisplayHierarchy(FbxScene* pScene)
{
    int i;
    FbxNode* lRootNode = pScene->GetRootNode();

    for (i = 0; i < lRootNode->GetChildCount(); i++)
    {
        DisplayHierarchy(lRootNode->GetChild(i), 0);
    }
}

void DisplayHierarchy(FbxNode* pNode, int pDepth)
{
    FbxString lString;
    int i;

    for (i = 0; i < pDepth; i++)
    {
        lString += "     ";
    }

    lString += pNode->GetName();
    lString += "\n";

    FBXSDK_printf(lString.Buffer());

    for (i = 0; i < pNode->GetChildCount(); i++)
    {
        DisplayHierarchy(pNode->GetChild(i), pDepth + 1);
    }
}

void InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene)
{
    //The first thing to do is to create the FBX Manager which is the object allocator for almost all the classes in the SDK
    pManager = FbxManager::Create();
    if (!pManager)
    {
        FBXSDK_printf("Error: Unable to create FBX Manager!\n");
        exit(1);
    }
    else FBXSDK_printf("Autodesk FBX SDK version %s\n", pManager->GetVersion());

    //Create an IOSettings object. This object holds all import/export settings.
    FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
    pManager->SetIOSettings(ios);

    //Load plugins from the executable directory (optional)
    FbxString lPath = FbxGetApplicationDirectory();
    pManager->LoadPluginsDirectory(lPath.Buffer());

    //Create an FBX scene. This object holds most objects imported/exported from/to files.
    pScene = FbxScene::Create(pManager, "My Scene");
    if (!pScene)
    {
        FBXSDK_printf("Error: Unable to create FBX scene!\n");
        exit(1);
    }
}

bool LoadScene(FbxManager* pManager, FbxScene* pScene, FbxArray<FbxString*> pAnimStackNameArray, const char* pFilename)
{
    int lFileMajor, lFileMinor, lFileRevision;
    int lSDKMajor, lSDKMinor, lSDKRevision;
    int i, lAnimStackCount;
    bool lStatus;
    char lPassword[1024];

    // Get the file version number generate by the FBX SDK.
    FbxManager::GetFileFormatVersion(lSDKMajor, lSDKMinor, lSDKRevision);

    // Create an importer.
    FbxImporter* lImporter = FbxImporter::Create(pManager, "");

    // Initialize the importer by providing a filename.
    const bool lImportStatus = lImporter->Initialize(pFilename, -1, pManager->GetIOSettings());
    lImporter->GetFileVersion(lFileMajor, lFileMinor, lFileRevision);

    if (!lImportStatus)
    {
        FbxString error = lImporter->GetStatus().GetErrorString();
        FBXSDK_printf("Call to FbxImporter::Initialize() failed.\n");
        FBXSDK_printf("Error returned: %s\n\n", error.Buffer());

        if (lImporter->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion)
        {
            FBXSDK_printf("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);
            FBXSDK_printf("FBX file format version for file '%s' is %d.%d.%d\n\n", pFilename, lFileMajor, lFileMinor, lFileRevision);
        }

        return false;
    }

    FBXSDK_printf("FBX file format version for this FBX SDK is %d.%d.%d\n", lSDKMajor, lSDKMinor, lSDKRevision);

    if (lImporter->IsFBX())
    {
        FBXSDK_printf("FBX file format version for file '%s' is %d.%d.%d\n\n", pFilename, lFileMajor, lFileMinor, lFileRevision);

        // From this point, it is possible to access animation stack information without
        // the expense of loading the entire file.

        FBXSDK_printf("Animation Stack Information\n");

        lAnimStackCount = lImporter->GetAnimStackCount();

        FBXSDK_printf("    Number of Animation Stacks: %d\n", lAnimStackCount);
        FBXSDK_printf("    Current Animation Stack: \"%s\"\n", lImporter->GetActiveAnimStackName().Buffer());
        FBXSDK_printf("\n");

        for (i = 0; i < lAnimStackCount; i++)
        {
            FbxTakeInfo* lTakeInfo = lImporter->GetTakeInfo(i);

            FBXSDK_printf("    Animation Stack %d\n", i);
            FBXSDK_printf("         Name: \"%s\"\n", lTakeInfo->mName.Buffer());
            FBXSDK_printf("         Description: \"%s\"\n", lTakeInfo->mDescription.Buffer());

            // Change the value of the import name if the animation stack should be imported 
            // under a different name.
            FBXSDK_printf("         Import Name: \"%s\"\n", lTakeInfo->mImportName.Buffer());

            // Set the value of the import state to false if the animation stack should be not
            // be imported. 
            FBXSDK_printf("         Import State: %s\n", lTakeInfo->mSelect ? "true" : "false");
            FBXSDK_printf("\n");
        }
    }

    // Import the scene.
    lStatus = lImporter->Import(pScene);

    if (lStatus == false && lImporter->GetStatus().GetCode() == FbxStatus::ePasswordError)
    {
        FBXSDK_printf("Please enter password: ");

        lPassword[0] = '\0';

        FBXSDK_CRT_SECURE_NO_WARNING_BEGIN
            scanf("%s", lPassword);
        FBXSDK_CRT_SECURE_NO_WARNING_END

            FbxString lString(lPassword);

        IOS_REF.SetStringProp(IMP_FBX_PASSWORD, lString);
        IOS_REF.SetBoolProp(IMP_FBX_PASSWORD_ENABLE, true);

        lStatus = lImporter->Import(pScene);

        if (lStatus == false && lImporter->GetStatus().GetCode() == FbxStatus::ePasswordError)
        {
            FBXSDK_printf("\nPassword is wrong, import aborted.\n");
        }
    }

    if (lStatus)
    {
        // Convert Axis System to up = Y Axis, Right-Handed Coordinate (OpenGL Style)
        FbxAxisSystem SceneAxisSystem = pScene->GetGlobalSettings().GetAxisSystem();
        FbxAxisSystem OurAxisSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
        if (SceneAxisSystem != OurAxisSystem)
        {
            OurAxisSystem.ConvertScene(pScene);
        }

        // Convert Unit System to what is used in this example, if needed
        FbxSystemUnit SceneSystemUnit = pScene->GetGlobalSettings().GetSystemUnit();
        if (SceneSystemUnit.GetScaleFactor() != 100.0)
        {
            //The unit in this example is centimeter.
            FbxSystemUnit::m.ConvertScene(pScene);
        }

        // Get the list of all the animation stack.
        pScene->FillAnimStackNameArray(pAnimStackNameArray);

        // Convert mesh, NURBS and patch into triangle mesh
        FbxGeometryConverter lGeomConverter(pManager);
        lGeomConverter.Triangulate(pScene, true);
    }

    // Destroy the importer.
    lImporter->Destroy();

    return lStatus;
}

void DestroySdkObjects(FbxManager* pManager, bool pExitStatus)
{
    //Delete the FBX Manager. All the objects that have been allocated using the FBX Manager and that haven't been explicitly destroyed are also automatically destroyed.
    if (pManager) pManager->Destroy();
    if (pExitStatus) FBXSDK_printf("Program Success!\n");
}

void DisplayMetaData(FbxScene* pScene)
{
    FbxDocumentInfo* sceneInfo = pScene->GetSceneInfo();
    if (sceneInfo)
    {
        FBXSDK_printf("\n\n--------------------\nMeta-Data\n--------------------\n\n");
        FBXSDK_printf("    Title: %s\n", sceneInfo->mTitle.Buffer());
        FBXSDK_printf("    Subject: %s\n", sceneInfo->mSubject.Buffer());
        FBXSDK_printf("    Author: %s\n", sceneInfo->mAuthor.Buffer());
        FBXSDK_printf("    Keywords: %s\n", sceneInfo->mKeywords.Buffer());
        FBXSDK_printf("    Revision: %s\n", sceneInfo->mRevision.Buffer());
        FBXSDK_printf("    Comment: %s\n", sceneInfo->mComment.Buffer());

        FbxThumbnail* thumbnail = sceneInfo->GetSceneThumbnail();
        if (thumbnail)
        {
            FBXSDK_printf("    Thumbnail:\n");

            switch (thumbnail->GetDataFormat())
            {
            case FbxThumbnail::eRGB_24:
                FBXSDK_printf("        Format: RGB\n");
                break;
            case FbxThumbnail::eRGBA_32:
                FBXSDK_printf("        Format: RGBA\n");
                break;
            }

            switch (thumbnail->GetSize())
            {
            default:
                break;
            case FbxThumbnail::eNotSet:
                FBXSDK_printf("        Size: no dimensions specified (%ld bytes)\n", thumbnail->GetSizeInBytes());
                break;
            case FbxThumbnail::e64x64:
                FBXSDK_printf("        Size: 64 x 64 pixels (%ld bytes)\n", thumbnail->GetSizeInBytes());
                break;
            case FbxThumbnail::e128x128:
                FBXSDK_printf("        Size: 128 x 128 pixels (%ld bytes)\n", thumbnail->GetSizeInBytes());
            }
        }
    }
}

// Get specific property value and connected texture if any.
// Value = Property value * Factor property value (if no factor property, multiply by 1).
FbxDouble3 GetMaterialProperty(const FbxSurfaceMaterial * pMaterial,
    const char * pPropertyName,
    const char * pFactorPropertyName,
    string & pTextureName)
{
    FbxDouble3 lResult(0, 0, 0);
    const FbxProperty lProperty = pMaterial->FindProperty(pPropertyName);
    const FbxProperty lFactorProperty = pMaterial->FindProperty(pFactorPropertyName);
    if (lProperty.IsValid() && lFactorProperty.IsValid())
    {
        lResult = lProperty.Get<FbxDouble3>();
        double lFactor = lFactorProperty.Get<FbxDouble>();
        if (lFactor != 1)
        {
            lResult[0] *= lFactor;
            lResult[1] *= lFactor;
            lResult[2] *= lFactor;
        }
    }

    if (lProperty.IsValid())
    {
        const int lTextureCount = lProperty.GetSrcObjectCount<FbxFileTexture>();
        if (lTextureCount)
        {
            const FbxFileTexture* lTexture = lProperty.GetSrcObject<FbxFileTexture>();
            if (lTexture)
            {
                pTextureName = lTexture->GetFileName();
            }
        }
    }

    return lResult;
}

void LoadAnimationRecursive(FbxScene* pScene, FbxNode *pNode, FbxAMatrix& pParentGlobalPosition, FbxTime& pTime)
{
    FbxAMatrix lGlobalPosition = GetGlobalPosition(pNode, pTime, 0, &pParentGlobalPosition);
    const int lMaterialCount = pNode->GetMaterialCount();
    FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
    if (lNodeAttribute)
    {
        // Bake mesh as VBO(vertex buffer object) into GPU.
        if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            FbxMesh * lMesh = pNode->GetMesh();
            if (lMesh)
            {
                const int lPolygonCount = lMesh->GetPolygonCount();
                bool lAllByControlPoint = true; // => true: glDrawElements / false: glDrawArrays

                                                // Count the polygon count of each material
                FbxLayerElementArrayTemplate<int>* lMaterialIndice = NULL;
                FbxGeometryElement::EMappingMode lMaterialMappingMode = FbxGeometryElement::eNone;
                if (lMesh->GetElementMaterial())
                {
                    lMaterialIndice = &lMesh->GetElementMaterial()->GetIndexArray();
                    lMaterialMappingMode = lMesh->GetElementMaterial()->GetMappingMode();
                }

                // Congregate all the data of a mesh to be cached in VBOs.
                // If normal or UV is by polygon vertex, record all vertex attributes by polygon vertex.
                bool lHasNormal = lMesh->GetElementNormalCount() > 0;
                bool lHasUV = lMesh->GetElementUVCount() > 0;
                FbxGeometryElement::EMappingMode lNormalMappingMode = FbxGeometryElement::eNone;
                FbxGeometryElement::EMappingMode lUVMappingMode = FbxGeometryElement::eNone;
                if (lHasNormal)
                {
                    lNormalMappingMode = lMesh->GetElementNormal(0)->GetMappingMode();
                    if (lNormalMappingMode == FbxGeometryElement::eNone)
                    {
                        lHasNormal = false;
                    }
                    if (lHasNormal && lNormalMappingMode != FbxGeometryElement::eByControlPoint)
                    {
                        lAllByControlPoint = false;
                    }
                }
                if (lHasUV)
                {
                    lUVMappingMode = lMesh->GetElementUV(0)->GetMappingMode();
                    if (lUVMappingMode == FbxGeometryElement::eNone)
                    {
                        lHasUV = false;
                    }
                    if (lHasUV && lUVMappingMode != FbxGeometryElement::eByControlPoint)
                    {
                        lAllByControlPoint = false;
                    }
                }

                // Allocate the array memory, by control point or by polygon vertex.
                int lPolygonVertexCount = lMesh->GetControlPointsCount();
                if (!lAllByControlPoint)
                {
                    lPolygonVertexCount = lPolygonCount * 3;
                }
                vector<float> lVertices;
                lVertices.resize(lPolygonVertexCount * 3);
                vector<unsigned int> lIndices;
                lIndices.resize(lPolygonCount * 3);

                // Populate the array with vertex attribute, if by control point.
                FbxVector4 * lControlPoints = lMesh->GetControlPoints();
                /////////////////////////
                if((FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin) != 0)
                {
                    lControlPoints = new FbxVector4[lMesh->GetControlPointsCount()];
                    memcpy(lControlPoints, lMesh->GetControlPoints(), lMesh->GetControlPointsCount() * sizeof(FbxVector4));

                    // select the base layer from the animation stack
                    FbxAnimStack * lCurrentAnimationStack = pScene->GetSrcObject<FbxAnimStack>(0);
                    // we assume that the first animation layer connected to the animation stack is the base layer
                    // (this is the assumption made in the FBXSDK)
                    FbxAnimLayer *mCurrentAnimLayer = lCurrentAnimationStack->GetMember<FbxAnimLayer>();

                    // ComputeShapeDeformation(lMesh, pTime, mCurrentAnimLayer, lControlPoints);
                    FbxAMatrix lGeometryOffset = GetGeometry(pNode);
                    FbxAMatrix lGlobalOffPosition = lGlobalPosition * lGeometryOffset;
                    ComputeSkinDeformation(lGlobalOffPosition, lMesh, pTime, lControlPoints, NULL);
                }
                /////////////////////////
                vector<int> vertexControlIndices;
                FbxVector4 lCurrentVertex;
                FbxVector4 lCurrentNormal;
                FbxVector2 lCurrentUV;
                if (lAllByControlPoint)
                {
                    for (int lIndex = 0; lIndex < lPolygonVertexCount; ++lIndex)
                    {
                        // Save the vertex position.
                        lCurrentVertex = lControlPoints[lIndex];
                        lVertices[lIndex * 3] = static_cast<float>(lCurrentVertex[0]);
                        lVertices[lIndex * 3 + 1] = static_cast<float>(lCurrentVertex[1]);
                        lVertices[lIndex * 3 + 2] = static_cast<float>(lCurrentVertex[2]);
                        vertexControlIndices.push_back(lIndex);
                    }

                }

                int lVertexCount = 0;
                for (int lPolygonIndex = 0; lPolygonIndex < lPolygonCount; ++lPolygonIndex)
                {
                    for (int lVerticeIndex = 0; lVerticeIndex < 3; ++lVerticeIndex)
                    {
                        const int lControlPointIndex = lMesh->GetPolygonVertex(lPolygonIndex, lVerticeIndex);

                        if (lAllByControlPoint)
                        {
                            lIndices[lPolygonIndex * 3 + lVerticeIndex] = static_cast<unsigned int>(lControlPointIndex);
                        }
                        // Populate the array with vertex attribute, if by polygon vertex.
                        else
                        {
                            lIndices[lPolygonIndex * 3 + lVerticeIndex] = static_cast<unsigned int>(lVertexCount);

                            lCurrentVertex = lControlPoints[lControlPointIndex];
                            lVertices[lVertexCount * 3] = static_cast<float>(lCurrentVertex[0]);
                            lVertices[lVertexCount * 3 + 1] = static_cast<float>(lCurrentVertex[1]);
                            lVertices[lVertexCount * 3 + 2] = static_cast<float>(lCurrentVertex[2]);
                            vertexControlIndices.push_back(lControlPointIndex);
                        }
                        ++lVertexCount;
                    }
                }
                shape_t shape;
                shape.mesh.positions = lVertices;
                gShapeAnim.push_back(shape);

                if ((FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin) != 0)
                {
                    delete [] lControlPoints;
                }
            }
        }
    }

    const int lChildCount = pNode->GetChildCount();
    for (int lChildIndex = 0; lChildIndex < lChildCount; ++lChildIndex)
    {
        LoadAnimationRecursive(pScene, pNode->GetChild(lChildIndex), lGlobalPosition, pTime);
    }
}

void LoadMaterials(FbxNode *pNode)
{
    // Bake material and hook as user data.
    int lMaterialIndexBase = gMaterials.size();
    const int lMaterialCount = pNode->GetMaterialCount();
    for (int lMaterialIndex = 0; lMaterialIndex < lMaterialCount; ++lMaterialIndex)
    {
        FbxSurfaceMaterial * lMaterial = pNode->GetMaterial(lMaterialIndex);
        material_t material;
        if (lMaterial)
        {
            string lTextureNameTemp;
            const FbxDouble3 lAmbient = GetMaterialProperty(lMaterial,
                FbxSurfaceMaterial::sAmbient, FbxSurfaceMaterial::sAmbientFactor, lTextureNameTemp);
            material.ambient[0] = static_cast<GLfloat>(lAmbient[0]);
            material.ambient[1] = static_cast<GLfloat>(lAmbient[1]);
            material.ambient[2] = static_cast<GLfloat>(lAmbient[2]);
            material.ambient_texname = lTextureNameTemp;

            const FbxDouble3 lDiffuse = GetMaterialProperty(lMaterial,
                FbxSurfaceMaterial::sDiffuse, FbxSurfaceMaterial::sDiffuseFactor, lTextureNameTemp);
            material.diffuse[0] = static_cast<GLfloat>(lDiffuse[0]);
            material.diffuse[1] = static_cast<GLfloat>(lDiffuse[1]);
            material.diffuse[2] = static_cast<GLfloat>(lDiffuse[2]);
            material.diffuse_texname = lTextureNameTemp;

            const FbxDouble3 lSpecular = GetMaterialProperty(lMaterial,
                FbxSurfaceMaterial::sSpecular, FbxSurfaceMaterial::sSpecularFactor, lTextureNameTemp);
            material.specular[0] = static_cast<GLfloat>(lSpecular[0]);
            material.specular[1] = static_cast<GLfloat>(lSpecular[1]);
            material.specular[2] = static_cast<GLfloat>(lSpecular[2]);
            material.specular_texname = lTextureNameTemp;

            FbxProperty lShininessProperty = lMaterial->FindProperty(FbxSurfaceMaterial::sShininess);
            if (lShininessProperty.IsValid())
            {
                double lShininess = lShininessProperty.Get<FbxDouble>();
                material.shininess = static_cast<GLfloat>(lShininess);
            }
        }
        gMaterials.push_back(material);
    }
}

void LoadCacheRecursive(FbxScene* pScene, FbxNode * pNode, FbxAMatrix& pParentGlobalPosition, FbxTime& pTime)
{
    FbxAMatrix lGlobalPosition = GetGlobalPosition(pNode, pTime, 0, &pParentGlobalPosition);
    // Bake material and hook as user data.
    int lMaterialIndexBase = gMaterials.size();
    LoadMaterials(pNode);

    FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
    if (lNodeAttribute)
    {
        // Bake mesh as VBO(vertex buffer object) into GPU.
        if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            FbxMesh * lMesh = pNode->GetMesh();
            if (lMesh)
            {
                const int lPolygonCount = lMesh->GetPolygonCount();
                bool lAllByControlPoint = true; // => true: glDrawElements / false: glDrawArrays

                // Count the polygon count of each material
                FbxLayerElementArrayTemplate<int>* lMaterialIndice = NULL;
                FbxGeometryElement::EMappingMode lMaterialMappingMode = FbxGeometryElement::eNone;
                if (lMesh->GetElementMaterial())
                {
                    lMaterialIndice = &lMesh->GetElementMaterial()->GetIndexArray();
                    lMaterialMappingMode = lMesh->GetElementMaterial()->GetMappingMode();
                }

                // Congregate all the data of a mesh to be cached in VBOs.
                // If normal or UV is by polygon vertex, record all vertex attributes by polygon vertex.
                bool lHasNormal = lMesh->GetElementNormalCount() > 0;
                bool lHasUV = lMesh->GetElementUVCount() > 0;
                FbxGeometryElement::EMappingMode lNormalMappingMode = FbxGeometryElement::eNone;
                FbxGeometryElement::EMappingMode lUVMappingMode = FbxGeometryElement::eNone;
                if (lHasNormal)
                {
                    lNormalMappingMode = lMesh->GetElementNormal(0)->GetMappingMode();
                    if (lNormalMappingMode == FbxGeometryElement::eNone)
                    {
                        lHasNormal = false;
                    }
                    if (lHasNormal && lNormalMappingMode != FbxGeometryElement::eByControlPoint)
                    {
                        lAllByControlPoint = false;
                    }
                }
                if (lHasUV)
                {
                    lUVMappingMode = lMesh->GetElementUV(0)->GetMappingMode();
                    if (lUVMappingMode == FbxGeometryElement::eNone)
                    {
                        lHasUV = false;
                    }
                    if (lHasUV && lUVMappingMode != FbxGeometryElement::eByControlPoint)
                    {
                        lAllByControlPoint = false;
                    }
                }

                // Allocate the array memory, by control point or by polygon vertex.
                int lPolygonVertexCount = lMesh->GetControlPointsCount();
                if (!lAllByControlPoint)
                {
                    lPolygonVertexCount = lPolygonCount * 3;
                }
                printf("All By Control Point: %s\n", lAllByControlPoint ? "Yes" : "No");
                vector<float> lVertices;
                lVertices.resize(lPolygonVertexCount * 3);
                vector<unsigned int> lIndices;
                lIndices.resize(lPolygonCount * 3);
                vector<float> lNormals;
                if (lHasNormal)
                {
                    lNormals.resize(lPolygonVertexCount * 3);
                }
                vector<float> lUVs;
                FbxStringList lUVNames;
                lMesh->GetUVSetNames(lUVNames);
                const char * lUVName = NULL;
                if (lHasUV && lUVNames.GetCount())
                {
                    lUVs.resize(lPolygonVertexCount * 2);
                    lUVName = lUVNames[0];
                }

                // Populate the array with vertex attribute, if by control point.
                FbxVector4 * lControlPoints = lMesh->GetControlPoints();
                /////////////////////////
                if ((FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin) != 0)
                {
                    lControlPoints = new FbxVector4[lMesh->GetControlPointsCount()];
                    memcpy(lControlPoints, lMesh->GetControlPoints(), lMesh->GetControlPointsCount() * sizeof(FbxVector4));

                    FbxTimeSpan lTimeLineTimeSpan;
                    pScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
                    FbxTime pTime = lTimeLineTimeSpan.GetStart();

                    // select the base layer from the animation stack
                    FbxAnimStack * lCurrentAnimationStack = pScene->GetSrcObject<FbxAnimStack>(0);
                    // we assume that the first animation layer connected to the animation stack is the base layer
                    // (this is the assumption made in the FBXSDK)
                    FbxAnimLayer *mCurrentAnimLayer = lCurrentAnimationStack->GetMember<FbxAnimLayer>();

                    // ComputeShapeDeformation(lMesh, pTime, mCurrentAnimLayer, lControlPoints);
                    FbxAMatrix lGeometryOffset = GetGeometry(pNode);
                    FbxAMatrix lGlobalOffPosition = lGlobalPosition * lGeometryOffset;
                    ComputeSkinDeformation(lGlobalOffPosition, lMesh, pTime, lControlPoints, NULL);
                }
                /////////////////////////
                vector<int> lMaterialIndices;
                lMaterialIndices.resize(lPolygonVertexCount);
                vector<int> vertexControlIndices;
                FbxVector4 lCurrentVertex;
                FbxVector4 lCurrentNormal;
                FbxVector2 lCurrentUV;
                if (lAllByControlPoint)
                {
                    const FbxGeometryElementNormal * lNormalElement = NULL;
                    const FbxGeometryElementUV * lUVElement = NULL;
                    if (lHasNormal)
                    {
                        lNormalElement = lMesh->GetElementNormal(0);
                    }
                    if (lHasUV)
                    {
                        lUVElement = lMesh->GetElementUV(0);
                    }
                    for (int lIndex = 0; lIndex < lPolygonVertexCount; ++lIndex)
                    {
                        // The material for current face.
                        int lMaterialIndex = 0;
                        if (lMaterialIndice && lMaterialMappingMode == FbxGeometryElement::eByPolygon)
                        {
                            lMaterialIndex = lMaterialIndice->GetAt(lIndex);
                        }
                        // Save the vertex position.
                        lCurrentVertex = lControlPoints[lIndex];
                        lVertices[lIndex * 3] = static_cast<float>(lCurrentVertex[0]);
                        lVertices[lIndex * 3 + 1] = static_cast<float>(lCurrentVertex[1]);
                        lVertices[lIndex * 3 + 2] = static_cast<float>(lCurrentVertex[2]);
                        lMaterialIndices[lIndex] = lMaterialIndex + lMaterialIndexBase;
                        vertexControlIndices.push_back(lIndex);

                        // Save the normal.
                        if (lHasNormal)
                        {
                            int lNormalIndex = lIndex;
                            if (lNormalElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                            {
                                lNormalIndex = lNormalElement->GetIndexArray().GetAt(lIndex);
                            }
                            lCurrentNormal = lNormalElement->GetDirectArray().GetAt(lNormalIndex);
                            lNormals[lIndex * 3] = static_cast<float>(lCurrentNormal[0]);
                            lNormals[lIndex * 3 + 1] = static_cast<float>(lCurrentNormal[1]);
                            lNormals[lIndex * 3 + 2] = static_cast<float>(lCurrentNormal[2]);
                        }

                        // Save the UV.
                        if (lHasUV)
                        {
                            int lUVIndex = lIndex;
                            if (lUVElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
                            {
                                lUVIndex = lUVElement->GetIndexArray().GetAt(lIndex);
                            }
                            lCurrentUV = lUVElement->GetDirectArray().GetAt(lUVIndex);
                            lUVs[lIndex * 2] = static_cast<float>(lCurrentUV[0]);
                            lUVs[lIndex * 2 + 1] = static_cast<float>(lCurrentUV[1]);
                        }
                    }

                }

                int lVertexCount = 0;
                for (int lPolygonIndex = 0; lPolygonIndex < lPolygonCount; ++lPolygonIndex)
                {
                    // The material for current face.
                    int lMaterialIndex = 0;
                    if (lMaterialIndice && lMaterialMappingMode == FbxGeometryElement::eByPolygon)
                    {
                        lMaterialIndex = lMaterialIndice->GetAt(lPolygonIndex);
                    }
                    if (lMaterialIndex != 0)
                    {
                        int i = 0;
                    }

                    for (int lVerticeIndex = 0; lVerticeIndex < 3; ++lVerticeIndex)
                    {
                        const int lControlPointIndex = lMesh->GetPolygonVertex(lPolygonIndex, lVerticeIndex);

                        if (lAllByControlPoint)
                        {
                            lIndices[lPolygonIndex * 3 + lVerticeIndex] = static_cast<unsigned int>(lControlPointIndex);
                        }
                        // Populate the array with vertex attribute, if by polygon vertex.
                        else
                        {
                            lIndices[lPolygonIndex * 3 + lVerticeIndex] = static_cast<unsigned int>(lVertexCount);

                            lCurrentVertex = lControlPoints[lControlPointIndex];
                            lVertices[lVertexCount * 3] = static_cast<float>(lCurrentVertex[0]);
                            lVertices[lVertexCount * 3 + 1] = static_cast<float>(lCurrentVertex[1]);
                            lVertices[lVertexCount * 3 + 2] = static_cast<float>(lCurrentVertex[2]);
                            lMaterialIndices[lVertexCount] = lMaterialIndex + lMaterialIndexBase;
                            vertexControlIndices.push_back(lControlPointIndex);

                            if (lHasNormal)
                            {
                                lMesh->GetPolygonVertexNormal(lPolygonIndex, lVerticeIndex, lCurrentNormal);
                                lNormals[lVertexCount * 3] = static_cast<float>(lCurrentNormal[0]);
                                lNormals[lVertexCount * 3 + 1] = static_cast<float>(lCurrentNormal[1]);
                                lNormals[lVertexCount * 3 + 2] = static_cast<float>(lCurrentNormal[2]);
                            }

                            if (lHasUV)
                            {
                                bool lUnmappedUV;
                                lMesh->GetPolygonVertexUV(lPolygonIndex, lVerticeIndex, lUVName, lCurrentUV, lUnmappedUV);
                                lUVs[lVertexCount * 2] = static_cast<float>(lCurrentUV[0]);
                                lUVs[lVertexCount * 2 + 1] = static_cast<float>(lCurrentUV[1]);
                            }
                        }
                        ++lVertexCount;
                    }
                }
                shape_t shape;
                shape.mesh.indices = lIndices;
                shape.mesh.material_ids = lMaterialIndices;
                shape.mesh.positions = lVertices;
                shape.mesh.normals = lNormals;
                shape.mesh.texcoords = lUVs;
                gShapes.push_back(shape);

                if ((FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin) != 0)
                {
                    delete [] lControlPoints;
                }
                /*
                // For all skins and all clusters, accumulate their deformation and weight
                // on each vertices and store them in lClusterDeformation and lClusterWeight.
                vector<int> vertexJointIndices;
                vertexJointIndices.resize(lMesh->GetControlPointsCount() * 4, -1);
                vector<float> vertexJointWeights;
                vertexJointWeights.resize(lMesh->GetControlPointsCount() * 4, -1);
                vector<vector<float> > jointTransformMatrices;
                int num_frames;

                FbxTimeSpan lTimeLineTimeSpan;
                pScene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
                FbxTime lStartTime = lTimeLineTimeSpan.GetStart();
                FbxTime lEndTime = lTimeLineTimeSpan.GetStop();
                FbxTime lStepTime = (lEndTime - lStartTime) / 100;
                // select the base layer from the animation stack
                FbxAnimStack * lCurrentAnimationStack = pScene->GetSrcObject<FbxAnimStack>(0);
                // we assume that the first animation layer connected to the animation stack is the base layer
                // (this is the assumption made in the FBXSDK)
                FbxAnimLayer *lCurrentAnimLayer = lCurrentAnimationStack->GetMember<FbxAnimLayer>();
                int lSkinCount = lMesh->GetDeformerCount(FbxDeformer::eSkin);
                for (int lSkinIndex = 0; lSkinIndex < lSkinCount && lSkinIndex < 1; ++lSkinIndex)
                {
                    FbxSkin * lSkinDeformer = (FbxSkin *)lMesh->GetDeformer(lSkinIndex, FbxDeformer::eSkin);
                    int lClusterCount = lSkinDeformer->GetClusterCount();
                    for (int lClusterIndex = 0; lClusterIndex < lClusterCount; ++lClusterIndex)
                    {
                        FbxCluster* lCluster = lSkinDeformer->GetCluster(lClusterIndex);
                        if (!lCluster->GetLink())
                            continue;

                        FbxAMatrix globalPosition = FbxAMatrix(FbxVector4(0, 0, 0, 1), FbxVector4(0, 0, 0, 0), FbxVector4(1, 1, 1, 1));
                        
                        num_frames = 0;
                        vector<float> OneJointTransformMatrices;
                        for (FbxTime lTime = lStartTime; lTime < lEndTime; lTime += lStepTime)
                        {
                            FbxAMatrix lVertexTransformMatrix;
                            ComputeClusterDeformation(globalPosition, lMesh, lCluster, lVertexTransformMatrix, lTime, 0);
                            for (int n = 0; n < 16; n++)
                            {
                                OneJointTransformMatrices.push_back(*((double*)(lVertexTransformMatrix) + n));
                            }
                            num_frames++;
                        }
                        jointTransformMatrices.push_back(OneJointTransformMatrices);

                        int lVertexIndexCount = lCluster->GetControlPointIndicesCount();
                        for (int k = 0; k < lVertexIndexCount; ++k)
                        {
                            int lIndex = lCluster->GetControlPointIndices()[k];
                            // Sometimes, the mesh can have less points than at the time of the skinning
                            // because a smooth operator was active when skinning but has been deactivated during export.
                            if (lIndex >= lVertexCount)
                                continue;
                            double lWeight = lCluster->GetControlPointWeights()[k];
                            if (lWeight == 0.0)
                            {
                                continue;
                            }
                            int m = 0;
                            while (m < 4 && vertexJointIndices[lIndex * 4 + m] != -1)
                            {
                                m++;
                            }
                            if (m != 4)
                            {
                                vertexJointIndices[lIndex * 4 + m] = lClusterIndex;
                                vertexJointWeights[lIndex * 4 + m] = (float) lWeight;
                            }
                        } //For each vertex

                    } //lClusterCount
                }
                animation_t animation;
                animation.vertexJointIndices = vertexJointIndices;
                animation.vertexJointWeights = vertexJointWeights;
                animation.jointTransformMatrices = jointTransformMatrices;
                animation.num_frames = num_frames;
                animation.vertexControlIndices = vertexControlIndices;
                gAnimations.push_back(animation);
                */
            }
        }
    }

    const int lChildCount = pNode->GetChildCount();
    for (int lChildIndex = 0; lChildIndex < lChildCount; ++lChildIndex)
    {
        LoadCacheRecursive(pScene, pNode->GetChild(lChildIndex), lGlobalPosition, pTime);
    }
}

// Deform the vertex array with the shapes contained in the mesh.
void ComputeShapeDeformation(FbxMesh* pMesh, FbxTime& pTime, FbxAnimLayer * pAnimLayer, FbxVector4* pVertexArray)
{
    int lVertexCount = pMesh->GetControlPointsCount();

    FbxVector4* lSrcVertexArray = pVertexArray;
    FbxVector4* lDstVertexArray = new FbxVector4[lVertexCount];
    memcpy(lDstVertexArray, pVertexArray, lVertexCount * sizeof(FbxVector4));

    int lBlendShapeDeformerCount = pMesh->GetDeformerCount(FbxDeformer::eBlendShape);
    for (int lBlendShapeIndex = 0; lBlendShapeIndex<lBlendShapeDeformerCount; ++lBlendShapeIndex)
    {
        FbxBlendShape* lBlendShape = (FbxBlendShape*)pMesh->GetDeformer(lBlendShapeIndex, FbxDeformer::eBlendShape);

        int lBlendShapeChannelCount = lBlendShape->GetBlendShapeChannelCount();
        for (int lChannelIndex = 0; lChannelIndex<lBlendShapeChannelCount; ++lChannelIndex)
        {
            FbxBlendShapeChannel* lChannel = lBlendShape->GetBlendShapeChannel(lChannelIndex);
            if (lChannel)
            {
                // Get the percentage of influence on this channel.
                FbxAnimCurve* lFCurve = pMesh->GetShapeChannel(lBlendShapeIndex, lChannelIndex, pAnimLayer);
                if (!lFCurve) continue;
                double lWeight = lFCurve->Evaluate(pTime);

                /*
                If there is only one targetShape on this channel, the influence is easy to calculate:
                influence = (targetShape - baseGeometry) * weight * 0.01
                dstGeometry = baseGeometry + influence

                But if there are more than one targetShapes on this channel, this is an in-between
                blendshape, also called progressive morph. The calculation of influence is different.

                For example, given two in-between targets, the full weight percentage of first target
                is 50, and the full weight percentage of the second target is 100.
                When the weight percentage reach 50, the base geometry is already be fully morphed
                to the first target shape. When the weight go over 50, it begin to morph from the
                first target shape to the second target shape.

                To calculate influence when the weight percentage is 25:
                1. 25 falls in the scope of 0 and 50, the morphing is from base geometry to the first target.
                2. And since 25 is already half way between 0 and 50, so the real weight percentage change to
                the first target is 50.
                influence = (firstTargetShape - baseGeometry) * (25-0)/(50-0) * 100
                dstGeometry = baseGeometry + influence

                To calculate influence when the weight percentage is 75:
                1. 75 falls in the scope of 50 and 100, the morphing is from the first target to the second.
                2. And since 75 is already half way between 50 and 100, so the real weight percentage change
                to the second target is 50.
                influence = (secondTargetShape - firstTargetShape) * (75-50)/(100-50) * 100
                dstGeometry = firstTargetShape + influence
                */

                // Find the two shape indices for influence calculation according to the weight.
                // Consider index of base geometry as -1.

                int lShapeCount = lChannel->GetTargetShapeCount();
                double* lFullWeights = lChannel->GetTargetShapeFullWeights();

                // Find out which scope the lWeight falls in.
                int lStartIndex = -1;
                int lEndIndex = -1;
                for (int lShapeIndex = 0; lShapeIndex<lShapeCount; ++lShapeIndex)
                {
                    if (lWeight > 0 && lWeight <= lFullWeights[0])
                    {
                        lEndIndex = 0;
                        break;
                    }
                    if (lWeight > lFullWeights[lShapeIndex] && lWeight < lFullWeights[lShapeIndex + 1])
                    {
                        lStartIndex = lShapeIndex;
                        lEndIndex = lShapeIndex + 1;
                        break;
                    }
                }

                FbxShape* lStartShape = NULL;
                FbxShape* lEndShape = NULL;
                if (lStartIndex > -1)
                {
                    lStartShape = lChannel->GetTargetShape(lStartIndex);
                }
                if (lEndIndex > -1)
                {
                    lEndShape = lChannel->GetTargetShape(lEndIndex);
                }

                //The weight percentage falls between base geometry and the first target shape.
                if (lStartIndex == -1 && lEndShape)
                {
                    double lEndWeight = lFullWeights[0];
                    // Calculate the real weight.
                    lWeight = (lWeight / lEndWeight) * 100;
                    // Initialize the lDstVertexArray with vertex of base geometry.
                    memcpy(lDstVertexArray, lSrcVertexArray, lVertexCount * sizeof(FbxVector4));
                    for (int j = 0; j < lVertexCount; j++)
                    {
                        // Add the influence of the shape vertex to the mesh vertex.
                        FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lSrcVertexArray[j]) * lWeight * 0.01;
                        lDstVertexArray[j] += lInfluence;
                    }
                }
                //The weight percentage falls between two target shapes.
                else if (lStartShape && lEndShape)
                {
                    double lStartWeight = lFullWeights[lStartIndex];
                    double lEndWeight = lFullWeights[lEndIndex];
                    // Calculate the real weight.
                    lWeight = ((lWeight - lStartWeight) / (lEndWeight - lStartWeight)) * 100;
                    // Initialize the lDstVertexArray with vertex of the previous target shape geometry.
                    memcpy(lDstVertexArray, lStartShape->GetControlPoints(), lVertexCount * sizeof(FbxVector4));
                    for (int j = 0; j < lVertexCount; j++)
                    {
                        // Add the influence of the shape vertex to the previous shape vertex.
                        FbxVector4 lInfluence = (lEndShape->GetControlPoints()[j] - lStartShape->GetControlPoints()[j]) * lWeight * 0.01;
                        lDstVertexArray[j] += lInfluence;
                    }
                }
            }//If lChannel is valid
        }//For each blend shape channel
    }//For each blend shape deformer

    memcpy(pVertexArray, lDstVertexArray, lVertexCount * sizeof(FbxVector4));

    delete[] lDstVertexArray;
}

//Compute the transform matrix that the cluster will transform the vertex.
void ComputeClusterDeformation(FbxAMatrix& pGlobalPosition,
    FbxMesh* pMesh,
    FbxCluster* pCluster,
    FbxAMatrix& pVertexTransformMatrix,
    FbxTime pTime,
    FbxPose* pPose)
{
    FbxCluster::ELinkMode lClusterMode = pCluster->GetLinkMode();

    FbxAMatrix lReferenceGlobalInitPosition;
    FbxAMatrix lReferenceGlobalCurrentPosition;
    FbxAMatrix lAssociateGlobalInitPosition;
    FbxAMatrix lAssociateGlobalCurrentPosition;
    FbxAMatrix lClusterGlobalInitPosition;
    FbxAMatrix lClusterGlobalCurrentPosition;

    FbxAMatrix lReferenceGeometry;
    FbxAMatrix lAssociateGeometry;
    FbxAMatrix lClusterGeometry;

    FbxAMatrix lClusterRelativeInitPosition;
    FbxAMatrix lClusterRelativeCurrentPositionInverse;

    if (lClusterMode == FbxCluster::eAdditive && pCluster->GetAssociateModel())
    {
        pCluster->GetTransformAssociateModelMatrix(lAssociateGlobalInitPosition);
        // Geometric transform of the model
        lAssociateGeometry = GetGeometry(pCluster->GetAssociateModel());
        lAssociateGlobalInitPosition *= lAssociateGeometry;
        lAssociateGlobalCurrentPosition = GetGlobalPosition(pCluster->GetAssociateModel(), pTime, pPose);

        pCluster->GetTransformMatrix(lReferenceGlobalInitPosition);
        // Multiply lReferenceGlobalInitPosition by Geometric Transformation
        lReferenceGeometry = GetGeometry(pMesh->GetNode());
        lReferenceGlobalInitPosition *= lReferenceGeometry;
        lReferenceGlobalCurrentPosition = pGlobalPosition;

        // Get the link initial global position and the link current global position.
        pCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);
        // Multiply lClusterGlobalInitPosition by Geometric Transformation
        lClusterGeometry = GetGeometry(pCluster->GetLink());
        lClusterGlobalInitPosition *= lClusterGeometry;
        lClusterGlobalCurrentPosition = GetGlobalPosition(pCluster->GetLink(), pTime, pPose);

        // Compute the shift of the link relative to the reference.
        //ModelM-1 * AssoM * AssoGX-1 * LinkGX * LinkM-1*ModelM
        pVertexTransformMatrix = lReferenceGlobalInitPosition.Inverse() * lAssociateGlobalInitPosition * lAssociateGlobalCurrentPosition.Inverse() *
            lClusterGlobalCurrentPosition * lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;
    }
    else
    {
        pCluster->GetTransformMatrix(lReferenceGlobalInitPosition);
        lReferenceGlobalCurrentPosition = pGlobalPosition;
        // Multiply lReferenceGlobalInitPosition by Geometric Transformation
        lReferenceGeometry = GetGeometry(pMesh->GetNode());
        lReferenceGlobalInitPosition *= lReferenceGeometry;

        // Get the link initial global position and the link current global position.
        pCluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);
        lClusterGlobalCurrentPosition = GetGlobalPosition(pCluster->GetLink(), pTime, pPose);

        // Compute the initial position of the link relative to the reference.
        lClusterRelativeInitPosition = lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;

        // Compute the current position of the link relative to the reference.
        lClusterRelativeCurrentPositionInverse = lReferenceGlobalCurrentPosition.Inverse() * lClusterGlobalCurrentPosition;

        // Compute the shift of the link relative to the reference.
        pVertexTransformMatrix = lClusterRelativeCurrentPositionInverse * lClusterRelativeInitPosition;
    }
}

// Deform the vertex array in classic linear way.
void ComputeLinearDeformation(FbxAMatrix& pGlobalPosition,
    FbxMesh* pMesh,
    FbxTime& pTime,
    FbxVector4* pVertexArray,
    FbxPose* pPose)
{
    // All the links must have the same link mode.
    FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

    int lVertexCount = pMesh->GetControlPointsCount();
    FbxAMatrix* lClusterDeformation = new FbxAMatrix[lVertexCount];
    memset(lClusterDeformation, 0, lVertexCount * sizeof(FbxAMatrix));

    double* lClusterWeight = new double[lVertexCount];
    memset(lClusterWeight, 0, lVertexCount * sizeof(double));

    if (lClusterMode == FbxCluster::eAdditive)
    {
        for (int i = 0; i < lVertexCount; ++i)
        {
            lClusterDeformation[i].SetIdentity();
        }
    }

    // For all skins and all clusters, accumulate their deformation and weight
    // on each vertices and store them in lClusterDeformation and lClusterWeight.
    int lSkinCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int lSkinIndex = 0; lSkinIndex<lSkinCount; ++lSkinIndex)
    {
        FbxSkin * lSkinDeformer = (FbxSkin *)pMesh->GetDeformer(lSkinIndex, FbxDeformer::eSkin);

        int lClusterCount = lSkinDeformer->GetClusterCount();
        for (int lClusterIndex = 0; lClusterIndex<lClusterCount; ++lClusterIndex)
        {
            FbxCluster* lCluster = lSkinDeformer->GetCluster(lClusterIndex);
            if (!lCluster->GetLink())
                continue;

            FbxAMatrix lVertexTransformMatrix;
            ComputeClusterDeformation(pGlobalPosition, pMesh, lCluster, lVertexTransformMatrix, pTime, pPose);

            int lVertexIndexCount = lCluster->GetControlPointIndicesCount();
            for (int k = 0; k < lVertexIndexCount; ++k)
            {
                int lIndex = lCluster->GetControlPointIndices()[k];

                // Sometimes, the mesh can have less points than at the time of the skinning
                // because a smooth operator was active when skinning but has been deactivated during export.
                if (lIndex >= lVertexCount)
                    continue;

                double lWeight = lCluster->GetControlPointWeights()[k];

                if (lWeight == 0.0)
                {
                    continue;
                }

                // Compute the influence of the link on the vertex.
                FbxAMatrix lInfluence = lVertexTransformMatrix;
                MatrixScale(lInfluence, lWeight);

                if (lClusterMode == FbxCluster::eAdditive)
                {
                    // Multiply with the product of the deformations on the vertex.
                    MatrixAddToDiagonal(lInfluence, 1.0 - lWeight);
                    lClusterDeformation[lIndex] = lInfluence * lClusterDeformation[lIndex];

                    // Set the link to 1.0 just to know this vertex is influenced by a link.
                    lClusterWeight[lIndex] = 1.0;
                }
                else // lLinkMode == FbxCluster::eNormalize || lLinkMode == FbxCluster::eTotalOne
                {
                    // Add to the sum of the deformations on the vertex.
                    MatrixAdd(lClusterDeformation[lIndex], lInfluence);

                    // Add to the sum of weights to either normalize or complete the vertex.
                    lClusterWeight[lIndex] += lWeight;
                }
            }//For each vertex			
        }//lClusterCount
    }

    //Actually deform each vertices here by information stored in lClusterDeformation and lClusterWeight
    for (int i = 0; i < lVertexCount; i++)
    {
        FbxVector4 lSrcVertex = pVertexArray[i];
        FbxVector4& lDstVertex = pVertexArray[i];
        double lWeight = lClusterWeight[i];

        // Deform the vertex if there was at least a link with an influence on the vertex,
        if (lWeight != 0.0)
        {
            lDstVertex = lClusterDeformation[i].MultT(lSrcVertex);
            if (lClusterMode == FbxCluster::eNormalize)
            {
                // In the normalized link mode, a vertex is always totally influenced by the links. 
                lDstVertex /= lWeight;
            }
            else if (lClusterMode == FbxCluster::eTotalOne)
            {
                // In the total 1 link mode, a vertex can be partially influenced by the links. 
                lSrcVertex *= (1.0 - lWeight);
                lDstVertex += lSrcVertex;
            }
        }
    }

    delete[] lClusterDeformation;
    delete[] lClusterWeight;
}

// Deform the vertex array in Dual Quaternion Skinning way.
void ComputeDualQuaternionDeformation(FbxAMatrix& pGlobalPosition,
    FbxMesh* pMesh,
    FbxTime& pTime,
    FbxVector4* pVertexArray,
    FbxPose* pPose)
{
    // All the links must have the same link mode.
    FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

    int lVertexCount = pMesh->GetControlPointsCount();
    int lSkinCount = pMesh->GetDeformerCount(FbxDeformer::eSkin);

    FbxDualQuaternion* lDQClusterDeformation = new FbxDualQuaternion[lVertexCount];
    memset(lDQClusterDeformation, 0, lVertexCount * sizeof(FbxDualQuaternion));

    double* lClusterWeight = new double[lVertexCount];
    memset(lClusterWeight, 0, lVertexCount * sizeof(double));

    // For all skins and all clusters, accumulate their deformation and weight
    // on each vertices and store them in lClusterDeformation and lClusterWeight.
    for (int lSkinIndex = 0; lSkinIndex<lSkinCount; ++lSkinIndex)
    {
        FbxSkin * lSkinDeformer = (FbxSkin *)pMesh->GetDeformer(lSkinIndex, FbxDeformer::eSkin);
        int lClusterCount = lSkinDeformer->GetClusterCount();
        for (int lClusterIndex = 0; lClusterIndex<lClusterCount; ++lClusterIndex)
        {
            FbxCluster* lCluster = lSkinDeformer->GetCluster(lClusterIndex);
            if (!lCluster->GetLink())
                continue;

            FbxAMatrix lVertexTransformMatrix;
            ComputeClusterDeformation(pGlobalPosition, pMesh, lCluster, lVertexTransformMatrix, pTime, pPose);

            FbxQuaternion lQ = lVertexTransformMatrix.GetQ();
            FbxVector4 lT = lVertexTransformMatrix.GetT();
            FbxDualQuaternion lDualQuaternion(lQ, lT);

            int lVertexIndexCount = lCluster->GetControlPointIndicesCount();
            for (int k = 0; k < lVertexIndexCount; ++k)
            {
                int lIndex = lCluster->GetControlPointIndices()[k];

                // Sometimes, the mesh can have less points than at the time of the skinning
                // because a smooth operator was active when skinning but has been deactivated during export.
                if (lIndex >= lVertexCount)
                    continue;

                double lWeight = lCluster->GetControlPointWeights()[k];

                if (lWeight == 0.0)
                    continue;

                // Compute the influence of the link on the vertex.
                FbxDualQuaternion lInfluence = lDualQuaternion * lWeight;
                if (lClusterMode == FbxCluster::eAdditive)
                {
                    // Simply influenced by the dual quaternion.
                    lDQClusterDeformation[lIndex] = lInfluence;

                    // Set the link to 1.0 just to know this vertex is influenced by a link.
                    lClusterWeight[lIndex] = 1.0;
                }
                else // lLinkMode == FbxCluster::eNormalize || lLinkMode == FbxCluster::eTotalOne
                {
                    if (lClusterIndex == 0)
                    {
                        lDQClusterDeformation[lIndex] = lInfluence;
                    }
                    else
                    {
                        // Add to the sum of the deformations on the vertex.
                        // Make sure the deformation is accumulated in the same rotation direction. 
                        // Use dot product to judge the sign.
                        double lSign = lDQClusterDeformation[lIndex].GetFirstQuaternion().DotProduct(lDualQuaternion.GetFirstQuaternion());
                        if (lSign >= 0.0)
                        {
                            lDQClusterDeformation[lIndex] += lInfluence;
                        }
                        else
                        {
                            lDQClusterDeformation[lIndex] -= lInfluence;
                        }
                    }
                    // Add to the sum of weights to either normalize or complete the vertex.
                    lClusterWeight[lIndex] += lWeight;
                }
            }//For each vertex
        }//lClusterCount
    }

    //Actually deform each vertices here by information stored in lClusterDeformation and lClusterWeight
    for (int i = 0; i < lVertexCount; i++)
    {
        FbxVector4 lSrcVertex = pVertexArray[i];
        FbxVector4& lDstVertex = pVertexArray[i];
        double lWeightSum = lClusterWeight[i];

        // Deform the vertex if there was at least a link with an influence on the vertex,
        if (lWeightSum != 0.0)
        {
            lDQClusterDeformation[i].Normalize();
            lDstVertex = lDQClusterDeformation[i].Deform(lDstVertex);

            if (lClusterMode == FbxCluster::eNormalize)
            {
                // In the normalized link mode, a vertex is always totally influenced by the links. 
                lDstVertex /= lWeightSum;
            }
            else if (lClusterMode == FbxCluster::eTotalOne)
            {
                // In the total 1 link mode, a vertex can be partially influenced by the links. 
                lSrcVertex *= (1.0 - lWeightSum);
                lDstVertex += lSrcVertex;
            }
        }
    }

    delete[] lDQClusterDeformation;
    delete[] lClusterWeight;
}

// Deform the vertex array according to the links contained in the mesh and the skinning type.
void ComputeSkinDeformation(FbxAMatrix& pGlobalPosition,
    FbxMesh* pMesh,
    FbxTime& pTime,
    FbxVector4* pVertexArray,
    FbxPose* pPose)
{
    FbxSkin * lSkinDeformer = (FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin);
    FbxSkin::EType lSkinningType = lSkinDeformer->GetSkinningType();

    if (lSkinningType == FbxSkin::eLinear || lSkinningType == FbxSkin::eRigid)
    {
        ComputeLinearDeformation(pGlobalPosition, pMesh, pTime, pVertexArray, pPose);
    }
    else if (lSkinningType == FbxSkin::eDualQuaternion)
    {
        ComputeDualQuaternionDeformation(pGlobalPosition, pMesh, pTime, pVertexArray, pPose);
    }
    else if (lSkinningType == FbxSkin::eBlend)
    {
        int lVertexCount = pMesh->GetControlPointsCount();

        FbxVector4* lVertexArrayLinear = new FbxVector4[lVertexCount];
        memcpy(lVertexArrayLinear, pMesh->GetControlPoints(), lVertexCount * sizeof(FbxVector4));

        FbxVector4* lVertexArrayDQ = new FbxVector4[lVertexCount];
        memcpy(lVertexArrayDQ, pMesh->GetControlPoints(), lVertexCount * sizeof(FbxVector4));

        ComputeLinearDeformation(pGlobalPosition, pMesh, pTime, lVertexArrayLinear, pPose);
        ComputeDualQuaternionDeformation(pGlobalPosition, pMesh, pTime, lVertexArrayDQ, pPose);

        // To blend the skinning according to the blend weights
        // Final vertex = DQSVertex * blend weight + LinearVertex * (1- blend weight)
        // DQSVertex: vertex that is deformed by dual quaternion skinning method;
        // LinearVertex: vertex that is deformed by classic linear skinning method;
        int lBlendWeightsCount = lSkinDeformer->GetControlPointIndicesCount();
        for (int lBWIndex = 0; lBWIndex<lBlendWeightsCount; ++lBWIndex)
        {
            double lBlendWeight = lSkinDeformer->GetControlPointBlendWeights()[lBWIndex];
            pVertexArray[lBWIndex] = lVertexArrayDQ[lBWIndex] * lBlendWeight + lVertexArrayLinear[lBWIndex] * (1 - lBlendWeight);
        }
    }
}

FbxAMatrix GetGlobalPosition(FbxNode* pNode, const FbxTime& pTime, FbxPose* pPose, FbxAMatrix* pParentGlobalPosition)
{
    FbxAMatrix lGlobalPosition;
    bool        lPositionFound = false;

    if (pPose)
    {
        int lNodeIndex = pPose->Find(pNode);

        if (lNodeIndex > -1)
        {
            // The bind pose is always a global matrix.
            // If we have a rest pose, we need to check if it is
            // stored in global or local space.
            if (pPose->IsBindPose() || !pPose->IsLocalMatrix(lNodeIndex))
            {
                lGlobalPosition = GetPoseMatrix(pPose, lNodeIndex);
            }
            else
            {
                // We have a local matrix, we need to convert it to
                // a global space matrix.
                FbxAMatrix lParentGlobalPosition;

                if (pParentGlobalPosition)
                {
                    lParentGlobalPosition = *pParentGlobalPosition;
                }
                else
                {
                    if (pNode->GetParent())
                    {
                        lParentGlobalPosition = GetGlobalPosition(pNode->GetParent(), pTime, pPose);
                    }
                }

                FbxAMatrix lLocalPosition = GetPoseMatrix(pPose, lNodeIndex);
                lGlobalPosition = lParentGlobalPosition * lLocalPosition;
            }

            lPositionFound = true;
        }
    }

    if (!lPositionFound)
    {
        // There is no pose entry for that node, get the current global position instead.

        // Ideally this would use parent global position and local position to compute the global position.
        // Unfortunately the equation 
        //    lGlobalPosition = pParentGlobalPosition * lLocalPosition
        // does not hold when inheritance type is other than "Parent" (RSrs).
        // To compute the parent rotation and scaling is tricky in the RrSs and Rrs cases.
        lGlobalPosition = pNode->EvaluateGlobalTransform(pTime);
    }

    return lGlobalPosition;
}

// Get the matrix of the given pose
FbxAMatrix GetPoseMatrix(FbxPose* pPose, int pNodeIndex)
{
    FbxAMatrix lPoseMatrix;
    FbxMatrix lMatrix = pPose->GetMatrix(pNodeIndex);

    memcpy((double*)lPoseMatrix, (double*)lMatrix, sizeof(lMatrix.mData));

    return lPoseMatrix;
}

// Get the geometry offset to a node. It is never inherited by the children.
FbxAMatrix GetGeometry(FbxNode* pNode)
{
    const FbxVector4 lT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
    const FbxVector4 lR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
    const FbxVector4 lS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

    return FbxAMatrix(lT, lR, lS);
}

// Scale all the elements of a matrix.
void MatrixScale(FbxAMatrix& pMatrix, double pValue)
{
    int i, j;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            pMatrix[i][j] *= pValue;
        }
    }
}


// Add a value to all the elements in the diagonal of the matrix.
void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue)
{
    pMatrix[0][0] += pValue;
    pMatrix[1][1] += pValue;
    pMatrix[2][2] += pValue;
    pMatrix[3][3] += pValue;
}


// Sum two matrices element by element.
void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix)
{
    int i, j;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            pDstMatrix[i][j] += pSrcMatrix[i][j];
        }
    }
}