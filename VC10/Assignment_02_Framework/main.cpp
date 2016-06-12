#include <glew.h>
#include <freeglut.h>
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <cstdlib>
#include <tiny_obj_loader.h>
#include "fbxloader.h"
#include <IL/il.h>
#include <vector>
#include <map>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3
#define MENU_ANI_DEAD 4
#define MENU_ANI_WALK 5
#define MENU_ANI_FURY 6
#define SHADOW_MAP_SIZE 4096

const int window_width = 1000;
const int window_height = 1000;

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

using namespace glm;

bool isPress = false;
bool pressCmpBar = false;
float mouseX;
float mouseY;

mat4 mvp;
GLint um4mvp;
GLint control_signal = 0;
GLint cmp_bar = 300;
GLuint um4img_size, um4cmp_bar, um4control_signal, um4mouse_position;
GLuint program, floor_program;

GLfloat mroll = radians(0.0f), mpitch = radians(-16.7f), myaw = radians(300.5f);

mat4 viewMatrix, water_viewMatrix;
mat4 skybox_viewMatrix;
vec3 eyeVector = vec3(830.2f, -642.0f, -445.01f);
//vec3 eyeVector = vec3(-100, -100, 0); 
int animation_flag;
float getDegree(float rad){
	return rad*(180.0f / 3.141592653589f);
}

/*
 *	global lighting switch
 *
 */
bool trigger_lighting = true;
bool trigger_fog = true;
bool trigger_shadow = true;

GLfloat mPos[2];
typedef struct _texture_data
{
	_texture_data() : width(0), height(0), data(0) {}
	int width;
	int height;
	unsigned char* data;
} texture_data;

texture_data load_png(const char* path)
{
    texture_data texture;
	int n;
	stbi_uc *data = stbi_load(path, &texture.width, &texture.height, &n, 4);
	if(data != NULL)
	{
		texture.data = new unsigned char[texture.width * texture.height * 4 * sizeof(unsigned char)];
		memcpy(texture.data, data, texture.width * texture.height * 4 * sizeof(unsigned char));
		// vertical-mirror image data
		for (size_t i = 0; i < texture.width; i++)
		{
			for (size_t j = 0; j < texture.height / 2; j++)
			{
				for(size_t k = 0; k < 4; k++) {
					std::swap(texture.data[(j * texture.width + i) * 4 + k], texture.data[((texture.height - j - 1) * texture.width + i) * 4 + k]);
				}
			}
		}
		stbi_image_free(data);
	}
    return texture;
}

char** loadShaderSource(const char* file)
{
	FILE* fp = fopen(file, "rb");
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *src = new char[sz + 1];
	fread(src, sizeof(char), sz, fp);
	src[sz] = '\0';
	char **srcp = new char*[1];
	srcp[0] = src;
	return srcp;
}

void freeShaderSource(char** srcp)
{
	delete srcp[0];
	delete srcp;
}

void shaderLog(GLuint shader)
{

	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if(isCompiled == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character
		GLchar* errorLog = new GLchar[maxLength];
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

		printf("%s\n", errorLog);
		delete errorLog;
	}
}
static const GLfloat window_vertex[] =
{
	//vec2 position vec2 texture_coord
	1.0f,-1.0f,1.0f,0.0f,
	-1.0f,-1.0f,0.0f,0.0f,
	-1.0f,1.0f,0.0f,1.0f,
	1.0f,1.0f,1.0f,1.0f
};
class SkyBox{

	public :	
		SkyBox(){
			strcpy(filename[0], "CubeMap/skybox_autum_forest_right.png");
			strcpy(filename[1], "CubeMap/skybox_pine_forest_left.png");
			strcpy(filename[2], "CubeMap/skybox_autum_forest_bottom.png");
			strcpy(filename[3], "CubeMap/skybox_autum_forest_top.png");
			strcpy(filename[4], "CubeMap/skybox_pine_forest_front.png");
			strcpy(filename[5], "CubeMap/skybox_autum_forest_back.png");
		};
		GLuint env_tex;
		GLuint vao;
		GLuint program;
		GLuint um4mvp;
		void testFileName(){
			for(int i=0;i<6;i++)
				printf("%s\n", filename[i]);
		}
		void init(){
			// set up skybox texture and vao
			glGenVertexArrays(1, &this->vao);
			glBindVertexArray(this->vao);
			GLfloat cube_vertices[] = {
			  -1.0,  1.0,  1.0,
			  -1.0, -1.0,  1.0,
			   1.0, -1.0,  1.0,
			   1.0,  1.0,  1.0,
			  -1.0,  1.0, -1.0,
			  -1.0, -1.0, -1.0,
			   1.0, -1.0, -1.0,
			   1.0,  1.0, -1.0,
			};
			GLuint vbo_cube_vertices;
			glGenBuffers(1, &vbo_cube_vertices);
			glBindBuffer(GL_ARRAY_BUFFER, vbo_cube_vertices);
			glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

			/*GLushort cube_indices[] = {
			  0, 1, 2, 3,
			  3, 2, 6, 7,
			  7, 6, 5, 4,
			  4, 5, 1, 0,
			  0, 3, 7, 4,
			  1, 2, 6, 5,
			};*/
			GLushort cube_indices[] = {
			  0, 1, 2,
			  3, 0, 2,
			  3, 2, 6,
			  3, 6, 7,
			  0, 3, 4,
			  3, 7, 4,
			  1, 2, 6,
			  6, 5, 1,
			  4, 7, 5,
			  5, 7, 6,
			  1, 0, 4,
			  4, 5, 1
			};
			GLuint ibo_cube_indices;
			glGenBuffers(1, &ibo_cube_indices);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_cube_indices);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

			glGenTextures(1, &this->env_tex);
			glBindTexture(GL_TEXTURE_CUBE_MAP, this->env_tex);
			for(int i=0;i<6;i++){
				texture_data envmap_data = load_png(this->filename[i]);
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, envmap_data.width, envmap_data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, envmap_data.data );
				delete[] envmap_data.data;
			}
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		}
		void draw(mat4 mvp){

			glUseProgram(this->program);
			glBindVertexArray(this->vao);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_CUBE_MAP, this->env_tex);

			glUniformMatrix4fv(this->um4mvp, 1, GL_FALSE, value_ptr(mvp));
			glUniform1i(glGetUniformLocation(this->program, "trigger_fog"), trigger_fog);
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
			
			/*glDisable(GL_DEPTH_TEST);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			glEnable(GL_DEPTH_TEST);*/
		}
	private:	
		char filename[6][100] ;
};
SkyBox skyBox;

class Shape{
	public :
		GLuint vao;
		GLuint buffers[3];
		GLuint iBuffer;
		GLuint mid; // material id
		GLint iBuffer_elements, nVertices, iBufNumber;
		GLuint *midVector; // material ids of indices
		GLuint *iBufVector; // buffers of indices
		GLuint *iBufElement; // elements of each buffer
};

class Scene{
	public :
		GLuint *material_ids; // texture buffers
		GLint (*material_texture_size)[2];
		Shape *shapes;
		GLuint shapeCount;
};

Scene scene;

