#pragma once
#include "renderpass.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glad.h>
#include <map>

class TextRenderPass;
class TextMaterial {
public:
    struct Metric {
        glm::ivec2 size;
        glm::ivec2 bearing;
        long advance;
    };

public:
    TextMaterial() : textureArray(0), glyphSize(0) { }
    void loadFont(const char* path, int size = 64);

    friend class TextRenderPass;

private:
    GLuint textureArray;
    std::map<char, Metric> metrics;
    int glyphSize;
};

class TextRenderPass : public RenderPass {
public:
    TextRenderPass(GLFWwindow* window);
    void drawText(TextMaterial* material, float size, const glm::vec2& pos, const char* text);
    void drawText(TextMaterial* material, const glm::vec4& color, float size, const glm::vec2& pos, const char* text);
    void drawText(TextMaterial* material, const glm::mat4& transform, const glm::vec4& color, float size, const glm::vec2& pos, const char* text);

private:
    static GLuint textprog;
    static GLuint color_Location;
    GLuint vao;
    GLuint transformBuffer;
    GLuint layerBuffer;
};
