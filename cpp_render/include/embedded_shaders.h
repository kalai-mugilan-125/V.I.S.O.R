// ============================================================================
//  embedded_shaders.h  --  All GLSL shader sources embedded as string literals.
//  This eliminates the runtime dependency on the shaders/ folder, making
//  VISOR.exe fully self-contained.
// ============================================================================
#pragma once
#include <string>

namespace Shaders {

// ---------------------------------------------------------------------------
//  Vertex Shaders
// ---------------------------------------------------------------------------

inline const std::string DEFAULT_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aTangent;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec3 Tangent;
out vec3 Bitangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 N = normalize(normalMatrix * aNormal);
    Normal = N;
    vec3 T = normalize(normalMatrix * aTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * (aTangent.w != 0.0 ? aTangent.w : 1.0);
    Tangent = T;
    Bitangent = B;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)GLSL";

inline const std::string GRID_VERT = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;

out vec3 nearPoint;
out vec3 farPoint;

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 p = aPos;
    nearPoint = UnprojectPoint(p.x, p.y, 0.0, view, projection).xyz;
    farPoint  = UnprojectPoint(p.x, p.y, 1.0, view, projection).xyz;
    gl_Position = vec4(p, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
//  Fragment Shaders
// ---------------------------------------------------------------------------

inline const std::string GRID_FRAG = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec3 nearPoint;
in vec3 farPoint;

uniform mat4 view;
uniform mat4 projection;

vec4 grid(vec3 fragPos3D, float scale, bool drawAxis) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1.0);
    float minimumx = min(derivative.x, 1.0);
    vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx && drawAxis) color.z = 1.0;
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz && drawAxis) color.x = 1.0;
    return color;
}

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = projection * view * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = projection * view * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0;
    float linearDepth = (2.0 * 0.1 * 100.0) / (100.0 + 0.1 - clip_space_depth * (100.0 - 0.1));
    return linearDepth / 100.0;
}

void main() {
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);
    gl_FragDepth = computeDepth(fragPos3D);
    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0.0, (0.5 - linearDepth));
    if (t > 0.0) {
        FragColor = (grid(fragPos3D, 1.0, true) + grid(fragPos3D, 10.0, false)) * float(t > 0.0);
        FragColor.a *= fading;
    } else {
        discard;
    }
}
)GLSL";

inline const std::string PBR_FRAG = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 Tangent;
in vec3 Bitangent;

uniform vec3 lightPos;
uniform vec3 cameraPos;

uniform vec3  u_BaseColorFactor;
uniform float u_AlphaFactor;
uniform float u_MetallicFactor;
uniform float u_RoughnessFactor;
uniform vec3  u_EmissiveFactor;

uniform sampler2D u_BaseColorSampler;
uniform sampler2D u_NormalSampler;
uniform sampler2D u_MetallicRoughnessSampler;
uniform sampler2D u_EmissiveSampler;

uniform bool u_HasBaseColorTex;
uniform bool u_HasNormalTex;
uniform bool u_HasMetallicRoughnessTex;
uniform bool u_HasEmissiveTex;

const float PI = 3.14159265359;

vec3 getNormalFromMap() {
    if (!u_HasNormalTex) return normalize(Normal);
    vec3 tangentNormal = texture(u_NormalSampler, TexCoords).xyz * 2.0 - 1.0;
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent);
    vec3 B = normalize(Bitangent);
    if (length(T) < 0.1 || length(B) < 0.1) return N;
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 baseColorTex = u_HasBaseColorTex ? texture(u_BaseColorSampler, TexCoords) : vec4(1.0);
    vec3 albedo = u_BaseColorFactor * baseColorTex.rgb;
    float alpha = u_AlphaFactor * baseColorTex.a;
    if (alpha < 0.05) discard;

    float metallic = u_MetallicFactor;
    float roughness = u_RoughnessFactor;
    if (u_HasMetallicRoughnessTex) {
        vec4 mrSample = texture(u_MetallicRoughnessSampler, TexCoords);
        roughness *= mrSample.g;
        metallic  *= mrSample.b;
    }
    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = clamp(metallic, 0.0, 1.0);

    vec3 N = getNormalFromMap();
    vec3 V = normalize(cameraPos - FragPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 lightColor = vec3(350.0);
    vec3 L = normalize(lightPos - FragPos);
    vec3 H = normalize(V + L);
    float distance = length(lightPos - FragPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = lightColor * attenuation;

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.05) * albedo;
    vec3 emissive = u_EmissiveFactor;
    if (u_HasEmissiveTex) emissive *= texture(u_EmissiveSampler, TexCoords).rgb;

    vec3 color = ambient + Lo + emissive;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, alpha);
}
)GLSL";

inline const std::string BLINN_PHONG_FRAG = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform vec3 u_BaseColorFactor;
uniform float u_AlphaFactor;
uniform sampler2D u_BaseColorSampler;
uniform bool u_HasBaseColorTex;

void main() {
    vec4 baseSample = u_HasBaseColorTex ? texture(u_BaseColorSampler, TexCoords) : vec4(1.0);
    vec3 baseColor = (u_BaseColorFactor.r > 0.0 || u_BaseColorFactor.g > 0.0 || u_BaseColorFactor.b > 0.0)
                        ? u_BaseColorFactor * baseSample.rgb
                        : vec3(0.6, 0.6, 0.6) * baseSample.rgb;
    vec3 lightColor = vec3(1.0);
    vec3 ambient = 0.15 * lightColor;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 diffuse = max(dot(norm, lightDir), 0.0) * lightColor;
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    vec3 specular = 0.4 * pow(max(dot(norm, halfwayDir), 0.0), 32.0) * lightColor;
    FragColor = vec4((ambient + diffuse + specular) * baseColor, u_AlphaFactor * baseSample.a);
}
)GLSL";

inline const std::string HOLOGRAPHIC_FRAG = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 cameraPos;
uniform float u_DepthFade;
uniform sampler2D u_BaseColorSampler;
uniform bool u_HasBaseColorTex;

void main() {
    vec3 baseColor = vec3(0.1, 0.7, 1.0);
    if (u_HasBaseColorTex) baseColor = mix(baseColor, texture(u_BaseColorSampler, TexCoords).rgb, 0.4);
    vec3 viewDir = normalize(cameraPos - FragPos);
    float rim = 1.0 - max(dot(viewDir, normalize(Normal)), 0.0);
    rim = smoothstep(0.4, 1.0, rim);
    float scanline = sin(FragPos.y * 80.0) * 0.1 + 0.9;
    vec3 finalColor = (baseColor + (baseColor * rim * 2.5)) * scanline;
    FragColor = vec4(finalColor, u_DepthFade * (0.35 + rim * 0.65));
}
)GLSL";

inline const std::string TOON_FRAG = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 cameraPos;

void main() {
    vec3 baseColor = vec3(0.5, 0.5, 0.5);
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float intensity = dot(norm, lightDir);
    float t;
    if      (intensity > 0.95) t = 1.0;
    else if (intensity > 0.50) t = 0.6;
    else if (intensity > 0.25) t = 0.3;
    else                       t = 0.1;
    FragColor = vec4(baseColor * t, 1.0);
}
)GLSL";

inline const std::string NORMALS_FRAG = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

void main() {
    vec3 color = normalize(Normal) * 0.5 + 0.5;
    FragColor = vec4(color, 1.0);
}
)GLSL";

inline const std::string LIGHT_FRAG = R"GLSL(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(1.0, 0.9, 0.7, 1.0);
}
)GLSL";

} // namespace Shaders
