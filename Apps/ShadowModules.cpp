// ShadowModules.cpp - based on Vulcan/drive.cpp by Devon McKee

const int SHADOW_DIM = 16384;
GLuint shadowProgram = 0;
GLuint shadowFramebuffer = 0;
GLuint shadowTexture = 0;
int shadowEdgeSamples = 16;

const char* shadowVert = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	uniform mat4 depth_vp;
	uniform mat4 model;
	uniform mat4 transform;
	void main() {
		gl_Position = depth_vp * transform * model * vec4(point, 1);
	}
)";

const char* shadowFrag = R"(
	#version 410 core
	void main() {}
)";

GLuint mainProgram = 0;

// NOTE: no normals used

const char* mainVert = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	layout(location = 1) in vec2 uv;
	out vec2 vUv;
	out vec4 shadowCoord;
	uniform mat4 model;
	uniform mat4 transform;
	uniform mat4 view;
	uniform mat4 persp;
	uniform mat4 depth_vp;
	void main() {
		shadowCoord = depth_vp * transform * model * vec4(point, 1);
		vUv = uv;
		gl_Position = persp * view * transform * model * vec4(point, 1);
	}
)";

const char* mainFrag = R"(
	#version 410 core
	in vec2 vUv;
	in vec4 shadowCoord;
	out vec4 pColor;
	uniform sampler2D txtr;
	uniform sampler2DShadow shadow;
	uniform vec4 ambient = vec4(vec3(0.1), 1);
	uniform vec3 lightColor;
	uniform int edgeSamples;
	vec2 uniformSamples[32] = vec2[](vec2(0.49338352, -0.58302237), vec2(-0.39376479, 0.12189280), vec2(-0.38876976, 0.39560871), vec2(-0.82853213, 0.29121478), vec2(-0.62251564, 0.27426500), vec2(0.44906493, 0.72971920), vec2(0.99295605, 0.02762058), vec2(-0.61054051, -0.74474791), vec2(-0.49073490, 0.09812672), vec2(0.64145907, -0.23052487), vec2(-0.47168601, 0.81892203), vec2(0.95110052, 0.97483373), vec2(0.84048903, 0.82753596), vec2(-0.94147225, 0.42333745), vec2(-0.97706586, 0.22633662), vec2(0.00977269, 0.02378330), vec2(-0.21250551, 0.39536213), vec2(0.46426639, 0.17288661), vec2(-0.44197788, 0.33506576), vec2(0.80805167, -0.29359674), vec2(-0.66379370, 0.04307460), vec2(0.26607188, 0.79704354), vec2(0.20652568, 0.81991369), vec2(0.64959186, -0.64564514), vec2(0.93534138, 0.83045920), vec2(0.31952140, 0.95451090), vec2(-0.85996893, 0.29045370), vec2(-0.33230688, -0.34582716), vec2(0.87055498, 0.64248681), vec2(-0.19631182, -0.83353633), vec2(0.70041707, 0.58055892), vec2(0.78863981, -0.50693407));
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
	void main() {
		pColor = ambient + texture(txtr, vUv) * vec4(lightColor, 1) * calcShadow(shadowCoord);
	}
)";

void ShadowSetup() {
	if (!(shadowProgram = LinkProgramViaCode(&shadowVert, &shadowFrag)))
		throw runtime_error("Failed to compile shadow render program!");
	// Set up shadowmap resources
	glGenFramebuffers(1, &shadowFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glGenTextures(1, &shadowTexture);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SHADOW_DIM, SHADOW_DIM, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		throw runtime_error("Failed to set up shadow framebuffer!");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowCleanup() {
	// Cleanup shadow map resources
	glDeleteFramebuffers(1, &shadowFramebuffer);
	glDeleteTextures(1, &shadowTexture);
}

void ShadowDraw() {
	// Draw scene to depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glViewport(0, 0, SHADOW_DIM, SHADOW_DIM);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(shadowProgram);
	glCullFace(GL_FRONT);
	mat4 depthProj = Orthographic(-80, 80, -80, 80, -20, 100);
	mat4 depthView = LookAt(vec3(20, 30, 20), vec3(0, 0, 0), vec3(0, 1, 0));
	mat4 depthVP = depthProj * depthView;
	SetUniform(shadowProgram, "depth_vp", depthVP);
	SetUniform(shadowProgram, "model", floor_mesh.model);
	SetUniform(shadowProgram, "transform", Scale(60));
	floor_mesh.render();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DrawSceneWithShadows() {
	glViewport(0, 0, win_width, win_height);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glUseProgram(mainProgram);
	glCullFace(GL_BACK);
	// Draw scene as usual
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	SetUniform(mainProgram, "txtr", 0);
	SetUniform(mainProgram, "shadow", 1);
	SetUniform(mainProgram, "lightColor", vec3(lightColor[0], lightColor[1], lightColor[2]));
	SetUniform(mainProgram, "edgeSamples", shadowEdgeSamples);
	SetUniform(mainProgram, "depth_vp", depthVP);
	SetUniform(mainProgram, "persp", camera.persp);
	SetUniform(mainProgram, "view", camera.view);
	SetUniform(mainProgram, "model", floor_mesh.model);
	SetUniform(mainProgram, "transform", Scale(60));
	floor_mesh.render();
	//dTextureDebug::show(shadowTexture, 0, 0, win_width / 4, win_height / 4);
	glFlush();
}

void setup() {
	if (!(mainProgram = LinkProgramViaCode(&mainVert, &mainFrag)))
		throw runtime_error("Failed to compile main render program!");
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
}

void cleanup() {
	glDeleteFramebuffers(1, &shadowFramebuffer);
	glDeleteTextures(1, &shadowTexture);
}

void draw() {
	// Draw scene to depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glViewport(0, 0, SHADOW_DIM, SHADOW_DIM);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(shadowProgram);
	glCullFace(GL_FRONT);
	mat4 depthProj = Orthographic(-80, 80, -80, 80, -20, 100);
	mat4 depthView = LookAt(vec3(20, 30, 20), vec3(0, 0, 0), vec3(0, 1, 0));
	mat4 depthVP = depthProj * depthView;
	SetUniform(shadowProgram, "depth_vp", depthVP);
	SetUniform(shadowProgram, "model", floor_mesh.model);
	SetUniform(shadowProgram, "transform", Scale(60));
	floor_mesh.render();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// Draw scene as usual
	glViewport(0, 0, win_width, win_height);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glUseProgram(mainProgram);
	glCullFace(GL_BACK);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	SetUniform(mainProgram, "txtr", 0);
	SetUniform(mainProgram, "shadow", 1);
	SetUniform(mainProgram, "lightColor", vec3(lightColor[0], lightColor[1], lightColor[2]));
	SetUniform(mainProgram, "edgeSamples", shadowEdgeSamples);
	SetUniform(mainProgram, "depth_vp", depthVP);
	SetUniform(mainProgram, "persp", camera.persp);
	SetUniform(mainProgram, "view", camera.view);
	SetUniform(mainProgram, "model", floor_mesh.model);
	SetUniform(mainProgram, "transform", Scale(60));
	floor_mesh.render();
	//dTextureDebug::show(shadowTexture, 0, 0, win_width / 4, win_height / 4);
	glFlush();
}

int main() {
	setup();
	while (!glfwWindowShouldClose(window)) {
		draw();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();
}
