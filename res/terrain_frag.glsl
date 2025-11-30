#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec3 fragNormal;

// Output fragment color
out vec4 finalColor;

uniform vec4 ambient;
uniform vec3 viewPos;

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = vec4(1.0, 1.0, 1.0, 1.0);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);

    {
        vec3 light = -normalize(vec3(0.0, 0.0, 0.0) - vec3(1.0, 1.0, 1.0));

        float NdotL = max(dot(normal, light), 0.0);
        lightDot += vec3(1.0, 1.0, 1.0)*NdotL;

        float specCo = 0.0;
        if (NdotL > 0.0) specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 16.0); // 16 refers to shine
        specular += specCo;
    }

    finalColor = (texelColor*((vec4(specular, 1.0))*vec4(lightDot, 1.0)));
    finalColor += texelColor*(ambient/10.0);

    // Gamma correction
    finalColor = pow(finalColor, vec4(1.0/2.2));
}