// zombie parts
class FbxOBJ{
	public :
		Scene scene;
		fbx_handles myFbx;
};
void loadOBJ(Scene &scene, char *file_path){
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;

	//bool ret = tinyobj::LoadObj(shapes, materials, err, "sponza.obj");
	//bool ret = tinyobj::LoadObj(shapes, materials, err, "Small Tropical Island.obj");
	//bool ret = tinyobj::LoadObj(shapes, materials, err, "castle.obj");
	//bool ret = tinyobj::LoadObj(shapes, materials, err, "The City.obj");
	bool ret = tinyobj::LoadObj(shapes, materials, err, file_path);
	//bool ret = tinyobj::LoadObj(shapes, materials, err, "Bulbasaur.obj");
	//bool ret = tinyobj::LoadObj(shapes, materials, err, "ghv.obj");
	//bool ret = tinyobj::LoadObj(shapes, materials, err, "Red_Orange_Flower.obj");
	// TODO: If You Want to Load FBX, Use these. The Returned Values are The Same.
	//fbx_handles myFbx; // Save this Object, You Will Need It to Retrieve Animations Later.
	//bool ret = LoadFbx(myFbx, shapes, materials, err, "FBX_FILE_NAME");

	if(ret)
	{
		// For Each Material
		scene.material_ids = new GLuint[materials.size()];
		scene.material_texture_size = new GLint[materials.size()][2];
		for(int i = 0; i < materials.size(); i++)
		{
			// materials[i].diffuse_texname; // This is the Texture Path You Need
			ILuint ilTexName;
			ilGenImages(1, &ilTexName);
			ilBindImage(ilTexName);
			//printf("texture path :%s\n", materials[i].diffuse_texname.c_str());
			if(ilLoadImage(materials[i].diffuse_texname.c_str()))
			{
				unsigned char *data = new unsigned char[ilGetInteger(IL_IMAGE_WIDTH) * ilGetInteger(IL_IMAGE_HEIGHT) * 4];
				// Image Width = ilGetInteger(IL_IMAGE_WIDTH)
				// Image Height = ilGetInteger(IL_IMAGE_HEIGHT)
				ilCopyPixels(0, 0, 0, ilGetInteger(IL_IMAGE_WIDTH), ilGetInteger(IL_IMAGE_HEIGHT), 1, IL_RGBA, IL_UNSIGNED_BYTE, data);

				// TODO: Generate an OpenGL Texture and use the [unsigned char *data] as Input Here.
				GLuint texture;
				glActiveTexture(GL_TEXTURE0);
				glGenTextures(1, &scene.material_ids[i]);
				scene.material_texture_size[i][0] = ilGetInteger(IL_IMAGE_WIDTH);
				scene.material_texture_size[i][1] = ilGetInteger(IL_IMAGE_HEIGHT);
				glBindTexture(GL_TEXTURE_2D, scene.material_ids[i]);
				if(!strcmp(materials[i].diffuse_texname.c_str(), "Maps/sw1qa.jpg")){
					printf("water id :%d\n", i);
				}
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH), ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glGenerateMipmap(GL_TEXTURE_2D);

				delete[] data;
				ilDeleteImages(1, &ilTexName);
			}
		}

		// For Each Shape (or Mesh, Object)
		scene.shapes = new Shape[shapes.size()];
		scene.shapeCount = shapes.size();
		for(int i = 0; i < shapes.size(); i++) 
		{
			// shapes[i].mesh.positions; // VertexCount * 3 Floats, Load Them to a GL_ARRAY_BUFFER
			// shapes[i].mesh.normals; // VertexCount * 3 Floats, Load Them to a GL_ARRAY_BUFFER
			// shapes[i].mesh.texcoords; // VertexCount * 2 Floats, Load Them to a GL_ARRAY_BUFFER
			// shapes[i].mesh.indices; // TriangleCount * 3 Unsigned Integers, Load Them to a GL_ELEMENT_ARRAY_BUFFER
			// shapes[i].mesh.material_ids[0] // The Material ID of This Shape

			// TODO: 
			// 1. Generate and Bind a VAO
			// 2. Generate and Bind a Buffer for position/normal/texcoord
			// 3. Upload Data to The Buffers
			// 4. Generate and Bind a Buffer for indices (Will Be Saved In The VAO, You Can Restore Them By Binding The VAO)
			// 5. glVertexAttribPointer Calls (Will Be Saved In The VAO, You Can Restore Them By Binding The VAO)
			glGenVertexArrays(1, &scene.shapes[i].vao);
			glBindVertexArray(scene.shapes[i].vao);
			
			std::map<int, std::vector<unsigned int>> dictionary;
			for(int j=0;j<shapes[i].mesh.material_ids.size();j++){
				if(shapes[i].mesh.material_ids[j] == 8){
					printf("water vertices\n");
					printf("v1 : %f\n", shapes[i].mesh.indices[j*3+0]);
					printf("v2 : %f\n", shapes[i].mesh.indices[j*3+1]);
					printf("v3 : %f\n", shapes[i].mesh.indices[j*3+2]);
				}
				dictionary[shapes[i].mesh.material_ids[j]].push_back(shapes[i].mesh.indices[j*3+0]);
				dictionary[shapes[i].mesh.material_ids[j]].push_back(shapes[i].mesh.indices[j*3+1]);
				dictionary[shapes[i].mesh.material_ids[j]].push_back(shapes[i].mesh.indices[j*3+2]);
			}
			
			scene.shapes[i].midVector = new GLuint[dictionary.size()];
			scene.shapes[i].iBufVector = new GLuint[dictionary.size()];
			scene.shapes[i].iBufElement = new GLuint[dictionary.size()];
			std::map<int, std::vector<unsigned int>> :: iterator it;
			int cnt = 0;
			for(it = dictionary.begin();it!=dictionary.end();++it){
				int id = it->first;
				std::vector<unsigned int> indices = it->second;

				scene.shapes[i].iBufElement[cnt] = indices.size();
				scene.shapes[i].midVector[cnt] = id;
				glGenBuffers(1, &scene.shapes[i].iBufVector[cnt]);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene.shapes[i].iBufVector[cnt]);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*indices.size(), &indices[0], GL_STATIC_DRAW);
				cnt++;
			}
			scene.shapes[i].iBufNumber = dictionary.size();

			// the buffer of positions
			glGenBuffers(1, &scene.shapes[i].buffers[0]);
			glBindBuffer(GL_ARRAY_BUFFER, scene.shapes[i].buffers[0]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*shapes[i].mesh.positions.size(), &shapes[i].mesh.positions[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
			// the buffer of normals
			glGenBuffers(1, &scene.shapes[i].buffers[1]);
			glBindBuffer(GL_ARRAY_BUFFER, scene.shapes[i].buffers[1]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*shapes[i].mesh.normals.size(), &shapes[i].mesh.normals[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
			// the buffer of texcoords
			glGenBuffers(1, &scene.shapes[i].buffers[2]);
			glBindBuffer(GL_ARRAY_BUFFER, scene.shapes[i].buffers[2]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*shapes[i].mesh.texcoords.size(), &shapes[i].mesh.texcoords[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
			// indices texcoords
			glGenBuffers(1, &scene.shapes[i].iBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene.shapes[i].iBuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*shapes[i].mesh.indices.size(), &shapes[i].mesh.indices[0], GL_STATIC_DRAW);
			// material ID
			scene.shapes[i].mid = shapes[i].mesh.material_ids[0];
			// number of elements
			scene.shapes[i].iBuffer_elements = shapes[i].mesh.indices.size();
			// number of vertices
			scene.shapes[i].nVertices = shapes[i].mesh.positions.size();
		}
	}
}
void loadFBX(FbxOBJ &fbxObj, char *file_path){
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	//fbx_handles myFbx; // Save this Object, You Will Need It to Retrieve Animations Later.
	bool ret = LoadFbx(fbxObj.myFbx, shapes, materials, err, file_path);
	//bool ret = LoadFbx(fbxObj.myFbx, shapes, materials, err, "testMeow.fbx");
	if(ret)
	{
		// For Each Material
		fbxObj.scene.material_ids = new GLuint[materials.size()];
		for(int i = 0; i < materials.size(); i++)
		{
			// materials[i].diffuse_texname; // This is the Texture Path You Need
			ILuint ilTexName;
			ilGenImages(1, &ilTexName);
			ilBindImage(ilTexName);
			//printf("filename:%s\n", materials[i].diffuse_texname.c_str());
			if(ilLoadImage(materials[i].diffuse_texname.c_str()))
			{
				//puts("================IamHere==============");
				unsigned char *data = new unsigned char[ilGetInteger(IL_IMAGE_WIDTH) * ilGetInteger(IL_IMAGE_HEIGHT) * 4];
				// Image Width = ilGetInteger(IL_IMAGE_WIDTH)
				// Image Height = ilGetInteger(IL_IMAGE_HEIGHT)
				ilCopyPixels(0, 0, 0, ilGetInteger(IL_IMAGE_WIDTH), ilGetInteger(IL_IMAGE_HEIGHT), 1, IL_RGBA, IL_UNSIGNED_BYTE, data);

				// TODO: Generate an OpenGL Texture and use the [unsigned char *data] as Input Here.
				GLuint texture;
				glActiveTexture(GL_TEXTURE0);
				glGenTextures(1, &fbxObj.scene.material_ids[i]);
				glBindTexture(GL_TEXTURE_2D, fbxObj.scene.material_ids[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH), ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
				glGenerateMipmap(GL_TEXTURE_2D);

				delete[] data;
				ilDeleteImages(1, &ilTexName);
			}
		}

		// For Each Shape (or Mesh, Object)
		fbxObj.scene.shapes = new Shape[shapes.size()];
		fbxObj.scene.shapeCount = shapes.size();
		for(int i = 0; i < shapes.size(); i++) 
		{
			// shapes[i].mesh.positions; // VertexCount * 3 Floats, Load Them to a GL_ARRAY_BUFFER
			// shapes[i].mesh.normals; // VertexCount * 3 Floats, Load Them to a GL_ARRAY_BUFFER
			// shapes[i].mesh.texcoords; // VertexCount * 2 Floats, Load Them to a GL_ARRAY_BUFFER
			// shapes[i].mesh.indices; // TriangleCount * 3 Unsigned Integers, Load Them to a GL_ELEMENT_ARRAY_BUFFER
			// shapes[i].mesh.material_ids[0] // The Material ID of This Shape

			// TODO: 
			// 1. Generate and Bind a VAO
			// 2. Generate and Bind a Buffer for position/normal/texcoord
			// 3. Upload Data to The Buffers
			// 4. Generate and Bind a Buffer for indices (Will Be Saved In The VAO, You Can Restore Them By Binding The VAO)
			// 5. glVertexAttribPointer Calls (Will Be Saved In The VAO, You Can Restore Them By Binding The VAO)
			glGenVertexArrays(1, &fbxObj.scene.shapes[i].vao);
			glBindVertexArray(fbxObj.scene.shapes[i].vao);
			
			/*std::map<int, std::vector<unsigned int>> dictionary;
			for(int j=0;j<shapes[i].mesh.material_ids.size();j++){
				dictionary[shapes[i].mesh.material_ids[j]].push_back(shapes[i].mesh.indices[j*3+0]);
				dictionary[shapes[i].mesh.material_ids[j]].push_back(shapes[i].mesh.indices[j*3+1]);
				dictionary[shapes[i].mesh.material_ids[j]].push_back(shapes[i].mesh.indices[j*3+2]);
			}
			
			fbxObj.scene.shapes[i].midVector = new GLuint[dictionary.size()];
			fbxObj.scene.shapes[i].iBufVector = new GLuint[dictionary.size()];
			fbxObj.scene.shapes[i].iBufElement = new GLuint[dictionary.size()];
			std::map<int, std::vector<unsigned int>> :: iterator it;
			int cnt = 0;
			for(it = dictionary.begin();it!=dictionary.end();++it){
				int id = it->first;
				std::vector<unsigned int> indices = it->second;

				fbxObj.scene.shapes[i].iBufElement[cnt] = indices.size();
				fbxObj.scene.shapes[i].midVector[cnt] = id;
				glGenBuffers(1, &fbxObj.scene.shapes[i].iBufVector[cnt]);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fbxObj.scene.shapes[i].iBufVector[cnt]);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*indices.size(), &indices[0], GL_STATIC_DRAW);
				cnt++;
			}
			fbxObj.scene.shapes[i].iBufNumber = dictionary.size();*/

			// the buffer of positions
			glGenBuffers(1, &fbxObj.scene.shapes[i].buffers[0]);
			glBindBuffer(GL_ARRAY_BUFFER, fbxObj.scene.shapes[i].buffers[0]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*shapes[i].mesh.positions.size(), &shapes[i].mesh.positions[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
			// the buffer of normals
			glGenBuffers(1, &fbxObj.scene.shapes[i].buffers[1]);
			glBindBuffer(GL_ARRAY_BUFFER, fbxObj.scene.shapes[i].buffers[1]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*shapes[i].mesh.normals.size(), &shapes[i].mesh.normals[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
			// the buffer of texcoords
			glGenBuffers(1, &fbxObj.scene.shapes[i].buffers[2]);
			glBindBuffer(GL_ARRAY_BUFFER, fbxObj.scene.shapes[i].buffers[2]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*shapes[i].mesh.texcoords.size(), &shapes[i].mesh.texcoords[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
			// indices texcoords
			glGenBuffers(1, &fbxObj.scene.shapes[i].iBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fbxObj.scene.shapes[i].iBuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*shapes[i].mesh.indices.size(), &shapes[i].mesh.indices[0], GL_STATIC_DRAW);
			// material ID
			fbxObj.scene.shapes[i].mid = shapes[i].mesh.material_ids[0];
			// number of elements
			fbxObj.scene.shapes[i].iBuffer_elements = shapes[i].mesh.indices.size();
			// number of vertices
			fbxObj.scene.shapes[i].nVertices = shapes[i].mesh.positions.size();
		}
	}
}

class Terrain{
	public:
		GLuint vao;
		GLuint tex_displacement, tex_color;
		GLuint program;
		GLuint vertex_buffer;
		GLuint loc_mv_matrix, loc_mvp_matrix, loc_proj_matrix, loc_dmap_depth, loc_enable_fog, loc_tex_color, loc_m_matrix, loc_v_matrix;
		GLuint LocTex_heightMap, LocTex_surfaceMap;
		float dmap_depth;
		bool wireframe, enable_fog, enable_displacement;
		void init(){
			// init program
			this->program = glCreateProgram();

			GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
			GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
			GLuint tcs = glCreateShader(GL_TESS_CONTROL_SHADER);
			GLuint tes = glCreateShader(GL_TESS_EVALUATION_SHADER);

			char** vertexShaderSource = loadShaderSource("terrain_vertex.vs.glsl");
			char** fragmentShaderSource = loadShaderSource("terrain_fragment.fs.glsl");
			char** tcsShaderSource = loadShaderSource("terrain_tcs.glsl");
			char** tesShaderSource = loadShaderSource("terrain_tes.glsl");

			glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
			glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
			glShaderSource(tcs, 1, tcsShaderSource, NULL);
			glShaderSource(tes, 1, tesShaderSource, NULL);

			freeShaderSource(vertexShaderSource);
			freeShaderSource(fragmentShaderSource);
			freeShaderSource(tcsShaderSource);
			freeShaderSource(tesShaderSource);

			glCompileShader(vertexShader);
			glCompileShader(fragmentShader);
			glCompileShader(tcs);
			glCompileShader(tes);

			shaderLog(vertexShader);
			shaderLog(fragmentShader);
			shaderLog(tcs);
			shaderLog(tes);

			glAttachShader(this->program, vertexShader);
			glAttachShader(this->program, fragmentShader);
			glAttachShader(this->program, tcs);
			glAttachShader(this->program, tes);

			glLinkProgram(this->program);
			glUseProgram(this->program);

			glGenVertexArrays(1, &this->vao);
			glBindVertexArray(this->vao);

			this->loc_mv_matrix = glGetUniformLocation(this->program, "mv_matrix");
			this->loc_v_matrix = glGetUniformLocation(this->program, "v_matrix");
			this->loc_mvp_matrix = glGetUniformLocation(this->program, "mvp_matrix");
			this->loc_proj_matrix = glGetUniformLocation(this->program, "proj_matrix");
			this->loc_dmap_depth = glGetUniformLocation(this->program, "dmap_depth");
			this->loc_enable_fog = glGetUniformLocation(this->program, "enable_fog");
			this->loc_tex_color = glGetUniformLocation(this->program, "tex_color");
			this->loc_m_matrix = glGetUniformLocation(this->program, "m_matrix");
			this->loc_v_matrix = glGetUniformLocation(this->program, "v_matrix");

			this->dmap_depth = 307.0f;

			//texture_data tdata =  load_png("Terrain/heightmap.png");
			printf("load Terrain/test.png\n");
			texture_data tdata =  load_png("Terrain/test.png");
			//texture_data tdata =  load_png("Terrain/terragen.png");

			glEnable(GL_TEXTURE_2D);
			glGenTextures( 1, &this->tex_displacement );
			glBindTexture( GL_TEXTURE_2D, this->tex_displacement);
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, tdata.width, tdata.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata.data );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

			//texture_data tdata2 =  load_png("Terrain/grassFlowers.png");
			printf("load Terrain/grass_texture1.png\n");
			texture_data tdata2 =  load_png("Terrain/grass_texture1.png");
	
			glGenTextures( 1, &this->tex_color );
			glBindTexture( GL_TEXTURE_2D, this->tex_color);
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, tdata2.width, tdata2.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata2.data );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	
			glPatchParameteri(GL_PATCH_VERTICES, 4);

			

			this->enable_displacement =true;
			this->wireframe = false;
			this->enable_fog = true;

		}
		void draw(mat4& M, mat4& V, mat4& P, bool trigger_fog){
			glEnable(GL_CULL_FACE);
			static const GLfloat black[] = { 0.85f, 0.95f, 1.0f, 1.0f };
			static const GLfloat one = 1.0f;

			float f_timer_cnt = glutGet(GLUT_ELAPSED_TIME);
			float currentTime = f_timer_cnt* 0.001f;
			float f = (float)currentTime;

			static double last_time = 0.0;
			static double total_time = 0.0;
	
			total_time += (currentTime - last_time);
			last_time = currentTime;

			float t = (float)total_time * 0.03f;
			float r = sinf(t * 5.37f) * 15.0f + 16.0f;
			float h = cosf(t * 4.79f) * 2.0f + 3.2f;
			glBindVertexArray(this->vao);
			//glClearBufferfv(GL_COLOR, 0, black);
			//glClearBufferfv(GL_DEPTH, 0, &one);

			glUseProgram(this->program);

			//mat4 mv_matrix = lookAt(vec3(sinf(t) * r, h, cosf(t) * r), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f)) * translate(mat4(), vec3(0, -10, 0));
			//mat4 mv_matrix = lookAt(vec3(0, -eyeVector.y, 0), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
			mat4 m_matrix = rotate(scale(translate(mat4(), vec3(-3140, 10, -1900)), vec3(24.0, 2.5, 28.0)), radians(-30.0f), vec3(0.0, 1.0, 0.0));
			mat4 mv_matrix = V  * m_matrix;
			mat4 proj_matrix = P;
			//mat4 proj_matrix = mat4();
			glUniformMatrix4fv(this->loc_mv_matrix, 1, GL_FALSE, value_ptr(mv_matrix));
			glUniformMatrix4fv(this->loc_m_matrix, 1, GL_FALSE, value_ptr(m_matrix));
			glUniformMatrix4fv(this->loc_v_matrix, 1, GL_FALSE, value_ptr(V));
			glUniformMatrix4fv(this->loc_proj_matrix, 1, GL_FALSE, value_ptr(proj_matrix));
			glUniformMatrix4fv(this->loc_mvp_matrix, 1, GL_FALSE, value_ptr(proj_matrix * mv_matrix));
			glUniform1f(this->loc_dmap_depth, this->enable_displacement ? this->dmap_depth : 0.0f);
			glUniform1i(this->loc_enable_fog, trigger_fog ? 1 : 0);
			glUniform1i(this->loc_tex_color, 1);


			if (wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			else
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			glActiveTexture(GL_TEXTURE0);
			GLuint texLoc  = glGetUniformLocation(this->program, "tex_displacement");
			glUniform1i(texLoc, 0);
			glBindTexture( GL_TEXTURE_2D, this->tex_displacement);

			glActiveTexture(GL_TEXTURE1);
			texLoc  = glGetUniformLocation(this->program, "texcolor");
			glUniform1i(texLoc, 1);
			glBindTexture( GL_TEXTURE_2D, this->tex_color);

			glDrawArraysInstanced(GL_PATCHES, 0, 4, 128 * 128);
			glDisable(GL_CULL_FACE);
		}
		void decrease_dmap_depth(){
			printf("curremt dmap_depth:%f\n", this->dmap_depth);
			this->dmap_depth -= 1.0f;
		}
		void increase_dmap_depth(){
			printf("curremt dmap_depth:%f\n", this->dmap_depth);
			this->dmap_depth += 1.0f;
		}
		void toggle_enable_fog(){
			enable_fog = !enable_fog;
		}
		void toggle_enable_displacement(){
			enable_displacement = !enable_displacement;
		}
		void toggle_enable_wireframe(){
			wireframe = !wireframe;
		}
};
Terrain terrain;
class WaterRendering{
	public :
		GLuint vao;
		GLuint reflectionFbo, refractionFbo;
		GLuint reflectionDepthrbo, refractionDepthrbo;
		GLuint program;
		GLuint window_vertex_buffer;
		GLuint reflectionFboDataTexture, refractionFboDataTexture, dudvMapTexture;
		float moveFactor;
		void init(){
			moveFactor = 0;
			glGenVertexArrays(1, &this->vao);
			glBindVertexArray(this->vao);

			// set up VAO and VBO
			const float vertices[] = {
				170, 92, -350,
				-170, 92, -350,
				170, 92, 350,
				-170, 92, -350,
				-170, 92, 350,
				170, 92, 350
			};
			glGenBuffers(1, &this->window_vertex_buffer);
			glBindBuffer(GL_ARRAY_BUFFER, this->window_vertex_buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices),
			vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, 0);
			glEnableVertexAttribArray( 0 );
			// set up FBO and TEXTURE
			// refraction fbo
			glGenFramebuffers( 1, &this->refractionFbo);//Create FBO
			glGenRenderbuffers( 1, &this->refractionDepthrbo); //Create Depth RBO
			glBindRenderbuffer( GL_RENDERBUFFER, this->refractionDepthrbo);
			glRenderbufferStorage( GL_RENDERBUFFER,GL_DEPTH_COMPONENT32,
			window_width, window_height);
			
			// reflection fbo
			glGenFramebuffers( 1, &this->reflectionFbo);//Create FBO
			glGenRenderbuffers( 1, &this->reflectionDepthrbo); //Create Depth RBO
			glBindRenderbuffer( GL_RENDERBUFFER, this->reflectionDepthrbo);
			glRenderbufferStorage( GL_RENDERBUFFER,GL_DEPTH_COMPONENT32,
			window_width, window_height);

			// TEXTURE doesn't have initial value.
			// refraction texture
			glGenTextures( 1, &this->refractionFboDataTexture);//Create fobDataTexture
			glBindTexture( GL_TEXTURE_2D, this->refractionFboDataTexture);
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
			window_width, window_height, 0, GL_RGBA,GL_UNSIGNED_BYTE, NULL);
			glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR );
			glBindFramebuffer( GL_DRAW_FRAMEBUFFER, this->refractionFbo );
			//Set depthrbo to current fbo
			glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->refractionDepthrbo);
			//Set buffer texture to current fbo
			glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->refractionFboDataTexture, 0 );

			// reflection texture
			glGenTextures( 1, &this->reflectionFboDataTexture);//Create fobDataTexture
			glBindTexture( GL_TEXTURE_2D, this->reflectionFboDataTexture);
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
			window_width, window_height, 0, GL_RGBA,GL_UNSIGNED_BYTE, NULL);
			glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR );
			glBindFramebuffer( GL_DRAW_FRAMEBUFFER, this->reflectionFbo );
			//Set depthrbo to current fbo
			glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->reflectionDepthrbo);
			//Set buffer texture to current fbo
			glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->reflectionFboDataTexture, 0 );

			// distortion dudv map
			glGenTextures(1, &this->dudvMapTexture);
			glBindTexture(GL_TEXTURE_2D, this->dudvMapTexture);

			printf("load Maps/waterDUDV.png\n");
			//texture_data map_data = load_png("Maps/waterDUDV.png");
			texture_data map_data = load_png("Maps/new_waterDUDV.png");
			//glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, envmap_data.width, envmap_data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, envmap_data.data );
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, map_data.width, map_data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, map_data.data);
			delete[] map_data.data;

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT );
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT );
		}
};
WaterRendering waterRendering;
class SecondFrame{
	public :
		GLuint vao;
		GLuint fbo;
		GLuint depthrbo;
		GLuint program;
		GLuint window_vertex_buffer;
		GLuint fboDataTexture;
};
SecondFrame secondFrame;
enum Mode{START_MENU,GAME_MODE,VIEW_MODE};
Mode gameMode;
class GameUI{
	public:
		GLuint vao;
		GLuint vbo;
		GLuint tex[10];
		GLuint program;
		GLuint mapPress;
		GLuint setPress;
		void GameStartMenu()
		{
			glGenVertexArrays(1, &this->vao);
			glBindVertexArray(this->vao);
			static const GLfloat window_vertex[] =
			{
				//LOGO
				0.75f*window_height/window_width,-0.15f,1.0f,0.0f,
				-0.75f*window_height/window_width,-0.15f,0.0f,0.0f,
				-0.75f*window_height/window_width,0.6f,0.0f,1.0f,
				0.75f*window_height/window_width,0.6f,1.0f,1.0f,
				//GAME MODE
				0.3f*window_height/window_width,-0.45f,1.0f,0.0f,
				-0.3f*window_height/window_width,-0.45f,0.0f,0.0f,
				-0.3f*window_height/window_width,-0.25f,0.0f,1.0f,
				0.3f*window_height/window_width,-0.25f,1.0f,1.0f,
				//VIEW MODE
				0.3f*window_height/window_width,-0.7f,1.0f,0.0f,
				-0.3f*window_height/window_width,-0.7f,0.0f,0.0f,
				-0.3f*window_height/window_width,-0.5f,0.0f,1.0f,
				0.3f*window_height/window_width,-0.5f,1.0f,1.0f,
				//SETTING LIST
				-0.35f*window_height/window_width,-1.0f,1.0f,0.0f,
				-1.0f*window_height/window_width,-1.0f,0.0f,0.0f,
				-1.0f*window_height/window_width,0.3f,0.0f,1.0f,
				-0.35f*window_height/window_width,0.3f,1.0f,1.0f,
				//MENU
				-0.8f*window_height/window_width,-0.95f,1.0f,0.0f,
				-0.95f*window_height/window_width,-0.95f,0.0f,0.0f,
				-0.95f*window_height/window_width,-0.8f,0.0f,1.0f,
				-0.8f*window_height/window_width,-0.8f,1.0f,1.0f,
				//LIST
				-0.62f*window_height/window_width,-0.95f,1.0f,0.0f,
				-0.77f*window_height/window_width,-0.95f,0.0f,0.0f,
				-0.77f*window_height/window_width,-0.8f,0.0f,1.0f,
				-0.62f*window_height/window_width,-0.8f,1.0f,1.0f,
				//SETTINGS
				-0.44f*window_height/window_width,-0.95f,1.0f,0.0f,
				-0.59f*window_height/window_width,-0.95f,0.0f,0.0f,
				-0.59f*window_height/window_width,-0.8f,0.0f,1.0f,
				-0.44f*window_height/window_width,-0.8f,1.0f,1.0f

			};
			/*const float vertices[] = {
				-1,1,-1,-1,1,1,1,-1,1,1,1,-1
			};*/
			glGenBuffers(1, &this->vbo);
			glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(window_vertex),window_vertex, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)* 4, 0);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)* 4, (const GLvoid*)(sizeof(GL_FLOAT)* 2));
			glEnableVertexAttribArray( 0 );
			glEnableVertexAttribArray( 1 );

			char filename[10][100];
			strcpy(filename[0], "GameUI/pokemonLogo.png");
			strcpy(filename[1], "GameUI/gameMode.png");
			strcpy(filename[2], "GameUI/viewMode.png");
			strcpy(filename[3], "GameUI/settingList1.png");
			strcpy(filename[4], "GameUI/home-icon2.png");
			strcpy(filename[5], "GameUI/Book-icon.png");
			strcpy(filename[6], "GameUI/settings.png");


			for(int i=0;i<7;i++)
			{
				glActiveTexture(GL_TEXTURE0);
				glGenTextures(1, &this->tex[i]);
				glBindTexture(GL_TEXTURE_2D, this->tex[i]);

				printf("load %s\n",filename[i]);
				texture_data logo_data = load_png(filename[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logo_data.width, logo_data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, logo_data.data);
				delete[] logo_data.data;
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

		}
		void DrawMenu()
		{
			glUseProgram(this->program);
			glBindVertexArray(this->vao);
			for(int i=0;i<3;i++){
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, this->tex[i]);
				glDrawArrays(GL_TRIANGLE_FAN,4*i,4);
			}
		}
		void DrawMode()
		{
			glUseProgram(this->program);
			glBindVertexArray(this->vao);
			for(int i=4;i<7;i++){
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, this->tex[i]);
				glDrawArrays(GL_TRIANGLE_FAN,4*i,4);
			}
		}
		void DrawSettingList()
		{
			glUseProgram(this->program);
			glBindVertexArray(this->vao);
			for(int i=3;i<4;i++){
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, this->tex[i]);
				glDrawArrays(GL_TRIANGLE_FAN,4*i,4);
			}
		}

};
GameUI gameUI;

