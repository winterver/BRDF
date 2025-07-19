#pragma once
#include "../lib/glad.h"
#include "renderpass.h"
#include <glm/glm.hpp>

class SkyboxMaterial {
public:
    SkyboxMaterial()
        : cubeMap(0)
        , irradianceMap(0)
        , prefilterMap(0)
        , brdflutMap(0)
    { }

    void bake(const char* hdr, const char* lut);

    GLuint getCubeMap() { return cubeMap; }
    GLuint getIrradianceMap() { return irradianceMap; }
    GLuint getPrefilterMap() { return prefilterMap; }
    GLuint getBRDFLUTMap() { return brdflutMap; }

private:
    GLuint cubeMap;
    GLuint irradianceMap;
    GLuint prefilterMap;
    GLuint brdflutMap;
};

class Camera;

class SkyboxRenderPass : public RenderPass {
public:
    SkyboxRenderPass();
    void drawSkybox(Camera* camera, SkyboxMaterial* material);

private:
    static GLuint vao;
    static GLuint skyboxprog;
    static GLuint uProj_Location;
    static GLuint uView_Location;
    static GLuint skybox_Location;
};
