#ifndef SHADOW
#define SHADOW

#include "glad.h"
#include "Camera.h"
#include "Mesh.h"

// Shadow Operations

void ShadowDraw(Camera cam, vec3 light, int winWidth, int winHeight, Mesh *meshes[], int nMeshes);
	// display meshes with shadow map built from light

bool ShadowSetup();
	// set up shadow map framebuffer
	
#endif