class Shadow{
	public:
		GLuint fbo;
		GLuint depthrbo;
		GLuint fboDataTexture;
		GLuint program;
		void init(){
			this->program = glCreateProgram();

			GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
			GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

			char** vertexShaderSource = loadShaderSource("shadow_vertex.vs.glsl");
			char** fragmentShaderSource = loadShaderSource("shadow_fragment.fs.glsl");

			glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
			glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);

			freeShaderSource(vertexShaderSource);
			freeShaderSource(fragmentShaderSource);

			glCompileShader(vertexShader);
			glCompileShader(fragmentShader);

			shaderLog(vertexShader);
			shaderLog(fragmentShader);

			glAttachShader(this->program, vertexShader);
			glAttachShader(this->program, fragmentShader);

			glLinkProgram(this->program);
			glUseProgram(this->program);

			// set up fbo and texture
			glGenFramebuffers( 1, &this->fbo);//Create FBO
			glBindFramebuffer(GL_FRAMEBUFFER, this->fbo);

			// TEXTURE doesn't have initial value.
			// fbo texture
			glGenTextures( 1, &this->fboDataTexture);//Create fobDataTexture
			glBindTexture( GL_TEXTURE_2D, this->fboDataTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0,  GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			//Set buffer texture to current fbo
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, this->fboDataTexture, 0);



		}
};

