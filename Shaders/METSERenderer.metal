#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

struct FrameUniforms {
    float time;
    float2 resolution;
};

vertex VertexOut metseVertex(uint vertexID [[vertex_id]]) {
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };
    VertexOut out;
    out.position = float4(positions[vertexID], 0.0, 1.0);
    out.uv = positions[vertexID] * 0.5 + 0.5;
    return out;
}

fragment float4 metseFragment(VertexOut in [[stage_in]],
                              constant FrameUniforms &u [[buffer(0)]]) {
    float2 uv = in.uv;
    float2 gridUV = uv * float2(28.0, 16.0);
    float2 cell = abs(fract(gridUV - 0.5) - 0.5) / fwidth(gridUV);
    float grid = 1.0 - min(min(cell.x, cell.y), 1.0);

    float pulse = 0.5 + 0.5 * sin(u.time * 0.55 + uv.x * 2.4);
    float vignette = smoothstep(1.2, 0.15, distance(uv, float2(0.5)));
    float3 base = mix(float3(0.012, 0.020, 0.022), float3(0.020, 0.040, 0.041), uv.y);
    float3 tactical = float3(0.12, 0.34, 0.29) * grid * 0.12;
    float3 glow = float3(0.08, 0.24, 0.20) * pulse * vignette * 0.10;
    return float4(base + tactical + glow, 1.0);
}
