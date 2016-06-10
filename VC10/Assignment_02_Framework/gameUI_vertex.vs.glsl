#version 420 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 iv2texcoord;
out vec2 vv2texcoord;

//uniform mat4 transMatrix;

void main()
{
	//gl_Position = transMatrix * vec4(pos,0,1.0);
	gl_Position = vec4(pos,0,1.0);
	vv2texcoord = iv2texcoord;//vec2((pos.x+1.0)/2,1-(pos.y+1.0)/2);
}