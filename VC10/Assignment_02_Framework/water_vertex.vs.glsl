#version 420 core

layout(location = 0) in vec3 iv3vertex;

uniform mat4 mvp;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 proj_matrix;
uniform vec3 cameraPosition;

//out vec2 vv3color;
out vec4 clipSpace;
out vec2 texCoord;
out vec3 toCameraVector;
out vec3 vv3vertex;

out float visibility;

const float density = 0.0005;
const float grad = 5;

const float tiling = 3.0;

void main()
{
	vec4 worldPosition = model_matrix * vec4(iv3vertex, 1.0);
	clipSpace = proj_matrix * view_matrix * model_matrix * vec4(iv3vertex, 1.0);
	gl_Position = mvp * vec4(iv3vertex, 1.0);
	texCoord = vec2(iv3vertex.x/2.0+0.5, iv3vertex.z/2.0+0.5) /* tiling*/;
	toCameraVector = cameraPosition - worldPosition.xyz;
	
	vec4 posToCam = view_matrix * worldPosition;
	float dis = length(posToCam.xyz);
	visibility = exp(-pow((dis*density),grad));
	visibility = clamp(visibility,0.3,1.0);
	
	vv3vertex = iv3vertex;
}