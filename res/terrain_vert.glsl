#version 410

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 v_position;
out vec3 v_normal;

void main(){
    v_position = vec3(matModel * vec4(a_position, 1.0));
    v_normal = normalize(vec3(matNormal * vec4(a_normal, 1.0)));

    gl_Position = mvp * vec4(a_position, 1.0);
}