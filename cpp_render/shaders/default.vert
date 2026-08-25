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
