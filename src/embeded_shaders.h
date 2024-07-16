const char* brdf_vert =
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
const char* brdf_frag =
R"( #version 330 core

    in vec3 WorldPos;
    in vec3 Normal;
    in vec2 TexCoords;

    out vec4 FragColor;

    uniform sampler2D albedoMap;
    uniform sampler2D normalMap;
    uniform sampler2D metallicMap;
    uniform sampler2D roughnessMap;

    uniform vec3 viewPos;

    vec3 materialcolor()
    {
        return texture(albedoMap, TexCoords).rgb;
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
        vec3 F0 = mix(vec3(0.04), materialcolor(), metallic); // * material.specular
        vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
        return F;    
    }

    vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness)
    {
        // Precalculate vectors and dot products    
        vec3 H = normalize (V + L);
        float dotNV = clamp(dot(N, V), 0.0, 1.0);
        float dotNL = clamp(dot(N, L), 0.0, 1.0);
        float dotLH = clamp(dot(L, H), 0.0, 1.0);
        float dotNH = clamp(dot(N, H), 0.0, 1.0);

        // Light color fixed
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
        float M = texture(metallicMap, TexCoords).r;
        float R = texture(roughnessMap, TexCoords).r;

        #define NUM_LIGHTS 2
        vec3 lightPos[] = vec3[](
            vec3(0.0f, 0.0f, 10.0f),
            vec3(10.0f, 0.0f, 0.0f)
        );

        vec3 Lo = vec3(0.0);
        for (int i = 0; i < NUM_LIGHTS; i++) {
          vec3 L = normalize(lightPos[i] - WorldPos);
          //vec3 L = normalize(vec3(10, 10, 0));
          Lo += BRDF(L, V, N, M, R);
        }

        Lo += materialcolor() * 0.03;

        FragColor = vec4(pow(Lo, vec3(1.0/2.2)), 1);
    }
)";
const char* bakehdr_vert =
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
const char* bakehdr_frag =
R"( #version 330 core
    #define MATH_PI 3.1415926535897932384626433832795

    in vec2 TexCoords;
    out vec4 FragColor;

    uniform int face;
    uniform sampler2D HDR;

    vec3 uvToXYZ(int face, vec2 uv)
    {
        vec3 XYZ[] = vec3[](
            vec3( 1.0f,  uv.y, -uv.x),
            vec3(-1.0f,  uv.y,  uv.x),
            vec3( uv.x, -1.0f,  uv.y),
            vec3( uv.x,  1.0f, -uv.y),
            vec3( uv.x,  uv.y,  1.0f),
            vec3(-uv.x,  uv.y, -1.0f)
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
const char* bakehdr_irradiance_convolution_frag =
R"( #version 330 core

    in vec2 TexCoords;
    out vec4 FragColor;

    uniform int face;
    uniform samplerCube environmentMap;
    const float PI = 3.14159265359;

    vec3 uvToXYZ(int face, vec2 uv)
    {
        vec3 XYZ[] = vec3[](
            vec3( 1.0f,  uv.y, -uv.x),
            vec3(-1.0f,  uv.y,  uv.x),
            vec3( uv.x, -1.0f,  uv.y),
            vec3( uv.x,  1.0f, -uv.y),
            vec3( uv.x,  uv.y,  1.0f),
            vec3(-uv.x,  uv.y, -1.0f)
        );
        return XYZ[face];
    }

    void main()
    {
        /*
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
        */
        FragColor = vec4(1);
    }
)";
const char* skybox_vert =
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
const char* skybox_frag =
R"( #version 330 core

    in vec3 TexCoords;
    out vec4 FragColor;

    uniform samplerCube skybox;

    void main()
    {    
        FragColor = texture(skybox, TexCoords);
    }
)";