Shadow shadow;
class Player{
	public:
		// 0 for standing, 1 for walking, and 2 for attacking
		FbxOBJ playerAnimation[3];
		int animationState;
		mat4 model_matrix;
		mat4 proj_matrix;
		//t_position for transform 
		vec3 t_position ;
		//position for player actual position
		vec3 position ;
		//turn for turn right , left , back
		float turn ;
		void loadContent(){
			loadFBX(playerAnimation[0], "Pokemon/trainerStand.fbx");
			loadFBX(playerAnimation[1], "Pokemon/trainer3.fbx");
			loadFBX(playerAnimation[2], "Pokemon/trainerThrow1.fbx");
			this->animationState = 1;

			this->t_position = vec3(-800, 50, -180);
			this->position = vec3(800, -283.13f, 253.04);
			this->turn = 0.0f;

			//this->model_matrix = translate(mat4(),vec3(-800,50,-180))*scale(mat4(),vec3(4.0f,8.0f,4.0f))*rotate(mat4(),radians(180.0f),vec3(0,0,1))*rotate(mat4(),radians(-90.0f),vec3(1,0,0));
			this->model_matrix = translate(mat4(), position)*scale(mat4(), vec3(4.0f, 15.0f, 4.0f))*rotate(mat4(), radians(180.0f), vec3(0, 0, 1))*rotate(mat4(), radians(-90.0f), vec3(1, 0, 0));
			this->proj_matrix = perspective(radians(60.0f), 1.0f,0.3f, 10000.0f);
		}
		void updatePosition(vec3 dv, float angle){
			this->t_position += dv;
			position -= dv;
			turn = angle;
			this->model_matrix =
			translate(mat4(), t_position)*
			scale(mat4(), vec3(4.0f, 15.0f, 4.0f))*
			rotate(mat4(), radians(180.0f), vec3(0, 0, 1))
			*rotate(mat4(), radians(-90.0f), vec3(1, 0, 0))
			*rotate(mat4(), radians(angle), vec3(0, 0, 1));// -:left turn, +:right turn

		}
		void updateContent(){
			std::vector<tinyobj::shape_t> new_shapes;
			float timer = ( (float) timer_cnt/255.0f);
			GetFbxAnimation(this->playerAnimation[animationState].myFbx, new_shapes, timer); // The Last Parameter is A Float in [0, 1], Specifying The Animation Location You Want to Retrieve
			for(int i = 0; i < new_shapes.size(); i++)
			{
				glBindVertexArray (this->playerAnimation[animationState].scene.shapes[i].vao);
				glBindBuffer(GL_ARRAY_BUFFER, this->playerAnimation[animationState].scene.shapes[i].buffers[0]);
				glBufferData(GL_ARRAY_BUFFER, new_shapes[i].mesh.positions.size() * sizeof(float), 
							&new_shapes[i].mesh.positions[0], GL_STATIC_DRAW);
			}
		}
		void draw(bool isLighting){
			
			glActiveTexture(GL_TEXTURE0);
			for(int i=0;i<this->playerAnimation[animationState].scene.shapeCount;i++){
				// 1. Bind The VAO of the Shape
				// 3. Bind Textures
				// 4. Update Uniform Values by glUniform*
				// 5. glDrawElements Call
				glBindVertexArray (this->playerAnimation[animationState].scene.shapes[i].vao);
				mat4 mvp1 = proj_matrix *
					viewMatrix *
					translate(mat4(),vec3(-300,70,-180))*scale(mat4(),vec3(4.0f,8.0f,4.0f))*rotate(mat4(),radians(180.0f),vec3(0,0,1))*rotate(mat4(),radians(-90.0f),vec3(1,0,0));
				
				glUniformMatrix4fv(glGetUniformLocation(program, "um4mvp"), 1, GL_FALSE, value_ptr(mvp1));
				glUniformMatrix4fv(glGetUniformLocation(program, "M"), 1, GL_FALSE, value_ptr(this->model_matrix));
				glUniformMatrix4fv(glGetUniformLocation(program, "V"), 1, GL_FALSE, value_ptr(viewMatrix));
				glUniformMatrix4fv(glGetUniformLocation(program, "P"), 1, GL_FALSE, value_ptr(this->proj_matrix));
				glUniform1i(glGetUniformLocation(program, "trigger_lighting"), isLighting);
				glUniform1i(glGetUniformLocation(program, "trigger_shadow"), trigger_shadow);

				glBindTexture(GL_TEXTURE_2D,this->playerAnimation[animationState].scene.material_ids[this->playerAnimation[animationState].scene.shapes[i].mid]);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->playerAnimation[animationState].scene.shapes[i].iBuffer);
				glDrawElements(GL_TRIANGLES, this->playerAnimation[animationState].scene.shapes[i].iBuffer_elements, GL_UNSIGNED_INT, 0);
			}
		}
		void ready(){
			this->model_matrix = translate(mat4(),vec3(-80,50,-180))*scale(mat4(),vec3(4.0f,8.0f,4.0f))*rotate(mat4(),radians(180.0f),vec3(0,0,1))*rotate(mat4(),radians(-90.0f),vec3(1,0,0));
		}
		void start(){
			this->model_matrix = translate(mat4(),vec3(-80,50,-180))*scale(mat4(),vec3(4.0f,8.0f,4.0f))*rotate(mat4(),radians(180.0f),vec3(0,0,1))*rotate(mat4(),radians(-90.0f),vec3(1,0,0));
		}
		vec3 getPosition(){
			return position;
		}

};
Player player;
class Camera{
	public:
		float distanceFromPlayer;
		float angleAroundPlayer;
		vec3 position;
		float pitch;
		float yaw;
		float roll;
		float playerx, playery, playerz;
		Player* player;
		int  mode3D ; // 3 -> 3rd person perspective,    1 -> 1st person perspective
		Camera(Player *player, vec3 position){
			this->player = player;
			this->position = position;

			// initialize
			this->distanceFromPlayer = 500;
			this->angleAroundPlayer = 0;

			this->pitch = 2.205892f;
			this->yaw = 385.87f;
			this->roll = 0.0f;
			this->mode3D = 3;
		}
		bool isMode3D(){
			return mode3D;
		}
		void setCamera(vec3 position){
			this->position = position;
		}
		void updateCamera(vec3 position, float roll, float yaw, float pitch){
			this->position = position;
			this->roll = roll;
			this->pitch = pitch;
			this->yaw = yaw;
		}
		vec3 getPosition(){
			return position;
		}
		float getPitch(){
			return pitch;
		}
		float getRoll(){
			return roll;
		}
		float getYaw(){
			return yaw;
		}
		void setPlayer(Player *player){
			this->player = player;
		}

