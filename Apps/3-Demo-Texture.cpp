// Demo-Texture.cpp

#include <glad.h>                                         
#include <glfw3.h>                                        
#include "GLXtras.h"  
#include "IO.h"														// ReadTexture

GLuint program = 0;
GLuint textureName = 0, textureNames[] = {0, 0}, textureUnit = 0;	// OpenGL identifiers
float scales[] = { 1, .45f };

const char *pixFiles[] = {
	"C:/USers/Jules/Code/Assets/Images/Parrots.jpg",
	"C:/Users/jules/Code/Assets/Images/MattedNumber1.png"
};

const char *vertexShader = R"(
	#version 130
	uniform float scale = 1;                                        // from CPU
	in vec2 point, uv;                                              // from GPU
	out vec2 vuv;           
	void main() {
		gl_Position = vec4(scale*point, 0, 1);                      // transform
		vuv = uv;                                                   // send to pixel shader
	}
)";

const char *pixelShader = R"(
	#version 330
	in vec2 vuv;                                                    // from vertex shader
	uniform sampler2D textureImage;                                 // from CPU
	out vec4 pColor;
	void main() {
		pColor = texture(textureImage, vuv);
		if (pColor.a < .02)											// if nearly transparent,
			discard;												// don't tag z-buffer
	}
)";

void MouseWheel(float spin) {
	for (size_t i = 0; i < sizeof(scales)/sizeof(float); i++)
		scales[i] *= spin > 0? 1.1f : .9f;
}

void Display() {
	glClearColor(0, 0, 1, 1);                                       // set background color
	glClear(GL_COLOR_BUFFER_BIT);                                   // clear background
	SetUniform(program, "textureImage", (int) textureUnit);         // set texture map for pixel shader
	glActiveTexture(GL_TEXTURE0+textureUnit);                       // make texture active
	glBindTexture(GL_TEXTURE_2D, textureNames[0]);
	SetUniform(program, "scale", scales[0]);                        // set scale for vertex shader
	glDrawArrays(GL_QUADS, 0, 4);                                   // draw object                     
	glBindTexture(GL_TEXTURE_2D, textureNames[1]);
	SetUniform(program, "scale", scales[1]);                        // set scale for vertex shader
	glDrawArrays(GL_QUADS, 0, 4);                                   // draw object                     
	glFlush();                                                      // finish scene
}

int main() {
	GLFWwindow *w = InitGLFW(200, 200, 600, 600, "Scale & Texture");
	RegisterMouseWheel(MouseWheel);
	program = LinkProgramViaCode(&vertexShader, &pixelShader);
	glUseProgram(program);
	GLuint vBuffer = 0;
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	vec2 pts[] = { {-1,-1}, {-1,1}, {1,1}, {1,-1} };                // corner locations
	vec2 uvs[] = { {0, 0}, {0, 1}, {1, 1}, {1, 0} };                // corner texture coordinates
	int spts = sizeof(pts), suvs = sizeof(uvs);                     // array sizes
	glBufferData(GL_ARRAY_BUFFER, spts+suvs, NULL, GL_STATIC_DRAW); // allocate GPU buffer
	glBufferSubData(GL_ARRAY_BUFFER, 0, spts, pts);                 // store pts
	glBufferSubData(GL_ARRAY_BUFFER, spts, suvs, uvs);              // store uvs after pts in mem
	textureNames[0] = ReadTexture(pixFiles[0]);						// store texture image
	textureNames[1] = ReadTexture(pixFiles[1]);						// store texture image
	VertexAttribPointer(program, "point", 2, 0, (void *) 0);        // feed points to vertex shader
	VertexAttribPointer(program, "uv", 2, 0, (void *) spts);        // feed uvs to vertex shader
	printf("mouse wheel to zoom in/out\n");
	while (!glfwWindowShouldClose(w)) {
		Display();
		glfwSwapBuffers(w);                          
		glfwPollEvents();
	}
}
