#include "skybox.h"
#include "camera.h"
#include "shaders.h"

void SkyboxMaterial::bake(const char* hdr, const char* lut) {
    RenderPass::bakeHDR(hdr, &cubeMap, &irradianceMap, &prefilterMap);
    RenderPass::loadBRDFLUT(lut, &brdflutMap);
}

GLuint SkyboxRenderPass::vao;
GLuint SkyboxRenderPass::skyboxprog;
GLuint SkyboxRenderPass::uProj_Location;
GLuint SkyboxRenderPass::uView_Location;
GLuint SkyboxRenderPass::skybox_Location;

SkyboxRenderPass::SkyboxRenderPass() {
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

void SkyboxRenderPass::drawSkybox(Camera* camera, SkyboxMaterial* material) {
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
