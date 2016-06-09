#version 420 core

layout(location = 0) in vec2 iv2vertex;
layout(location = 1) in vec2 iv2texcoord;

//out vec2 vv3color;
out vec2 vv2texcoord;

void main()
{
	gl_Position = vec4(iv2vertex, 0.0, 1.0);
	//vv3color = iv3normal;
	vv2texcoord = iv2texcoord;
}