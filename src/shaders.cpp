#include "shaders.h"
#include <string>
#include <vector>
#include <stdexcept>

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        std::vector<char> message(length+1);
        glGetShaderInfoLog(shader, length, NULL, message.data());
        throw std::runtime_error(std::string(message.data()));
    }

    return shader;
}

constexpr const char* pbr_vert_source =
R"( #version 330 core

    in vec3 aPosition;
    in vec3 aNormal;
    in vec2 aTexCoords;

    out vec3 WorldPos;
    out vec3 Normal;
    out vec2 TexCoords;

    uniform mat4 MVP;
    uniform mat4 uModel;

    void main() {
        gl_Position = MVP * vec4(aPosition, 1.0);
        WorldPos = vec3(uModel * vec4(aPosition, 1));
        Normal = mat3(uModel) * aNormal;
        TexCoords = aTexCoords;
    }
)";
constexpr const char* pbr_frag_source =
R"( #version 330 core

    in vec3 WorldPos;
    in vec3 Normal;
    in vec2 TexCoords;

    out vec4 FragColor;

    uniform sampler2D albedoMap;
    uniform sampler2D normalMap;
    uniform sampler2D metallicMap;
    uniform sampler2D roughnessMap;
    uniform samplerCube irradianceMap;
    uniform samplerCube prefilterMap;
    uniform sampler2D brdflutMap;

    uniform vec3 viewPos;

    vec3 materialcolor()
    {
        return pow(texture(albedoMap, TexCoords).rgb, vec3(2.2));
    }

    vec3 computeTBN()
    {
        vec3 tangentNormal = texture(normalMap, TexCoords).xyz * 2.0 - 1.0;

        vec3 Q1  = dFdx(WorldPos);
        vec3 Q2  = dFdy(WorldPos);
        vec2 st1 = dFdx(TexCoords);
        vec2 st2 = dFdy(TexCoords);

        vec3 N   = normalize(Normal);
        vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
        vec3 B  = -normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);

        return normalize(TBN * tangentNormal);
    }

    const float PI = 3.14159265359;

    float D_GGX(float dotNH, float roughness)
    {
        float alpha = roughness * roughness;
        float alpha2 = alpha * alpha;
        float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
        return (alpha2)/(PI * denom*denom);
    }

    float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
    {
        float r = (roughness + 1.0);
        float k = (r*r) / 8.0;
        float GL = dotNL / (dotNL * (1.0 - k) + k);
        float GV = dotNV / (dotNV * (1.0 - k) + k);
        return GL * GV;
    }

    vec3 F_Schlick(float cosTheta, float metallic)
    {
        vec3 F0 = mix(vec3(0.04), materialcolor(), metallic);
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }

    vec3 F_SchlickRoughness(float cosTheta, float metallic, float roughness)
    {
        vec3 F0 = mix(vec3(0.04), materialcolor(), metallic);
        return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }

    vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness)
    {
        vec3 H = normalize (V + L);
        float dotNV = clamp(dot(N, V), 0.0, 1.0);
        float dotNL = clamp(dot(N, L), 0.0, 1.0);
        float dotLH = clamp(dot(L, H), 0.0, 1.0);
        float dotNH = clamp(dot(N, H), 0.0, 1.0);

        vec3 lightColor = vec3(1.0);

        vec3 color = vec3(0);

        if (dotNL > 0.0)
        {
            float R = max(0.05, roughness);
            float D = D_GGX(dotNH, R);
            float G = G_SchlicksmithGGX(dotNL, dotNV, R);
            vec3 F = F_Schlick(dotNV, metallic);

            vec3 spec = D * F * G / (4.0 * dotNL * dotNV);

            color += spec * dotNL * lightColor;
        }

        return color;
    }

    void main()
    {
        vec3 N = computeTBN();
        vec3 V = normalize(viewPos - WorldPos);
        float metallic = texture(metallicMap, TexCoords).r;
        float roughness = texture(roughnessMap, TexCoords).r;

        #define NUM_LIGHTS 4
        vec3 lightPos[] = vec3[](
            vec3(1.0f, 0.0f, 0.0f),
            vec3(-1.0f, 0.0f, 0.0f),
            vec3(0.0f, 0.0f, 1.0f),
            vec3(0.0f, 0.0f, -1.0f)
        );

        vec3 Lo = vec3(0.0);
        for (int i = 0; i < NUM_LIGHTS; i++) {
          vec3 L = normalize(lightPos[i] - WorldPos);
          Lo += BRDF(L, V, N, metallic, roughness);
        }

        vec3 kS = F_SchlickRoughness(max(dot(N, V), 0.0), metallic, roughness);
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        vec3 irradiance = texture(irradianceMap, N).rgb;
        vec3 diffuse    = irradiance * materialcolor();

        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilter = textureLod(prefilterMap, reflect(-V, N), roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf      = texture(brdflutMap, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular  = prefilter * (kS * brdf.x + brdf.y);

        vec3 ambient = (kD * diffuse + specular); // * ao
        vec3 color = Lo + ambient;

        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        FragColor = vec4(color, 1.0);
    }
)";
constexpr const char* bakehdr_vert_source =
R"( #version 330 core

    out vec2 TexCoords;

    void main()
    {
        float x = float((gl_VertexID & 1) << 2);
        float y = float((gl_VertexID & 2) << 1);
        gl_Position = vec4(x - 1.0, y - 1.0, 0, 1);
        TexCoords = vec2(x, y) * 0.5;
    }
)";
constexpr const char* bakehdr_frag_source =
R"( #version 330 core
    #define MATH_PI 3.1415926535897932384626433832795

    in vec2 TexCoords;
    out vec4 FragColor;

    uniform int face;
    uniform sampler2D HDR;

    vec3 uvToXYZ(int face, vec2 uv)
    {
        vec3 XYZ[] = vec3[](
            vec3( 1.0f, -uv.y, -uv.x),
            vec3(-1.0f, -uv.y,  uv.x),
            vec3( uv.x,  1.0f,  uv.y),
            vec3( uv.x, -1.0f, -uv.y),
            vec3( uv.x, -uv.y,  1.0f),
            vec3(-uv.x, -uv.y, -1.0f)
        );
        return XYZ[face];
    }

    vec2 dirToUV(vec3 dir)
    {
        return vec2(
            0.5f + 0.5f * atan(dir.z, dir.x) / MATH_PI,
            1.0f - acos(dir.y) / MATH_PI);
    }

    vec3 BakeCubeMap(sampler2D HDR, int face, vec2 coord)
    {
        coord = coord * 2.0 - 1.0;
        vec3 scan = uvToXYZ(face, coord);
        vec3 direction = normalize(scan);
        vec2 src = dirToUV(direction);
        return texture(HDR, src).rgb;
    }

    void main(void)
    {
        FragColor = vec4(BakeCubeMap(HDR, face, TexCoords), 1);
    }
)";
constexpr const char* bakehdr_irradiance_convolution_frag_source =
R"( #version 330 core

    in vec2 TexCoords;
    out vec4 FragColor;

    uniform int face;
    uniform samplerCube environmentMap;
    const float PI = 3.14159265359;

    vec3 uvToXYZ(int face, vec2 uv)
    {
        vec3 XYZ[] = vec3[](
            vec3( 1.0f, -uv.y, -uv.x),
            vec3(-1.0f, -uv.y,  uv.x),
            vec3( uv.x,  1.0f,  uv.y),
            vec3( uv.x, -1.0f, -uv.y),
            vec3( uv.x, -uv.y,  1.0f),
            vec3(-uv.x, -uv.y, -1.0f)
        );
        return XYZ[face];
    }

    void main()
    {
        vec3 scan = uvToXYZ(face, TexCoords*2.0-1.0);
        vec3 N = normalize(scan);
        vec3 irradiance = vec3(0.0);

        vec3 up    = vec3(0.0, 1.0, 0.0);
        vec3 right = normalize(cross(up, N));
        up         = normalize(cross(N, right));

        float sampleDelta = 0.025;
        float nrSamples = 0.0;

        for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
        {
            for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
            {
                vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
                vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

                irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
                nrSamples++;
            }
        }

        irradiance = PI * irradiance * (1.0 / float(nrSamples));
        FragColor = vec4(irradiance, 1.0);
    }
)";
constexpr const char* bakehdr_prefilter_frag_source =
R"( #version 330 core

    in vec2 TexCoords;
    out vec4 FragColor;

    uniform int face;
    uniform samplerCube environmentMap;
    uniform float roughness;

    const float PI = 3.14159265359;

    float DistributionGGX(vec3 N, vec3 H, float roughness)
    {
        float a = roughness*roughness;
        float a2 = a*a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH*NdotH;

        float nom   = a2;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;

        return nom / denom;
    }

    float RadicalInverse_VdC(uint bits)
    {
         bits = (bits << 16u) | (bits >> 16u);
         bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
         bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
         bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
         bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
         return float(bits) * 2.3283064365386963e-10; // / 0x100000000
    }

    vec2 Hammersley(uint i, uint N)
    {
        return vec2(float(i)/float(N), RadicalInverse_VdC(i));
    }

    vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
    {
        float a = roughness*roughness;

        float phi = 2.0 * PI * Xi.x;
        float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
        float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

        vec3 H;
        H.x = cos(phi) * sinTheta;
        H.y = sin(phi) * sinTheta;
        H.z = cosTheta;

        vec3 up          = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent   = normalize(cross(up, N));
        vec3 bitangent = cross(N, tangent);

        vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
        return normalize(sampleVec);
    }

    vec3 uvToXYZ(int face, vec2 uv)
    {
        vec3 XYZ[] = vec3[](
            vec3( 1.0f, -uv.y, -uv.x),
            vec3(-1.0f, -uv.y,  uv.x),
            vec3( uv.x,  1.0f,  uv.y),
            vec3( uv.x, -1.0f, -uv.y),
            vec3( uv.x, -uv.y,  1.0f),
            vec3(-uv.x, -uv.y, -1.0f)
        );
        return XYZ[face];
    }

    void main()
    {
        vec3 scan = uvToXYZ(face, TexCoords*2.0-1.0);
        vec3 N = normalize(scan);
        vec3 R = N;
        vec3 V = R;

        const uint SAMPLE_COUNT = 1024u;
        vec3 prefilteredColor = vec3(0.0);
        float totalWeight = 0.0;

        for(uint i = 0u; i < SAMPLE_COUNT; ++i)
        {
            // generates a sample vector that's biased towards the preferred alignment direction (importance sampling).
            vec2 Xi = Hammersley(i, SAMPLE_COUNT);
            vec3 H = ImportanceSampleGGX(Xi, N, roughness);
            vec3 L  = normalize(2.0 * dot(V, H) * H - V);

            float NdotL = max(dot(N, L), 0.0);
            if(NdotL > 0.0)
            {
                // sample from the environment's mip level based on roughness/pdf
                float D   = DistributionGGX(N, H, roughness);
                float NdotH = max(dot(N, H), 0.0);
                float HdotV = max(dot(H, V), 0.0);
                float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

                float resolution = 512.0; // resolution of source cubemap (per face)
                float saTexel  = 4.0 * PI / (6.0 * resolution * resolution);
                float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

                float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

                prefilteredColor += textureLod(environmentMap, L, mipLevel).rgb * NdotL;
                totalWeight      += NdotL;
            }
        }

        prefilteredColor = prefilteredColor / totalWeight;

        FragColor = vec4(prefilteredColor, 1.0);
    }
)";
constexpr const char* skybox_vert_source =
R"( #version 330 core

    out vec3 TexCoords;
    uniform mat4 uProj;
    uniform mat4 uView;

    const vec3 vertices[] = vec3[](
        vec3(0, 0, 0),
        vec3(1, 0, 0),
        vec3(0, 1, 0),
        vec3(1, 1, 0),
        vec3(0, 0, 1),
        vec3(1, 0, 1),
        vec3(0, 1, 1),
        vec3(1, 1, 1)
    );

    const int faces[] = int[](
        5, 3, 1, 5, 7, 3, // +x
        0, 6, 4, 0, 2, 6, // -x
        4, 7, 5, 4, 6, 7, // +z
        1, 2, 0, 1, 3, 2, // -z
        6, 3, 7, 6, 2, 3, // +y
        0, 5, 1, 0, 4, 5  // -y
    );

    void main()
    {
        TexCoords = (vertices[faces[gl_VertexID]] - 0.5)*2;
        gl_Position = uProj * mat4(mat3(uView)) * vec4(TexCoords, 1.0);
    }
)";
constexpr const char* skybox_frag_source =
R"( #version 330 core

    in vec3 TexCoords;
    out vec4 FragColor;

    uniform samplerCube skybox;

    void main()
    {
        vec3 color = texture(skybox, TexCoords).rgb;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        FragColor = vec4(color, 1);
    }
)";

