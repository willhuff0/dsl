#version 410

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec3 a_normal;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec3 fragNormal;

// NOTE: Add your custom variables here

void main()
{
    // Send vertex attributes to fragment shader
    fragPosition = vec3(matModel*vec4(a_position, 1.0));
    fragNormal = normalize(vec3(matNormal*vec4(a_normal, 1.0)));

    // Calculate final vertex position
    gl_Position = mvp*vec4(a_position, 1.0);
}