#include <iostream>
#include <unordered_map>
#include <thread>

#include "glad.h"
#include "stb_image.h"
#include "tiny_obj_loader.hpp"
#include "embeded_shaders.h"

#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/hash.hpp>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoords;
};

class Camera
{
public:
    float fov = 60.0f;
    float vertical = 0.0f;
    float horizontal = glm::pi<float>();
    float speed = 3.0f;
    float mouseSpeed = 0.002f;
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 5.0f);

    GLFWwindow* window;
    double lastX = 0, lastY = 0;

    Camera(GLFWwindow* window) : window(window) {
        glfwGetCursorPos(window, &lastX, &lastY);
    }

    glm::mat4 update(float deltaTime)
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
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)width/height, 0.1f, 1000.0f);
        glm::mat4 view       = glm::lookAt(position, position + direction, glm::vec3(0, 1, 0));

        return projection * view;
    }
};

void loadModel(const char* path, GLuint* buffer, GLuint* index, int* nIndices)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, nullptr, &warn, &err, path)) {
        throw std::runtime_error(warn + err);
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::unordered_map<glm::ivec3, uint32_t> uniqueVertices{};
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2],
            };
            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2],
            };
            vertex.texcoords = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                attrib.texcoords[2 * index.texcoord_index + 1],
            };

            glm::ivec3 vtx = {
                index.vertex_index, 
                index.normal_index, 
                index.texcoord_index,
            };
            if (uniqueVertices.count(vtx) == 0) {
                uniqueVertices[vtx] = (uint32_t)vertices.size();
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vtx]);
        }
    }

    glGenBuffers(1, buffer);
    glBindBuffer(GL_ARRAY_BUFFER, *buffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, index);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *index);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
    *nIndices = (int)indices.size();
}

void loadTexture(const char* path, GLuint* texture)
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error(path);
    }

    glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(pixels);
}

void makeTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a, GLuint* texture)
{
    unsigned char data[] = { r, g, b, a };
    glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void compileShaders(GLuint* program, const char* vertex, const char* fragment)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &vertex, NULL);
    glShaderSource(fs, 1, &fragment, NULL);
    glCompileShader(vs);
    glCompileShader(fs);

    int length;

	glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &length);
	if (length > 0) {
		std::vector<char> message(length+1);
		glGetShaderInfoLog(vs, length, NULL, message.data());
		throw std::runtime_error(std::string(message.data()));
	}

    glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &length);
	if (length > 0) {
		std::vector<char> message(length+1);
		glGetShaderInfoLog(fs, length, NULL, message.data());
		throw std::runtime_error(std::string(message.data()));
	}

    *program = glCreateProgram();
    glAttachShader(*program, vs);
	glAttachShader(*program, fs);
	glLinkProgram(*program);

    glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &length);
	if (length > 0) {
		std::vector<char> message(length+1);
		glGetProgramInfoLog(*program, length, NULL, message.data());
		throw std::runtime_error(std::string(message.data()));
	}

	glDetachShader(*program, fs);
    glDetachShader(*program, vs);
	glDeleteShader(fs);
	glDeleteShader(vs);
}

int main()
{
    glfwInit();
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    constexpr int width = 1600, height = 900;
    GLFWwindow* window = glfwCreateWindow(width, height, "BRDF", nullptr, nullptr);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	glfwSetWindowPos(window, (mode->width - width)/2, (mode->height - height)/2);

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwMakeContextCurrent(window);
    gladLoadGL();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);

    GLuint vbo;
    GLuint ibo;
    int count;
    GLuint albedoMap;
    GLuint normalMap;
    GLuint metallicMap;
    GLuint roughnessMap;
    GLuint program;

    try {
        loadModel("src/models/MAC10.obj", &vbo, &ibo, &count);
        loadTexture("src/models/MAC10_albedo.png", &albedoMap);
        loadTexture("src/models/MAC10_normal.png", &normalMap);
        makeTexture(255, 255, 255, 255, &metallicMap);
        makeTexture(255, 255, 255, 255, &roughnessMap);
        /*
        loadModel("src/models/Sphere.obj", &vbo, &ibo, &count);
        loadTexture("src/models/rustediron2_basecolor.png", &albedoMap);
        loadTexture("src/models/rustediron2_normal.png", &normalMap);
        loadTexture("src/models/rustediron2_metallic.png", &metallicMap);
        loadTexture("src/models/rustediron2_roughness.png", &roughnessMap);
        */
        compileShaders(&program, brdf_vert, brdf_frag);
    } catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aNormal");
    glBindAttribLocation(program, 2, "aTexCoords");
    GLuint MVP_Location = glGetUniformLocation(program, "MVP");
    GLuint uModel_Location = glGetUniformLocation(program, "uModel");
    GLuint viewPos_Location = glGetUniformLocation(program, "viewPos");
    GLuint albedoMap_Location = glGetUniformLocation(program, "albedoMap");
    GLuint normalMap_Location = glGetUniformLocation(program, "normalMap");
    GLuint metallicMap_Location = glGetUniformLocation(program, "metallicMap");
    GLuint roughnessMap_Location = glGetUniformLocation(program, "roughnessMap");

    glUseProgram(program);
    glUniform1i(albedoMap_Location, 0);
    glUniform1i(normalMap_Location, 1);
    glUniform1i(metallicMap_Location, 2);
    glUniform1i(roughnessMap_Location, 3);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedoMap);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalMap);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, metallicMap);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, roughnessMap);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoords));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    int framerate = 120;
    double lastTime = 0;
    Camera camera(window);

    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && !glfwWindowShouldClose(window)) {
        glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float deltaTime = float(glfwGetTime() - lastTime);
        while (glfwGetTime() < (lastTime + 1.0/framerate)) {
            deltaTime = 1.0f/framerate;
            std::this_thread::yield();
        }
        lastTime = glfwGetTime();

        glm::mat4 uModel = glm::mat4(1.0f);
        glm::mat4 PV = camera.update(deltaTime);
        glm::mat4 MVP = PV * uModel;

        glUseProgram(program);
        glUniformMatrix4fv(MVP_Location, 1, GL_FALSE, &MVP[0][0]);
        glUniformMatrix4fv(uModel_Location, 1, GL_FALSE, &uModel[0][0]);
        glUniform3fv(viewPos_Location, 1, &camera.position[0]);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
