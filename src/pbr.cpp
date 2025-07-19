#include "pbr.h"
#include "camera.h"
#include "mesh.h"
#include "skybox.h"
#include "shaders.h"

GLuint PBRRenderPass::program;
GLuint PBRRenderPass::MVP_Location;
GLuint PBRRenderPass::uModel_Location;
GLuint PBRRenderPass::viewPos_Location;

PBRRenderPass::PBRRenderPass() {
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

void PBRRenderPass::drawVAO(Camera* camera, int vao, int count, const glm::mat4& model, PBRMaterial* material, SkyboxMaterial* skybox) {
    glUseProgram(program);
    setupMatrix(camera, model);
    useMaterial(material, skybox);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
}

void PBRRenderPass::drawMesh(Camera* camera, Mesh* mesh, const glm::mat4& model, PBRMaterial* material, SkyboxMaterial* skybox) {
    glUseProgram(program);
    setupMatrix(camera, model);
    useMaterial(material, skybox);
    glBindVertexArray(mesh->getVAO());
    glDrawElements(GL_TRIANGLES, mesh->getCount(), GL_UNSIGNED_INT, 0);
}

void PBRRenderPass::drawSphere(Camera* camera, const glm::mat4& model, PBRMaterial* material, SkyboxMaterial* skybox) {
    glUseProgram(program);
    setupMatrix(camera, model);
    useMaterial(material, skybox);
    renderSphere();
}

void PBRRenderPass::setupMatrix(Camera* camera, const glm::mat4& model) { 
    glm::mat4 MVP = camera->projection * camera->view * model;
    glUniformMatrix4fv(MVP_Location, 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(uModel_Location, 1, GL_FALSE, &model[0][0]);
    glUniform3fv(viewPos_Location, 1, &camera->position[0]);
}

void PBRRenderPass::useMaterial(PBRMaterial* material, SkyboxMaterial* skybox) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, material->getAlbedoMap());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, material->getNormalMap());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, material->getMetallicMap());
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, material->getRoughnessMap());
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->getIrradianceMap());
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->getPrefilterMap());
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, skybox->getBRDFLUTMap());
}
