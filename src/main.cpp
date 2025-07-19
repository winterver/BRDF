#include <iostream>
#include <thread>

#include "../lib/glad.h"
#include "camera.h"
#include "mesh.h"
#include "renderpass.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "shaders.h"

class SkyboxMaterial {
public:
    SkyboxMaterial()
        : cubeMap(0)
        , irradianceMap(0)
        , prefilterMap(0)
        , brdflutMap(0)
    { }

    void bake(const char* hdr, const char* lut) {
        RenderPass::bakeHDR(hdr, &cubeMap, &irradianceMap, &prefilterMap);
        RenderPass::loadBRDFLUT(lut, &brdflutMap);
    }

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

class SkyboxRenderPass : public RenderPass {
public:
    SkyboxRenderPass() {
        if (vao == 0) {
            glGenVertexArrays(1, &vao);
            linkProgram(&skyboxprog,
                Shaders::skyboxVertexShader(),
                Shaders::skyboxFragmentShader());
            uProj_Location = glGetUniformLocation(skyboxprog, "uProj");
            uView_Location = glGetUniformLocation(skyboxprog, "uView");
            skybox_Location = glGetUniformLocation(skyboxprog, "skybox");
        }
    }

    void drawSkybox(Camera* camera, SkyboxMaterial* material) {
        glUseProgram(skyboxprog);
        glUniformMatrix4fv(uProj_Location, 1, GL_FALSE, &camera->projection[0][0]);
        glUniformMatrix4fv(uView_Location, 1, GL_FALSE, &camera->view[0][0]);
        glUniform1i(skybox_Location, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, material->getCubeMap());
        glDepthMask(GL_FALSE);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);
    }

private:
    static GLuint vao;
    static GLuint skyboxprog;
    static GLuint uProj_Location;
    static GLuint uView_Location;
    static GLuint skybox_Location;
};

GLuint SkyboxRenderPass::vao;
GLuint SkyboxRenderPass::skyboxprog;
GLuint SkyboxRenderPass::uProj_Location;
GLuint SkyboxRenderPass::uView_Location;
GLuint SkyboxRenderPass::skybox_Location;

class PBRMaterial
{
public:
    PBRMaterial()
        : albedoMap(0)
        , normalMap(0)
        , metallicMap(0)
        , roughnessMap(0)
        , irradianceMap(0)
        , prefilterMap(0)
        , brdflutMap(0)
    { }

    void setAlbedoMap(GLuint map) { albedoMap = map; }
    void setNormalMap(GLuint map) { normalMap = map; }
    void setMetallicMap(GLuint map) { metallicMap = map; }
    void setRoughnessMap(GLuint map) { roughnessMap = map; }
    void setIrradianceMap(GLuint map) { irradianceMap = map; }
    void setPrefilterMap(GLuint map) { prefilterMap = map; }
    void setBRDFLUTMap(GLuint map) { brdflutMap = map; }
    GLuint getAlbedoMap() { return albedoMap; }
    GLuint getNormalMap() { return normalMap; }
    GLuint getMetallicMap() { return metallicMap; }
    GLuint getRoughnessMap() { return roughnessMap; }
    GLuint getIrradianceMap() { return irradianceMap; }
    GLuint getPrefilterMap() { return prefilterMap; }
    GLuint getBRDFLUTMap() { return brdflutMap; }

private:
    GLuint albedoMap;
    GLuint normalMap;
    GLuint metallicMap;
    GLuint roughnessMap;
    GLuint irradianceMap;
    GLuint prefilterMap;
    GLuint brdflutMap;
};

class PBRRenderPass : public RenderPass {
public:
    PBRRenderPass() {
        if (program == 0) {
            linkProgram(&program,
                Shaders::pbrVertexShader(),
                Shaders::pbrFragmentShader());

            glBindAttribLocation(program, 0, "aPosition");
            glBindAttribLocation(program, 1, "aNormal");
            glBindAttribLocation(program, 2, "aTexCoords");

            glUseProgram(program);
            glUniform1i(glGetUniformLocation(program, "albedoMap"), 0);
            glUniform1i(glGetUniformLocation(program, "normalMap"), 1);
            glUniform1i(glGetUniformLocation(program, "metallicMap"), 2);
            glUniform1i(glGetUniformLocation(program, "roughnessMap"), 3);
            glUniform1i(glGetUniformLocation(program, "irradianceMap"), 4);
            glUniform1i(glGetUniformLocation(program, "prefilterMap"), 5);
            glUniform1i(glGetUniformLocation(program, "brdflutMap"), 6);
            MVP_Location = glGetUniformLocation(program, "MVP");
            uModel_Location = glGetUniformLocation(program, "uModel");
            viewPos_Location = glGetUniformLocation(program, "viewPos");
        }
    }

    void drawVAO(Camera* camera, int vao, int count, const glm::mat4& model, PBRMaterial* material) {
        glUseProgram(program);
        setupMatrix(camera, model);
        useMaterial(material);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    }

    void drawMesh(Camera* camera, Mesh* mesh, const glm::mat4& model, PBRMaterial* material) {
        glUseProgram(program);
        setupMatrix(camera, model);
        useMaterial(material);
        glBindVertexArray(mesh->getVAO());
        glDrawElements(GL_TRIANGLES, mesh->getCount(), GL_UNSIGNED_INT, 0);
    }

