// Elijah Parker
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
Camera	camera(0, 0, winWidth, winHeight, vec3(0, 7, 0), vec3(0, 0, -80));
vec3 light = (20.0, 20.0, 20.0);

// scene
string	dir = "C:/Users/Elija/source/repos/Graphics/Assets/", objDir = dir+"Objects/", imgDir = dir+"Images/";
Mesh	room, dresser, sofa, rug, tv;
Mesh   *meshes[] = { &room, &dresser, &sofa, &rug, &tv };
int		nMeshes = sizeof(meshes)/sizeof(Mesh *);

// Sofa Matrices
float sofaX, sofaY, sofaZ, sofaR, sofaS;


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
	sofaX = 4, sofaY = -1.3, sofaZ = 0.0, sofaR = -90, sofaS = 8;

	ReadObject(room, "OpenBox.obj", "Floor.jpg");
	ReadObject(dresser, "dresser.obj", "dresser.tga");
	ReadObject(sofa, "Sofa.obj", "Sofa.png");
	ReadObject(rug, "Rug.obj", "Rug.png");
	ReadObject(tv, "TV.obj", "TV.png");
	room.toWorld = Translate(0, -3.5, 0)*RotateX(90)*Scale(20, 20, .1f);
	dresser.toWorld = Translate(-10, -1.8, 0)*RotateY(90)*Scale(3);
	sofa.toWorld = Translate(4, -1.3, 0)*RotateY(-90)*Scale(8);
	rug.toWorld = Translate(-2.5, -3.4, 0)*Scale(5, 30, 5);
	tv.toWorld = Translate(-10, 1, 0) * RotateY(90) * Scale(2);
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

// Keyboard inputs

void KeyboardInput(int key, bool press, bool shift, bool control) {
	// Light translate
	if (key == GLFW_KEY_A && press == GLFW_PRESS)
		light.x += 1.0;
	if (key == GLFW_KEY_D && press == GLFW_PRESS)
		light.x -= 1.0;
	if (key == GLFW_KEY_W && press == GLFW_PRESS)
		light.z -= 1.0;
	if (key == GLFW_KEY_S && press == GLFW_PRESS)
		light.z += 1.0;
	if (key == GLFW_KEY_Q && press == GLFW_PRESS)
		light.y += 1.0;
	if (key == GLFW_KEY_E && press == GLFW_PRESS)
		light.y -= 1.0;

	// Sofa object translate
	if (key == GLFW_KEY_H && press == GLFW_PRESS) {
		sofaX += 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}
	if (key == GLFW_KEY_K && press == GLFW_PRESS) {
		sofaX -= 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}
	if (key == GLFW_KEY_U && press == GLFW_PRESS) {
		sofaZ -= 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}
	if (key == GLFW_KEY_J && press == GLFW_PRESS) {
		sofaZ += 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}
	if (key == GLFW_KEY_Y && press == GLFW_PRESS) {
		sofaY += 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}
	if (key == GLFW_KEY_I && press == GLFW_PRESS) {
		sofaY -= 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}

	// Sofa object rotation
	if (key == GLFW_KEY_V && press == GLFW_PRESS) {
		sofaR += 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}
	if (key == GLFW_KEY_B && press == GLFW_PRESS) {
		sofaR -= 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}

	// Sofa object scale
	if (key == GLFW_KEY_N && press == GLFW_PRESS) {
		sofaS += 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}
	if (key == GLFW_KEY_M && press == GLFW_PRESS) {
		sofaS -= 1.0;
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}

	// Sofa object reset
	if (key == GLFW_KEY_1 && press == GLFW_PRESS) {
		sofaX = 4, sofaY = -1.3, sofaZ = 0.0, sofaR = -90, sofaS = 8;
		light = (20.0, 20.0, 20.0);
		sofa.toWorld = Translate(sofaX, sofaY, sofaZ) * RotateY(sofaR) * Scale(sofaS);
	}
}

// Application

const char *usage = R"(
	Camera Movement:
	mouse-drag:  rotate
	with shift:  translate xy
	mouse-wheel: translate z
	
	Light Movement:
	A: translate x
	D: translate -x
	W: translate -z
	S: translate z
	Q: translate y
	E: translate -y

	Sofa Movement:
	H: translate x
	K: translate -x
	U: translate -z
	J: translate z
	Y: translate y
	I: translate -y
	V: rotate clockwise
	B: rotate counter-clockwise
	N: scale up
	M: scale down

	1: Reset the scene
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
	RegisterKeyboard(KeyboardInput);
	printf("Usage: %s", usage);
	// event loop
	while (!glfwWindowShouldClose(w)) {
		Display();
		glfwPollEvents();
		glfwSwapBuffers(w);
	}
}
