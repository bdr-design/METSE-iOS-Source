#include <metal_stdlib>
using namespace metal;
struct VSOut { float4 position [[position]]; };
struct Uniforms { float4 timing; float4 camera; float4 state; };
vertex VSOut metseVertex(uint vid [[vertex_id]]) { float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; VSOut o; o.position=float4(p[vid],0,1); return o; }
float gridLine(float2 p){ float2 g=abs(fract(p)-0.5)/fwidth(p); return 1.0-min(min(g.x,g.y),1.0); }
fragment float4 metseFragment(VSOut in [[stage_in]], constant Uniforms& u [[buffer(0)]]) {
    float2 res=max(u.timing.yz,float2(1)); float2 uv=(in.position.xy/res)*2.0-1.0; uv.y=-uv.y; float aspect=res.x/res.y; uv.x*=aspect;
    float yaw=u.camera.z, pitch=u.camera.w; float sy=sin(yaw), cy=cos(yaw); float3 forward=normalize(float3(sy,0.0,cy)); float3 right=float3(cy,0,-sy); float3 up=float3(0,1,0);
    float3 ray=normalize(forward + right*(uv.x*0.72) + up*((uv.y+pitch*0.72)*0.58)); float3 cam=float3(u.camera.x,1.68,u.camera.y);
    float3 col=float3(0.018,0.032,0.034) + max(ray.y,0.0)*float3(0.018,0.038,0.040);
    if(ray.y < -0.015){ float t=-cam.y/ray.y; float3 w=cam+ray*t; float fade=exp(-t*0.028); float major=gridLine(w.xz*0.10); float minor=gridLine(w.xz*0.50)*0.34; float3 ground=float3(0.040,0.055,0.048); ground += (major+minor)*float3(0.08,0.18,0.13)*fade; col=mix(col,ground,fade); }
    float2 targetWorld=float2(8.0,18.0); float2 rel=targetWorld-u.camera.xy; float forwardD=dot(rel,float2(sy,cy)); float sideD=dot(rel,float2(cy,-sy)); if(forwardD>1.0){ float sx=(sideD/forwardD)/(0.72*aspect); float syScreen=(-0.12/forwardD - pitch*0.72)/0.58; float2 d=float2(uv.x/aspect,uv.y)-float2(sx,syScreen); float marker=smoothstep(0.035,0.012,length(d)); col=mix(col,float3(0.94,0.29,0.12),marker); }
    float cross=(1.0-smoothstep(0.0025,0.0065,abs(uv.x)))*step(abs(uv.y),0.025) + (1.0-smoothstep(0.0025,0.0065,abs(uv.y)))*step(abs(uv.x),0.025); col=mix(col,float3(0.76,0.90,0.82),clamp(cross,0.0,1.0)*0.7);
    col += u.timing.w*float3(0.35,0.22,0.08)*smoothstep(0.9,0.0,length(uv-float2(0.28,-0.55)));
    return float4(col,1);
}
