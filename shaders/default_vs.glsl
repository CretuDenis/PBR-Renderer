#version 450 core
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec4 aColor;
layout(location = 1) in vec4 aTan;
layout(location = 2) in vec4 aWeights;
layout(location = 3) in vec3 aPos;
layout(location = 4) in vec3 aNormal;
layout(location = 5) in vec2 aTexCoord0;
layout(location = 6) in vec2 aTexCoord1;
layout(location = 7) in uvec4 aJoints;

uniform mat4 proj;
uniform mat4 view;
uniform vec3 camera_pos;

out mat3 tbn;
out vec4 color;
out vec4 weights;
out vec3 tangent;
out vec3 normal;
out vec3 view_vec;
out vec3 frag_pos;
out vec2 tex_coord;
out vec2 extra_tex_coord;
out float handedness;

flat out uint v_DrawID;

void main() {
    gl_Position = proj * view * vec4(aPos,1.0);

    vec3 T = normalize(aTan.xyz);
    vec3 B = normalize(cross(aNormal,aTan.xyz) * aTan.w);
    vec3 N = normalize(aNormal);
    tbn = mat3(T, B, N);

    tex_coord = aTexCoord0;
    extra_tex_coord = aTexCoord1;
    color = aColor;
    weights = aWeights;
    frag_pos = aPos;
    tangent = T;
    normal = N;
    view_vec = normalize(camera_pos - aPos.xyz);
    handedness = aTan.w;
    v_DrawID = gl_DrawIDARB;
}
