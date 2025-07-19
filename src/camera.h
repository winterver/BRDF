#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

class Camera
{
public:
    float fov = 60.0f;
    float vertical = 0.0f;
    float horizontal = glm::pi<float>();
    float speed = 3.0f;
    float mouseSpeed = 0.002f;
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::mat4 projection;
    glm::mat4 view;

    GLFWwindow* window;
    double lastX = 0, lastY = 0;

    Camera(GLFWwindow* window);
    void update(float deltaTime);
};
