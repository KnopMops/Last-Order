#pragma pack_matrix(row_major)

#ifndef DEBUG_VIEW
#define DEBUG_VIEW 0
#endif
#ifndef HD_DX12
#define HD_DX12 0
#endif
#ifndef HD_RT
#define HD_RT 0
#endif
#define MAX_LIGHTS 8
#define PI 3.14159265

cbuffer CBFrame : register(b0)
{
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gSpotViewProj;
    float4 gCamPos;
    float4 gCamFwd;
    float4 gScreen;
    float4 gProj;
    float4 gAmbientSky;
    float4 gAmbientGround;
    float4 gFog;
    float4 gSpotPos;
    float4 gSpotDir;
    float4 gSpotColor;
    float4 gSpotCones;
    float4 gLightPos[MAX_LIGHTS];
    float4 gLightColor[MAX_LIGHTS];
    float4 gCounts;
    float4 gGrade;
    float4 gFx;
    float4 gRt;
    float4x4 gPrevViewProj;
}

#if HD_DX12
cbuffer CBObject : register(b1)
{
    float4 gTint;
    float4 gMat;
    uint4 gIdx;
}
#else
cbuffer CBObject : register(b1)
{
    float4 gTint;
    float4 gMat;
}
#endif

cbuffer CBPost : register(b2)
{
    float4 gP0;
    float4 gP1;
}

#if HD_DX12
Texture2D gTexArray[] : register(t0, space1);
#define ALBEDO_WRAP(uv) gTexArray[gIdx.x].Sample(gLinear, uv)
#define ALBEDO_CLAMP(uv) gTexArray[gIdx.x].Sample(gClamp, uv)
float2 AlbedoDim()
{
    uint w, h;
    gTexArray[gIdx.x].GetDimensions(w, h);
    return float2(w, h);
}
#else
Texture2D gAlbedo : register(t0);
#define ALBEDO_WRAP(uv) gAlbedo.Sample(gLinear, uv)
#define ALBEDO_CLAMP(uv) gAlbedo.Sample(gClamp, uv)
float2 AlbedoDim()
{
    uint w, h;
    gAlbedo.GetDimensions(w, h);
    return float2(w, h);
}
#endif

Texture2D<float> gAO : register(t1);
Texture2D<float> gSpotShadow : register(t2);
TextureCubeArray<float> gCubeShadow : register(t3);
Texture2D<float> gDepthTex : register(t4);
Texture2D gNormalTex : register(t5);
Texture2D gSrc0 : register(t6);
Texture2D gSrc1 : register(t7);
Texture2D gSrc2 : register(t8);
Texture2D gSrc3 : register(t9);

#if HD_RT
RaytracingAccelerationStructure gScene : register(t10);
StructuredBuffer<float4> gInstColor : register(t11);
#endif

SamplerState gLinear : register(s0);
SamplerState gClamp : register(s1);

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
    float4 bake : TEXCOORD1;
};

struct VSOut
{
    float4 pos : SV_Position;
    float3 wpos : TEXCOORD0;
    float3 nrm : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 bake : TEXCOORD3;
};

struct FSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float Hash13(float3 p3)
{
    p3 = frac(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return frac((p3.x + p3.y) * p3.z);
}

float ValueNoise3(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = Hash13(i);
    float n100 = Hash13(i + float3(1, 0, 0));
    float n010 = Hash13(i + float3(0, 1, 0));
    float n110 = Hash13(i + float3(1, 1, 0));
    float n001 = Hash13(i + float3(0, 0, 1));
    float n101 = Hash13(i + float3(1, 0, 1));
    float n011 = Hash13(i + float3(0, 1, 1));
    float n111 = Hash13(i + float3(1, 1, 1));
    return lerp(lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y), lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y), f.z);
}

float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float LinearDepth(float d)
{
    return gProj.x * gProj.y / (gProj.y - d * (gProj.y - gProj.x));
}

float3 WorldFromDepth(float2 uv, float d)
{
    float4 clip = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, d, 1.0);
    float4 w = mul(clip, gInvViewProj);
    return w.xyz / w.w;
}

