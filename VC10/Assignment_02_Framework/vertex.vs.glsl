#version 410

layout(location = 0) in vec3 iv3vertex;
layout(location = 1) in vec3 iv3normal;
layout(location = 2) in vec2 iv2texcoord;

uniform mat4 um4mvp;

out vec3 vv3color;
out vec2 vv2texcoord;

void main()
{
	gl_Position = um4mvp*vec4(iv3vertex, 1.0);
	vv3color = iv3normal;
	vv2texcoord = iv2texcoord;
}