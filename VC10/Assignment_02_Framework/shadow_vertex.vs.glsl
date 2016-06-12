#version 410 core                         
	                                          
uniform mat4 um4mvp;                      
                                          
layout(location = 0) in vec3 iv3vertex;
layout(location = 1) in vec3 iv3normal;
layout(location = 2) in vec2 iv2texcoord;

out vec3 vv3normal;
out vec4 position;
	                                        
void main(void)                           
{                                         
	gl_Position = um4mvp * vec4(iv3vertex, 1.0);         
	position = gl_Position;
	vv3normal = iv3normal;
}                                         