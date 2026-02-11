#version 450

out vec4 FragColor;

in vec2 tex_coord;

uniform sampler2D sampler;

void main() {
    FragColor = vec4(vec3(texture(sampler,tex_coord).r),1.0);
}
