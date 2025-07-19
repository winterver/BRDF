#pragma once
#include "../lib/glad.h"

class RenderPass {
public:
    static void linkProgram(GLuint* program, GLuint vs, GLuint fs);
    static void bakeHDR(const char* path, GLuint* cubeMap, GLuint* irradianceMap, GLuint* prefilterMap);
    static void loadBRDFLUT(const char* path, GLuint* brdflutMap, int size = 512);
    static void renderSphere();
    static GLuint loadTexture(const char* path);
    static GLuint makeTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
};
