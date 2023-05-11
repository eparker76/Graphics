#include "Shadow.h"
#include "GLXtras.h"
#include "Draw.h"
#include "Misc.h"

namespace {
	GLuint meshShaderNoLines = 0, shadowShader = 0;

	// shadow vertex shader
	const char* shadowVert = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	uniform mat4 depth_vp;
	uniform mat4 modeltransform;
	void main() {
		gl_Position = depth_vp * modeltransform * vec4(point, 1);
	}
)";

	// shadow pixel shader
	const char* shadowFrag = R"(
	#version 410 core
	void main() {}
)";

	// vertex shader
	// For operations with a shadow buffer
	const char* meshVertexShader = R"(
	#version 410 core
	layout (location = 0) in vec3 point;
	layout (location = 1) in vec3 normal;
	layout (location = 2) in vec2 uv;
	layout (location = 3) in mat4 instance; // for use with glDrawArrays/ElementsInstanced
											// uses locations 3,4,5,6 for 4 vec4s = mat4
	layout (location = 7) in vec3 color;	// for instanced color (vec4?)
	out vec3 vPoint;
	out vec3 vNormal;
	out vec2 vUv;
	out vec4 shadowCoord;
	uniform bool useInstance = false;
	uniform mat4 modelview;
	uniform mat4 persp;

	uniform mat4 modeltransform;
	uniform mat4 depth_vp;

	void main() {
		mat4 m = useInstance? modelview*instance : modelview;
		mat4 mv = modelview * modeltransform;
		vPoint = (m*vec4(point, 1)).xyz;
		vNormal = (m*vec4(normal, 0)).xyz;
		shadowCoord = depth_vp * modeltransform  * vec4(point, 1);
		gl_Position = persp*vec4(vPoint, 1);
		vUv = uv;
	}
)";
	// Pixel Shader
	// For operations with a shadow buffer
	const char* meshPixelShaderNoLines = R"(
	#version 410 core
	in vec3 vPoint, vNormal;
	in vec2 vUv;
	in vec4 shadowCoord;
	uniform sampler2D textureImage;
	uniform sampler2D txtr;
	uniform sampler2DShadow shadow;
	uniform vec3 lightColor;
	uniform int edgeSamples;
	uniform int nLights = 0;
	uniform vec3 lights[20];
	uniform vec3 defaultLight = vec3(1, 1, 1);
	uniform vec3 color = vec3(1);
	uniform float opacity = 1;
	uniform float ambient = .2;
	uniform bool useLight = true;
	uniform bool useTexture = true;
	uniform bool useTint = false;
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

	float Intensity(vec3 normalV, vec3 eyeV, vec3 point, vec3 light) {
		vec3 lightV = normalize(light-point);		// light vector
		vec3 reflectV = reflect(lightV, normalV);   // highlight vector
		float d = max(0, dot(normalV, lightV));     // one-sided diffuse
		float s = max(0, dot(reflectV, eyeV));      // one-sided specular
		return clamp(d+pow(s, 50), 0, 1);
	}

	void Intensity(vec3 light) {
		vec3 L = normalize(light-vPoint);
		float dd = dot(L, N);
		bool sideLight = dd > 0;
		bool sideViewer = gl_FrontFacing;
		if (sideLight == sideViewer) {
			d += abs(dd);
			vec3 R = reflect(L, N);					// highlight vector
			float h = max(0, dot(R, E));			// highlight term
			s += pow(h, 50);						// specular term
		}
	}
	void main() {
		N = normalize(facetedShading? cross(dFdx(vPoint), dFdy(vPoint)) : vNormal);
		if (fwdFacingOnly && N.z < 0)
			discard;
		E = normalize(vPoint);						// eye vector
		float ads = useLight? 0 : 1;
		if (useLight) {
			if (nLights == 0)
				Intensity(defaultLight);
			else
				for (int i = 0; i < nLights; i++)
					Intensity(lights[i]);
			ads = clamp(amb+dif*d, 0, 1)+spc*s;
		}
		if (useTexture) {
			float shadow = calcShadow(shadowCoord);
			pColor = vec4(ads*shadow*texture(textureImage, vUv).rgb, opacity);
		//	pColor = vec4(ads*texture(textureImage, vUv).rgb, opacity);

			if (useTint) {
				pColor.r *= color.r;
				pColor.g *= color.g;
				pColor.b *= color.b;
			}
		}
		else
			pColor = vec4(ads*color, opacity);	
	}
)";
} // end namespace

