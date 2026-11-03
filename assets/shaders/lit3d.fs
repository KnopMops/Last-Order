#version 330

#define MAX_LIGHTS 8

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec3 fragPowerLight;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 uViewPos;

uniform float uFlashIntensity;
uniform vec3 uFlashPos;
uniform vec3 uFlashDir;
uniform vec3 uFlashColor;
uniform float uFlashInner;
uniform float uFlashOuter;
uniform float uFlashRange;

uniform sampler2D uShadowMap;
uniform mat4 uFlashViewProj;
uniform int uShadowEnabled;
uniform float uShadowNear;
uniform float uShadowFar;
uniform float uShadowTexel;

uniform vec3 uLightPos[MAX_LIGHTS];
uniform vec3 uLightColor[MAX_LIGHTS];
uniform float uLightRadius[MAX_LIGHTS];
uniform int uLightCount;

uniform vec3 uAmbientSky;
uniform vec3 uAmbientGround;
uniform float uBakedScale;
uniform float uPowerLevel;
uniform vec3 uExtraLight;
uniform float uEmissive;

uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

uniform vec4 uClipPlane;
uniform int uClipEnabled;
uniform int uDither;

out vec4 finalColor;

float LinearDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * uShadowNear * uShadowFar) / (uShadowFar + uShadowNear - z * (uShadowFar - uShadowNear));
}

float FlashShadow(vec3 normal, vec3 lightDir) {
    if (uShadowEnabled == 0) return 1.0;
    vec4 ls = uFlashViewProj * vec4(fragPosition, 1.0);
    if (ls.w <= 0.0) return 1.0;
    vec3 proj = ls.xyz / ls.w * 0.5 + 0.5;
    if (proj.x <= 0.0 || proj.x >= 1.0 || proj.y <= 0.0 || proj.y >= 1.0) return 1.0;
    float current = LinearDepth(proj.z);
    float slope = 1.0 - max(dot(normal, lightDir), 0.0);
    float bias = 0.035 + 0.12 * slope + current * 0.004;
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closest = LinearDepth(texture(uShadowMap, proj.xy + vec2(x, y) * uShadowTexel).r);
            lit += (current - bias > closest) ? 0.0 : 1.0;
        }
    }
    return lit / 9.0;
}

float Bayer4(vec2 p) {
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    int idx = x + y * 4;
    float m[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
    return m[idx] / 16.0;
}

void main() {
    if (uClipEnabled == 1 && dot(vec4(fragPosition, 1.0), uClipPlane) < 0.0) discard;

    vec4 texColor = texture(texture0, fragTexCoord);
    vec4 baseColor = texColor * colDiffuse;
    if (baseColor.a < 0.1) discard;

    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing) normal = -normal;

    float ao = fragColor.a;
    float hemi = normal.y * 0.5 + 0.5;
    vec3 light = mix(uAmbientGround, uAmbientSky, hemi) * ao;
    light += fragColor.rgb * uBakedScale;
    light += fragPowerLight * uPowerLevel;
    light += uExtraLight;

    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i >= uLightCount) break;
        vec3 toLight = uLightPos[i] - fragPosition;
        float dist = length(toLight);
        float r = max(uLightRadius[i], 0.01);
        if (dist >= r) continue;
        vec3 l = toLight / max(dist, 0.0001);
        float att = 1.0 - (dist * dist) / (r * r);
        att *= att;
        float ndl = max(dot(normal, l), 0.0) * 0.85 + 0.15;
        light += uLightColor[i] * att * ndl;
    }

    if (uFlashIntensity > 0.001) {
        vec3 toLight = uFlashPos - fragPosition;
        float dist = length(toLight);
        vec3 l = toLight / max(dist, 0.0001);
        float theta = dot(-l, normalize(uFlashDir));
        float cone = smoothstep(uFlashOuter, uFlashInner, theta);
        if (cone > 0.0 && dist < uFlashRange) {
            float rangeFade = 1.0 - smoothstep(uFlashRange * 0.55, uFlashRange, dist);
            float att = 1.0 / (1.0 + 0.10 * dist + 0.045 * dist * dist);
            float ndl = max(dot(normal, l), 0.0);
            float hotspot = 1.0 + 0.6 * smoothstep(uFlashInner, 1.0, theta);
            vec3 viewDir = normalize(uViewPos - fragPosition);
            vec3 halfDir = normalize(l + viewDir);
            float spec = pow(max(dot(normal, halfDir), 0.0), 24.0) * 0.25;
            float shadow = FlashShadow(normal, l);
            light += uFlashColor * (ndl + spec) * cone * att * rangeFade * hotspot * shadow * uFlashIntensity * mix(1.0, ao, 0.5);
        }
    }

    vec3 color = mix(baseColor.rgb * light, baseColor.rgb, clamp(uEmissive, 0.0, 1.0));

    float distToCam = length(uViewPos - fragPosition);
    float fog = clamp((distToCam - uFogStart) / max(0.001, uFogEnd - uFogStart), 0.0, 1.0);
    fog = fog * fog * (3.0 - 2.0 * fog);
    color = mix(color, uFogColor, fog * (1.0 - uEmissive * 0.6));

    if (uDither == 1) {
        float b = Bayer4(gl_FragCoord.xy) - 0.5;
        color += b / 40.0;
        color = floor(color * 40.0 + 0.5) / 40.0;
    }

    finalColor = vec4(color, baseColor.a);
}
