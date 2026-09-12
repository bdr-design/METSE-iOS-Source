#include <metal_stdlib>
using namespace metal;

constant uint kMaxObstacles=20;
constant uint kMaxProjectiles=8;
constant uint kMaxTargets=32;
constant uint kMaxFX=16;

struct VSOut { float4 position [[position]]; };
struct Uniforms {
    float4 timing; float4 camera; float4 character; float4 weapon; float4 weapon2;
    float4 worldMeta; float4 worldExtra; float4 presentationMeta;
    float4 obstacleBounds[kMaxObstacles]; float4 obstacleMeta[kMaxObstacles];
    float4 projectilePositions[kMaxProjectiles];
    float4 targetData[kMaxTargets]; float4 targetMeta[kMaxTargets];
    float4 fxData[kMaxFX]; float4 fxMeta[kMaxFX];
};
struct MeshVertex { float3 position [[attribute(0)]]; float3 normal [[attribute(1)]]; };
struct MeshUniforms {
    float4x4 modelViewProjection; float4x4 model;
    float4 baseColorMetallic; float4 lightAndObstruction;
};
struct MeshOut { float4 position [[position]]; float3 normal; float3 viewPosition; };

vertex VSOut metseVertex(uint id [[vertex_id]]) {
    float2 positions[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    VSOut output; output.position = float4(positions[id], 0.999, 1); return output;
}

vertex MeshOut metseMeshVertex(MeshVertex input [[stage_in]],
                               constant MeshUniforms &uniforms [[buffer(1)]]) {
    MeshOut output; float4 local = float4(input.position, 1.0);
    output.position = uniforms.modelViewProjection * local;
    output.viewPosition = (uniforms.model * local).xyz;
    output.normal = normalize((uniforms.model * float4(input.normal, 0.0)).xyz);
    return output;
}

fragment float4 metseMeshFragment(MeshOut input [[stage_in]],
                                  constant MeshUniforms &uniforms [[buffer(1)]]) {
    float3 normal = normalize(input.normal);
    float3 light = normalize(uniforms.lightAndObstruction.xyz);
    float3 view = normalize(-input.viewPosition);
    float3 halfVector = normalize(light + view);
    float diffuse = max(dot(normal, light), 0.0);
    float rim = pow(1.0 - max(dot(normal, view), 0.0), 3.0);
    float roughness = clamp(1.0 - uniforms.baseColorMetallic.w, 0.12, 0.95);
    float specular = pow(max(dot(normal, halfVector), 0.0), mix(90.0, 10.0, roughness));
    float weaponMask = 1.0; // Native geometry replaced the old 2D weapon mask.
    float3 lit = uniforms.baseColorMetallic.rgb * (0.23 + diffuse * 0.86);
    lit += specular * mix(0.12, 0.52, uniforms.baseColorMetallic.w);
    lit += rim * float3(0.10, 0.12, 0.10);
    lit = mix(lit, float3(0.34, 0.07, 0.035), uniforms.lightAndObstruction.w * 0.34);
    return float4(lit * weaponMask, 1.0);
}

float gridLine(float2 position) {
    float2 grid = abs(fract(position) - 0.5) / max(fwidth(position), float2(0.0001));
    return 1.0 - min(min(grid.x, grid.y), 1.0);
}
float hash21(float2 position) {
    position = fract(position * float2(123.34, 456.21));
    position += dot(position, position + 45.32); return fract(position.x * position.y);
}
float noise21(float2 position) {
    float2 cell = floor(position), fraction = fract(position);
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    return mix(mix(hash21(cell), hash21(cell + float2(1, 0)), fraction.x),
               mix(hash21(cell + float2(0, 1)), hash21(cell + 1.0), fraction.x), fraction.y);
}
float rayBox(float3 origin, float3 direction, float4 footprint, float2 yBounds) {
    float3 minimum = float3(footprint.x, yBounds.x, footprint.y);
    float3 maximum = float3(footprint.z, yBounds.y, footprint.w);
    float3 safeDirection = select(direction, copysign(float3(0.00001), direction),
                                  abs(direction) < float3(0.00001));
    float3 inverse = 1.0 / safeDirection;
    float3 a = (minimum - origin) * inverse, b = (maximum - origin) * inverse;
    float3 nearValues = min(a, b), farValues = max(a, b);
    float nearT = max(max(nearValues.x, nearValues.y), nearValues.z);
    float farT = min(min(farValues.x, farValues.y), farValues.z);
    if (farT < max(nearT, 0.0)) return 1e20; return nearT > 0 ? nearT : farT;
}
float sdBox(float2 position, float2 bounds) {
    float2 delta = abs(position) - bounds;
    return length(max(delta, 0.0)) + min(max(delta.x, delta.y), 0.0);
}
void cameraBasis(float yaw, float pitch, float roll,
                 thread float3 &forward, thread float3 &right, thread float3 &up) {
    float cp = cos(pitch), sp = sin(pitch), sy = sin(yaw), cy = cos(yaw);
    forward = normalize(float3(sy * cp, sp, cy * cp));
    float3 baseRight = normalize(float3(cy, 0.0, -sy));
    float3 baseUp = normalize(cross(forward, baseRight));
    float cr = cos(roll), sr = sin(roll);
    right = normalize(baseRight * cr + baseUp * sr); up = normalize(baseUp * cr - baseRight * sr);
}
float3 projectWorld(float3 world, float3 camera, float3 forward, float3 right, float3 up,
                    float tanHalfVFov, float aspect) {
    float3 relative = world - camera; float forwardDistance = dot(relative, forward);
    if (forwardDistance <= 0.05) return float3(99, 99, forwardDistance);
    return float3(dot(relative, right) / (forwardDistance * tanHalfVFov * aspect),
                  dot(relative, up) / (forwardDistance * tanHalfVFov), forwardDistance);
}
float3 materialColor(float material) {
    if (material < 0.5) return float3(0.34, 0.32, 0.26);
    if (material < 1.5) return float3(0.18, 0.20, 0.19);
    if (material < 2.5) return float3(0.26, 0.18, 0.10);
    if (material < 3.5) return float3(0.38, 0.19, 0.11);
    if (material < 4.5) return float3(0.10, 0.29, 0.32);
    if (material < 5.5) return float3(0.34, 0.27, 0.17);
    return float3(0.24, 0.23, 0.20);
}

fragment float4 metseFragment(VSOut input [[stage_in]], constant Uniforms &uniforms [[buffer(0)]]) {
    float2 resolution = max(uniforms.timing.yz, float2(1));
    float2 screen = (input.position.xy / resolution) * 2.0 - 1.0; screen.y = -screen.y;
    float aspect = resolution.x / resolution.y, ads = clamp(uniforms.weapon.x, 0.0, 1.0);
    float3 forward, right, up;
    cameraBasis(uniforms.camera.z, uniforms.camera.w,
                uniforms.character.w + uniforms.worldExtra.w, forward, right, up);
    float tanHalfVFov = mix(0.58, 0.42, ads);
    float3 ray = normalize(forward + right * (screen.x * aspect * tanHalfVFov) + up * (screen.y * tanHalfVFov));
    float3 camera = float3(uniforms.camera.x, uniforms.character.x + uniforms.character.y, uniforms.camera.y);

    float horizon = smoothstep(-0.12, 0.38, ray.y);
    float sun = pow(max(dot(ray, normalize(float3(-0.35, 0.72, 0.42))), 0.0), 420.0);
    float haze = pow(max(1.0 - abs(ray.y), 0.0), 5.0);
    float3 color = mix(float3(0.68, 0.72, 0.70), float3(0.17, 0.31, 0.42), horizon);
    color += haze * float3(0.15, 0.10, 0.045) + sun * float3(1.0, 0.72, 0.36);

    float groundT = ray.y < -0.015 ? -camera.y / ray.y : 1e20, nearest = 1e20;
    uint nearestIndex = 0, obstacleCount = min((uint)round(uniforms.worldMeta.x), kMaxObstacles);
    for (uint i = 0; i < obstacleCount; ++i) {
        float hit = rayBox(camera, ray, uniforms.obstacleBounds[i], uniforms.obstacleMeta[i].xy);
        if (hit > 0 && hit < nearest) { nearest = hit; nearestIndex = i; }
    }
    if (groundT < nearest && groundT < 1e19) {
        float3 world = camera + ray * groundT; float fog = exp(-groundT * 0.018);
        float broadNoise = noise21(world.xz * 0.045), grain = noise21(world.xz * 0.72) * 0.035;
        float tracks = smoothstep(0.96, 0.985, abs(sin(world.x * 0.62 + noise21(world.xz * 0.08))));
        float3 ground = mix(float3(0.25, 0.20, 0.13), float3(0.43, 0.34, 0.21), broadNoise);
        ground += grain - tracks * 0.028 + gridLine(world.xz * 0.10) * float3(0.010, 0.012, 0.009);
        float boundary = max(1.0 - smoothstep(0.06, 0.14,
            min(abs(world.x - uniforms.worldMeta.y), abs(world.x - uniforms.worldMeta.z))),
            1.0 - smoothstep(0.06, 0.14,
            min(abs(world.z - uniforms.worldMeta.w), abs(world.z - uniforms.worldExtra.x))));
        ground = mix(ground, float3(0.64, 0.45, 0.12), boundary * 0.66); color = mix(color, ground, fog);
    } else if (nearest < 1e19) {
        float3 hitPoint = camera + ray * nearest; float2 yBounds = uniforms.obstacleMeta[nearestIndex].xy;
        float vertical = clamp((hitPoint.y - yBounds.x) / max(0.1, yBounds.y - yBounds.x), 0.0, 1.0);
        float3 base = materialColor(uniforms.obstacleMeta[nearestIndex].z);
        float variation = noise21(hitPoint.xz * 2.2 + hitPoint.yy) * 0.12;
        float edgeShade = smoothstep(0.0, 0.12, vertical) * smoothstep(1.0, 0.88, vertical);
        color = mix(color, base * (0.52 + edgeShade * 0.52 + variation), exp(-nearest * 0.016));
    }

    uint targetCount = min((uint)round(uniforms.worldExtra.y), kMaxTargets);
    for (uint i = 0; i < targetCount; ++i) {
        float4 target = uniforms.targetData[i], metadata = uniforms.targetMeta[i];
        float4 tm=metadata; float tier=tm.x;
        if (target.w < 0.5 || metadata.y < 0.5 || tier >= 2.5) continue;
        float3 center = projectWorld(float3(target.x, 1.015, target.y), camera, forward, right, up, tanHalfVFov, aspect);
        if (center.z <= 1 || abs(center.x) > 2 || abs(center.y) > 2) continue;
        float3 targetColor = mix(float3(0.38, 0.055, 0.035), float3(0.50, 0.22, 0.08), clamp(target.z / 100.0, 0.0, 1.0));
        if (tier >= 1.5) {
            float radius = max(0.005, 0.13 / (center.z * tanHalfVFov * aspect));
            color = mix(color, targetColor, (1.0 - smoothstep(radius * 0.65, radius, length(screen - center.xy))) * 0.42); continue;
        }
        float3 feet = projectWorld(float3(target.x, 0.15, target.y), camera, forward, right, up, tanHalfVFov, aspect);
        float3 head = projectWorld(float3(target.x, 1.88, target.y), camera, forward, right, up, tanHalfVFov, aspect);
        float height = max(0.012, abs(head.y - feet.y));
        float halfWidth = max(0.008, 0.36 / (center.z * tanHalfVFov * aspect));
        float body = 1.0 - smoothstep(0.0, 0.008, sdBox(screen - float2(center.x, (feet.y + head.y) * 0.5), float2(halfWidth, height * 0.34)));
        float headRadius = max(halfWidth * 0.72, height * 0.085);
        float headMask = tier < 0.5 ? 1.0 - smoothstep(headRadius * 0.86, headRadius,
            length(screen - float2(center.x, head.y - headRadius * 0.75))) : 0.0;
        color = mix(color, targetColor, clamp(body + headMask, 0.0, 1.0) * (tier < 0.5 ? 0.84 : 0.62));
    }

    uint projectileCount = min((uint)round(uniforms.worldExtra.z), kMaxProjectiles);
    for (uint i = 0; i < projectileCount; ++i) {
        float3 projected = projectWorld(uniforms.projectilePositions[i].xyz, camera, forward, right, up, tanHalfVFov, aspect);
        if (projected.z > 0 && abs(projected.x) < 2 && abs(projected.y) < 2)
            color += smoothstep(0.018, 0.002, length(screen - projected.xy)) * float3(1.0, 0.62, 0.18);
    }
    uint fxCount = min((uint)round(uniforms.presentationMeta.x), kMaxFX);
    for (uint i = 0; i < fxCount; ++i) {
        float4 data = uniforms.fxData[i], metadata = uniforms.fxMeta[i];
        float3 projected = projectWorld(data.xyz, camera, forward, right, up, tanHalfVFov, aspect);
        if (projected.z <= 0.05 || abs(projected.x) > 2 || abs(projected.y) > 2) continue;
        float radius = max(0.008, (0.09 + 0.16 * (1.0 - data.w)) / (projected.z * tanHalfVFov * aspect));
        float mask = 1.0 - smoothstep(radius * 0.25, radius, length(screen - projected.xy));
        color += (materialColor(metadata.y) * 1.8 + float3(0.14, 0.10, 0.04)) * mask * data.w * clamp(metadata.z, 0.0, 1.0) * 0.48;
    }

    float crossScale = mix(1.0, 0.62, ads);
    float horizontal = (1.0 - smoothstep(0.0025, 0.0065, abs(screen.x))) * step(abs(screen.y), 0.024 * crossScale);
    float vertical = (1.0 - smoothstep(0.0025, 0.0065, abs(screen.y))) * step(abs(screen.x), 0.024 * crossScale);
    color = mix(color, uniforms.weapon.w > 0.5 ? float3(1, 0.25, 0.18) : float3(0.76, 0.90, 0.82), clamp(horizontal + vertical, 0.0, 1.0) * 0.68);
    float2 muzzle = mix(float2(0.24, -0.30), float2(0.0, -0.12), ads);
    color += uniforms.timing.w * smoothstep(0.13, 0.0, length(screen - muzzle)) * float3(1.0, 0.48, 0.10);
    color = color / (color + 0.82); color = pow(max(color, 0.0), float3(0.92));
    return float4(color, 1.0);
}