    void drawSphere(Camera* camera, const glm::mat4& model, PBRMaterial* material) {
        glUseProgram(program);
        setupMatrix(camera, model);
        useMaterial(material);
        renderSphere();
    }

private:
    void setupMatrix(Camera* camera, const glm::mat4& model) { 
        glm::mat4 MVP = camera->projection * camera->view * model;
        glUniformMatrix4fv(MVP_Location, 1, GL_FALSE, &MVP[0][0]);
        glUniformMatrix4fv(uModel_Location, 1, GL_FALSE, &model[0][0]);
        glUniform3fv(viewPos_Location, 1, &camera->position[0]);
    }

    void useMaterial(PBRMaterial* material) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, material->getAlbedoMap());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, material->getNormalMap());
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material->getMetallicMap());
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, material->getRoughnessMap());
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_CUBE_MAP, material->getIrradianceMap());
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, material->getPrefilterMap());
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, material->getBRDFLUTMap());
    }

private:
    static GLuint program;
    static GLuint MVP_Location;
    static GLuint uModel_Location;
    static GLuint viewPos_Location;
};

GLuint PBRRenderPass::program;
GLuint PBRRenderPass::MVP_Location;
GLuint PBRRenderPass::uModel_Location;
GLuint PBRRenderPass::viewPos_Location;

void APIENTRY DebugOutputCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    /* parameter 'message', on windows, does not end in '\n',
     * but on linux, it does. */
#ifndef _WIN32
    printf("DebugOutputCallback: %s", message);
#else
    if (severity != 0x826b) // ignore notification
        printf("DebugOutputCallback: %s\n", message);
#endif
}

int main()
{
    glfwInit();
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_SAMPLES, 4);
#ifndef _WIN32
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // float at center of screen for tiling WMs.
#endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // must be set to false for GLFW_FLOATING to take effect
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    constexpr int width = 1600, height = 900;
    GLFWwindow* window = glfwCreateWindow(width, height, "BRDF", nullptr, nullptr);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowPos(window, (mode->width - width)/2, (mode->height - height)/2);

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwMakeContextCurrent(window);
    gladLoadGL();
    glDebugMessageCallbackARB(&DebugOutputCallback, NULL);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);

    Shaders::compile();

    SkyboxRenderPass skybox;
    PBRRenderPass pbr;

    SkyboxMaterial skyboxMaterial;
    skyboxMaterial.bake("models/dawn.hdr", "models/BRDF_LUT.dds");

    PBRMaterial material;
    material.setAlbedoMap(RenderPass::loadTexture("models/MAC10_albedo.png"));
    material.setNormalMap(RenderPass::loadTexture("models/MAC10_normal.png"));
    material.setMetallicMap(RenderPass::makeTexture(255, 255, 255, 255));
    material.setRoughnessMap(RenderPass::makeTexture(0, 0, 0, 255));
    material.setIrradianceMap(skyboxMaterial.getIrradianceMap());
    material.setPrefilterMap(skyboxMaterial.getPrefilterMap());
    material.setBRDFLUTMap(skyboxMaterial.getBRDFLUTMap());

    PBRMaterial chromium;
    chromium.setAlbedoMap(RenderPass::makeTexture(255, 255, 255, 255));
    chromium.setNormalMap(RenderPass::makeTexture(128, 128, 255, 255));
    chromium.setMetallicMap(RenderPass::makeTexture(255, 255, 255, 255));
    chromium.setRoughnessMap(RenderPass::makeTexture(0, 0, 0, 255));
    chromium.setIrradianceMap(skyboxMaterial.getIrradianceMap());
    chromium.setPrefilterMap(skyboxMaterial.getPrefilterMap());
    chromium.setBRDFLUTMap(skyboxMaterial.getBRDFLUTMap());

    PBRMaterial rustediron2; 
    rustediron2.setAlbedoMap(RenderPass::loadTexture("models/rustediron2_basecolor.png"));
    rustediron2.setNormalMap(RenderPass::loadTexture("models/rustediron2_normal.png"));
    rustediron2.setMetallicMap(RenderPass::loadTexture("models/rustediron2_metallic.png"));
    rustediron2.setRoughnessMap(RenderPass::loadTexture("models/rustediron2_roughness.png"));
    rustediron2.setIrradianceMap(skyboxMaterial.getIrradianceMap());
    rustediron2.setPrefilterMap(skyboxMaterial.getPrefilterMap());
    rustediron2.setBRDFLUTMap(skyboxMaterial.getBRDFLUTMap());

    Mesh mac10;
    mac10.loadObj("models/MAC10.obj");

    int framerate = 120;
    double lastTime = 0;
    Camera camera(window);

    glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && !glfwWindowShouldClose(window)) {
        float deltaTime = float(glfwGetTime() - lastTime);
        while (glfwGetTime() < (lastTime + 1.0/framerate)) {
            deltaTime = 1.0f/framerate;
            std::this_thread::yield();
        }
        lastTime = glfwGetTime();

        camera.update(deltaTime);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        skybox.drawSkybox(&camera, &skyboxMaterial);

        pbr.drawMesh(&camera, &mac10, glm::mat4(1.0), &material);
        pbr.drawSphere(&camera, glm::translate(glm::vec3(1.5, 0, 0)), &chromium);
        pbr.drawSphere(&camera, glm::translate(glm::vec3(-1.5, 0, 0)), &rustediron2);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
