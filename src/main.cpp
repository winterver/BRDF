#include <iostream>
#include <unordered_map>
#include <thread>
#include <fstream>

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

    void update(float deltaTime, glm::mat4& projection, glm::mat4& view)
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

void createFramebuffer(int width, int height, GLuint* framebuffer, GLuint* texture, GLuint* renderbuffer)
{
    glGenFramebuffers(1, framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, *framebuffer);

    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0);

    if (renderbuffer) {
        glGenRenderbuffers(1, renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, *renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *renderbuffer);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Incomplete framebuffer");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void bakeHDR(const char* path, GLuint* cubeMap, GLuint* irradianceMap, GLuint* prefilterMap)
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    float* pixels = stbi_loadf(path, &width, &height, &channels, STBI_rgb);

    if (!pixels) {
        throw std::runtime_error(path);
    }

    GLuint hdr;
    glGenTextures(1, &hdr);
    glBindTexture(GL_TEXTURE_2D, hdr);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, pixels); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenTextures(1, cubeMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, *cubeMap);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 0, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 1, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 2, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 3, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 4, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 5, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, *irradianceMap);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 0, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 1, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 2, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 3, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 4, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 5, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, *prefilterMap);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 0, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 1, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 2, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 3, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 4, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 5, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    static GLuint program = 0;
    static GLuint convolution;
    static GLuint prefilter;
    static GLuint vao;

    if (program == 0) {
        compileShaders(&program, bakehdr_vert, bakehdr_frag);
        compileShaders(&convolution, bakehdr_vert, bakehdr_irradiance_convolution_frag);
        compileShaders(&prefilter, bakehdr_vert, bakehdr_prefilter_frag);
        glGenVertexArrays(1, &vao);
    }

    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);

    GLint view[4];
    glGetIntegerv(GL_VIEWPORT, view);
    glBindVertexArray(vao);

    for (int i = 0; i < 6; i++) {
        glViewport(0, 0, 1024, 1024);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *cubeMap, 0); 
        glUseProgram(program);
        int face_Location = glGetUniformLocation(program, "face");
        glUniform1i(face_Location, i);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdr);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, *cubeMap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    for (int i = 0; i < 6; i++) {
        glViewport(0, 0, 1024, 1024);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *irradianceMap, 0); 
        glUseProgram(convolution);
        int face_Location = glGetUniformLocation(convolution, "face");
        glUniform1i(face_Location, i);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, *cubeMap);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        int mipWidth  = (int)(1024 * std::pow(0.5, mip));
        int mipHeight = (int)(1024 * std::pow(0.5, mip));
        float roughness = (float)mip / (float)(maxMipLevels - 1);

        for (unsigned int i = 0; i < 6; ++i)
        {
            glViewport(0, 0, mipWidth, mipHeight);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *prefilterMap, mip);
            glUseProgram(prefilter);
            int face_Location = glGetUniformLocation(prefilter, "face");
            int roughness_Location = glGetUniformLocation(prefilter, "roughness");
            glUniform1i(face_Location, i);
            glUniform1f(roughness_Location, roughness);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, *cubeMap);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }
    
    glViewport(view[0], view[1], view[2], view[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &hdr);
    stbi_image_free(pixels);
}

void loadBRDFLUT(const char* path, GLuint* brdflutMap, int size = 512)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to load BRDF LUT");
    }

    file.seekg(128); // skip header
    char* data = new char[4*size*size];
    file.read(data, 4*size*size);

    glGenTextures(1, brdflutMap);
    glBindTexture(GL_TEXTURE_2D, *brdflutMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_HALF_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    delete[] data;
}