VSOut VSScene(VSIn i)
{
    VSOut o;
    o.wpos = i.pos;
    o.pos = mul(float4(i.pos, 1.0), gViewProj);
    o.nrm = i.nrm;
    o.uv = i.uv;
    o.bake = i.bake;
    return o;
}

float4 PSPrepass(VSOut i) : SV_Target
{
    return float4(normalize(i.nrm), 1.0);
}

float4 VSShadowPos(VSIn i, out float3 wpos : TEXCOORD0) : SV_Position
{
    wpos = i.pos;
    return mul(float4(i.pos, 1.0), gSpotViewProj);
}

float PSShadow(float4 pos : SV_Position, float3 wpos : TEXCOORD0) : SV_Target
{
    return length(wpos - gP0.xyz) * gP0.w;
}

float3 BumpNormal(float3 N, float3 P, float h, float fade)
{
    if (fade <= 0.001)
        return N;
    float3 dpdx = ddx(P);
    float3 dpdy = ddy(P);
    float dhdx = ddx(h);
    float dhdy = ddy(h);
    float3 r1 = cross(dpdy, N);
    float3 r2 = cross(N, dpdx);
    float det = dot(dpdx, r1);
    float3 grad = sign(det) * (dhdx * r1 + dhdy * r2) * fade;
    return normalize(abs(det) * N - grad);
}

float DistGGX(float NdH, float a)
{
    float a2 = a * a;
    float d = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-5);
}

float GeoSmith(float NdV, float NdL, float rough)
{
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float gv = NdV / (NdV * (1.0 - k) + k);
    float gl = NdL / (NdL * (1.0 - k) + k);
    return gv * gl;
}

float3 Brdf(float3 N, float3 V, float3 L, float3 radiance, float3 diffuseCol, float3 F0, float rough, float specOcc)
{
    float NdL = saturate(dot(N, L));
    if (NdL <= 0.0)
        return 0.0;
    float3 H = normalize(V + L);
    float NdV = max(dot(N, V), 1e-3);
    float NdH = saturate(dot(N, H));
    float VdH = saturate(dot(V, H));
    float a = max(rough * rough, 0.04);
    float3 F = F0 + (1.0 - F0) * pow(1.0 - VdH, 5.0);
    float3 spec = DistGGX(NdH, a) * GeoSmith(NdV, NdL, rough) * F / (4.0 * NdV * NdL + 1e-3);
    float3 kd = 1.0 - F;
    return (kd * diffuseCol / PI + spec * specOcc) * radiance * NdL;
}

float CubeShadow(int slot, float3 P, float3 lp, float radius, float3 N)
{
    float3 d = P + N * 0.04 - lp;
    float dist = length(d);
    float3 dir = d / max(dist, 1e-4);
    float3 t = normalize(cross(dir, abs(dir.y) < 0.9 ? float3(0, 1, 0) : float3(1, 0, 0)));
    float3 b = cross(dir, t);
    float bias = 0.05 + 0.02 * dist;
    float lit = 0.0;
    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        float ang = k * 1.5708 + 0.6;
        float3 o = (t * cos(ang) + b * sin(ang)) * (0.012 + 0.004 * dist);
        float stored = gCubeShadow.SampleLevel(gClamp, float4(dir + o, slot), 0) * radius;
        lit += (dist - bias <= stored) ? 1.0 : 0.0;
    }
    return lit * 0.25;
}

float SpotShadowAt(float3 P, float3 N, float normalBias)
{
    float4 ls = mul(float4(P + N * normalBias, 1.0), gSpotViewProj);
    if (ls.w <= 0.0)
        return 1.0;
    float2 uv = ls.xy / ls.w * float2(0.5, -0.5) + 0.5;
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0)
        return 1.0;
    float dist = length(P - gSpotPos.xyz);
    float bias = 0.06 + 0.012 * dist;
    float texel = gSpotCones.z;
    float lit = 0.0;
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float stored = gSpotShadow.SampleLevel(gClamp, uv + float2(x, y) * texel, 0) * gSpotPos.w;
            lit += (dist - bias <= stored) ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

