#pragma once

#define FBXSDK_SHARED
#include <fbxsdk.h>
#include <vector>
#include <string>
#include <tiny_obj_loader.h>

typedef struct _fbx_handles
{
    _fbx_handles()
    {
        lSdkManager = NULL;
        lScene = NULL;
    }

    FbxManager* lSdkManager;
    FbxScene* lScene;
    FbxArray<FbxString*> lAnimStackNameArray;
} fbx_handles;

void GetFbxAnimation(fbx_handles &handles, std::vector<tinyobj::shape_t> &shapes, float frame);
bool LoadFbx(fbx_handles &handles, std::vector<tinyobj::shape_t> &shapes, std::vector<tinyobj::material_t> &materials, std::string err, const char* fbxFile);
void ReleaseFbx(fbx_handles &handles);