void APIENTRY DebugOutputCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    printf("Message : %s\n", message);
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

    GLuint vbo;
    GLuint ibo;
    int count;
    GLuint albedoMap;
    GLuint normalMap;
    GLuint metallicMap;
    GLuint roughnessMap;
    GLuint program;
    GLuint skyboxprog;
    GLuint cubeMap;
    GLuint irradianceMap;
    GLuint prefilterMap;
    GLuint brdflutMap;

    try {
        loadModel("models/MAC10.obj", &vbo, &ibo, &count);
        loadTexture("models/MAC10_albedo.png", &albedoMap);
        loadTexture("models/MAC10_normal.png", &normalMap);
        makeTexture(255, 255, 255, 255, &metallicMap);
        makeTexture(255, 255, 255, 255, &roughnessMap);
        /*
        loadModel("models/Sphere.obj", &vbo, &ibo, &count);
        loadTexture("models/rustediron2_basecolor.png", &albedoMap);
        loadTexture("models/rustediron2_normal.png", &normalMap);
        loadTexture("models/rustediron2_metallic.png", &metallicMap);
        loadTexture("models/rustediron2_roughness.png", &roughnessMap);
        */
        compileShaders(&program, pbr_vert, pbr_frag);
        compileShaders(&skyboxprog, skybox_vert, skybox_frag);
        bakeHDR("models/dawn.hdr", &cubeMap, &irradianceMap, &prefilterMap);
        loadBRDFLUT("models/BRDF_LUT.dds", &brdflutMap);
    } catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aNormal");
    glBindAttribLocation(program, 2, "aTexCoords");

    glUseProgram(program);
    GLuint albedoMap_Location = glGetUniformLocation(program, "albedoMap");
    GLuint normalMap_Location = glGetUniformLocation(program, "normalMap");
    GLuint metallicMap_Location = glGetUniformLocation(program, "metallicMap");
    GLuint roughnessMap_Location = glGetUniformLocation(program, "roughnessMap");
    GLuint irradianceMap_Location = glGetUniformLocation(program, "irradianceMap");
    GLuint prefilterMap_Location = glGetUniformLocation(program, "prefilterMap");
    GLuint brdflutMap_Location = glGetUniformLocation(program, "brdflutMap");
    glUniform1i(albedoMap_Location, 0);
    glUniform1i(normalMap_Location, 1);
    glUniform1i(metallicMap_Location, 2);
    glUniform1i(roughnessMap_Location, 3);
    glUniform1i(irradianceMap_Location, 4);
    glUniform1i(prefilterMap_Location, 5);
    glUniform1i(brdflutMap_Location, 6);

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

    glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS && !glfwWindowShouldClose(window)) {
        float deltaTime = float(glfwGetTime() - lastTime);
        while (glfwGetTime() < (lastTime + 1.0/framerate)) {
            deltaTime = 1.0f/framerate;
            std::this_thread::yield();
        }
        lastTime = glfwGetTime();

        glm::mat4 uProj, uView, uModel = glm::mat4(1.0f);
        camera.update(deltaTime, uProj, uView);
        glm::mat4 MVP = uProj * uView * uModel;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(skyboxprog);
        int uProj_Location = glGetUniformLocation(skyboxprog, "uProj");
        int uView_Location = glGetUniformLocation(skyboxprog, "uView");
        int skybox_Location = glGetUniformLocation(skyboxprog, "skybox");
        glUniformMatrix4fv(uProj_Location, 1, GL_FALSE, &uProj[0][0]);
        glUniformMatrix4fv(uView_Location, 1, GL_FALSE, &uView[0][0]);
        glUniform1i(skybox_Location, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);

        glDepthMask(GL_FALSE);
        glBindVertexArray(vao); // placeholder
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);

        glUseProgram(program);
        int MVP_Location = glGetUniformLocation(program, "MVP");
        int uModel_Location = glGetUniformLocation(program, "uModel");
        int viewPos_Location = glGetUniformLocation(program, "viewPos");
        glUniformMatrix4fv(MVP_Location, 1, GL_FALSE, &MVP[0][0]);
        glUniformMatrix4fv(uModel_Location, 1, GL_FALSE, &uModel[0][0]);
        glUniform3fv(viewPos_Location, 1, &camera.position[0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, albedoMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normalMap);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, metallicMap);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, roughnessMap);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, brdflutMap);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
