#version 420 core

//in vec3 vv3color;
in vec4 clipSpace;
in vec2 texCoord;

uniform sampler2D reflectionTex;
uniform sampler2D refractionTex;
uniform sampler2D dudvMap;

uniform float moveFactor;
layout(location = 0) out vec4 fragColor;

const float waveStrength = 0.01;
void main(){
	vec2 ndc = (vec2(clipSpace.x, clipSpace.y)/clipSpace.w)/2.0+0.5;
	vec2 refractionTexCoord = vec2(ndc.x, ndc.y);
	vec2 reflectionTexCoord = vec2(ndc.x, -(ndc.y));
	
	vec2 distortion1 = (texture(dudvMap, vec2(texCoord.x + moveFactor*10, texCoord.y)).rg * 2.0 - 1.0) * waveStrength;
	vec2 distortion2 = (texture(dudvMap, vec2(-texCoord.x + moveFactor, texCoord.y + moveFactor)).rg * 2.0 - 1.0) * waveStrength;
	vec2 totaldistortion = distortion1 + distortion2;

	refractionTexCoord += totaldistortion;
	refractionTexCoord = clamp(refractionTexCoord, 0.001, 0.999);

	reflectionTexCoord += totaldistortion;
	reflectionTexCoord.x = clamp(reflectionTexCoord.x, 0.001, 0.999);
	reflectionTexCoord.y = clamp(reflectionTexCoord.y, -0.999, -0.001);

	vec4 reflectionColor = texture(reflectionTex, reflectionTexCoord);
	vec4 refractionColor = texture(refractionTex, refractionTexCoord);

	fragColor = mix(reflectionColor, refractionColor, 0.5);
	fragColor = mix(fragColor, vec4(0.0, 0.2, 0.4, 1.0), 0.2);

}