		void move(){
			float horizontalDistance = calculateHorizontalDistance();
			float verticalDistance = calculateVerticalDistance();
			calculateCameraPosition(horizontalDistance, verticalDistance);
		}
		void calculateCameraPosition(float horizontalDistance, float verticalDistance){
			//float theta = player.getRotY() + angleAroundPlayer;
			printf("--------------player-turn:%f\n", player->turn);
			float theta = angleAroundPlayer;//+ player->turn;
			float offsetX = (float)(horizontalDistance * sin(radians(theta)));
			float offsetZ = (float)(horizontalDistance * cos(radians(theta)));

			if (mode3D == 3){
				position.x = player->position.x - offsetX;//+40;
				position.z = player->position.z - offsetZ;//+ 700.0f;
				printf("^^^offsetX:%.5f, offsetZ:%.5f, vertiD:%f\n", offsetX, offsetZ, verticalDistance);
				position.y = player->position.y - verticalDistance;
				printf("---------------------\n[1]player.position:(%f, %f, %f)\n", player->position.x, player->position.y, player->position.z);
				printf("---------------------\n[1]camera.position:(%f, %f, %f)\n", position.x, position.y, position.z);
			}
			else if (mode3D == 1){
				position.x = player->position.x;
				position.z = player->position.z;// + 20.0f;
				position.y = player->position.y;



				angleAroundPlayer = -player->turn;
				yaw = player->turn;

				printf("---------------------\n[2]player.position:(%f, %f, %f)\n", player->position.x, player->position.y, player->position.z);
				printf("---------------------\n[2]player.yaw:%f, pitch: %f, angle:%f\n", this->yaw, this->pitch, this->angleAroundPlayer);

			}
		}

		void setZoom(float zoomLevel){
			if (distanceFromPlayer > 800.0f && zoomLevel > 0)
				return;
			distanceFromPlayer += zoomLevel;
			move();

			printf("--in camera.setZoon: distance:%f\n", distanceFromPlayer);
		}
		void setPitch(float pitchChange){
			pitch += pitchChange;
			printf("--in camera.setPitch: pitch:%f\n", pitch);
			//move(playerx, playery, playerz+0.1*(pitchChange>0.0f)?1:-1);

			if (pitchChange > 0){
				move();
			}
			else {
				move();
			}

		}
		void setYaw(float yawChange){
			yaw += yawChange;
			angleAroundPlayer -= yawChange;
			printf("--in camera.setYawn: yaw:%f\n", yaw);
		}
		void calculateAngleAroundPlayer(float angleChange){
			angleAroundPlayer += angleChange;
		}
		float calculateHorizontalDistance(){
			return (float)(distanceFromPlayer * cos(radians(pitch)));
			printf("in set hori--in camera.setPitch: pitch:%f\n", pitch);
		}
		float calculateVerticalDistance(){
			return (float)(distanceFromPlayer * sin(radians(pitch)));
		}
};
Camera camera(&player, eyeVector);
FbxOBJ zombie[3];
char zombie_filename[3][100] = {
	"zombie_dead.FBX",
	"zombie_walk.FBX",
	"zombie_fury.FBX"
};
void checkError(const char *functionName)
{
    GLenum error;
    while (( error = glGetError() ) != GL_NO_ERROR) {
        fprintf (stderr, "GL error 0x%X detected in %s\n", error, functionName);
    }
}
// Print OpenGL context related information.
void dumpInfo(void)
{
    printf("Vendor: %s\n", glGetString (GL_VENDOR));
    printf("Renderer: %s\n", glGetString (GL_RENDERER));
    printf("Version: %s\n", glGetString (GL_VERSION));
    printf("GLSL: %s\n", glGetString (GL_SHADING_LANGUAGE_VERSION));
}
void updateView()
{
	mat4 matRoll = mat4(1.0f);
	mat4 matPitch = mat4(1.0f);
	mat4 matYaw = mat4(1.0f);

	matRoll = rotate(matRoll, mroll, vec3(0.0f,0.0f,1.0f));
	matPitch = rotate(matPitch, mpitch, vec3(1.0f,0.0f,0.0f));
	matYaw = rotate(matYaw, myaw, vec3(0.0f,1.0f,0.0f));

	mat4 rot = matRoll*matPitch*matYaw;

	mat4 trans = mat4(1.0f);
	trans = translate(trans, eyeVector);
	skybox_viewMatrix = matPitch*matYaw * translate(mat4(1.0), vec3(eyeVector.x, eyeVector.y, eyeVector.z));
	float angle = -2.5f ;
	water_viewMatrix = matPitch/*rotate(mat4(), radians(angle)+mpitch, vec3(1.0f,0.0f,0.0f))*/ * matYaw * translate(mat4(1.0), vec3(eyeVector.x, eyeVector.y, eyeVector.z));
	viewMatrix = rot *  trans;

	/*if (camera.isMode3D()){
		camera.setPlayer(&player);
		matRoll = mat4(1.0f);
		matPitch = mat4(1.0f);
		matYaw = mat4(1.0f);

		matRoll = rotate(matRoll, radians(camera.getRoll()), vec3(0.0f, 0.0f, 1.0f));
		matPitch = rotate(matPitch, radians(camera.pitch), vec3(1.0f, 0.0f, 0.0f));
		matYaw = rotate(matYaw, radians(camera.yaw), vec3(0.0f, 1.0f, 0.0f));

		rot = matRoll*matPitch*matYaw;
		trans = mat4(1.0f);
		trans = translate(trans, camera.position);
		skybox_viewMatrix = matPitch*matYaw * translate(mat4(1.0), camera.position);
		float angle = -2.5f;
		water_viewMatrix = matPitch *
			matYaw *
			translate(mat4(1.0), camera.position);
		viewMatrix = rot*  trans;
		
		puts("++++++++++++_____________________");
		printf("In, update view, camera.position:%f, %f, %f\n", camera.position.x, camera.position.y, camera.position.z);
		printf("In, update view, pitch:%f, yaw:%f\n", camera.pitch, camera.yaw);
		printf("In, update view, distance:%f\n", camera.distanceFromPlayer);
		printf("In, update view, player.position:%f, %f, %f\n", player.position.x, player.position.y, player.position.z);
		printf("In, update view, eyeVector:%f, %f, %f\n", eyeVector.x, eyeVector.y, eyeVector.z);

		puts("++++++++++++_____________________");
		
	}*/
}
void My_Init()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	// initialize camera
	camera.position = eyeVector;
	camera.yaw = getDegree(myaw);
	camera.pitch = getDegree(mpitch);
	camera.roll = getDegree(mroll);
	camera.setPlayer(&player);
	camera.move();

	// the program for skybox
	skyBox.program = glCreateProgram();
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource = loadShaderSource("skybox_vertex.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("skybox_frag.fs.glsl");
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
	shaderLog(vertexShader);
    shaderLog(fragmentShader);
    glAttachShader(skyBox.program, vertexShader);
    glAttachShader(skyBox.program, fragmentShader);
    glLinkProgram(skyBox.program);
	skyBox.um4mvp = glGetUniformLocation(skyBox.program, "um4mvp");
    glUseProgram(skyBox.program);

	// the program for frame buffer rendering
	secondFrame.program = glCreateProgram();
    GLuint vertexShader2 = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader2 = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource2 = loadShaderSource("s_vertex.vs.glsl");
	char** fragmentShaderSource2 = loadShaderSource("s_frag.fs.glsl");
    glShaderSource(vertexShader2, 1, vertexShaderSource2, NULL);
    glShaderSource(fragmentShader2, 1, fragmentShaderSource2, NULL);
	freeShaderSource(vertexShaderSource2);
	freeShaderSource(fragmentShaderSource2);
    glCompileShader(vertexShader2);
    glCompileShader(fragmentShader2);
	shaderLog(vertexShader2);
    shaderLog(fragmentShader2);
    glAttachShader(secondFrame.program, vertexShader2);
    glAttachShader(secondFrame.program, fragmentShader2);
    glLinkProgram(secondFrame.program);
	um4img_size = glGetUniformLocation(secondFrame.program, "img_size");
	um4cmp_bar = glGetUniformLocation(secondFrame.program, "cmp_bar");
	um4control_signal = glGetUniformLocation(secondFrame.program, "control_signal");
	um4mouse_position = glGetUniformLocation(secondFrame.program, "mouse_position");
	
		
	// the program of first scene 
    program = glCreateProgram();
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	vertexShaderSource = loadShaderSource("vertex.vs.glsl");
	fragmentShaderSource = loadShaderSource("fragment.fs.glsl");
    glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
	shaderLog(vertexShader);
    shaderLog(fragmentShader);
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
	um4mvp = glGetUniformLocation(program, "um4mvp");
    glUseProgram(program);

	// the program for water rendering
    waterRendering.program = glCreateProgram();
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	vertexShaderSource = loadShaderSource("water_vertex.vs.glsl");
	fragmentShaderSource = loadShaderSource("water_frag.fs.glsl");
    glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
	shaderLog(vertexShader);
    shaderLog(fragmentShader);
    glAttachShader(waterRendering.program, vertexShader);
    glAttachShader(waterRendering.program, fragmentShader);
    glLinkProgram(waterRendering.program);

	// the program for GameUI
	gameUI.program = glCreateProgram();
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	vertexShaderSource = loadShaderSource("gameUI_vertex.vs.glsl");
	fragmentShaderSource = loadShaderSource("gameUI_frag.fs.glsl");
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
	shaderLog(vertexShader);
    shaderLog(fragmentShader);
    glAttachShader(gameUI.program, vertexShader);
    glAttachShader(gameUI.program, fragmentShader);
    glLinkProgram(gameUI.program);

	// enable some settings
	//glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// initialize FAO
	glGenVertexArrays(1, &secondFrame.vao);
	glBindVertexArray(secondFrame.vao);


	// set up VAO and VBO
	glGenBuffers(1, &secondFrame.window_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, secondFrame.window_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(window_vertex),
	window_vertex, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*4, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*4, (const GLvoid*)(sizeof(GL_FLOAT)*2));
	glEnableVertexAttribArray( 0 );
	glEnableVertexAttribArray( 1 );
	// set up FBO and TEXTURE
	glGenFramebuffers( 1, &secondFrame.fbo);//Create FBO
	glGenRenderbuffers( 1, &secondFrame.depthrbo); //Create Depth RBO
	glBindRenderbuffer( GL_RENDERBUFFER, secondFrame.depthrbo);
	glRenderbufferStorage( GL_RENDERBUFFER,GL_DEPTH_COMPONENT32,
	window_width, window_height);
	glGenTextures( 1, &secondFrame.fboDataTexture);//Create fobDataTexture
	glBindTexture( GL_TEXTURE_2D, secondFrame.fboDataTexture);
	// TEXTURE doesn't have initial value.
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
	window_width, window_height, 0, GL_RGBA,GL_UNSIGNED_BYTE, NULL);
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, secondFrame.fbo );
	//Set depthrboto current fbo
	glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, secondFrame.depthrbo);
	//Set buffertextureto current fbo
	glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, secondFrame.fboDataTexture, 0 );

	// initialize skyBox
	skyBox.init();
	// initialize water rendering
	waterRendering.init();
	glEnable(GL_CLIP_DISTANCE0);
	// initialize terrain
	terrain.init();
	//GameUI init
	gameMode = START_MENU;
	gameUI.GameStartMenu();
	// initialize shadow object
	shadow.init();
}

