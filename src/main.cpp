#include <iostream>
#include <thread>

#include <glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "shaders.h"
#include "skybox.h"
#include "pbr.h"
#include "mesh.h"
#include "camera.h"
#include "text.h"

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
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // float at the center of the screen in tiling WM environments.
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
    glfwSwapInterval(0);

    gladLoadGL();
    glDebugMessageCallbackARB(&DebugOutputCallback, NULL);

    Shaders::compile();

    SkyboxRenderPass skybox;
    PBRRenderPass pbr;
    Camera camera(window);
    TextRenderPass text(window);

    TextMaterial cour;
    cour.loadFont("models/Courier-New.ttf");

    TextMaterial font;
    font.loadFont("models/CormorantGaramond-Light.ttf");

    Mesh mac10;
    mac10.loadObj("models/MAC10.obj");

    SkyboxMaterial skyboxMaterial;
    skyboxMaterial.bake("models/dawn.hdr", "models/BRDF_LUT.dds");

    PBRMaterial material;
    material.setAlbedoMap(RenderPass::loadTexture("models/MAC10_albedo.png"));
    material.setNormalMap(RenderPass::loadTexture("models/MAC10_normal.png"));
    material.setMetallicMap(RenderPass::makeTexture(255, 255, 255, 255));
    material.setRoughnessMap(RenderPass::makeTexture(0, 0, 0, 255));

    PBRMaterial chromium;
    chromium.setAlbedoMap(RenderPass::makeTexture(255, 255, 255, 255));
    chromium.setNormalMap(RenderPass::makeTexture(128, 128, 255, 255));
    chromium.setMetallicMap(RenderPass::makeTexture(255, 255, 255, 255));
    chromium.setRoughnessMap(RenderPass::makeTexture(0, 0, 0, 255));

    PBRMaterial rustediron2; 
    rustediron2.setAlbedoMap(RenderPass::loadTexture("models/rustediron2_basecolor.png"));
    rustediron2.setNormalMap(RenderPass::loadTexture("models/rustediron2_normal.png"));
    rustediron2.setMetallicMap(RenderPass::loadTexture("models/rustediron2_metallic.png"));
    rustediron2.setRoughnessMap(RenderPass::loadTexture("models/rustediron2_roughness.png"));

    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.5f, 0.5f, 1.0f, 1.0f);

    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && !glfwWindowShouldClose(window)) {
        const int framerate = 120;
        static double lastTime = 0;
        float deltaTime = float(glfwGetTime() - lastTime);
        while (glfwGetTime() < (lastTime + 1.0/framerate)) {
            deltaTime = 1.0f/framerate;
            std::this_thread::yield();
        }
        lastTime = glfwGetTime();

        camera.update(deltaTime);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        skybox.drawSkybox(&camera, &skyboxMaterial);

        pbr.drawMesh(&camera, &skyboxMaterial, &material, glm::mat4(1.0), &mac10);
        pbr.drawSphere(&camera, &skyboxMaterial, &chromium, glm::translate(glm::vec3(2, 0, 0)));
        pbr.drawSphere(&camera, &skyboxMaterial, &rustediron2, glm::translate(glm::vec3(-2, 0, 0)));

        static char buf[64];
        static float accumulated = 1.0f;
        accumulated += deltaTime;
        if (accumulated >= 1.0f) {
            accumulated = 0.0f;
            sprintf(buf, "FPS: %.0f", 1.0f/deltaTime);
        }
        text.drawText(&cour, 64, glm::vec2(0, 0), buf);

        char buf2[64];
        sprintf(buf2, "dir: %f %f %f", camera.direction.x, camera.direction.y, camera.direction.z);
        text.drawText(&cour, 64, glm::vec2(0, 100), buf2);
        sprintf(buf2, "pos: %f %f %f", camera.position.x, camera.position.y, camera.position.z);
        text.drawText(&cour, 64, glm::vec2(0, 200), buf2);

        text.drawText(&cour, 64, glm::vec2(0, 300), "Hello Text!");
        text.drawText(&font, 64, glm::vec2(0, 400),
            "Gallia est omnis divisa in partes tres,\n"
            "Quarum unam incolunt Belgae, aliam Aquitani,\n"
            "Tertiam qui ipsorum linuga Celtae, nostra\n"
            "Galli appellantur.");

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
