#version 450 core

in vec3 vertexColor;
out vec4 FragColor;

uniform float opacity;

void main()
{
    // Discard fragments outside a circle to make round points
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    if (dot(coord, coord) > 1.0)
        discard;

    FragColor = vec4(vertexColor, opacity);
}
