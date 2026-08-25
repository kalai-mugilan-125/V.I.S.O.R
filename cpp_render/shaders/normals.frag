#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

void main() {
    // Map normals from [-1, 1] range to [0, 1] for RGB display
    vec3 color = normalize(Normal) * 0.5 + 0.5;
    FragColor = vec4(color, 1.0);
}
