#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 cameraPos;

void main() {
    vec3 baseColor = vec3(0.5, 0.5, 0.5); // Neutral Gray
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    
    float intensity = dot(norm, lightDir);
    
    // Discretize the intensity for the toon effect
    float toonIntensity;
    if (intensity > 0.95)
        toonIntensity = 1.0;
    else if (intensity > 0.5)
        toonIntensity = 0.6;
    else if (intensity > 0.25)
        toonIntensity = 0.3;
    else
        toonIntensity = 0.1;
        
    vec3 result = baseColor * lightColor * toonIntensity;
    FragColor = vec4(result, 1.0);
}
