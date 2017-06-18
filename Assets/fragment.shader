#version 410

layout(location = 0) out vec4 fragColor;

#define LIGHT_NUM 1

uniform sampler2D tex;
uniform mat4 um4mv;
uniform mat4 um4p;
uniform vec2 test;

///====================Lighting====================

in VertexData
{
	vec3 N; // eye space normal
	vec3 L; // eye space light vector
	vec3 H; // eye space halfway vector
} vertexData[LIGHT_NUM];
in vec2 texcoord;

vec3 Ks = vec3(1);
vec3 Ia = vec3(0.2);
vec3 Id = vec3(4); // 0.5
vec3 Is = vec3(0.112); // * 0.515;
float shiness = 0.177 * 5; // 4.43
float c1 = 0.9;
float c2 = 0.225;
float c3 = 0.022;
float c4 = -0.0001;

vec3 directionalLighting(vec3 color, int i) {
	vec3 N = normalize(vertexData[i].N);
	vec3 L = normalize(vertexData[i].L);
	vec3 H = normalize(vertexData[i].H);

	float dist = length(vertexData[i].L);
	float theta = max(dot(N, L), 0);
	float phi = max(dot(N, H), 0);
	float fa = min(1 / (c1 + c2 * dist + c3 * dist * dist), 1);
	//float fa = exp(c4 * dist * dist * dist);

	vec3 ambient = color * Ia;
	vec3 diffuse = color * Id * theta;
	vec3 specular = Ks * Is * pow(phi, shiness);

	return ambient + fa * (diffuse + specular);
}

vec3 pointLighting(vec3 color, int i) {
	vec3 N = normalize(vertexData[i].N);
	vec3 L = normalize(vertexData[i].L);
	vec3 H = normalize(vertexData[i].H);

	float dist = length(vertexData[i].L);
	float theta = max(dot(N, L), 0);
	float phi = max(dot(N, H), 0);
	float fa = min(1 / (c1 + c2 * dist + c3 * dist * dist), 1);
	//float fa = exp(c4 * dist * dist * dist);

	vec3 ambient = color * Ia;
	vec3 diffuse = color * Id * theta;
	vec3 specular = Ks * Is * pow(phi, shiness);

	return ambient + fa * (diffuse + specular);
}

///====================Lighting====================

///====================Fog====================

in vec4 viewCoord;

const vec3 fogColor = vec3(0.5, 0.5, 0.5);
const float fogDensity = 0.005;

vec3 makeFog(vec3 color) {
	float dist = length(viewCoord);
	float fogFactor = clamp(1.0 / exp(dist * fogDensity), 0.0, 1.0);
	return mix(fogColor, color, fogFactor);
}

///====================Fog====================

/* skybox */
uniform samplerCube skybox;
uniform int state;
in vec3 _vertex;
/* skybox */

void testFunc() {
	if (test.x == 0 && test.y == 0) return;

//	Is = vec3(1) * test.x;
//	shiness = 5 * test.y;
//	c2 = test.x;
//	c3 = test.y;
//	c4 = -0.0001 * test.x;
}

void main()
{
	testFunc();

	if (state == 0) {
		vec3 color = texture(tex, texcoord).rgb;
		if (color == vec3(0)) discard;

		for (int i = 0; i < LIGHT_NUM; ++i) {
			color = pointLighting(color, i);
		}
		//color = makeFog(color);

		fragColor = vec4(color, 1.0);
	}
	else if (state == 1) {
		fragColor = texture(skybox, _vertex);
	}
}