namespace Shaders {
    GLuint pbr_vert;
    GLuint bakehdr_vert;
    GLuint skybox_vert;

    GLuint pbr_frag;
    GLuint bakehdr_frag;
    GLuint bakehdr_irradiance_convolution_frag;
    GLuint bakehdr_prefilter_frag;
    GLuint skybox_frag;

    GLuint pbrVertexShader()                             { return pbr_vert; }
    GLuint bakehdrVertexShader()                         { return bakehdr_vert; }
    GLuint skyboxVertexShader()                          { return skybox_vert; }

    GLuint pbrFragmentShader()                           { return pbr_frag; }
    GLuint bakehdrFragmentShader()                       { return bakehdr_frag; }
    GLuint bakehdrIrradianceConvolutionFragmentShader()  { return bakehdr_irradiance_convolution_frag; }
    GLuint bakehdrPrefilterFragmentShader()              { return bakehdr_prefilter_frag; }
    GLuint skyboxFragmentShader()                        { return skybox_frag; }

    void compile() {
        pbr_vert                            = compileShader(GL_VERTEX_SHADER, pbr_vert_source);
        bakehdr_vert                        = compileShader(GL_VERTEX_SHADER, bakehdr_vert_source);
        skybox_vert                         = compileShader(GL_VERTEX_SHADER, skybox_vert_source);

        pbr_frag                            = compileShader(GL_FRAGMENT_SHADER, pbr_frag_source);
        bakehdr_frag                        = compileShader(GL_FRAGMENT_SHADER, bakehdr_frag_source);
        bakehdr_irradiance_convolution_frag = compileShader(GL_FRAGMENT_SHADER, bakehdr_irradiance_convolution_frag_source);
        bakehdr_prefilter_frag              = compileShader(GL_FRAGMENT_SHADER, bakehdr_prefilter_frag_source);
        skybox_frag                         = compileShader(GL_FRAGMENT_SHADER, skybox_frag_source);
    }
}
