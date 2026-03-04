#version 450 core

noperspective in vec2 vDelta;
flat in vec3 vConic;
in vec3 vColor;
in float vOpacity;

out vec4 FragColor;

void main() {
    float power = -0.5 * (vConic.x * vDelta.x * vDelta.x
                         + 2.0 * vConic.y * vDelta.x * vDelta.y
                         + vConic.z * vDelta.y * vDelta.y);

    if (power > 0.0) discard;

    float gaussian = exp(power);
    float alpha = min(0.99, vOpacity * gaussian);

    if (alpha < 0.004) discard;  // 1/255

    FragColor = vec4(vColor * alpha, alpha);  // premultiplied alpha
}
