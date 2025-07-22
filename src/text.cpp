#include "text.h"
#include "shaders.h"

#include <glm/gtx/transform.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdexcept>
#include <vector>
#include <iostream>

void TextMaterial::loadFont(const char* path, int size) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        throw std::runtime_error("Failed to initialize freetype2");
    }

    FT_Face face;
    if (FT_New_Face(ft, path, 0, &face)) {
        throw std::runtime_error(std::string("Failed to load font: ") + path);
    }

    this->glyphSize = size;
    FT_Set_Pixel_Sizes(face, size, size);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &textureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, size, size, 128, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    for (int c = 0; c < 128; c++) {
        if (FT_Load_Char(face, (char)c, FT_LOAD_RENDER)) {
            throw std::runtime_error("Failed to load glyph");
        }

        if (face->glyph->bitmap.width > 0
            && face->glyph->bitmap.width <= 64
            && face->glyph->bitmap.rows > 0
            && face->glyph->bitmap.rows <= 64)
        {
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, c,
                face->glyph->bitmap.width, face->glyph->bitmap.rows,
                1, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
        }

        Metric metric = {
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x >> 6,
        };
        metrics[(char)c] = metric;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

GLuint TextRenderPass::textprog;
GLuint TextRenderPass::color_Location;

TextRenderPass::TextRenderPass(GLFWwindow* window) {
    if (textprog == 0) {
        linkProgram(&textprog,
            Shaders::textVertexShader(),
            Shaders::textFragmentShader());
        color_Location = glGetUniformLocation(textprog, "color");
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glm::mat4 projection = glm::ortho(0.0f, (float)width, -(float)height, 0.0f);

    glUseProgram(textprog);
    GLuint projection_Location = glGetUniformLocation(textprog, "projection");
    glUniformMatrix4fv(projection_Location, 1, GL_FALSE, &projection[0][0]);
    glUniform1i(glGetUniformLocation(textprog, "text"), 0);

    GLuint transform_Location = glGetAttribLocation(textprog, "transform");
    GLuint layer_Location = glGetAttribLocation(textprog, "layer");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &transformBuffer);
    glGenBuffers(1, &layerBuffer);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, transformBuffer);
    glVertexAttribPointer(transform_Location + 0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 16, (void*)(sizeof(GLfloat) * 0));
    glVertexAttribPointer(transform_Location + 1, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 16, (void*)(sizeof(GLfloat) * 4));
    glVertexAttribPointer(transform_Location + 2, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 16, (void*)(sizeof(GLfloat) * 8));
    glVertexAttribPointer(transform_Location + 3, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 16, (void*)(sizeof(GLfloat) * 12));
    glVertexAttribDivisor(transform_Location + 0, 1);
    glVertexAttribDivisor(transform_Location + 1, 1);
    glVertexAttribDivisor(transform_Location + 2, 1);
    glVertexAttribDivisor(transform_Location + 3, 1);
    glEnableVertexAttribArray(transform_Location + 0);
    glEnableVertexAttribArray(transform_Location + 1);
    glEnableVertexAttribArray(transform_Location + 2);
    glEnableVertexAttribArray(transform_Location + 3);

    glBindBuffer(GL_ARRAY_BUFFER, layerBuffer);
    glVertexAttribIPointer(layer_Location, 1, GL_INT, sizeof(GLint), (void*)0);
    glVertexAttribDivisor(layer_Location, 1);
    glEnableVertexAttribArray(layer_Location);
}

void TextRenderPass::drawText(TextMaterial* material, float size, const glm::vec2& pos, const char* text) {
    drawText(material, glm::mat4(1.0f), glm::vec4(1.0f), size, pos, text);
}

void TextRenderPass::drawText(TextMaterial* material, const glm::vec4& color, float size, const glm::vec2& pos, const char* text) {
    drawText(material, glm::mat4(1.0f), color, size, pos, text);
}

void TextRenderPass::drawText(TextMaterial* material, const glm::mat4& transform, const glm::vec4& color, float size, const glm::vec2& pos, const char* text) {
    std::vector<glm::mat4> transforms;
    std::vector<int> layers;
    
    float factor = size / material->glyphSize;
    float x = 0;
    float y = 0;

    for (int i = 0; text[i]; i++) {
        const auto& metric = material->metrics[text[i]];

        if (text[i] == '\n') {
            y += metric.size.y * factor;
            x = 0;
            continue;
        }
        else if (text[i] == ' ') {
            x += metric.advance * factor;
            continue;
        }

        float x2 = x + metric.bearing.x * factor;
        float y2 = y + (material->glyphSize - metric.bearing.y) * factor;

        glm::mat4 scale = glm::scale(glm::vec3(material->glyphSize, material->glyphSize, 0));
        glm::mat4 translation = glm::translate(glm::vec3(x2, -y2, 0));
        glm::mat4 position = glm::translate(glm::vec3(pos.x, -pos.y, 0));
        glm::mat4 final_matrix = position * transform * translation * scale;

        transforms.push_back(final_matrix);
        layers.push_back((int)text[i]);

        x += metric.advance * factor;
    }

    glBindBuffer(GL_ARRAY_BUFFER, transformBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * transforms.size(), transforms.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, layerBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(int) * layers.size(), layers.data(), GL_STREAM_DRAW);

    glUseProgram(textprog);
    glUniform4fv(color_Location, 1, &color[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, material->textureArray);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, transforms.size());
    glDepthMask(GL_TRUE);
}