float SlotOfLight(int i)
{
    return gLightColor[i].w;
}

#if HD_RT
float RtVisible(float3 origin, float3 dir, float tmax)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.02;
    ray.TMax = tmax;
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE> q;
    q.TraceRayInline(gScene, 0, 0xFF, ray);
    q.Proceed();
    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;
}

float RtSoftShadow(float3 P, float3 N, float3 lightPos, float lightRadius, float2 seed)
{
    float3 origin = P + N * 0.03;
    float3 d0 = lightPos - origin;
    float dl = length(d0);
    float3 L = d0 / dl;
    float3 t = normalize(cross(L, abs(L.y) < 0.9 ? float3(0, 1, 0) : float3(1, 0, 0)));
    float3 b = cross(L, t);
    float ign = frac(52.9829189 * frac(dot(seed, float2(0.06711056, 0.00583715))));
    float lit = 0.0;
    [unroll]
    for (int k = 0; k < 5; ++k)
    {
        float r = sqrt((k + 0.5) / 5.0) * lightRadius;
        float ang = k * 2.399963 + ign * 6.2831853;
        float3 p = lightPos + (t * cos(ang) + b * sin(ang)) * r;
        float3 d = p - origin;
        float len = length(d);
        lit += RtVisible(origin, d / len, len - 0.06);
    }
    return lit * 0.2;
}

