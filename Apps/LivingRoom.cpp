// LivingRoom.cpp

#include <glad.h>
#include <glfw3.h>
#include <vector>
#include "Camera.h"
#include "Draw.h"
#include "GLXtras.h"
#include "IO.h"
#include "Mesh.h"
#include "Shadow.h"

// view, display parameters
int		winWidth = 1024, winHeight = 1024;
Camera	camera(0, 0, winWidth, winHeight, vec3(0, 7, 0), vec3(0, 0, -8));

// scene
string	dir = "C:/Users/Elija/source/repos/Graphics/Assets/", objDir = dir+"Objects/", imgDir = dir+"Images/";
Mesh	room, dresser, sofa, rug, tv;
Mesh   *meshes[] = { &room, &dresser, &sofa, &rug, &tv };
int		nMeshes = sizeof(meshes)/sizeof(Mesh *);
vec3	light(10.f, 20.f, 0.f);

// Display

void Display() {
	glClearColor(.5f, .5f, .5f, 1);						// set background color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// clear background and z-buffer
	glEnable(GL_DEPTH_TEST);
	ShadowDraw(camera, light, winWidth, winHeight, meshes, nMeshes);
	glFlush();											// finish
}

// Scene

void ReadObject(Mesh &m, string objName, string imgName) { m.Read(objDir+objName, imgDir+imgName); }

void MakeScene() {
	ReadObject(room, "OpenBox.obj", "Floor.jpg");
	ReadObject(dresser, "dresser.obj", "dresser.tga");
	ReadObject(sofa, "Sofa.obj", "Sofa.png");
	ReadObject(rug, "Rug.obj", "Rug.png");
	ReadObject(tv, "TV.obj", "TV.png");
	room.toWorld = Translate(0, -1.1, 0)*RotateX(90)*Scale(1.8f, 1.2, .1f);
	dresser.toWorld = Translate(-1, -.8f, .15f)*RotateY(90)*Scale(.4f);
	sofa.toWorld = Translate(.8f, -.75f, .15f)*RotateY(-90)*Scale(.5f, 1, 1);
	rug.toWorld = Translate(0, -.99f, .15f)*Scale(.75f);
	tv.toWorld = Translate(-1.f, -.45f, .15f) * RotateY(90) * Scale(.2f);
}

// Callbacks

void MouseButton(float x, float y, bool left, bool down) {
	if (down)
		camera.Down(x, y, Shift()); else camera.Up();
}

void MouseMove(float x, float y, bool leftDown, bool rightDown) {
	if (leftDown)
		camera.Drag(x, y);
}

void MouseWheel(float spin) {
	camera.Wheel(spin, Shift());
}

void Resize(int width, int height) {
	winWidth = width;
	winHeight = height;
	camera.Resize(width, height);
	glViewport(0, 0, width, height);
}

// Application

const char *usage = R"(
	mouse-drag:  rotate
	with shift:  translate xy
	mouse-wheel: translate z
)";


int main(int ac, char **av) {
	// initialize
	GLFWwindow *w = InitGLFW(100, 50, winWidth, winHeight, "Living Room");
	MakeScene();
	if (!ShadowSetup()) printf("bad ShadowSetup\n");
	// callbacks
	RegisterMouseMove(MouseMove);
	RegisterMouseButton(MouseButton);
	RegisterMouseWheel(MouseWheel);
	RegisterResize(Resize);
	printf("Usage: %s", usage);
	// event loop
	while (!glfwWindowShouldClose(w)) {
		Display();
		glfwPollEvents();
		glfwSwapBuffers(w);
	}
}