void My_LoadModels()
{

	// load the landscape
	loadOBJ(scene, "Habitat.obj");

	player.loadContent();
	/*for(int k=0;k<3;k++){
		loadFBX(zombie[k], zombie_filename[k]);
	}*/
}

void drawOBJ(GLuint programForDraw, Scene& scene, mat4& mvp, mat4& M, mat4& V, mat4& P, vec4& plane_equation, bool isLighting, bool isFog, bool isShadow){
	glUseProgram(programForDraw);
	//printf("program inside : %u\n", programForDraw);
	for(int i = 0; i < scene.shapeCount; i++)
	{

		glBindVertexArray (scene.shapes[i].vao);

		glUniformMatrix4fv(glGetUniformLocation(programForDraw, "um4mvp"), 1, GL_FALSE, value_ptr(mvp));
		glUniformMatrix4fv(glGetUniformLocation(programForDraw, "M"), 1, GL_FALSE, value_ptr(M));
		glUniformMatrix4fv(glGetUniformLocation(programForDraw, "V"), 1, GL_FALSE, value_ptr(V));
		glUniformMatrix4fv(glGetUniformLocation(programForDraw, "P"), 1, GL_FALSE, value_ptr(P));
		glUniform4fv(glGetUniformLocation(programForDraw, "plane"), 1, &plane_equation[0]);
		glUniform3fv(glGetUniformLocation(programForDraw, "camera_position"), 1, &eyeVector[0]);
		glUniform1i(glGetUniformLocation(programForDraw, "trigger_lighting"), isLighting);
		glUniform1i(glGetUniformLocation(programForDraw, "trigger_fog"), isFog);
		glUniform1i(glGetUniformLocation(programForDraw, "trigger_shadow"), isShadow);

		// abandoned codes which used to draw scnens with only material_ids[0]
		/*glBindTexture(GL_TEXTURE_2D,scene.material_ids[scene.shapes[i].mid]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene.shapes[i].iBuffer);
		glDrawElements(GL_TRIANGLES, scene.shapes[i].iBuffer_elements, GL_UNSIGNED_INT, 0);*/
		if(programForDraw == program){
			glActiveTexture(GL_TEXTURE1);
			GLuint texLoc  = glGetUniformLocation(program, "shadow_tex");
			glUniform1i(texLoc, 1);
			glBindTexture( GL_TEXTURE_2D, shadow.fboDataTexture);
		}
		glActiveTexture(GL_TEXTURE0);
		for(int j=0;j<scene.shapes[i].iBufNumber;j++){

			glBindTexture(GL_TEXTURE_2D,scene.material_ids[scene.shapes[i].midVector[j]]);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene.shapes[i].iBufVector[j]);
			glDrawElements(GL_TRIANGLES, scene.shapes[i].iBufElement[j], GL_UNSIGNED_INT, 0);
		}
		//glDrawArrays(GL_TRIANGLES,0,scene.shapes[i].nVertices);

	}
}
// GLUT callback. Called to draw the scene.
void My_Display()
{
	glActiveTexture(GL_TEXTURE0);
	//glBindFramebuffer( GL_DRAW_FRAMEBUFFER,secondFrame.fbo);

	// update view
	mat4 P = perspective(radians(60.0f), 1.0f,0.3f, 100000.0f);
	mat4 M = scale(mat4(), vec3(8000, 8000, 8000));
	updateView();

	//which render buffer attachment is written
	glDrawBuffer( GL_COLOR_ATTACHMENT0);
	glViewport( 0, 0, window_width, window_height);
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//draw skybox
	mat4 mvp = P*
	viewMatrix*
	scale(mat4(), vec3(8000, 8000, 8000));
	skyBox.draw(mvp);

	// TODO: For Your FBX Model, Get New Animation Here
	/*std::vector<tinyobj::shape_t> new_shapes;
	float timer = ( (float) timer_cnt/255.0f);
	GetFbxAnimation(zombie[animation_flag].myFbx, new_shapes, timer); // The Last Parameter is A Float in [0, 1], Specifying The Animation Location You Want to Retrieve
	for(int i = 0; i < new_shapes.size(); i++)
	{
		glBindVertexArray (zombie[animation_flag].scene.shapes[i].vao);
		glBindBuffer(GL_ARRAY_BUFFER, zombie[animation_flag].scene.shapes[i].buffers[0]);
		glBufferData(GL_ARRAY_BUFFER, new_shapes[i].mesh.positions.size() * sizeof(float), 
					&new_shapes[i].mesh.positions[0], GL_STATIC_DRAW);
	}*/
	player.updateContent();


	glUseProgram(program);
	//draw water refraction
	glEnable(GL_CLIP_DISTANCE0);
	float const water_height = 40;
	float camera_distance = 2*fabs(eyeVector.y - (water_height));

	updateView();
	M = scale(mat4(), vec3(5, 20, 5)) * scale(mat4(), vec3(1, 0.5, 1)) * translate(mat4(),vec3(0,-90,0));
	mvp = P*
	viewMatrix *
	M;
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER,waterRendering.refractionFbo);
	//glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0);
	glDrawBuffer( GL_COLOR_ATTACHMENT0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawOBJ(program, scene, mvp, M, viewMatrix, P, vec4(0, -1, 0, water_height), 0, 0, 0);
	//drawOBJ(scene, mvp, M, vec4(0, 1, 0, -1000000));


	// draw water reflection
	eyeVector.y += camera_distance;
	mpitch = -mpitch;
	updateView();
	glUseProgram(program);
	M = scale(mat4(), vec3(5, 20, 5)) * scale(mat4(), vec3(1, 0.5, 1)) * translate(mat4(),vec3(0,-90,0));
	mvp = P*
	water_viewMatrix *
	M;
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER,waterRendering.reflectionFbo);
	//glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0);
	glDrawBuffer( GL_COLOR_ATTACHMENT0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	player.draw(0);
	drawOBJ(program, scene, mvp, M, water_viewMatrix, P, vec4(0, 1, 0, -water_height), 0, 0, 0);
	//drawOBJ(scene, mvp, M, vec4(0, 0, 0, 1000000), 0);
	mpitch = -mpitch;
	eyeVector.y -= camera_distance;
	updateView();

	/*
	 *	shadow depth map rendering
	 */
	
		//printf("program outside : %u\n", shadow.program);
		glCullFace(GL_FRONT);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glClearColor(1.0, 1.0, 1.0, 1.0);
		updateView();
		glUseProgram(shadow.program);
		glBindFramebuffer(GL_FRAMEBUFFER, shadow.fbo);
		//glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//glDrawBuffer(GL_NONE); 
		//glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0);
		//glDrawBuffer( GL_COLOR_ATTACHMENT0);

		mat4 mat = viewMatrix;
	 
		vec3 forward(mat[0][2], mat[1][2], mat[2][2]);
		vec3 target = forward * length(eyeVector) + eyeVector;
		mat4 light_proj_matrix = glm::ortho(-2500.0f, 2500.0f, -2500.0f, 2500.0f, 1.0f, 2000.0f);
		mat4 light_view_matrix = lookAt(vec3(-100, 100, 0), target, vec3(0, 1, 0));
		//mat4 light_view_matrix = viewMatrix;

		mat4 matRoll = mat4(1.0f);
		mat4 matPitch = mat4(1.0f);
		mat4 matYaw = mat4(1.0f);

		matRoll = rotate(matRoll, mroll, vec3(0.0f,0.0f,1.0f));
		matPitch = rotate(matPitch, mpitch, vec3(1.0f,0.0f,0.0f));
		matYaw = rotate(matYaw, myaw, vec3(0.0f,1.0f,0.0f));

		mat4 rot = matRoll*matPitch*matYaw;

		mat4 trans = mat4(1.0f);
		//trans = translate(trans, vec3(-100, -1600, 0));
		trans = translate(trans, vec3(150, -500, 150));

		light_view_matrix = rotate(mat4(1.0f), radians(90.0f), vec3(1.0f, 0.0f, 0.0f)) * trans;

		mat4 scale_bias_matrix = mat4(
			vec4(0.5f, 0.0f, 0.0f, 0.0f),
			vec4(0.0f, 0.5f, 0.0f, 0.0f),
			vec4(0.0f, 0.0f, 0.5f, 0.0f),
			vec4(0.5f, 0.5f, 0.5f, 1.0f)
		);
		mat4 light_vp_matrix = light_proj_matrix * light_view_matrix;

		mat4 shadow_sbpv_matrix = scale_bias_matrix * light_vp_matrix;
		M = scale(mat4(), vec3(5, 20, 5)) * scale(mat4(), vec3(1, 0.5, 1)) * translate(mat4(),vec3(0,-90,0));
		
		glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(4.0f, 4.0f);
		player.draw(0);
		drawOBJ(shadow.program, scene, light_vp_matrix * M, M, light_view_matrix, light_proj_matrix, vec4(0, 0, 0, 1000000), trigger_lighting, 0, 0);
		glDisable(GL_POLYGON_OFFSET_FILL);
	
	// draw city
	glCullFace(GL_BACK);
	glUseProgram(program);
	glViewport( 0, 0, window_width, window_height);
	M = scale(mat4(), vec3(5, 20, 5)) * scale(mat4(), vec3(1, 0.5, 1)) * translate(mat4(),vec3(0,-90,0));
	mvp = P*
	viewMatrix *
	M;
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0);
	glDrawBuffer( GL_COLOR_ATTACHMENT0);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	mat4 shadow_matrix = shadow_sbpv_matrix * M;
	glUniformMatrix4fv(glGetUniformLocation(program, "shadow_matrix"), 1, GL_FALSE, value_ptr(shadow_matrix));
	drawOBJ(program, scene, mvp, M, viewMatrix, P, vec4(0, 0, 0, 1000000), trigger_lighting, trigger_fog, trigger_shadow);


	// draw terrian
	M = mat4();
	terrain.draw(M, viewMatrix, P, trigger_fog);

	// draw water
	M = scale(mat4(), vec3(5, 20, 5)) * scale(mat4(), vec3(1, 0.5, 1)) * translate(mat4(),vec3(0,-90,0));
	mvp = P*
	viewMatrix *
	M;
	glUseProgram(waterRendering.program);
	glBindVertexArray(waterRendering.vao);	
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0);
	glViewport( 0, 0, window_width, window_height);
	//glClear( GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);
	
	GLuint texLoc;
	glActiveTexture(GL_TEXTURE0);
	texLoc  = glGetUniformLocation(waterRendering.program, "reflectionTex");
	glUniform1i(texLoc, 0);
	glBindTexture( GL_TEXTURE_2D, waterRendering.reflectionFboDataTexture);

	glActiveTexture(GL_TEXTURE1);
	texLoc  = glGetUniformLocation(waterRendering.program, "refractionTex");
	glUniform1i(texLoc, 1);
	glBindTexture( GL_TEXTURE_2D, waterRendering.refractionFboDataTexture);

	glActiveTexture(GL_TEXTURE2);
	texLoc  = glGetUniformLocation(waterRendering.program, "dudvMap");
	glUniform1i(texLoc, 2);
	glBindTexture( GL_TEXTURE_2D, waterRendering.dudvMapTexture);

	int f = glutGet(GLUT_ELAPSED_TIME);
	if(timer_cnt % 9 == 0 ){
		waterRendering.moveFactor += 0.0003 * f * 0.001f;
	}

	glUniformMatrix4fv(glGetUniformLocation(waterRendering.program, "mvp"), 1, GL_FALSE, value_ptr(mvp));
	glUniformMatrix4fv(glGetUniformLocation(waterRendering.program, "model_matrix"), 1, GL_FALSE, value_ptr(M));
	glUniformMatrix4fv(glGetUniformLocation(waterRendering.program, "view_matrix"), 1, GL_FALSE, value_ptr(water_viewMatrix));
	glUniformMatrix4fv(glGetUniformLocation(waterRendering.program, "proj_matrix"), 1, GL_FALSE, value_ptr(P));
	glUniform3fv(glGetUniformLocation(waterRendering.program, "cameraPosition"), 1, &eyeVector[0]);
	glUniform1f(glGetUniformLocation(waterRendering.program, "moveFactor"), waterRendering.moveFactor);
	glDrawArrays(GL_TRIANGLES,0,6 );

	if(gameMode == START_MENU)
	{
		gameUI.DrawMenu();
	}
	else if(gameMode == GAME_MODE)
	{
		if(gameUI.setPress) gameUI.DrawSettingList();
		gameUI.DrawMode();

	}
	else if(gameMode == VIEW_MODE)
	{
		if(gameUI.setPress) gameUI.DrawSettingList();
		gameUI.DrawMode();
	}

	glUseProgram(program);
	player.draw(trigger_lighting);

	/*glUseProgram(program);
	// draw the zombie
	for(int i=0;i<zombie[animation_flag].scene.shapeCount;i++){
		// 1. Bind The VAO of the Shape
		// 3. Bind Textures
		// 4. Update Uniform Values by glUniform*
		// 5. glDrawElements Call
		glBindVertexArray (zombie[animation_flag].scene.shapes[i].vao);
		mat4 mvp1 =perspective(radians(60.0f), 1.0f,0.3f, 3000.0f)*
			viewMatrix *
			translate(mat4(),vec3(-120,50,0))*scale(mat4(),vec3(4.0f,4.0f,4.0f))*rotate(mat4(),radians(180.0f),vec3(0,0,1))*rotate(mat4(),radians(90.0f),vec3(1,0,0));
		M = translate(mat4(),vec3(-120,50,0))*scale(mat4(),vec3(4.0f,4.0f,4.0f))*rotate(mat4(),radians(180.0f),vec3(0,0,1))*rotate(mat4(),radians(90.0f),vec3(1,0,0));
		glUniformMatrix4fv(glGetUniformLocation(program, "um4mvp"), 1, GL_FALSE, value_ptr(mvp1));
		glUniformMatrix4fv(glGetUniformLocation(program, "M"), 1, GL_FALSE, value_ptr(M));

		glBindTexture(GL_TEXTURE_2D,zombie[animation_flag].scene.material_ids[zombie[animation_flag].scene.shapes[i].mid]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, zombie[animation_flag].scene.shapes[i].iBuffer);
		glDrawElements(GL_TRIANGLES, zombie[animation_flag].scene.shapes[i].iBuffer_elements, GL_UNSIGNED_INT, 0);
	}*/

	// frame buffer rendering

	
	/*glUseProgram(program);
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0);
	glDrawBuffer( GL_COLOR_ATTACHMENT0);
	//glViewport( 0, 0, window_width, window_height);
	glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
	glClear( GL_COLOR_BUFFER_BIT| GL_DEPTH_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_2D, shadow.fboDataTexture);

	glUseProgram(secondFrame.program);
	glBindVertexArray(secondFrame.vao);	
	static const GLfloat img_size[] = {window_width, window_height};
	glUniform2fv(um4img_size, 1, img_size);
	glUniform1i(um4cmp_bar, cmp_bar);
	glUniform1i(um4control_signal, control_signal);
	glDrawArrays(GL_TRIANGLE_FAN,0,4 );*/

    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
	glViewport(0, 0, width, height);

	//float viewportAspect = (float)width / (float)height;
	//mvp = ortho(-10 * viewportAspect, 10 * viewportAspect, -10.0f, 10.0f, 0.0f, 10.0f);
	//mvp = perspective(radians(90.0f), viewportAspect, 0.1f, 10000.0f);
	//mvp = mvp * lookAt(vec3(200.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
}