float3 RtReflection(float3 P, float3 R)
{
    RayDesc ray;
    ray.Origin = P;
    ray.Direction = R;
    ray.TMin = 0.03;
    ray.TMax = 14.0;
    RayQuery<RAY_FLAG_FORCE_OPAQUE> q;
    q.TraceRayInline(gScene, 0, 0xFF, ray);
    q.Proceed();
    float3 sky = gFog.rgb + gAmbientSky.rgb;
    if (q.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
        return sky;
    float t = q.CommittedRayT();
    float3 hit = P + R * t;
    float4 inst = gInstColor[q.CommittedInstanceID()];
    float3 n = -R;
    float3 light = gAmbientSky.rgb * 1.5;
    int count = (int)gCounts.x;
    [loop]
    for (int li = 0; li < count; ++li)
    {
        float3 toL = gLightPos[li].xyz - hit;
        float dist = length(toL);
        float radius = gLightPos[li].w;
        if (dist >= radius)
            continue;
        float x = dist / radius;
        float win = saturate(1.0 - x * x * x * x);
        light += gLightColor[li].rgb * win * win / (dist * dist + 0.6) * saturate(dot(n, toL / dist) * 0.7 + 0.3) / PI;
    }
    float3 c = inst.rgb * (light + inst.a);
    float fog = 1.0 - exp(-gFog.w * t);
    return lerp(c, gFog.rgb, saturate(fog));
}
#endif

float4 PSScene(VSOut i) : SV_Target
{
    float4 tex = ALBEDO_WRAP(i.uv);
    if (gTint.a > 1.5)
    {
        float nz = ValueNoise3(i.wpos * 0.33);
        float4 t2 = ALBEDO_WRAP(i.uv * 0.61 + float2(0.37, 0.19));
        tex.rgb = lerp(tex.rgb, t2.rgb, 0.5 * smoothstep(0.25, 0.75, nz));
    }
    float3 albedo = tex.rgb * gTint.rgb;
    float rough = gMat.x;
    float metal = gMat.y;
    float3 N = normalize(i.nrm);

    float2 dim = AlbedoDim();
    float2 duv = max(abs(ddx(i.uv)), abs(ddy(i.uv))) * dim;
    float texelsPerPixel = max(duv.x, duv.y);
    float bumpFade = saturate(1.6 - texelsPerPixel * 1.6);
    float h = Luma(tex.rgb) * gMat.w;
    N = BumpNormal(N, i.wpos, h, bumpFade);
    float3 V = normalize(gCamPos.xyz - i.wpos);

    float2 suv = i.pos.xy * gScreen.zw;
    float ssao = gAO.SampleLevel(gClamp, suv, 0);
    float ao = i.bake.a * ssao;
    float specOcc = saturate(ao * 1.5);

    float3 diffuseCol = albedo * (1.0 - metal);
    float3 F0 = lerp(0.04, albedo, metal);

    float3 hemi = lerp(gAmbientGround.rgb, gAmbientSky.rgb, N.y * 0.5 + 0.5);
    float3 col = diffuseCol * (hemi + i.bake.rgb * gAmbientGround.w) * ao;

    int count = (int)gCounts.x;
    [loop]
    for (int li = 0; li < count; ++li)
    {
        float3 toL = gLightPos[li].xyz - i.wpos;
        float dist = length(toL);
        float radius = gLightPos[li].w;
        if (dist >= radius)
            continue;
        float3 L = toL / dist;
        float x = dist / radius;
        float win = saturate(1.0 - x * x * x * x);
        float att = win * win / (dist * dist + 1.6);
        att *= lerp(0.3, 1.0, smoothstep(-0.25, 0.55, L.y));
        att *= 1.6;
        float shadow = 1.0;
#if HD_RT
        if (gRt.x > 0.5)
            shadow = RtSoftShadow(i.wpos, N, gLightPos[li].xyz, 0.3, i.pos.xy + li * 11.0);
        else
#endif
        {
            int slot = (int)SlotOfLight(li);
            if (slot >= 0)
                shadow = CubeShadow(slot, i.wpos, gLightPos[li].xyz, radius, N);
        }
        float3 radiance = gLightColor[li].rgb * att * shadow;
        col += Brdf(N, V, L, radiance, diffuseCol, F0, rough, specOcc);
    }

    if (gSpotDir.w > 0.5)
    {
        float3 toL = gSpotPos.xyz - i.wpos;
        float dist = length(toL);
        float3 L = toL / dist;
        float theta = dot(-L, gSpotDir.xyz);
        float cone = smoothstep(gSpotCones.y, gSpotCones.x, theta);
        if (cone > 0.0 && dist < gSpotPos.w)
        {
            float range = 1.0 - smoothstep(gSpotPos.w * 0.55, gSpotPos.w, dist);
            float att = 1.0 / (1.0 + 0.3 * dist + 0.1 * dist * dist);
            float shadow = 1.0;
#if HD_RT
            if (gRt.x > 0.5)
                shadow = RtSoftShadow(i.wpos, N, gSpotPos.xyz, 0.06, i.pos.xy + 91.0);
            else
#endif
                shadow = SpotShadowAt(i.wpos, N, 0.03);
            col += Brdf(N, V, L, gSpotColor.rgb * cone * att * range * shadow, diffuseCol, F0, rough, specOcc);
        }
    }

#if HD_RT
    if (gRt.z > 0.5 && rough < 0.55)
    {
        float3 R = reflect(-V, N);
        float NdV = saturate(dot(N, V));
        float fres = 0.04 + 0.96 * pow(1.0 - NdV, 5.0);
        fres = lerp(fres, 1.0, metal * 0.7) * (1.0 - rough * 1.6);
        if (fres > 0.01)
            col += RtReflection(i.wpos + N * 0.03, R) * fres * lerp(1.0, albedo.r * 0.5 + 0.5, metal) * specOcc;
    }
#endif

#if DEBUG_VIEW == 1
    return float4(N * 0.5 + 0.5, 1.0);
#elif DEBUG_VIEW == 2
    return float4(col, 1.0);
#elif DEBUG_VIEW == 3
    return float4(albedo, 1.0);
#elif DEBUG_VIEW == 4
    return float4(ssao.xxx, 1.0);
#endif
    col += albedo * gMat.z;

    float dist = length(gCamPos.xyz - i.wpos);
    float fog = 1.0 - exp(-gFog.w * dist);
    col = lerp(col, gFog.rgb, saturate(fog));
    return float4(col, 1.0);
}

struct BBOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
    float3 wpos : TEXCOORD1;
};

