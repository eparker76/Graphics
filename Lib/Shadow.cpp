#include "Shadow.h"
#include "GLXtras.h"

// OpenGL shader programs
GLuint	shadowProgram = 0;
GLuint	mainProgram = 0;

// shadow depth-buffer, resulting texture
GLuint	shadowFramebuffer = 0;
GLuint	shadowTexture = 0;
int		shadowTextureUnit = 1, meshTextureUnit = 5;

const	int SHADOW_RES = 16384, SHADOW_WIDTH = SHADOW_RES, SHADOW_HEIGHT = SHADOW_RES; //  2**14 (2**28 pixels)
int		shadowEdgeSamples = 16;

// shadow vertex shader
const char *shadowVert = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	uniform mat4 depth_vp;
	uniform mat4 modeltransform;
	void main() {
		gl_Position = depth_vp * modeltransform * vec4(point, 1);
	}
)";

// shadow pixel shader
const char *shadowFrag = R"(
	#version 410 core
	void main() {}
)";

// main vertex shader for operations with a shadow buffer
const char *mainVert = R"(
	#version 410 core
	in vec3 point;
	in vec3 normal;
	in vec2 uv;
	out vec3 vPoint;
	out vec3 vNormal;
	out vec2 vUv;
	out vec4 shadowCoord;
	uniform mat4 modelview;			// camera.modelview*mesh.toWorld
	uniform mat4 persp;				// camera.persp
	uniform mat4 modeltransform;	// mesh.toWorld
	uniform mat4 depth_vp;			// shadow transform
	void main() {
		shadowCoord = depth_vp*modeltransform*vec4(point, 1);
		vPoint = (modelview*vec4(point, 1)).xyz;
		vNormal = (modelview*vec4(normal, 0)).xyz;
		gl_Position = persp*vec4(vPoint, 1);
		vUv = uv;
	}
)";

// main pixel shader for operations with a shadow buffer
const char *mainFrag = R"(
	#version 410 core
	in vec3 vPoint, vNormal;
	in vec2 vUv;
	in vec4 shadowCoord;
	uniform sampler2D textureImage;
	uniform sampler2DShadow shadow;
	uniform vec3 lightColor;
	uniform vec3 color = vec3(1, 1, 0);
	uniform int edgeSamples;
	uniform int nLights = 0;
	uniform vec3 lights[20];
	uniform float opacity = 1;
	uniform bool useLight = true;
	uniform bool useTexture = true;
	uniform bool useShadow = true;
	uniform bool fwdFacingOnly = false;
	uniform bool facetedShading = false;
	uniform float amb = .1, dif = .7, spc =.7;		// ambient, diffuse, specular
	out vec4 pColor;

	vec2 uniformSamples[32] = vec2[](vec2(0.49338352, -0.58302237), vec2(-0.39376479, 0.12189280), 
	vec2(-0.38876976, 0.39560871), vec2(-0.82853213, 0.29121478), vec2(-0.62251564, 0.27426500), 
	vec2(0.44906493, 0.72971920), vec2(0.99295605, 0.02762058), vec2(-0.61054051, -0.74474791), 
	vec2(-0.49073490, 0.09812672), vec2(0.64145907, -0.23052487), vec2(-0.47168601, 0.81892203), 
	vec2(0.95110052, 0.97483373), vec2(0.84048903, 0.82753596), vec2(-0.94147225, 0.42333745), 
	vec2(-0.97706586, 0.22633662), vec2(0.00977269, 0.02378330), vec2(-0.21250551, 0.39536213), 
	vec2(0.46426639, 0.17288661), vec2(-0.44197788, 0.33506576), vec2(0.80805167, -0.29359674), 
	vec2(-0.66379370, 0.04307460), vec2(0.26607188, 0.79704354), vec2(0.20652568, 0.81991369), 
	vec2(0.64959186, -0.64564514), vec2(0.93534138, 0.83045920), vec2(0.31952140, 0.95451090), 
	vec2(-0.85996893, 0.29045370), vec2(-0.33230688, -0.34582716), vec2(0.87055498, 0.64248681), 
	vec2(-0.19631182, -0.83353633), vec2(0.70041707, 0.58055892), vec2(0.78863981, -0.50693407));

	float d = 0, s = 0;								// diffuse, specular terms
	vec3 N, E;

	float random(vec4 seed) {
		float dot_product = dot(seed, vec4(12.9898,78.233,45.164,94.673));
		return fract(sin(dot_product) * 43758.5453);
	}

	float calcShadow(vec4 shadow_coord) {
		// Perspective division and transforming shadow coord from [-1, 1] to [0, 1]
		vec3 coord = shadow_coord.xyz / shadow_coord.w;
		coord = coord * 0.5 + 0.5;
		// Calculating total texel size and value at shadow
		vec2 texelSize = 1.0 / textureSize(shadow, 0);
		float shadowVal = texture(shadow, vec3(coord.xy, coord.z)) == 0.0f ? 0.4f : 1.0f;
		// Early bailing on extra sampling if nearby values are the same (not on shadow edge)
		bool different = false;
		for (int x = -1; x <= 1; x += 2) {
			for (int y = -1; y <= 1; y += 2) {
				float diffVal = texture(shadow, vec3(coord.xy + vec2(x, y) * texelSize, coord.z)) == 0.0f ? 0.4f : 1.0f;
				if (diffVal != shadowVal) different = true;
			}
		}
		if (!different) return shadowVal == 1.0f ? 1.0f : shadowVal / 5.0;
		// If on shadow edge, sample using nearby precalculated uniform random coordinates
		for (int i = 0; i < edgeSamples; i++) {
			int ind = int(float(edgeSamples)*random(vec4(gl_FragCoord.xyy, i))) % edgeSamples;
			shadowVal += texture(shadow, vec3(coord.xy + uniformSamples[ind] * texelSize, coord.z)) == 0.0f ? 0.4f : 1.0f;
		}
		return shadowVal / (float(edgeSamples) + 5.0f);
	}	

	void Intensity(vec3 light) {
		vec3 L = normalize(light-vPoint);
		float dd = dot(L, N);
		bool sideLight = dd > 0;
		bool sideViewer = gl_FrontFacing;
		if (true) { // sideLight == sideViewer) {
			d += abs(dd);
			vec3 R = reflect(L, N);					// highlight vector
			float h = max(0, dot(R, E));			// highlight term
			s += pow(h, 50);						// specular term
		}
	}
	void main() {
		N = normalize(facetedShading? cross(dFdx(vPoint), dFdy(vPoint)) : vNormal);
		// if (fwdFacingOnly && N.z < 0)
		// 	discard;
		E = normalize(vPoint);						// eye vector
		float ds = 1;
		if (useLight) {
			for (int i = 0; i < nLights; i++)
				Intensity(lights[i]);
			ds = dif*d+spc*s;
		}
		pColor = useTexture?
			vec4(texture(textureImage, vUv).rgb, opacity) :
			vec4(color, opacity);
		if (useShadow) {
			float shadow = calcShadow(shadowCoord);
			pColor *= shadow;
		};
		pColor = (amb+ds)*pColor;
	}
)";

