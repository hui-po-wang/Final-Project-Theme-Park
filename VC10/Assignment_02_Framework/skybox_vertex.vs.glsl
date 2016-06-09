#version 420 core

layout(location = 0) in vec3 iv3vertex;

out VS_OUT{
	vec3 tc;
}vs_out;

uniform mat4 um4mvp;

void main(){
	vs_out.tc = iv3vertex;
	vs_out.tc.y *= -1;
	gl_Position = um4mvp * vec4(iv3vertex, 1.0);
}