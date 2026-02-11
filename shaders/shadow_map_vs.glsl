#version 450 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 3) in vec3 aPos;

uniform mat4 ortho;
uniform mat4 view;

out vec3 frag_pos;

void main() {
    gl_Position = ortho * view * vec4(aPos,1.0);
    frag_pos = aPos;
}