BBOut VSBillboard(float3 pos : POSITION, float2 uv : TEXCOORD0, float4 col : COLOR0)
{
    BBOut o;
    o.pos = mul(float4(pos, 1.0), gViewProj);
    o.uv = uv;
    o.col = col;
    o.wpos = pos;
    return o;
}

float4 PSBillboard(BBOut i) : SV_Target
{
    float4 tex = ALBEDO_CLAMP(i.uv);
    float a = tex.a * i.col.a;
    float3 c = tex.rgb * i.col.rgb * gMat.z;
    return float4(c * a, a);
}

FSOut VSFull(uint id : SV_VertexID)
{
    FSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float PSSsao(FSOut i) : SV_Target
{
    float d = gDepthTex.SampleLevel(gClamp, i.uv, 0);
    if (d >= 0.99999)
        return 1.0;
    float3 P = WorldFromDepth(i.uv, d);
    float3 N = normalize(gNormalTex.SampleLevel(gClamp, i.uv, 0).xyz);
    float viewZ = LinearDepth(d);
    float rot = Hash12(i.pos.xy) * 6.2831853;
    float3 up = abs(N.y) < 0.95 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    float radius = 0.55;
    float occ = 0.0;
    [unroll]
    for (int k = 0; k < 12; ++k)
    {
        float u = (k + 0.5) / 12.0;
        float ang = k * 2.399963 + rot;
        float r = sqrt(u);
        float3 local = float3(r * cos(ang), r * sin(ang), sqrt(max(1.0 - u, 0.0)));
        float scale = lerp(0.15, 1.0, (float)k / 11.0);
        scale *= scale;
        scale = lerp(0.15, 1.0, scale);
        float3 sp = P + (T * local.x + B * local.y + N * local.z) * radius * scale;
        float4 clip = mul(float4(sp, 1.0), gViewProj);
        float2 suv = clip.xy / clip.w * float2(0.5, -0.5) + 0.5;
        float sd = gDepthTex.SampleLevel(gClamp, suv, 0);
        float sceneZ = LinearDepth(sd);
        float sampleZ = dot(sp - gCamPos.xyz, gCamFwd.xyz);
        float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(viewZ - sceneZ), 1e-3));
        occ += (sceneZ < sampleZ - 0.03 ? 1.0 : 0.0) * rangeCheck;
    }
    float ao = 1.0 - occ / 12.0;
    return pow(saturate(ao), 1.4);
}

#if HD_RT
float PSRtAo(FSOut i) : SV_Target
{
    float d = gDepthTex.SampleLevel(gClamp, i.uv, 0);
    if (d >= 0.99999)
        return 1.0;
    float3 P = WorldFromDepth(i.uv, d);
    float3 N = normalize(gNormalTex.SampleLevel(gClamp, i.uv, 0).xyz);
    float3 up = abs(N.y) < 0.95 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    float rot = frac(Hash12(i.pos.xy) + gCounts.y * 0.61803398) * 6.2831853;
    float vis = 0.0;
    const int RAYS = 6;
    [unroll]
    for (int k = 0; k < RAYS; ++k)
    {
        float u = (k + 0.5) / RAYS;
        float ang = k * 2.399963 + rot;
        float r = sqrt(u);
        float3 local = float3(r * cos(ang), r * sin(ang), sqrt(max(1.0 - u, 0.0)));
        float3 dir = normalize(T * local.x + B * local.y + N * local.z);
        vis += RtVisible(P + N * 0.03, dir, 1.4);
    }
    return pow(saturate(vis / RAYS), 1.25);
}
#endif

float PSBlurAo(FSOut i) : SV_Target
{
    float2 px = gScreen.zw * 2.0;
    float centerD = LinearDepth(gDepthTex.SampleLevel(gClamp, i.uv, 0));
    float sum = 0.0;
    float wsum = 0.0;
    [unroll]
    for (int x = -2; x <= 1; ++x)
    {
        [unroll]
        for (int y = -2; y <= 1; ++y)
        {
            float2 uv = i.uv + (float2(x, y) + 0.5) * px * 0.5;
            float dd = LinearDepth(gDepthTex.SampleLevel(gClamp, uv, 0));
            float w = exp(-abs(dd - centerD) * 4.0);
            sum += gSrc0.SampleLevel(gClamp, uv, 0).r * w;
            wsum += w;
        }
    }
    return sum / max(wsum, 1e-4);
}

