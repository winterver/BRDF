#include <iostream>
#include <unordered_map>
#include <thread>
#include <fstream>

#include "../lib/glad.h"
#include "../lib/stb_image.h"
#include "../lib/tiny_obj_loader.hpp"

#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/hash.hpp>

#include "shaders.h"

class Mesh {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texcoords;
    };

public:
    Mesh() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoords));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
    }

    void loadObj(const char* path)
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

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        count = (int)indices.size();
    }

    GLuint getVAO() { return vao; }
    GLuint getCount() { return count; }

private:
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
    int count;
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
    glm::mat4 projection;
    glm::mat4 view;

    GLFWwindow* window;
    double lastX = 0, lastY = 0;

    Camera(GLFWwindow* window) : window(window) {
        glfwGetCursorPos(window, &lastX, &lastY);
    }

    void update(float deltaTime)
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

class RenderPass {
public:
    static void linkProgram(GLuint* program, GLuint vs, GLuint fs)
    {
        *program = glCreateProgram();
        glAttachShader(*program, vs);
        glAttachShader(*program, fs);
        glLinkProgram(*program);

        int length;
        glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            std::vector<char> message(length+1);
            glGetProgramInfoLog(*program, length, NULL, message.data());
            throw std::runtime_error(std::string(message.data()));
        }

        glDetachShader(*program, fs);
        glDetachShader(*program, vs);
    }

    static void bakeHDR(const char* path, GLuint* cubeMap, GLuint* irradianceMap, GLuint* prefilterMap)
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
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 0, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 1, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 2, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 3, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 4, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 5, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, irradianceMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, *irradianceMap);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 0, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 1, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 2, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 3, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 4, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 5, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, prefilterMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, *prefilterMap);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 0, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 1, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 2, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 3, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 4, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + 5, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
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
            linkProgram(&program, Shaders::bakehdrVertexShader(), Shaders::bakehdrFragmentShader());
            linkProgram(&convolution, Shaders::bakehdrVertexShader(), Shaders::bakehdrIrradianceConvolutionFragmentShader());
            linkProgram(&prefilter, Shaders::bakehdrVertexShader(), Shaders::bakehdrPrefilterFragmentShader());
            glGenVertexArrays(1, &vao);
        }

        GLuint framebuffer;
        glGenFramebuffers(1, &framebuffer);

        GLint view[4];
        glGetIntegerv(GL_VIEWPORT, view);
        glBindVertexArray(vao);

        for (int i = 0; i < 6; i++) {
            glViewport(0, 0, 512, 512);
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
            glViewport(0, 0, 32, 32);
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
        for (unsigned int mip = 0; mip < maxMipLevels; mip++)
        {
            int mipWidth  = (int)(128 * std::pow(0.5, mip));
            int mipHeight = (int)(128 * std::pow(0.5, mip));
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

    static void loadBRDFLUT(const char* path, GLuint* brdflutMap, int size = 512)
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

    static void renderSphere()
    {
        static unsigned int sphereVAO = 0;
        static GLsizei indexCount;

        if (sphereVAO == 0)
        {
            glGenVertexArrays(1, &sphereVAO);

            unsigned int vbo, ebo;
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);

            std::vector<glm::vec3> positions;
            std::vector<glm::vec2> uv;
            std::vector<glm::vec3> normals;
            std::vector<unsigned int> indices;

            const unsigned int X_SEGMENTS = 64;
            const unsigned int Y_SEGMENTS = 64;
            const float PI = 3.14159265359f;
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
                {
                    float xSegment = (float)x / (float)X_SEGMENTS;
                    float ySegment = (float)y / (float)Y_SEGMENTS;
                    float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                    float yPos = std::cos(ySegment * PI);
                    float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                    positions.push_back(glm::vec3(xPos, yPos, zPos));
                    uv.push_back(glm::vec2(xSegment, ySegment));
                    normals.push_back(glm::vec3(xPos, yPos, zPos));
                }
            }

            bool oddRow = false;
            for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
            {
                if (!oddRow) // even rows: y == 0, y == 2; and so on
                {
                    for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
                    {
                        indices.push_back(y * (X_SEGMENTS + 1) + x);
                        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    }
                }
                else
                {
                    for (int x = X_SEGMENTS; x >= 0; --x)
                    {
                        indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                        indices.push_back(y * (X_SEGMENTS + 1) + x);
                    }
                }
                oddRow = !oddRow;
            }
            indexCount = static_cast<GLsizei>(indices.size());

            std::vector<float> data;
            for (unsigned int i = 0; i < positions.size(); ++i)
            {
                data.push_back(positions[i].x);
                data.push_back(positions[i].y);
                data.push_back(positions[i].z);
                if (normals.size() > 0)
                {
                    data.push_back(normals[i].x);
                    data.push_back(normals[i].y);
                    data.push_back(normals[i].z);
                }
                if (uv.size() > 0)
                {
                    data.push_back(uv[i].x);
                    data.push_back(uv[i].y);
                }
            }
            glBindVertexArray(sphereVAO);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
            unsigned int stride = (3 + 2 + 3) * sizeof(float);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        }

        glBindVertexArray(sphereVAO);
        glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    }

    static GLuint loadTexture(const char* path)
    {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(true);
        stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error(path);
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        stbi_image_free(pixels);

        return texture;
    }

    static GLuint makeTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        GLuint texture;
        unsigned char data[] = { r, g, b, a };

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        return texture;
    }
};

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

    /*
    GLuint vbo;
    GLuint ibo;
    int count;
    RenderPass::loadModel("models/MAC10.obj", &vbo, &ibo, &count);

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
    */
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
