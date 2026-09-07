#include <metal_stdlib>
using namespace metal;

constant uint kMaxObstacles = 6;

struct VSOut {
    float4 position [[position]];
};

struct Uniforms {
    float4 timing;
    float4 camera;
    float4 state;
    float4 character;
    float4 worldMeta;
    float4 worldExtra;
    float4 obstacles[kMaxObstacles];
    float4 obstacleHeightsA;
    float4 obstacleHeightsB;
};

vertex VSOut metseVertex(uint vertexID [[vertex_id]]) {
    float2 positions[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
    VSOut out;
    out.position = float4(positions[vertexID], 0.0, 1.0);
    return out;
}

float gridLine(float2 point) {
    float2 grid = abs(fract(point) - 0.5) / max(fwidth(point), float2(0.0001));
    return 1.0 - min(min(grid.x, grid.y), 1.0);
}

float obstacleHeight(constant Uniforms& uniforms, uint index) {
    if (index < 4) return uniforms.obstacleHeightsA[index];
    return uniforms.obstacleHeightsB[index - 4];
}

float rayBoxDistance(float3 rayOrigin, float3 rayDirection, float4 footprint, float height) {
    float3 boxMin = float3(footprint.x, 0.0, footprint.y);
    float3 boxMax = float3(footprint.z, height, footprint.w);
    float3 safeDirection = select(rayDirection,
                                  copysign(float3(0.00001), rayDirection),
                                  abs(rayDirection) < float3(0.00001));
    float3 inverseDirection = 1.0 / safeDirection;
    float3 t0 = (boxMin - rayOrigin) * inverseDirection;
    float3 t1 = (boxMax - rayOrigin) * inverseDirection;
    float3 tSmall = min(t0, t1);
    float3 tBig = max(t0, t1);
    float nearT = max(max(tSmall.x, tSmall.y), tSmall.z);
    float farT = min(min(tBig.x, tBig.y), tBig.z);
    if (farT < max(nearT, 0.0)) return 1.0e20;
    return nearT > 0.0 ? nearT : farT;
}

fragment float4 metseFragment(VSOut in [[stage_in]], constant Uniforms& uniforms [[buffer(0)]]) {
    float2 resolution = max(uniforms.timing.yz, float2(1.0));
    float2 screenUV = (in.position.xy / resolution) * 2.0 - 1.0;
    screenUV.y = -screenUV.y;

    float roll = uniforms.worldExtra.y;
    float cosineRoll = cos(roll);
    float sineRoll = sin(roll);
    screenUV = float2(cosineRoll * screenUV.x - sineRoll * screenUV.y,
                      sineRoll * screenUV.x + cosineRoll * screenUV.y);

    float aspect = resolution.x / resolution.y;
    float2 viewUV = screenUV;
    viewUV.x *= aspect;

    float yaw = uniforms.camera.z;
    float pitch = uniforms.camera.w;
    float sineYaw = sin(yaw);
    float cosineYaw = cos(yaw);
    float3 forward = normalize(float3(sineYaw, 0.0, cosineYaw));
    float3 right = float3(cosineYaw, 0.0, -sineYaw);
    float3 up = float3(0.0, 1.0, 0.0);
    float3 ray = normalize(forward + right * (viewUV.x * 0.72) + up * ((viewUV.y + pitch * 0.72) * 0.58));
    float3 camera = float3(uniforms.camera.x, uniforms.character.x + uniforms.character.y, uniforms.camera.y);

    float3 color = float3(0.018, 0.032, 0.034) + max(ray.y, 0.0) * float3(0.018, 0.038, 0.040);
    float groundDistance = ray.y < -0.015 ? -camera.y / ray.y : 1.0e20;

    float nearestObstacle = 1.0e20;
    uint nearestIndex = 0;
    uint obstacleCount = min((uint)round(uniforms.worldMeta.x), kMaxObstacles);
    for (uint i = 0; i < obstacleCount; ++i) {
        float hit = rayBoxDistance(camera, ray, uniforms.obstacles[i], obstacleHeight(uniforms, i));
        if (hit > 0.0 && hit < nearestObstacle) {
            nearestObstacle = hit;
            nearestIndex = i;
        }
    }

    if (groundDistance < nearestObstacle && groundDistance < 1.0e19) {
        float3 world = camera + ray * groundDistance;
        float fade = exp(-groundDistance * 0.026);
        float major = gridLine(world.xz * 0.10);
        float minor = gridLine(world.xz * 0.50) * 0.30;
        float3 ground = float3(0.040, 0.055, 0.048);
        ground += (major + minor) * float3(0.08, 0.18, 0.13) * fade;

        float minX = uniforms.worldMeta.y;
        float maxX = uniforms.worldMeta.z;
        float minZ = uniforms.worldMeta.w;
        float maxZ = uniforms.worldExtra.x;
        float boundary = max(1.0 - smoothstep(0.06, 0.14, min(abs(world.x - minX), abs(world.x - maxX))),
                             1.0 - smoothstep(0.06, 0.14, min(abs(world.z - minZ), abs(world.z - maxZ))));
        ground = mix(ground, float3(0.55, 0.42, 0.12), boundary * fade * 0.72);
        color = mix(color, ground, fade);
    } else if (nearestObstacle < 1.0e19) {
        float3 hitPoint = camera + ray * nearestObstacle;
        float height = max(obstacleHeight(uniforms, nearestIndex), 0.1);
        float vertical = clamp(hitPoint.y / height, 0.0, 1.0);
        float distanceFade = exp(-nearestObstacle * 0.018);
        float3 wallBase = float3(0.105, 0.125, 0.115);
        float3 wallTop = float3(0.16, 0.19, 0.17);
        color = mix(wallBase, wallTop, vertical) * (0.55 + 0.45 * distanceFade);
        float grid = gridLine(hitPoint.xz * 0.40) * 0.10;
        color += grid * float3(0.08, 0.16, 0.12);
    }

    float2 targetWorld = float2(8.0, 18.0);
    float2 relative = targetWorld - uniforms.camera.xy;
    float forwardDistance = dot(relative, float2(sineYaw, cosineYaw));
    float sideDistance = dot(relative, float2(cosineYaw, -sineYaw));
    if (forwardDistance > 1.0) {
        float targetX = (sideDistance / forwardDistance) / (0.72 * aspect);
        float targetY = (-0.12 / forwardDistance - pitch * 0.72) / 0.58;
        float2 delta = float2(screenUV.x, screenUV.y) - float2(targetX, targetY);
        float marker = smoothstep(0.035, 0.012, length(delta));
        color = mix(color, float3(0.94, 0.29, 0.12), marker);
    }

    float crossHorizontal = (1.0 - smoothstep(0.0025, 0.0065, abs(screenUV.x))) * step(abs(screenUV.y), 0.025);
    float crossVertical = (1.0 - smoothstep(0.0025, 0.0065, abs(screenUV.y))) * step(abs(screenUV.x), 0.025);
    color = mix(color, float3(0.76, 0.90, 0.82), clamp(crossHorizontal + crossVertical, 0.0, 1.0) * 0.7);

    color += uniforms.timing.w * float3(0.35, 0.22, 0.08) * smoothstep(0.9, 0.0, length(viewUV - float2(0.28, -0.55)));
    return float4(color, 1.0);
}
