#include "camera.h"
#include <glm/gtx/transform.hpp>

Camera::Camera(GLFWwindow* window) : window(window) {
    glfwGetCursorPos(window, &lastX, &lastY);
}

void Camera::update(float deltaTime)
{
    double currentX, currentY;
    glfwGetCursorPos(window, &currentX, &currentY);
    double dx = currentX - lastX;
    double dy = currentY - lastY;
    lastX = currentX;
    lastY = currentY;

    horizontal -= float(mouseSpeed * dx);
    vertical   -= float(mouseSpeed * dy);
    horizontal = glm::mod(horizontal, glm::two_pi<float>());
    vertical = glm::clamp(vertical, -1.57f, 1.57f);

    glm::vec3 direction(
        cos(vertical) * sin(horizontal),
        sin(vertical),
        cos(vertical) * cos(horizontal)
    );

    glm::vec3 forward(
        sin(horizontal),
        0,
        cos(horizontal)
    );

    glm::vec3 right(
        sin(horizontal - glm::half_pi<float>()),
        0,
        cos(horizontal - glm::half_pi<float>())
    );

    glm::vec3 up = glm::vec3(0, 1, 0);

    glm::vec3 velocity{};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS){
        velocity += forward;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){
        velocity -= forward;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS){
        velocity += right;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS){
        velocity -= right;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS){
        velocity += up;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS){
        velocity -= up;
    }
    if (glm::length(velocity) > 0) {
        position += glm::normalize(velocity) * speed * deltaTime;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    projection = glm::perspective(glm::radians(fov), (float)width/height, 0.1f, 1000.0f);
    view       = glm::lookAt(position, position + direction, glm::vec3(0, 1, 0));
}
