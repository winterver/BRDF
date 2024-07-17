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
    uniform samplerCube irradianceMap;

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

        vec3 kS = F_Schlick(max(dot(N, V), 0.0), M);
        vec3 kD = (1.0 - kS) * (1.0 - M);
        vec3 irradiance = texture(irradianceMap, N).rgb;
        vec3 diffuse    = irradiance * materialcolor();

        Lo += kD * diffuse;

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
        vec3 scan = uvToXYZ(face, TexCoords*2.0-1.0);
        scan.y = -scan.y;
        vec3 N = normalize(scan);
        vec3 irradiance = vec3(0.0);   
        
        vec3 up    = vec3(0.0, 1.0, 0.0);
        vec3 right = normalize(cross(up, N));
        up         = normalize(cross(N, right));
           
        float sampleDelta = 0.15; // setting this too small may crash your program
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
const char* bakehdr_prefilter_frag =
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
        vec3 scan = uvToXYZ(face, TexCoords*2.0-1.0);
        scan.y = -scan.y;
        vec3 N = normalize(scan);       
        vec3 R = N;
        vec3 V = R;

        const uint SAMPLE_COUNT = 102u;//1024u;
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
        vec3 color = textureLod(skybox, TexCoords, 1.2).rgb;
        //color = color / (color + vec3(1.0));
        //color = pow(color, vec3(1.0/2.2));
        FragColor = vec4(color, 1);
    }
)";