float3 Karis(float3 c)
{
    return c / (1.0 + Luma(c));
}

float4 PSBloomPrefilter(FSOut i) : SV_Target
{
    float2 px = gP0.xy;
    float3 a = gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, -1), 0).rgb;
    float3 b = gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, -1), 0).rgb;
    float3 c = gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, 1), 0).rgb;
    float3 d = gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, 1), 0).rgb;
    float3 avg = (Karis(a) + Karis(b) + Karis(c) + Karis(d)) * 0.25;
    float luma = Luma(avg);
    float knee = gP0.w;
    float soft = saturate(luma - gP0.z + knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    float contrib = max(soft, luma - gP0.z) / max(luma, 1e-4);
    return float4(max(avg * contrib, 0.0), 1.0);
}

float4 PSBloomDown(FSOut i) : SV_Target
{
    float2 px = gP0.xy;
    float3 c = gSrc0.SampleLevel(gClamp, i.uv, 0).rgb * 0.25;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, -1), 0).rgb * 0.0625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, -1), 0).rgb * 0.0625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, 1), 0).rgb * 0.0625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, 1), 0).rgb * 0.0625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-2, 0), 0).rgb * 0.0625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(2, 0), 0).rgb * 0.0625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(0, -2), 0).rgb * 0.0625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(0, 2), 0).rgb * 0.0625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-2, -2), 0).rgb * 0.015625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(2, -2), 0).rgb * 0.015625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-2, 2), 0).rgb * 0.015625;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(2, 2), 0).rgb * 0.015625;
    return float4(c, 1.0);
}

float4 PSBloomUp(FSOut i) : SV_Target
{
    float2 px = gP0.xy;
    float3 c = gSrc0.SampleLevel(gClamp, i.uv, 0).rgb * 4.0;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, 0), 0).rgb * 2.0;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, 0), 0).rgb * 2.0;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(0, -1), 0).rgb * 2.0;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(0, 1), 0).rgb * 2.0;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, -1), 0).rgb;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, -1), 0).rgb;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, 1), 0).rgb;
    c += gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, 1), 0).rgb;
    return float4(c / 16.0 * gP0.z, 1.0);
}

float PSLum(FSOut i) : SV_Target
{
    float2 px = gScreen.zw * 0.5;
    float l = 0.0;
    l += Luma(gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, -1), 0).rgb);
    l += Luma(gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, -1), 0).rgb);
    l += Luma(gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, 1), 0).rgb);
    l += Luma(gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, 1), 0).rgb);
    return log2(max(l * 0.25, 1e-3));
}

float PSLumDown(FSOut i) : SV_Target
{
    float2 px = gP0.xy * 0.5;
    float s = gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, -1), 0).r;
    s += gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, -1), 0).r;
    s += gSrc0.SampleLevel(gClamp, i.uv + px * float2(-1, 1), 0).r;
    s += gSrc0.SampleLevel(gClamp, i.uv + px * float2(1, 1), 0).r;
    return s * 0.25;
}

float PSAdapt(FSOut i) : SV_Target
{
    float avg = exp2(gSrc0.SampleLevel(gClamp, float2(0.5, 0.5), gP0.y).r);
    float prev = gSrc1.SampleLevel(gClamp, float2(0.5, 0.5), 0).r;
    float target = clamp(0.06 / max(avg, 0.002), 0.8, 1.7);
    float speed = target > prev ? 0.7 : 2.0;
    return lerp(prev, target, 1.0 - exp(-gP0.x * speed));
}

float HG(float cosT, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosT, 1.5));
}

