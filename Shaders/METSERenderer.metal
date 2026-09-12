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
struct BattlefieldVertex {
    float3 position [[attribute(0)]];
    float3 normal [[attribute(1)]];
};
struct BattlefieldInstance {
    float4 centerAndMaterial;
    float4 halfExtentsAndKind;
    float4 tintAndRoughness;
};
struct BattlefieldScene {
    float4x4 viewProjection;
    float4 cameraAndTime;
    float4 sunAndFog;
};
struct BattlefieldOut {
    float4 position [[position]];
    float3 worldPosition;
    float3 localPosition;
    float3 normal;
    float4 tintAndRoughness;
    float2 materialAndKind;
};
struct CombatantVertex {
    float4 positionAndPart [[attribute(0)]];
    float3 normal [[attribute(1)]];
};
struct CombatantInstance {
    float4 positionAndFacing;
    float4 healthTierStateLOS;
    float4 actionIdentityPhase;
};
struct CombatantScene {
    float4x4 viewProjection;
    float4 cameraAndTime;
    float4 sunAndFog;
};
struct CombatantOut {
    float4 position [[position]];
    float3 worldPosition;
    float3 normal;
    float4 state;
    float2 partAndIdentity;
};
struct BallisticFXVertex { float3 position [[attribute(0)]]; float3 normal [[attribute(1)]]; };
struct BallisticFXInstance { float4 positionAndKind; float4 directionAndLength; float4 materialSizeLife; };
struct BallisticFXScene { float4x4 viewProjection; float4 cameraAndTime; float4 sunAndFog; };
struct BallisticFXOut {
    float4 position [[position]];
    float3 worldPosition;
    float3 normal;
    float3 localPosition;
    float4 kindMaterialLife;
};

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

vertex BattlefieldOut metseBattlefieldVertex(
    BattlefieldVertex input [[stage_in]],
    constant BattlefieldScene &scene [[buffer(1)]],
    device const BattlefieldInstance *instances [[buffer(2)]],
    uint instanceID [[instance_id]]) {
    BattlefieldInstance instance = instances[instanceID];
    float3 world = instance.centerAndMaterial.xyz + input.position * instance.halfExtentsAndKind.xyz;
    BattlefieldOut output;
    output.position = scene.viewProjection * float4(world, 1.0);
    output.worldPosition = world;
    output.localPosition = input.position;
    output.normal = input.normal;
    output.tintAndRoughness = instance.tintAndRoughness;
    output.materialAndKind = float2(instance.centerAndMaterial.w, instance.halfExtentsAndKind.w);
    return output;
}

vertex CombatantOut metseCombatantVertex(
    CombatantVertex input [[stage_in]],
    constant CombatantScene &scene [[buffer(1)]],
    device const CombatantInstance *instances [[buffer(2)]],
    uint instanceID [[instance_id]]) {
    CombatantInstance instance=instances[instanceID];
    float3 local=input.positionAndPart.xyz;
    float combatState=instance.healthTierStateLOS.z;
    if(combatState>=2.0){
        float angle=combatState>2.5?1.48:0.82;
        float c=cos(angle), s=sin(angle);
        local.xy=float2(local.x*c-local.y*s,local.x*s+local.y*c);
        local.y+=combatState>2.5?0.30:0.16;
    } else {
        float action=instance.actionIdentityPhase.x;
        float moving=max(max(1.0-step(.4,abs(action-1.0)),1.0-step(.4,abs(action-5.0))),
                         max(1.0-step(.4,abs(action-6.0)),1.0-step(.4,abs(action-7.0))));
        local.y+=sin(scene.cameraAndTime.w*8.0+instance.actionIdentityPhase.z)*0.018*moving;
    }
    float yaw=instance.positionAndFacing.w, cy=cos(yaw), sy=sin(yaw);
    float3 rotated=float3(local.x*cy+local.z*sy,local.y,-local.x*sy+local.z*cy);
    float3 world=instance.positionAndFacing.xyz+rotated;
    float3 normal=input.normal;
    if(combatState>=2.0){
        float angle=combatState>2.5?1.48:0.82, c=cos(angle), s=sin(angle);
        normal.xy=float2(normal.x*c-normal.y*s,normal.x*s+normal.y*c);
    }
    normal=float3(normal.x*cy+normal.z*sy,normal.y,-normal.x*sy+normal.z*cy);
    CombatantOut output;
    output.position=scene.viewProjection*float4(world,1.0); output.worldPosition=world;
    output.normal=normalize(normal); output.state=instance.healthTierStateLOS;
    output.partAndIdentity=float2(input.positionAndPart.w,instance.actionIdentityPhase.y);
    return output;
}