// Display

void MeshDraw(Camera camera, vec3 light, Mesh *m) {
	int nTris = m->triangles.size(), nQuads = m->quads.size();
	int program;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	glBindVertexArray(m->vao);
	// texture
	bool useTexture = m->textureName > 0 && m->uvs.size() > 0;
	SetUniform(program, "useTexture", useTexture);
	if (useTexture) {
		glActiveTexture(GL_TEXTURE0+meshTextureUnit);
		glBindTexture(GL_TEXTURE_2D, m->textureName);
		SetUniform(program, "textureImage", meshTextureUnit);
	}
	// set matrices
	SetUniform(program, "modelview", camera.modelview*m->toWorld);
	SetUniform(program, "persp", camera.persp);
	SetUniform(program, "modeltransform", m->toWorld);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->eBufferId);
	glDrawElements(GL_TRIANGLES, 3*nTris, GL_UNSIGNED_INT, 0);
#ifdef GL_QUADS
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDrawElements(GL_QUADS, 4*nQuads, GL_UNSIGNED_INT, m->quads.data());
#endif
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void ShadowDraw(Camera camera, vec3 light, int winWidth, int winHeight, Mesh *meshes[], int nMeshes) {
	// draw scene to depth buffer
	mat4 depthProj = Orthographic(-80, 80, -80, 80, -20, 100);
	mat4 depthView = LookAt(light, vec3(0, 0, 0), vec3(0, 1, 0));
	mat4 depthVP = depthProj * depthView;
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glUseProgram(shadowProgram);
	glCullFace(GL_FRONT);
	SetUniform(shadowProgram, "depth_vp", depthVP);
	for (int i = 0; i < nMeshes; i++)
		MeshDraw(camera, light, meshes[i]);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// draw scene to visible buffer
	glViewport(0, 0, winWidth, winHeight);
	glUseProgram(mainProgram);
	glCullFace(GL_BACK);
	glActiveTexture(GL_TEXTURE0+shadowTextureUnit);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	SetUniform(mainProgram, "shadow", shadowTextureUnit);
	SetUniform(mainProgram, "edgeSamples", shadowEdgeSamples);
	SetUniform(mainProgram, "depth_vp", depthVP);
	vec3 xLight = Vec3(camera.modelview*vec4(light, 1));
	SetUniform(mainProgram, "nLights", 1);
	SetUniform3v(mainProgram, "lights", 1, (float *) &xLight);
	for (int i = 0; i < nMeshes; i++)
		MeshDraw(camera, light, meshes[i]);
}

// Initialization

bool ShadowSetup() {
	// make shader programs
	mainProgram = LinkProgramViaCode(&mainVert, &mainFrag);
	shadowProgram = LinkProgramViaCode(&shadowVert, &shadowFrag);
	// create 2D image for depth-buffer
	glGenTextures(1, &shadowTexture);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SHADOW_RES, SHADOW_RES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // edge not in shadow?
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	// create frame buffer for use with shadowProgram
	glGenFramebuffers(1, &shadowFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return ok;
}

