#version 410 core                                
	                  
in vec3 vv3normal;
in vec4 position;					
					                                 
out vec4 fragColor;                             
//layout(location = 0) out float fragmentdepth;
	                                                 
void main()                                      
{                                                
	//vec3 color = normalize(gl_FragCoord.xyz);
	fragColor = vec4((vec3(gl_FragCoord.z)), 1.0); 
	//fragColor = vec4((vec3(position.z)), 1.0); 
	//fragColor = vec4(vec3(color.z), 1.0); 
	//fragmentdepth = gl_FragCoord.z;
	//fragColor = vec4(vv3normal, 1.0);
} 