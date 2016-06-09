#version 420 core

layout(location = 0) in vec3 iv3vertex;

uniform mat4 mvp;

//out vec2 vv3color;
out vec4 clipSpace;
out vec2 texCoord;

const float tiling = 6.0;

void main()
{
	clipSpace = mvp * vec4(iv3vertex, 1.0);
	gl_Position = clipSpace;
	texCoord = vec2(iv3vertex.x/2.0+0.5, iv3vertex.y/2.0+0.5) * tiling;
}