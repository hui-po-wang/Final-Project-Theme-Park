#version 410

in vec3 vv3color;
in vec2 vv2texcoord;

uniform sampler2D s;
uniform vec2 img_size;

layout(location = 0) out vec4 fragColor;

void main()
{
    //fragColor = vec4(vec3(1,0,0), 1.0);
	fragColor = vec4(vv3color,1.0);
	vec4 diffuseColor = texture(s, vv2texcoord.st).rgba;
	if(diffuseColor.a < 0.5)
		discard;
	else
		fragColor = vec4(diffuseColor.rgb, 1.0);
}