GLuint GetMeshShader1() {
	if (!meshShaderNoLines)
		meshShaderNoLines = LinkProgramViaCode(&meshVertexShader, &meshPixelShaderNoLines);
	return meshShaderNoLines;
}

GLuint UseMeshShader1() {
	GLuint s = GetMeshShader1();
	glUseProgram(s);
	return s;
}

GLuint GetShadowShader1() {
	if (!shadowShader)
		shadowShader = LinkProgramViaCode(&shadowVert, &shadowFrag);
	return shadowShader;
}

GLuint UseShadowShader1() {
	GLuint s = GetShadowShader1();
	glUseProgram(s);
	return s;
}

// Shadow Class

void Shadow::Display(Camera camera, vec3 light, int textureUnit, bool useGroupColor) {
	size_t nTris = triangles.size(), nQuads = quads.size();
	// enable shader and vertex array object
	int shader = UseMeshShader1();
	glBindVertexArray(vao);
	// texture
	bool useTexture = textureName > 0 && uvs.size() > 0 && textureUnit >= 0;
	SetUniform(shader, "useTexture", useTexture);
	if (useTexture) {
		glActiveTexture(GL_TEXTURE0 + textureUnit);
		glBindTexture(GL_TEXTURE_2D, textureName);
		SetUniform(shader, "textureImage", textureUnit); // but app can unset useTexture
	}
	// set matrices
	SetUniform(shader, "modelview", camera.modelview * toWorld);
	SetUniform(shader, "persp", camera.persp);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eBufferId);
	if (useGroupColor) {
		int textureSet = 0;
		glGetUniformiv(shader, glGetUniformLocation(shader, "useTexture"), &textureSet);
		// show ungrouped triangles without texture mapping
		int nGroups = triangleGroups.size(), nUngrouped = nGroups ? triangleGroups[0].startTriangle : nTris;
		SetUniform(shader, "useTexture", false);
		glDrawElements(GL_TRIANGLES, 3 * nUngrouped, GL_UNSIGNED_INT, 0); // triangles.data());
		// show grouped triangles with texture mapping
		SetUniform(shader, "useTexture", textureSet == 1);
		for (int i = 0; i < nGroups; i++) {
			Group g = triangleGroups[i];
			SetUniform(shader, "color", g.color);
			glDrawElements(GL_TRIANGLES, 3 * g.nTriangles, GL_UNSIGNED_INT, (void*)(3 * g.startTriangle * sizeof(int)));
		}
	}
	else {
		glDrawElements(GL_TRIANGLES, 3 * nTris, GL_UNSIGNED_INT, 0);
#ifdef GL_QUADS
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDrawElements(GL_QUADS, 4 * nQuads, GL_UNSIGNED_INT, quads.data());
#endif
	}
	glBindVertexArray(0);
}

void Shadow::ShadowDisplay(Camera camera, vec3 light, int textureUnit, bool useGroupColor) {
	// Compute view projection matrix from point of view of light
	mat4 depthProj = Orthographic(-10, 10, -10, 10, -20, 100);
	mat4 depthView = LookAt(light, vec3(0, 0, 0), vec3(0, 1, 0));
	//mat4 depthView = LookAt(vec3(20, 30, 20), vec3(0, 0, 0), vec3(0, 1, 0));
	mat4 depthVP = depthProj * depthView;
	size_t nTris = triangles.size(), nQuads = quads.size();
	// enable shader and vertex array object
	int shader = UseShadowShader1();
	glBindVertexArray(vao);
	// texture
	bool useTexture = textureName > 0 && uvs.size() > 0 && textureUnit >= 0;
	//	if (!textureName || !uvs.size() || textureUnit < 0)
	//		SetUniform(shader, "useTexture", false);
	//	else {
	SetUniform(shader, "useTexture", useTexture);
	if (useTexture) {
		glActiveTexture(GL_TEXTURE0 + textureUnit);
		glBindTexture(GL_TEXTURE_2D, textureName);
		SetUniform(shader, "textureImage", textureUnit); // but app can unset useTexture
	}
	// set matrices
	SetUniform(shader, "modelview", camera.modelview);
	SetUniform(shader, "modeltransform", toWorld);
	SetUniform(shader, "persp", light);
	SetUniform(shader, "depth_vp", depthVP);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eBufferId);
	if (useGroupColor) {
		int textureSet = 0;
		glGetUniformiv(shader, glGetUniformLocation(shader, "useTexture"), &textureSet);
		// show ungrouped triangles without texture mapping
		int nGroups = triangleGroups.size(), nUngrouped = nGroups ? triangleGroups[0].startTriangle : nTris;
		SetUniform(shader, "useTexture", false);
		glDrawElements(GL_TRIANGLES, 3 * nUngrouped, GL_UNSIGNED_INT, 0); // triangles.data());
		// show grouped triangles with texture mapping
		SetUniform(shader, "useTexture", textureSet == 1);
		for (int i = 0; i < nGroups; i++) {
			Group g = triangleGroups[i];
			SetUniform(shader, "color", g.color);
			glDrawElements(GL_TRIANGLES, 3 * g.nTriangles, GL_UNSIGNED_INT, (void*)(3 * g.startTriangle * sizeof(int)));
		}
	}
	else {
		glDrawElements(GL_TRIANGLES, 3 * nTris, GL_UNSIGNED_INT, 0);
#ifdef GL_QUADS
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDrawElements(GL_QUADS, 4 * nQuads, GL_UNSIGNED_INT, quads.data());
#endif
	}
	glBindVertexArray(0);
}

