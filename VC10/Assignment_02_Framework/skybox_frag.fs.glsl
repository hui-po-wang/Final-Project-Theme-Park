#version 420 core

uniform samplerCube tex_cubemap; 
uniform int trigger_fog;

in VS_OUT{
	vec3 tc;
}fs_in;

layout (location = 0) out vec4 color;

void main(){
	color = texture(tex_cubemap, fs_in.tc);
	if(trigger_fog == 1){
		color = mix(vec4(0.82, 0.88, 0.9,1.0),color,0.3);
	}
}