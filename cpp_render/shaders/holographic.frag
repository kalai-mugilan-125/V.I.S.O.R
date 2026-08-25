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
    if (u_HasBaseColorTex) {
        baseColor = mix(baseColor, texture(u_BaseColorSampler, TexCoords).rgb, 0.4);
    }
    
    // Fresnel Rim lighting
    vec3 viewDir = normalize(cameraPos - FragPos);
    float rim = 1.0 - max(dot(viewDir, normalize(Normal)), 0.0);
    rim = smoothstep(0.4, 1.0, rim);
    
    // Holographic scanline pattern
    float scanline = sin(FragPos.y * 80.0) * 0.1 + 0.9;
    
    vec3 finalColor = (baseColor + (baseColor * rim * 2.5)) * scanline;
    float alpha = u_DepthFade * (0.35 + rim * 0.65);
    
    FragColor = vec4(finalColor, alpha);
}