void Enable1(int id, int ncomps, int offset) {
	glEnableVertexAttribArray(id);
	glVertexAttribPointer(id, ncomps, GL_FLOAT, GL_FALSE, 0, (void*)offset);
}

void Shadow::Buffer(vector<vec3>& pts, vector<vec3>* nrms, vector<vec2>* tex) {
	size_t nPts = pts.size(), nNrms = nrms ? nrms->size() : 0, nUvs = tex ? tex->size() : 0;
	if (!nPts) { printf("Buffer: no points!\n"); return; }
	// create vertex buffer
	if (!vBufferId)
		glGenBuffers(1, &vBufferId);
	glBindBuffer(GL_ARRAY_BUFFER, vBufferId);
	// allocate GPU memory for vertex position, texture, normals
	size_t sizePoints = nPts * sizeof(vec3), sizeNormals = nNrms * sizeof(vec3), sizeUvs = nUvs * sizeof(vec2);
	int bufferSize = sizePoints + sizeUvs + sizeNormals;
	glBufferData(GL_ARRAY_BUFFER, bufferSize, NULL, GL_STATIC_DRAW);
	// load vertex buffer
	if (nPts) glBufferSubData(GL_ARRAY_BUFFER, 0, sizePoints, pts.data());
	if (nNrms) glBufferSubData(GL_ARRAY_BUFFER, sizePoints, sizeNormals, nrms->data());
	if (nUvs) glBufferSubData(GL_ARRAY_BUFFER, sizePoints + sizeNormals, sizeUvs, tex->data());
	// create and load element buffer for triangles
	size_t sizeTriangles = sizeof(int3) * triangles.size();
	glGenBuffers(1, &eBufferId);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eBufferId);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeTriangles, triangles.data(), GL_STATIC_DRAW);
	// create vertex array object for mesh
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	// enable attributes
	if (nPts) Enable1(0, 3, 0);						// VertexAttribPointer(shader, "point", 3, 0, (void *) 0);
	if (nNrms) Enable1(1, 3, sizePoints);			// VertexAttribPointer(shader, "normal", 3, 0, (void *) sizePoints);
	if (nUvs) Enable1(2, 2, sizePoints + sizeNormals); // VertexAttribPointer(shader, "uv", 2, 0, (void *) (sizePoints+sizeNormals));
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void Shadow::Buffer() { Buffer(points, normals.size() ? &normals : NULL, uvs.size() ? &uvs : NULL); }

bool Shadow::Read(string objFile, mat4* m, bool standardize, bool buffer, bool forceTriangles) {
	if (!ReadAsciiObj((char*)objFile.c_str(), points, triangles, &normals, &uvs, &triangleGroups, &triangleMtls, forceTriangles ? NULL : &quads, NULL)) {
		printf("Mesh.Read: can't read %s\n", objFile.c_str());
		return false;
	}
	objFilename = objFile;
	if (standardize)
		Standardize(points.data(), points.size(), 1);
	if (buffer)
		Buffer();
	if (m)
		toWorld = *m;
	return true;
}

bool Shadow::Read(string objFile, string texFile, mat4* m, bool standardize, bool buffer, bool forceTriangles) {
	if (!Read(objFile, m, standardize, buffer, forceTriangles))
		return false;
	objFilename = objFile;
	texFilename = texFile;
	textureName = ReadTexture((char*)texFile.c_str());
	if (!textureName)
		printf("Mesh.Read: bad texture name\n");
	return textureName > 0;
}

bool Shadow::ShadowSetup() {
	// Create depth texture
	glGenTextures(1, &shadowTexture);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	GLfloat borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// Create framebuffer
	glGenFramebuffers(1, &shadowFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return false;
	return true;
}

