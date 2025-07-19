#pragma once
#include <glad.h>
#include <glm/glm.hpp>

class Mesh {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texcoords;
    };

public:
    Mesh();
    void loadObj(const char* path);

    GLuint getVAO() { return vao; }
    GLuint getCount() { return count; }

private:
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
    int count;
};
