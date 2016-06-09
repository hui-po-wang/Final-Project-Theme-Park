#version 420 core

//in vec3 vv3color;
in vec2 vv2texcoord;

uniform vec2 img_size;
uniform sampler2D s;
uniform int cmp_bar;
uniform int control_signal;
uniform vec2 mouse_position;

layout(location = 0) out vec4 fragColor;

float sigma_e = 2.0f;
float sigma_r = 2.8f;
float phi = 3.4f;
float tau = 0.99f;
float twoSigmaESquared = 2.0 * sigma_e * sigma_e;
float twoSigmaRSquared = 2.0 * sigma_r * sigma_r;
int halfWidth = int(ceil( 2.0 * sigma_r ));

vec4 DoG(){
	vec2 sum = vec2(0.0);
	vec2 norm = vec2(0.0);
	for ( int i= -halfWidth; i<= halfWidth; ++i) {
		for ( int j = -halfWidth; j <= halfWidth; ++j ) {
			float d = length(vec2(i,j));
			vec2 kernel= vec2( exp( -d * d / twoSigmaESquared),
			exp( -d * d / twoSigmaRSquared));
			vec4 c= texture(s, vv2texcoord+vec2(i,j)/img_size);
			vec2 L= vec2(0.299 * c.r+ 0.587 * c.g+ 0.114 * c.b);
			norm += 2.0 * kernel;
			sum += kernel * L;
		}
	}
	sum /= norm;
	float H = 100.0 * (sum.x-tau * sum.y);
	float edge =( H > 0.0 )?1.0:2.0 *smoothstep(-2.0, 2.0, phi * H );
	return vec4(edge,edge,edge,1.0 );
}
vec4 medianBlur(int blur_size){
	vec4 color = vec4(0);
	int n =0;
	int half_size = blur_size; //filter_half_size
	for ( int i = -half_size; i <= half_size ; ++i ) {
		for ( int j = -half_size; j <= half_size ; ++j ) {
			vec4 c= texture(s,vv2texcoord +vec2(i,j)/img_size);
			color+= c;
			n++;
		}
	}
	color /=n;
	return color;
}
vec4 spatialConvolution3x3(mat3 mask, bool isGrayScale){
	vec4 color = vec4(0);
	int n =0;
	int half_size = 1; //filter_half_size
	for ( int i = -half_size; i <= half_size ; ++i ) {
		for ( int j = -half_size; j <= half_size ; ++j ) {
			vec4 c= texture(s,vv2texcoord +vec2(i,j)/img_size);
			if(isGrayScale == true){
				float Y = 0.2126*c.r+0.7152*c.g+0.0722*c.b;
				float result = Y*mask[half_size+i][half_size+j];
				color += vec4(result, result, result, c.a);
			}
			else{
				color += vec4(c.rgb*mask[half_size+i][half_size+j], c.a);
			}
		}
	}
	return color;
}
vec4 quantization(){
	vec4 tex_color = texture(s, vv2texcoord);
	int nbins = 8;	

	float r= floor(tex_color.r * float(nbins)) / float(nbins);
	float g= floor(tex_color.g * float(nbins)) / float(nbins);
	float b = floor(tex_color.b * float(nbins)) / float(nbins);
	return vec4(r,g,b,tex_color.a) ;
}
void oilPainting(){
	int radius = 4;
    float n = float((radius + 1) * (radius + 1));

    vec3 m[4];
    vec3 s_m[4];
    for (int k = 0; k < 4; ++k) {
        m[k] = vec3(0.0);
        s_m[k] = vec3(0.0);
    }

    for (int j = -radius; j <= 0; ++j)  {
        for (int i = -radius; i <= 0; ++i)  {
            vec3 c = texture(s, vv2texcoord + vec2(i,j) / img_size).rgb;
            m[0] += c;
            s_m[0] += c * c;
        }
    }

    for (int j = -radius; j <= 0; ++j)  {
        for (int i = 0; i <= radius; ++i)  {
            vec3 c = texture(s, vv2texcoord + vec2(i,j) / img_size).rgb;
            m[1] += c;
            s_m[1] += c * c;
        }
    }

    for (int j = 0; j <= radius; ++j)  {
        for (int i = 0; i <= radius; ++i)  {
            vec3 c = texture(s, vv2texcoord + vec2(i,j) / img_size).rgb;
            m[2] += c;
            s_m[2] += c * c;
        }
    }

    for (int j = 0; j <= radius; ++j)  {
        for (int i = -radius; i <= 0; ++i)  {
            vec3 c = texture(s, vv2texcoord + vec2(i,j) / img_size).rgb;
            m[3] += c;
            s_m[3] += c * c;
        }
    }


    float min_sigma2 = 1e+2;
    for (int k = 0; k < 4; ++k) {
        m[k] /= n;
        s_m[k] = abs(s_m[k] / n - m[k] * m[k]);

        float sigma2 = s_m[k].r + s_m[k].g + s_m[k].b;
        if (sigma2 < min_sigma2) {
            min_sigma2 = sigma2;
            fragColor = vec4(m[k], 1.0);
        }
    }
}
void main()
{
	if(control_signal == 5){
		if(distance(gl_FragCoord.xy, /*vec2(300.300)*/mouse_position) <= 100){
			fragColor = texture(s, (vv2texcoord-0.5)*0.5+0.5);
		}
		else{
			fragColor = texture(s, vv2texcoord);
		}
	}
	else if(gl_FragCoord.x > cmp_bar){
		switch (control_signal){
			case 0:
				fragColor = medianBlur(1);
				vec4 quan = quantization();
				vec4 color_after_quantization = (quan + vec4(fragColor.r, fragColor.g, fragColor.b, 0.0))*0.5;
				fragColor = vec4( DoG().rgb * color_after_quantization.rgb, 1.0) ;
				break;
			case 1:
			{
				mat3 mask = mat3(vec3(-1, -1, -1), vec3(-1, 8, -1), vec3(-1, -1, -1));
				fragColor = spatialConvolution3x3(mask, true);
				break;
			}
			case 2:
			{
				mat3 mask = mat3(vec3(-1, -1, -1), vec3(-1, 8, -1), vec3(-1, -1, -1));
				fragColor = spatialConvolution3x3(mask, true)*2 + texture(s, vv2texcoord);
				break;
			}
			case 3:
			{
				float offset = 0.005;
				vec4 Left_Color = texture(s, vec2(vv2texcoord.x-offset, vv2texcoord.y));
				vec4 Right_Color = texture(s, vec2(vv2texcoord.x+offset, vv2texcoord.y));
				float Color_R= Left_Color.r*0.299+Left_Color.g*0.587+Left_Color.b*0.114;
				float Color_G= Right_Color.g;
				float Color_B= Right_Color.b;
				fragColor = vec4(Color_R,Color_G,Color_B,1.0);
				break;
			}
			case 4:
			{
				vec4 first_blur = medianBlur(4);
				vec4 second_blur = medianBlur(8);
				fragColor = texture(s, vv2texcoord) + first_blur*0.4 + second_blur*0.2;
				break;
			}
			case 6:
			{
				oilPainting();
				break;
			}
		}
	}
	else if(gl_FragCoord.x == cmp_bar){
		fragColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
	else{
		fragColor = texture(s, vv2texcoord);
	}
}