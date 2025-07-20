#pragma once
#include <glad.h>

namespace Shaders {
    void compile();
    GLuint pbrVertexShader();
    GLuint bakehdrVertexShader();
    GLuint skyboxVertexShader();
    GLuint textVertexShader();

    GLuint pbrFragmentShader();
    GLuint bakehdrFragmentShader();
    GLuint bakehdrIrradianceConvolutionFragmentShader();
    GLuint bakehdrPrefilterFragmentShader();
    GLuint skyboxFragmentShader();
    GLuint textFragmentShader();
}