float4 PSVolumetric(FSOut i) : SV_Target
{
    float d = gDepthTex.SampleLevel(gClamp, i.uv, 0);
    float3 end = WorldFromDepth(i.uv, min(d, 0.99995));
    float3 start = gCamPos.xyz;
    float3 ray = end - start;
    float len = length(ray);
    float maxLen = 26.0;
    if (len > maxLen)
    {
        ray *= maxLen / len;
        len = maxLen;
    }
    float3 dir = ray / max(len, 1e-4);
    int steps = (int)gP0.y;
    float jitter = Hash12(i.pos.xy);
    float stepLen = len / steps;
    float3 acc = 0.0;
    int count = (int)gCounts.x;
    [loop]
    for (int s = 0; s < steps; ++s)
    {
        float3 X = start + dir * stepLen * (s + jitter);
        float dust = 0.55 + 0.95 * ValueNoise3(X * 1.9 + float3(0.0, gCamPos.w * 0.07, gCamPos.w * 0.03));
        float3 light = 0.0;

        if (gSpotDir.w > 0.5)
        {
            float3 toX = X - gSpotPos.xyz;
            float dist = length(toX);
            float3 lDir = toX / max(dist, 1e-4);
            float theta = dot(lDir, gSpotDir.xyz);
            float cone = smoothstep(gSpotCones.y, gSpotCones.x, theta);
            if (cone > 0.0 && dist < gSpotPos.w)
            {
                float att = 1.0 / (1.0 + 0.12 * dist + 0.05 * dist * dist);
                float sh = SpotShadowAt(X, 0.0, 0.0);
                float phase = HG(dot(dir, -lDir), 0.45) * PI;
                light += gSpotColor.rgb * cone * att * sh * phase * 0.0045;
            }
        }

        [loop]
        for (int li = 0; li < count; ++li)
        {
            int slot = (int)SlotOfLight(li);
            if (slot < 0)
                continue;
            float3 toX = X - gLightPos[li].xyz;
            float dist = length(toX);
            float radius = gLightPos[li].w;
            if (dist >= radius)
                continue;
            float x = dist / radius;
            float win = saturate(1.0 - x * x * x * x);
            float att = win * win / (dist * dist + 0.6);
            float sh = CubeShadow(slot, X, gLightPos[li].xyz, radius, 0.0);
            light += gLightColor[li].rgb * att * sh * 0.0017;
        }
        acc += light * dust * gP0.x * stepLen;
    }
    return float4(acc, 1.0);
}

float4 PSBlurVol(FSOut i) : SV_Target
{
    float2 px = gP0.xy;
    float3 c = 0.0;
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
            c += gSrc0.SampleLevel(gClamp, i.uv + float2(x, y) * px, 0).rgb;
    }
    return float4(c / 9.0, 1.0);
}

float3 ACES(float3 x)
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

float4 PSComposite(FSOut i) : SV_Target
{
    float2 uv = i.uv;
    float3 hdr = gSrc0.SampleLevel(gClamp, uv, 0).rgb;
    float3 vol = gSrc1.SampleLevel(gClamp, uv, 0).rgb;
    float3 bloom = gSrc2.SampleLevel(gClamp, uv, 0).rgb;
    float exposure = gSrc3.SampleLevel(gClamp, float2(0.5, 0.5), 0).r;
    float3 c = (hdr + vol + bloom * gCounts.w) * exposure * gCounts.z;
    float l = Luma(c);
    c = lerp(float3(l, l, l), c, gGrade.z);
    c *= lerp(float3(0.92, 1.0, 1.08), float3(1.0, 1.0, 1.0), smoothstep(0.0, 0.25, l));
    c = ACES(c);
    float2 q = uv - 0.5;
    c *= saturate(1.0 - dot(q, q) * gGrade.y * 2.0);
    float n = Hash12(i.pos.xy) - 0.5;
    c += n * gGrade.x;
    c = pow(saturate(c), 1.0 / 2.2);
    return float4(c, 1.0);
}

