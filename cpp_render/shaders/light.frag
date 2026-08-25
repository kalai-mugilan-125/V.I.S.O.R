#version 330 core
out vec4 FragColor;

// A simple unlit shader that just outputs a solid bright color for the light object.
void main() {
    FragColor = vec4(1.0, 0.9, 0.7, 1.0); // Bright warm light color
}
