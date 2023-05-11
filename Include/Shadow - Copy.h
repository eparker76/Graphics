#pragma once
#ifndef SHADOW
#define SHADOW

#include <vector>
#include "glad.h"
#include "Camera.h"
#include "IO.h"
#include "Quaternion.h"
#include "VecMat.h"

using namespace std;

// Shadow Class and Operations

GLuint GetMeshShader1();
GLuint UseMeshShader1();
GLuint GetShadowShader1();
GLuint UseShadowShader1();

class Shadow {
public:
	Shadow() {};
	~Shadow() { glDeleteBuffers(1, &vBufferId); };
	void Buffer();
	void Buffer(vector<vec3>& pts, vector<vec3>* nrms = NULL, vector<vec2>* uvs = NULL);
	// If non-null, nrms and uvs assumed same size as pts
	void Display(Camera camera, vec3 light, int textureUnit = -1, bool useGroupColor = false);
	// This displays with a shadow map and needs a light source
		// texture is enabled if textureUnit >= 0 and textureName set
		// before this call, app must optionally change uniforms from their default, including:
		//     nLights, lights, color, opacity, ambient
		//     useLight, useTint, fwdFacingOnly, facetedShading
		//     outlineColor, outlineWidth, transition
	void ShadowDisplay(Camera camera, vec3 light, int textureUnit = -1, bool useGroupColor = false);
	// This renders an object from the point of view of the light to create a shadowmap
	bool Read(string objFile, mat4* m = NULL, bool standardize = true, bool buffer = true, bool forceTriangles = false);
	// Read in object file (with normals, uvs), initialize matrix, build vertex buffer
	bool Read(string objFile, string texFile, mat4* m = NULL, bool standardize = true, bool buffer = true, bool forceTriangles = false);
	// Read in object file (with normals, uvs) and texture file, initialize matrix, build vertex buffer
	bool ShadowSetup();
	// Sets up the framebuffer to hold the shadow map
	
	int shadowEdgeSamples = 16;
	const int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
	string objFilename, texFilename;
	// vertices and facets
	vector<vec3>	points;
	vector<vec3>	normals;
	vector<vec2>	uvs;
	vector<int3>	triangles;
	vector<int4>	quads;
	// ancillary data
	vector<Group>	triangleGroups;
	vector<Mtl>		triangleMtls;
	// position/orientation
	vec3			centerOfRotation;
	mat4			toWorld;
	// GPU vertex buffer and texture
	GLuint			vao = 0;		// vertex array object
	GLuint			vBufferId = 0;	// vertex buffer
	GLuint			eBufferId = 0;	// element (triangle) buffer
	GLuint			textureName = 0;
	GLuint			shadowProgram = 0;
	GLuint			shadowFramebuffer = 0;
	GLuint			shadowTexture = 0;
};

#endif
