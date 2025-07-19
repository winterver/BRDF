#include <iostream>
#include <thread>

#include "../lib/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "skybox.h"
#include "pbr.h"
#include "mesh.h"
#include "camera.h"
#include "shaders.h"

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
