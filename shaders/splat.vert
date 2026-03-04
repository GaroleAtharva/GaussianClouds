#version 450 core

layout (location = 0) in vec2 aQuadPos;

struct Splat2D {
    vec4 clipCenter;
    vec4 conicRadius;   // conic.xyz + radius
    vec4 colorOpacity;  // rgb + opacity
    vec4 axisA_axisB;   // xy = major axis, zw = minor axis (screen-space pixels)
};

struct SortEntry {
    uint key;
    uint index;
};

layout(std430, binding = 1) readonly buffer SortBuffer {
    // For bitonic sort: SortEntry array (key + index interleaved)
    // For radix sort: plain uint array of sorted indices
    uint sortData[];
};

layout(std430, binding = 2) readonly buffer Splat2DBuffer {
    Splat2D splats[];
};

uniform vec2 viewport;
uniform uint uStartOffset;
uniform uint uUseRadixSort;

out vec3 vColor;
out float vOpacity;
noperspective out vec2 vDelta;
flat out vec3 vConic;

void main() {
    uint origIdx;
    if (uUseRadixSort != 0u) {
        // Radix sort: binding 1 is a plain uint[] of sorted indices
        origIdx = sortData[uStartOffset + gl_InstanceID];
    } else {
        // Bitonic sort: binding 1 is SortEntry[] — index is at offset *2+1
        origIdx = sortData[(uStartOffset + gl_InstanceID) * 2u + 1u];
    }
    Splat2D s = splats[origIdx];

    float rad = s.conicRadius.w;

    // Skip invisible (culled in preprocess)
    if (rad <= 0.0 || s.colorOpacity.w <= 0.0) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
        vOpacity = 0.0;
        return;
    }

    // Ellipse-fitted quad: use major/minor axes instead of square
    vec2 axisA = s.axisA_axisB.xy;
    vec2 axisB = s.axisA_axisB.zw;
    vec2 off = aQuadPos.x * axisA + aQuadPos.y * axisB;

    gl_Position = s.clipCenter;
    gl_Position.xy += (off / (viewport * 0.5)) * s.clipCenter.w;

    vConic = s.conicRadius.xyz;
    vColor = s.colorOpacity.xyz;
    vOpacity = s.colorOpacity.w;
    vDelta = off;
}
