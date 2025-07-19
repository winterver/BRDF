#pragma once
#include <glad.h>
#include <glm/glm.hpp>
#include "renderpass.h"

class PBRMaterial
{
public:
    PBRMaterial()
        : albedoMap(0)
        , normalMap(0)
        , metallicMap(0)
        , roughnessMap(0)
    { }

    void setAlbedoMap(GLuint map) { albedoMap = map; }
    void setNormalMap(GLuint map) { normalMap = map; }
    void setMetallicMap(GLuint map) { metallicMap = map; }
    void setRoughnessMap(GLuint map) { roughnessMap = map; }

    GLuint getAlbedoMap() { return albedoMap; }
    GLuint getNormalMap() { return normalMap; }
    GLuint getMetallicMap() { return metallicMap; }
    GLuint getRoughnessMap() { return roughnessMap; }

private:
    GLuint albedoMap;
    GLuint normalMap;
    GLuint metallicMap;
    GLuint roughnessMap;
};

class Camera;
class Mesh;
class SkyboxMaterial;

class PBRRenderPass : public RenderPass {
public:
    PBRRenderPass();
    void drawVAO(Camera* camera, int vao, int count, const glm::mat4& model, PBRMaterial* material, SkyboxMaterial* skybox);
    void drawMesh(Camera* camera, Mesh* mesh, const glm::mat4& model, PBRMaterial* material, SkyboxMaterial* skybox);
    void drawSphere(Camera* camera, const glm::mat4& model, PBRMaterial* material, SkyboxMaterial* skybox);

private:
    void setupMatrix(Camera* camera, const glm::mat4& model);
    void useMaterial(PBRMaterial* material, SkyboxMaterial* skybox);

private:
    static GLuint program;
    static GLuint MVP_Location;
    static GLuint uModel_Location;
    static GLuint viewPos_Location;
};
