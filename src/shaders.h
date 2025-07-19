#pragma once
#include "../lib/glad.h"

namespace Shaders {
    void compile();
    GLuint pbrVertexShader();
    GLuint bakehdrVertexShader();
    GLuint skyboxVertexShader();
    GLuint pbrFragmentShader();
    GLuint bakehdrFragmentShader();
    GLuint bakehdrIrradianceConvolutionFragmentShader();
    GLuint bakehdrPrefilterFragmentShader();
    GLuint skyboxFragmentShader();
}