vertex BallisticFXOut metseBallisticFXVertex(
    BallisticFXVertex input [[stage_in]],
    constant BallisticFXScene &scene [[buffer(1)]],
    device const BallisticFXInstance *instances [[buffer(2)]],
    uint instanceID [[instance_id]]) {
    BallisticFXInstance instance=instances[instanceID];
    float kind=instance.positionAndKind.w, life=clamp(instance.materialSizeLife.z,0.0,1.0);
    float3 forward=normalize(instance.directionAndLength.xyz);
    float3 reference=abs(forward.y)<.94?float3(0,1,0):float3(1,0,0);
    float3 right=normalize(cross(reference,forward)),up=normalize(cross(forward,right));
    float size=max(instance.materialSizeLife.y,.006);
    float3 extent;
    if(kind<.5) extent=float3(size,size,instance.directionAndLength.w*.5);
    else if(kind<1.5) extent=float3(size*(1.1-life*.6),size*(.72-life*.32),size*(1.1-life*.6));
    else extent=float3(size*life,size*life,size*life*1.7);
    float3 center=instance.positionAndKind.xyz-(kind<.5?forward*extent.z:float3(0));
    float3 world=center+right*input.position.x*extent.x+up*input.position.y*extent.y+forward*input.position.z*extent.z;
    float3 normal=normalize(right*input.normal.x+up*input.normal.y+forward*input.normal.z);
    BallisticFXOut output;output.position=scene.viewProjection*float4(world,1);output.worldPosition=world;
    output.normal=normal;output.localPosition=input.position;
    output.kindMaterialLife=float4(kind,instance.materialSizeLife.x,life,size);return output;
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

fragment float4 metseBattlefieldFragment(
    BattlefieldOut input [[stage_in]],
    constant BattlefieldScene &scene [[buffer(1)]]) {
    float3 normal = normalize(input.normal);
    float material = round(input.materialAndKind.x);
    float kind = round(input.materialAndKind.y);
    float3 world = input.worldPosition;
    float3 base = input.tintAndRoughness.rgb;

    float2 face = abs(normal.y) > 0.8 ? world.xz : (abs(normal.x) > 0.8 ? world.zy : world.xy);
    float fineNoise = noise21(face * 2.7) - 0.5;
    float broadNoise = noise21(face * 0.19) - 0.5;
    base *= 1.0 + fineNoise * 0.10 + broadNoise * 0.14;

    if (material > 2.5 && material < 3.5) {
        float row = floor(world.y * 2.55);
        float2 brickUV = fract(float2(face.x * 1.35 + fmod(row, 2.0) * 0.5, world.y * 2.55));
        float mortar = 1.0 - step(0.055, min(min(brickUV.x, 1.0 - brickUV.x),
                                               min(brickUV.y, 1.0 - brickUV.y)));
        base = mix(base, float3(0.35, 0.30, 0.24), mortar * 0.72);
    } else if (material > 0.5 && material < 1.5) {
        float panel = 1.0 - smoothstep(0.025, 0.055,
            min(abs(fract(face.x * 0.24) - 0.5), abs(fract(face.y * 0.28) - 0.5)));
        float rust = smoothstep(0.62, 0.92, noise21(face * 0.42 + 8.0));
        base = mix(base, float3(0.31, 0.14, 0.055), rust * 0.45);
        base += panel * 0.035;
    } else if (material > 1.5 && material < 2.5) {
        float grain = sin(face.x * 21.0 + noise21(face * 0.8) * 5.0) * 0.055;
        base += grain;
    } else if (material > 3.5 && material < 4.5) {
        float reflection = pow(max(dot(reflect(normalize(world - scene.cameraAndTime.xyz), normal),
                                       normalize(scene.sunAndFog.xyz)), 0.0), 12.0);
        base += reflection * float3(0.22, 0.31, 0.34);
    } else if (material > 4.5 && material < 5.5) {
        float gravel = noise21(world.xz * 5.8) * 0.10;
        float tire = smoothstep(0.82, 0.96, abs(sin(world.x * 0.53 + noise21(world.xz * 0.11))));
        base += gravel - tire * 0.045;
    }

    if (kind == 1.0 && abs(normal.y) < 0.5 && world.y > 1.05 && material != 4.0) {
        float horizontal = abs(normal.x) > 0.8 ? world.z : world.x;
        float2 cell = fract(float2(horizontal * 0.29, world.y * 0.47));
        float window = step(0.20, cell.x) * step(cell.x, 0.72) * step(0.25, cell.y) * step(cell.y, 0.70);
        base = mix(base, float3(0.055, 0.075, 0.075), window * 0.58);
    }

    float3 edgeAxes = step(float3(0.90), abs(input.localPosition));
    float edge = step(1.5, edgeAxes.x + edgeAxes.y + edgeAxes.z);
    base = mix(base, min(base * 1.28 + float3(0.035), float3(1.0)), edge * 0.50);
    if (kind == 2.0) {
        float marker = step(0.5, fract((world.x + world.z) * 0.55));
        base = mix(base, float3(0.62, 0.43, 0.09), marker * 0.48);
    }

    float3 light = normalize(scene.sunAndFog.xyz);
    float diffuse = max(dot(normal, light), 0.0);
    float skyAmbient = 0.22 + max(normal.y, 0.0) * 0.18;
    float3 view = normalize(scene.cameraAndTime.xyz - world);
    float3 halfVector = normalize(light + view);
    float roughness = clamp(input.tintAndRoughness.w, 0.12, 1.0);
    float specular = pow(max(dot(normal, halfVector), 0.0), mix(72.0, 7.0, roughness));
    float3 lit = base * (skyAmbient + diffuse * 0.86) + specular * (1.0 - roughness) * 0.46;
    float distance = length(scene.cameraAndTime.xyz - world);
    float fog = 1.0 - exp(-distance * scene.sunAndFog.w);
    float3 fogColor = float3(0.54, 0.56, 0.52);
    lit = mix(lit, fogColor, clamp(fog, 0.0, 0.82));
    lit = lit / (lit + 0.82);
    return float4(pow(max(lit, 0.0), float3(0.92)), 1.0);
}

fragment float4 metseCombatantFragment(CombatantOut input [[stage_in]],
                                       constant CombatantScene &scene [[buffer(1)]]) {
    float part=round(input.partAndIdentity.x);
    float3 palette[6]={float3(.19,.22,.15),float3(.24,.27,.18),float3(.43,.31,.23),
                       float3(.12,.14,.10),float3(.055,.060,.057),float3(.075,.070,.060)};
    float3 base=palette[(uint)clamp(part,0.0,5.0)];
    float grime=noise21(input.worldPosition.xz*5.1+input.worldPosition.yy)*.10-.045;
    base+=grime;
    float3 n=normalize(input.normal), light=normalize(scene.sunAndFog.xyz);
    float diffuse=max(dot(n,light),0.0), ambient=.19+max(n.y,0.0)*.17;
    float3 view=normalize(scene.cameraAndTime.xyz-input.worldPosition);
    float rim=pow(1.0-max(dot(n,view),0.0),3.0);
    float health=clamp(input.state.x,0.0,1.0), lineOfSight=input.state.w;
    float3 lit=base*(ambient+diffuse*.88)+rim*float3(.07,.08,.06);
    lit=mix(lit,lit*float3(.62,.55,.52),1.0-health);
    lit*=mix(.72,1.0,lineOfSight);
    float distance=length(scene.cameraAndTime.xyz-input.worldPosition);
    float fog=clamp(1.0-exp(-distance*scene.sunAndFog.w),0.0,.82);
    lit=mix(lit,float3(.54,.56,.52),fog); lit=lit/(lit+.82);
    return float4(pow(max(lit,0.0),float3(.92)),1.0);
}

fragment float4 metseBallisticFXFragment(BallisticFXOut input [[stage_in]],
                                         constant BallisticFXScene &scene [[buffer(1)]]) {
    float kind=input.kindMaterialLife.x,material=input.kindMaterialLife.y,life=input.kindMaterialLife.z;
    float3 color;
    if(kind<.5){
        float core=1.0-smoothstep(.12,1.0,length(input.localPosition.xy));
        color=mix(float3(1.0,.24,.035),float3(1.0,.91,.52),core)*1.8;
    } else if(kind>1.5){
        color=float3(1.0,.38,.055)*(1.2+life);
    } else {
        color=materialColor(material);
        float3 n=normalize(input.normal),light=normalize(scene.sunAndFog.xyz);
        color*=.22+max(dot(n,light),0.0)*.84;
        color+=noise21(input.worldPosition.xz*8.0+input.worldPosition.yy)*.075;
    }
    float distance=length(scene.cameraAndTime.xyz-input.worldPosition);
    float fog=clamp(1.0-exp(-distance*scene.sunAndFog.w),0.0,kind < .5 ? .44 : .78);
    color=mix(color,float3(.54,.56,.52),fog);
    return float4(pow(max(color/(color+.82),0.0),float3(.92)),1.0);
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

    // World surfaces are rendered by metseBattlefieldVertex/metseBattlefieldFragment
    // from the same authoritative obstacle snapshot. This pass owns sky and overlays.

    // Historical target visibility ABI remains populated for diagnostics. Native,
    // depth-tested geometry consumes the same snapshot in METSECombatantRenderer.
    float4 tm=uniforms.targetMeta[0]; float tier=tm.x; color+=float3(tier*0.0);

    // Projectile and impact FX arrays remain in the diagnostic ABI. Their visible
    // representation is native world geometry in METSEBallisticFXRenderer.

    color = color / (color + 0.82); color = pow(max(color, 0.0), float3(0.92));
    return float4(color, 1.0);
}

fragment float4 metseReticleFragment(VSOut input [[stage_in]],
                                     constant Uniforms &uniforms [[buffer(0)]]) {
    float2 resolution = max(uniforms.timing.yz, float2(1));
    float2 screen = (input.position.xy / resolution) * 2.0 - 1.0; screen.y = -screen.y;
    float ads = clamp(uniforms.weapon.x, 0.0, 1.0);
    float crossScale = mix(1.0, 0.62, ads);
    float horizontal = (1.0 - smoothstep(0.0025, 0.0065, abs(screen.x))) * step(abs(screen.y), 0.024 * crossScale);
    float vertical = (1.0 - smoothstep(0.0025, 0.0065, abs(screen.y))) * step(abs(screen.x), 0.024 * crossScale);
    float reticle = clamp(horizontal + vertical, 0.0, 1.0) * 0.68;
    float2 muzzle = mix(float2(0.24, -0.30), float2(0.0, -0.12), ads);
    float flash = uniforms.timing.w * smoothstep(0.13, 0.0, length(screen - muzzle));
    float alpha = clamp(reticle + flash, 0.0, 1.0);
    if (alpha <= 0.001) discard_fragment();
    float3 reticleColor = uniforms.weapon.w > 0.5 ? float3(1.0, 0.25, 0.18) : float3(0.76, 0.90, 0.82);
    float3 color = reticleColor * reticle + float3(1.0, 0.48, 0.10) * flash;
    return float4(color / max(alpha, 0.001), alpha);
}