float3 SharpenAt(float2 uv, float3 c)
{
    float amt = gFx.x;
    if (amt <= 0.001)
        return c;
    float2 px = gScreen.zw;
    float3 avg = gSrc0.SampleLevel(gClamp, uv + float2(0, -px.y), 0).rgb + gSrc0.SampleLevel(gClamp, uv + float2(0, px.y), 0).rgb +
                 gSrc0.SampleLevel(gClamp, uv + float2(-px.x, 0), 0).rgb + gSrc0.SampleLevel(gClamp, uv + float2(px.x, 0), 0).rgb;
    avg *= 0.25;
    return saturate(c + (c - avg) * amt);
}

float4 PSFxaa(FSOut i) : SV_Target
{
    float2 px = gScreen.zw;
    float3 rgbNW = gSrc0.SampleLevel(gClamp, i.uv + float2(-1, -1) * px, 0).rgb;
    float3 rgbNE = gSrc0.SampleLevel(gClamp, i.uv + float2(1, -1) * px, 0).rgb;
    float3 rgbSW = gSrc0.SampleLevel(gClamp, i.uv + float2(-1, 1) * px, 0).rgb;
    float3 rgbSE = gSrc0.SampleLevel(gClamp, i.uv + float2(1, 1) * px, 0).rgb;
    float3 rgbM = gSrc0.SampleLevel(gClamp, i.uv, 0).rgb;
    float3 lw = float3(0.299, 0.587, 0.114);
    float lNW = dot(rgbNW, lw);
    float lNE = dot(rgbNE, lw);
    float lSW = dot(rgbSW, lw);
    float lSE = dot(rgbSE, lw);
    float lM = dot(rgbM, lw);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    float2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y = ((lNW + lSW) - (lNE + lSE));
    float dirReduce = max((lNW + lNE + lSW + lSE) * (0.25 * (1.0 / 8.0)), 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(float2(8.0, 8.0), max(float2(-8.0, -8.0), dir * rcpDirMin)) * px;
    float3 rgbA = 0.5 * (gSrc0.SampleLevel(gClamp, i.uv + dir * (1.0 / 3.0 - 0.5), 0).rgb +
                         gSrc0.SampleLevel(gClamp, i.uv + dir * (2.0 / 3.0 - 0.5), 0).rgb);
    float3 rgbB = rgbA * 0.5 + 0.25 * (gSrc0.SampleLevel(gClamp, i.uv + dir * -0.5, 0).rgb +
                                      gSrc0.SampleLevel(gClamp, i.uv + dir * 0.5, 0).rgb);
    float lB = dot(rgbB, lw);
    float3 outc = (lB < lMin || lB > lMax) ? rgbA : rgbB;
    return float4(SharpenAt(i.uv, outc), 1.0);
}

float4 PSCopy(FSOut i) : SV_Target
{
    return float4(SharpenAt(i.uv, gSrc0.SampleLevel(gClamp, i.uv, 0).rgb), 1.0);
}

float PSAoTemporal(FSOut i) : SV_Target
{
    float cur = gSrc0.SampleLevel(gClamp, i.uv, 0).r;
    float d = gDepthTex.SampleLevel(gClamp, i.uv, 0);
    if (d >= 0.99999)
        return 1.0;
    float3 P = WorldFromDepth(i.uv, d);
    float4 pc = mul(float4(P, 1.0), gPrevViewProj);
    float2 puv = pc.xy / pc.w * float2(0.5, -0.5) + 0.5;
    if (pc.w <= 0.0 || puv.x <= 0.0 || puv.x >= 1.0 || puv.y <= 0.0 || puv.y >= 1.0)
        return cur;
    float2 px = gScreen.zw * 2.0;
    float mn = 1.0;
    float mx = 0.0;
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float v = gSrc0.SampleLevel(gClamp, i.uv + float2(x, y) * px, 0).r;
            mn = min(mn, v);
            mx = max(mx, v);
        }
    }
    float hist = clamp(gSrc1.SampleLevel(gClamp, puv, 0).r, mn - 0.02, mx + 0.02);
    return lerp(cur, hist, 0.9);
}
