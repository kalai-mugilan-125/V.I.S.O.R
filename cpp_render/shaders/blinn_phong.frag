#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 cameraPos;

// Material properties
uniform vec3 u_BaseColorFactor;
uniform float u_AlphaFactor;
uniform sampler2D u_BaseColorSampler;
uniform bool u_HasBaseColorTex;

void main() {
    vec4 baseSample = u_HasBaseColorTex ? texture(u_BaseColorSampler, TexCoords) : vec4(1.0);
    vec3 baseColor = (u_BaseColorFactor.r > 0.0 || u_BaseColorFactor.g > 0.0 || u_BaseColorFactor.b > 0.0)
                        ? u_BaseColorFactor * baseSample.rgb
                        : vec3(0.6, 0.6, 0.6) * baseSample.rgb;
    
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    
    // Ambient
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular (Blinn-Phong)
    float specularStrength = 0.4;
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(result, u_AlphaFactor * baseSample.a);
}