void My_Timer(int val)
{
	timer_cnt++;
	glutPostRedisplay();
	if(timer_enabled)
	{
		glutTimerFunc(timer_speed, My_Timer, val);
	}
}

void onMouseMotion(int x, int y)
{
	if(isPress == false)
		return;
	//printf("mouse move x:%d, y:%d\n", x, y);
	vec2 delta = vec2(x,y)-vec2(mouseX, mouseY);
	if(!pressCmpBar){

		float mxSen = 0.005f;
		float mySen = 0.005f;

		myaw -= mxSen * delta.x;
		mpitch -= mySen * delta.y;
		if (camera.mode3D){
			camera.setPitch(getDegree(-mySen * delta.y));
			camera.setYaw(getDegree(-mxSen * delta.x));
		}

		mouseX = x;
		mouseY = y;

		updateView();
	}
	else{
		cmp_bar += delta.x;
		mouseX = x;
		mouseY = y;
	}
}

void My_Mouse(int button, int state, int x, int y)
{
	//Edit , new dx, dz
	float dx = 0;
	float dz = 0;
	//////////Edit!! : button == 0
	if (button == 0 && state == GLUT_DOWN)
	{
		if(control_signal == 5){
			mPos[0] = x;
			mPos[1] = window_height - y;
			glUniform2fv(um4mouse_position, 1, mPos);
		}
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
		printf("mPitch:%f roll:%f yaw:%f\n", degrees(mpitch), degrees(mroll), degrees(myaw));
		isPress = true;
		mouseX = x;
		mouseY = y;
		if(x<=cmp_bar+10 && x>=cmp_bar-10)
			pressCmpBar = true;

		if(gameMode == START_MENU)
		{
			gameUI.mapPress = 0;
			gameUI.setPress = 0;
			if(x>=350 && x<=650 && y>=500 && y<=600) gameMode = GAME_MODE;
			else if(x>=350 && x<=650 && y>=625 && y<=720) gameMode = VIEW_MODE;
		}
		else if(gameMode == GAME_MODE || gameMode == VIEW_MODE)
		{
			if((pow((x-62.5),2) + pow((y-812.5),2)) <= 1407) gameMode = START_MENU; //START MENU
			else if((pow((x-162.5),2) + pow((y-812.5),2)) <= 1407){ //MAP
				gameUI.mapPress = 1 - gameUI.mapPress;
				gameUI.setPress = 0;
			}
			else if((pow((x-262.5),2) + pow((y-812.5),2)) <= 1407){ //SETTINGS
				gameUI.mapPress = 0;
				gameUI.setPress = 1 - gameUI.setPress;
			}
			else if(gameUI.setPress && (x>325 || y<300)){ //SETING LIST

				gameUI.mapPress = 0;
				gameUI.setPress = 0;
			}
			else{ //DEBUG
				//gameUI.mapPress = 0;
				//gameUI.setPress = 0;
			}
		}
		/*else{
			gameMode = START_MENU;
		}*/
		
	}
	//////////Edit!! : button == 0
	else if (button == 0 && state == GLUT_UP)
	{
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
		isPress = false;
		pressCmpBar = false;
	}
	//////////////Edit, in camera mode , zoom in and zoom out
	else if (button == 3){
		//zoom out 
		camera.setZoom(-2);
		printf("camera.distance:%f\n", camera.distanceFromPlayer);
	}
	else if (button == 4){
		//zoom in 
		camera.setZoom(2);
		printf("camera.distance:%f\n", camera.distanceFromPlayer);
	}
	//////////////////
	mat4 mat = viewMatrix;

	vec3 forward(mat[0][2], mat[1][2], mat[2][2]);
	vec3 strafe(mat[0][0], mat[1][0], mat[2][0]);

	const float speed = 1.6f;//how fast we move
	eyeVector.z += dz*speed;
	updateView();
}

