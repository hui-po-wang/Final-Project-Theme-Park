#version 420 core

//in vec3 vv3color;
in vec4 clipSpace;
in vec2 texCoord;
in vec3 toCameraVector;
in float visibility;

uniform sampler2D reflectionTex;
uniform sampler2D refractionTex;
uniform sampler2D dudvMap;

uniform float moveFactor;

layout(location = 0) out vec4 fragColor;

const float waveStrength = 0.005;
void main(){
	vec2 ndc = (vec2(clipSpace.x, clipSpace.y)/clipSpace.w)/2.0+0.5;
	vec2 refractionTexCoord = vec2(ndc.x, ndc.y);
	vec2 reflectionTexCoord = vec2(ndc.x, -(ndc.y));
	
	//vec2 distortion1 = (texture(dudvMap, vec2(texCoord.x + moveFactor*0.5, texCoord.y)).rg * 2.0 - 1.0) * waveStrength;
	//vec2 distortion2 = (texture(dudvMap, vec2(-texCoord.x + moveFactor*0.5, texCoord.y + moveFactor*0.5)).rg * 2.0 - 1.0) * waveStrength;
	vec2 distortion1 = (texture(dudvMap, vec2(ndc.x + moveFactor, ndc.y)).rg * 2.0 - 1.0) * waveStrength;
	vec2 distortion2 = (texture(dudvMap, vec2(-ndc.x + moveFactor, ndc.y + moveFactor)).rg * 2.0 - 1.0) * waveStrength;
	vec2 totaldistortion = distortion1 + distortion2;

	refractionTexCoord += totaldistortion;
	refractionTexCoord = clamp(refractionTexCoord, 0.001, 0.999);

	reflectionTexCoord += totaldistortion;
	reflectionTexCoord.x = clamp(reflectionTexCoord.x, 0.001, 0.999);
	reflectionTexCoord.y = clamp(reflectionTexCoord.y, -0.999, -0.001);

	vec4 reflectionColor = texture(reflectionTex, reflectionTexCoord);
	vec4 refractionColor = texture(refractionTex, refractionTexCoord);

	vec3 viewVector = normalize(toCameraVector);
	float refractiveFactor = dot(viewVector, vec3(0.0, -1.0, 0.0));

	fragColor = mix(reflectionColor, refractionColor, refractiveFactor);
	fragColor = mix(fragColor, vec4(0.0, 0.3, 0.5, 1.0), 0.2);
	fragColor = mix(vec4(0.82, 0.88, 0.9,1.0),fragColor,visibility);

}