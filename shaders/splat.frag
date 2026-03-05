#version 450 core

noperspective in vec2 vDelta;
flat in vec3 vConic;
in vec3 vColor;
in float vOpacity;
flat in vec3 vNormal;

// Lighting uniforms
uniform vec3 uLightDir;        // normalized world-space light direction (toward light)
uniform vec3 uLightColor;      // light color * intensity
uniform float uAmbient;        // ambient factor [0,1]
uniform float uDiffuse;        // diffuse strength
uniform float uWrapFactor;     // wrap lighting factor [0,1] — 0=Lambert, 0.5=half-wrap, 1=full-wrap
uniform float uScatter;        // subsurface scatter strength
uniform float uLightingEnabled; // 0.0 = off, 1.0 = on (smooth toggle)

out vec4 FragColor;

void main() {
    float power = -0.5 * (vConic.x * vDelta.x * vDelta.x
                         + 2.0 * vConic.y * vDelta.x * vDelta.y
                         + vConic.z * vDelta.y * vDelta.y);

    if (power > 0.0) discard;

    float gaussian = exp(power);
    float alpha = min(0.99, vOpacity * gaussian);

    if (alpha < 0.004) discard;  // 1/255

    vec3 color = vColor;

    // Apply lighting if enabled and normal is valid
    if (uLightingEnabled > 0.0 && length(vNormal) > 0.5) {
        vec3 N = normalize(vNormal);
        vec3 L = uLightDir;

        // Two-sided lighting: use abs(NdotL) so both sides of
        // the Gaussian disc are lit consistently (no flip discontinuity)
        float NdotL = dot(N, L);
        float absNdotL = abs(NdotL);

        // Wrap diffuse on the absolute dot product
        float wrapDenom = 1.0 + uWrapFactor;
        float diffuse = (absNdotL + uWrapFactor) / (wrapDenom * wrapDenom);

        // Subsurface scatter: proportional to how edge-on the splat is to the light
        float scatter = uScatter * (1.0 - absNdotL) * 0.5;

        // Combine: ambient + diffuse + scatter
        vec3 lighting = uAmbient + (uDiffuse * diffuse + scatter) * uLightColor;

        // Mix between unlit and lit based on toggle
        color = mix(vColor, vColor * lighting, uLightingEnabled);
    }

    FragColor = vec4(color * alpha, alpha);  // premultiplied alpha
}
