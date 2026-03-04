#version 450 core

layout (location = 0) in vec2 aQuadPos;

struct Splat2D {
    vec4 clipCenter;
    vec4 conicRadius;   // conic.xyz + radius
    vec4 colorOpacity;  // rgb + opacity
};

struct SortEntry {
    uint key;
    uint index;
};

layout(std430, binding = 1) readonly buffer SortBuffer {
    SortEntry sortEntries[];
};

layout(std430, binding = 2) readonly buffer Splat2DBuffer {
    Splat2D splats[];
};

uniform vec2 viewport;
uniform uint uStartOffset;

out vec3 vColor;
out float vOpacity;
noperspective out vec2 vDelta;
flat out vec3 vConic;

void main() {
    uint origIdx = sortEntries[uStartOffset + gl_InstanceID].index;
    Splat2D s = splats[origIdx];

    float rad = s.conicRadius.w;

    // Skip invisible (culled in preprocess)
    if (rad <= 0.0 || s.colorOpacity.w <= 0.0) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
        vOpacity = 0.0;
        return;
    }

    vec2 off = aQuadPos * rad;

    gl_Position = s.clipCenter;
    gl_Position.xy += (off / (viewport * 0.5)) * s.clipCenter.w;

    vConic = s.conicRadius.xyz;
    vColor = s.colorOpacity.xyz;
    vOpacity = s.colorOpacity.w;
    vDelta = off;
}
