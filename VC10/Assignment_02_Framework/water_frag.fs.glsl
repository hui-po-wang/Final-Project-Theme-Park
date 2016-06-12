#version 420 core

//in vec3 vv3color;
in vec4 clipSpace;
in vec2 texCoord;
in vec3 toCameraVector;
in vec3 vv3vertex;
in float visibility;

uniform sampler2D reflectionTex;
uniform sampler2D refractionTex;
uniform sampler2D dudvMap;
uniform sampler2D normalMap;

uniform mat4 mvp;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 proj_matrix;
uniform vec3 cameraPosition;

uniform float moveFactor;

uniform int trigger_lighting;

layout(location = 0) out vec4 fragColor;

const float waveStrength = 0.005;

vec4 lighting(vec2 distortedTexcoords){
	// lighting constant
	mat4 model_view = view_matrix * model_matrix;
	vec4 resultLighting = vec4(0, 0, 0, 1);
	vec4 position = model_view * vec4(vv3vertex, 1.0);

	vec4 normalMapColor = texture(normalMap, distortedTexcoords);
	vec3 normal = vec3(normalMapColor.r*2.0-1.0, normalMapColor.b*1.0, normalMapColor.g*2.0-1.0);
	normal = normalize(normal);
	normal = -normal;

	mat3 normal_matrix = transpose(inverse(mat3(model_view)));
	normal = normalize(  ( normal_matrix * normal ).xyz );

	vec4 Lposition =  vec4(0, 1, 0.1, 1);
	vec3 Lp = normalize( (Lposition).xyz);
	float attenuation = 1.0;
		
	vec3 Vp = normalize(cameraPosition-position.xyz);

	float diffuseCoefficient = max(dot(Lp, normal), 0.0);

	vec3 reflectVec = reflect(-Lp, normal);
	float cosAngle = max(0.0, dot(reflectVec, Vp));
	float specularCoefficient;
	specularCoefficient = pow( cosAngle, 20.0);
		
	vec3 ambient = vec3(0.6, 0.6, 0.6);
	vec3 diffuse = diffuseCoefficient * vec3(1, 1, 1) * vec3(0.9, 0.9, 0.9);
	vec3 specular = specularCoefficient * vec3(1, 1, 1) * vec3(0.1, 0.1, 0.1);
	
	return vec4(specular, 1.0);

}
void main(){
	vec2 ndc = (vec2(clipSpace.x, clipSpace.y)/clipSpace.w)/2.0+0.5;
	vec2 refractionTexCoord = vec2(ndc.x, ndc.y);
	vec2 reflectionTexCoord = vec2(ndc.x, -(ndc.y));
	
	//vec2 distortion1 = (texture(dudvMap, vec2(ndc.x + moveFactor, ndc.y)).rg * 2.0 - 1.0) * waveStrength;
	//vec2 distortion2 = (texture(dudvMap, vec2(-ndc.x + moveFactor, ndc.y + moveFactor)).rg * 2.0 - 1.0) * waveStrength;
	//vec2 totaldistortion = distortion1 + distortion2;
	vec2 distortedTexcoord = texture(dudvMap, vec2(ndc.x + moveFactor, ndc.y)).rg * 0.1;
	distortedTexcoord = ndc + vec2(distortedTexcoord.x, distortedTexcoord.y + moveFactor);
	vec2 totaldistortion = (texture(dudvMap, distortedTexcoord).rg * 2.0 - 1.0) * waveStrength;

	refractionTexCoord += totaldistortion;
	refractionTexCoord = clamp(refractionTexCoord, 0.001, 0.999);

	reflectionTexCoord += totaldistortion;
	reflectionTexCoord.x = clamp(reflectionTexCoord.x, 0.001, 0.999);
	reflectionTexCoord.y = clamp(reflectionTexCoord.y, -0.999, -0.001);

	vec4 reflectionColor = texture(reflectionTex, reflectionTexCoord);
	vec4 refractionColor = texture(refractionTex, refractionTexCoord);

	vec3 viewVector = normalize(toCameraVector);
	float refractiveFactor = dot(viewVector, vec3(0.0, -1.0, 0.0));

	vec3 specularColor = lighting(distortedTexcoord).xyz;

	fragColor = mix(reflectionColor, refractionColor, refractiveFactor);
	fragColor = mix(fragColor, vec4(0.0, 0.3, 0.5, 1.0), 0.2) + vec4(specularColor, 0.0);

}