void My_Keyboard(unsigned char key, int x, int y)
{
	//printf("Key %c is pressed at (%d, %d)\n", key, x, y);
	float dx = 0; 
	float dz = 0;
	switch (key)
	{
		case '8':
		{
					camera.angleAroundPlayer -= 3.0f;
					camera.yaw += 3.0f;
					break;
		}
			////////////////////////////////For moderate: camera turn right
		case '9':
		{
					camera.angleAroundPlayer += 3.0f;
					camera.yaw -= 3.0f;
					break;
		}
			////////////////////////////////For Debug: show the eyeVector coordinate
		case 'p':
		{
					printf("eyevector:(%f, %f, %f)\n", eyeVector.x, eyeVector.y, eyeVector.z);
					printf("roll:%f, pitch:%f, yaw:%f\n", mroll, mpitch, myaw);
					break;
		}
			////////////////////////////////For Debug: show the camera coordinate
		case '[':
		{
					printf("camera.position:(%f, %f, %f)\n", camera.position.x, camera.position.y, camera.position.z);
					printf("roll:%f, pitch:%f, yaw:%f\n", radians(camera.roll), radians(camera.pitch), radians(camera.yaw));
					puts("----------------------------------------------------------------------");
					break;
		}
			////////////////////////////////For Debug: show the player coordinate
		case ']':
		{
					//player position
					printf("player.position:(%f, %f, %f)\n", player.position.x, player.position.y, player.position.z);
					break;
		}
			////////////////////////////////Edit: change to view  mode
		case '2':
		{
					camera.mode3D = 0;
					break;
		}
			////////////////////////////////Edit: change to 3rd person perspective mode
		case '3':
		{
					// 3rd perspective
					if (camera.mode3D != 3){
						camera.angleAroundPlayer = 0;
						camera.yaw = 363;
						camera.mode3D = 3;
						camera.move();
						updateView();
					}
					break;
		}
			////////////////////////////////Edit: change to 1rd person perspective mode
		case '1':
		{
					//1st perspective

					if (camera.mode3D != 1){
						camera.angleAroundPlayer = 0;
						camera.yaw = 363;
						camera.mode3D = 1;
						camera.move();
						updateView();
					}
					break;
		}
		case 'w':
			{
			  dz = -2;
			  break;
			}
		case 's':
			{
			  dz = 2;
			  break;
			}
		case 'a':
			{
			  dx = 2;
			  break;
			}
		case 'd':
			{
			  dx = -2;
			  break;
			}
		case 'z':
			{
				eyeVector[1] -= 4;
				break;
			}
		case 'x':
			{
				eyeVector[1] += 4;
				break;
			}
		case 'y':
			control_signal = 0;
			glUniform1i(um4control_signal, control_signal);
			break;
		case 'u':
			control_signal = 1;
			glUniform1i(um4control_signal, control_signal);
			break;
		case 'i':
			control_signal = 2;
			glUniform1i(um4control_signal, control_signal);
			break;
		case 'o':
			control_signal = 3;
			glUniform1i(um4control_signal, control_signal);
			break;
		case 'h':
			control_signal = 4;
			glUniform1i(um4control_signal, control_signal);
			break;
		case 'j':
			control_signal = 5;
			glUniform1i(um4control_signal, control_signal);

			mPos[0] = 300;
			mPos[1] = 300;
			glUniform2fv(um4mouse_position, 1, mPos);
			break;
		case 'k':
			control_signal = 6;
			break;
		case 't':
			printf("current position: %f %f %f\n", eyeVector[0], eyeVector[1], eyeVector[2]);
			break;
		case 'n':
			terrain.increase_dmap_depth();
			break;
		case 'm':
			terrain.decrease_dmap_depth();
			break;
		case 'c':
			terrain.toggle_enable_displacement();
			break;
		case 'v':
			terrain.toggle_enable_fog();
			break;
		case 'b':
			terrain.toggle_enable_wireframe();
			break;
	   default:
		 break;
	 }
	mat4 mat = viewMatrix;
	 
	vec3 forward(mat[0][2], mat[1][2], mat[2][2]);
	vec3 strafe( mat[0][0], mat[1][0], mat[2][0]);
  
	const float speed = 1.6f;//how fast we move
 
	 
	eyeVector += (-dz * forward + dx * strafe) * speed;
    
	updateView();
}

void My_SpecialKeys(int key, int x, int y)
{
	float dx = 0;
	float dz = 0;
	switch(key)
	{
	case GLUT_KEY_F1:
		//printf("F1 is pressed at (%d, %d)\n", x, y);
		trigger_lighting = !trigger_lighting;
		break;
	case GLUT_KEY_F2:
		//printf("F1 is pressed at (%d, %d)\n", x, y);
		trigger_fog = !trigger_fog;
		break;
	case GLUT_KEY_F3:
		//printf("F1 is pressed at (%d, %d)\n", x, y);
		trigger_shadow = !trigger_shadow;
		break;
	case GLUT_KEY_PAGE_UP:
		printf("Page up is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_UP:
		player.updatePosition(vec3(0.0, 0.0, -3.0), 0.0f);
		camera.move();
		printf("up is pressed at (%d, %d)\n", x, y);
		break;
		////////////////////Edit  : move backward
	case GLUT_KEY_DOWN:
		player.updatePosition(vec3(0.0, 0.0, 3.0), 180.0f);
		//dz = 2;
		camera.move();
		printf("Down arrow is pressed at (%d, %d)\n", x, y);
		break;
		////////////////////Edit  : move to left
	case GLUT_KEY_LEFT:
		player.updatePosition(vec3(-3.0, 0.0, 0.0), -90.0f);
		//dx = 2;
		camera.move();
		printf("Left arrow is pressed at (%d, %d)\n", x, y);
		break;

		////////////////////Edit  : move to right
	case GLUT_KEY_RIGHT:
		player.updatePosition(vec3(3.0, 0.0, 0.0), 90.0f);
		//dx = -2;
		camera.move();
		printf("right arrow is pressed at (%d, %d)\n", x, y);
		break;
	default:
		printf("Other special key is pressed at (%d, %d)\n", x, y);
		break;
	}
	mat4 mat = viewMatrix;

	vec3 forward(mat[0][2], mat[1][2], mat[2][2]);
	vec3 strafe(mat[0][0], mat[1][0], mat[2][0]);

	const float speed = 1.6f;//how fast we move


	eyeVector += (-dz * forward + dx * strafe) * speed;

	updateView();
}

void My_Menu(int id)
{
	switch(id)
	{
	case MENU_TIMER_START:
		if(!timer_enabled)
		{
			timer_enabled = true;
			glutTimerFunc(timer_speed, My_Timer, 0);
		}
		break;
	case MENU_TIMER_STOP:
		timer_enabled = false;
		break;
	case MENU_EXIT:
		exit(0);
		break;
	case MENU_ANI_DEAD:
		animation_flag = 0;
		break;
	case MENU_ANI_WALK:
		animation_flag = 1;
		break;
	case MENU_ANI_FURY:
		animation_flag = 2;
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
	skyBox.testFileName();
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowPosition(500, 0);
	glutInitWindowSize(window_width, window_height);
	glutCreateWindow("Assignment 03 102062309"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
	glewInit();
	ilInit();
    ilEnable(IL_ORIGIN_SET);
    ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
	dumpInfo();
	My_Init();
	My_LoadModels();
	////////////////////

	// Create a menu and bind it to mouse right button.
	////////////////////////////
	int menu_main = glutCreateMenu(My_Menu);
	int menu_timer = glutCreateMenu(My_Menu);
	int menu_animation = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	glutAddSubMenu("Timer", menu_timer);
	glutAddSubMenu("Animation", menu_animation);
	glutAddMenuEntry("Exit", MENU_EXIT);

	glutSetMenu(menu_timer);
	glutAddMenuEntry("Start", MENU_TIMER_START);
	glutAddMenuEntry("Stop", MENU_TIMER_STOP);

	glutSetMenu(menu_animation);
	glutAddMenuEntry("Dead", MENU_ANI_DEAD);
	glutAddMenuEntry("Walk", MENU_ANI_WALK);
	glutAddMenuEntry("Fury", MENU_ANI_FURY);

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
	////////////////////////////

	// Register GLUT callback functions.
	///////////////////////////////
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutMotionFunc  (onMouseMotion);
	glutTimerFunc(timer_speed, My_Timer, 0); 
	///////////////////////////////

	// Enter main event loop.
	//////////////
	glutMainLoop();
	//////////////
	return 0;
}