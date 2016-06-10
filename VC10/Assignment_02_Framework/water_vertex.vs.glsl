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

const float tiling = 3.0;

void main()
{
	vec4 worldPosition = model_matrix * vec4(iv3vertex, 1.0);
	clipSpace = mvp * vec4(iv3vertex, 1.0);
	gl_Position = clipSpace;
	texCoord = vec2(iv3vertex.x/2.0+0.5, iv3vertex.z/2.0+0.5) /* tiling*/;
	toCameraVector = cameraPosition - worldPosition.xyz;
}