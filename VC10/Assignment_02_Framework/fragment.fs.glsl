#version 420 core

in vec4 shadow_coord;
in vec3 vv3color;
in vec3 vv3vertex;
in vec2 vv2texcoord;
in float visibility;

uniform sampler2D s;
uniform sampler2D shadow_tex;

uniform vec3 camera_position;
uniform vec4 plane;
uniform int trigger_lighting;
uniform int trigger_fog;
uniform int trigger_shadow;

uniform mat4 M;
uniform mat4 V;
uniform mat4 P;

layout(location = 0) out vec4 fragColor;
float ShadowCalculation()
{
    // perform perspective divide
    vec3 projCoords = shadow_coord.xyz / shadow_coord.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // Get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadow_tex, projCoords.xy).r; 
	//float closestDepth = textureProj(shadow_tex, vec4(projCoords,1.0));
    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // Check whether current frag pos is in shadow
    float shadow = currentDepth > closestDepth  ? 1.0 : 0.0;

    return shadow;
} 
vec4 lighting(){
	// lighting constant
	mat4 model_view = V * M;
	vec4 resultLighting = vec4(0, 0, 0, 1);
	vec4 position = model_view * vec4(vv3vertex, 1.0);
	vec3 texColor = texture2D(s, vv2texcoord).rgb;
	vec3 normal = normalize(vv3color);
	normal = -normal;
	mat3 normal_matrix = transpose(inverse(mat3(model_view)));
	normal = normalize(  ( normal_matrix * normal ).xyz );
	vec4 Lposition =  vec4(0, 1, 0.1, 1);
	vec3 Lp = normalize( (Lposition).xyz);
	float attenuation = 1.0;
		
	vec3 Vp = normalize(camera_position-position.xyz);

	float diffuseCoefficient = max(dot(Lp, normal), 0.0);

	vec3 reflectVec = reflect(-Lp, normal);
	float cosAngle = max(0.0, dot(reflectVec, Vp));
	float specularCoefficient;
	specularCoefficient = pow( cosAngle, 1.0);
		
	vec3 ambient = vec3(0.6, 0.6, 0.6);
	vec3 diffuse = diffuseCoefficient * vec3(1, 1, 1) * vec3(0.9, 0.9, 0.9);
	vec3 specular = specularCoefficient * vec3(1, 1, 1) * vec3(0.3, 0.3, 0.3);
	
	if(trigger_shadow == 1){
		float bias = 0.1;
		float dt = 1.0f;
		if(texture(shadow_tex, shadow_coord.xy/shadow_coord.w).r < shadow_coord.z/shadow_coord.w - bias){
			dt = 0.0f;
		}

		if(dt == 0){
			resultLighting += vec4(ambient -vec3(0.2, 0.2, 0.2) +  dt * attenuation*(diffuse + specular),0);
		}
		else{
			resultLighting += vec4(ambient +  dt * attenuation*(diffuse + specular),0);
		}
	}
	else{
		resultLighting += vec4(ambient + attenuation*(diffuse + specular),0);
	}
	
	return resultLighting;

}
void main()
{
    //fragColor = vec4(vec3(1,0,0), 1.0);
	fragColor = vec4(vv3color,1.0);
	vec4 diffuseColor = texture(s, vv2texcoord.st).rgba;
	if(diffuseColor.a < 0.5)
		discard;
	else{
		if(trigger_lighting == 0){
			fragColor = vec4(diffuseColor.rgb, 1.0) ;
			//fragColor = mix(vec4(0.82, 0.88, 0.9,1.0),fragColor,visibility);
		}
		else{
			fragColor = vec4(diffuseColor.rgb, 1.0) * lighting();
			//fragColor = mix(vec4(0.82, 0.88, 0.9,1.0),fragColor,visibility);
		}

		if(trigger_fog == 1){
			fragColor = mix(vec4(0.82, 0.88, 0.9,1.0),fragColor,visibility);
		}
	}

}