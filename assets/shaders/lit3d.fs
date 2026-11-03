#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 uViewPos;
uniform vec3 uFlashlightPos;
uniform vec3 uFlashlightDir;
uniform vec3 uFlashlightColor;
uniform float uFlashlightCutoff;
uniform float uFlashlightOuterCutoff;
uniform float uFlashlightIntensity;

uniform vec3 uPointLightPos[4];
uniform vec3 uPointLightColor[4];
uniform float uPointLightRadius[4];
uniform int uNumPointLights;

uniform vec3 uAmbientColor;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

out vec4 finalColor;

void main() {
    vec4 texColor = texture(texture0, fragTexCoord);
    vec4 baseColor = texColor * fragColor * colDiffuse;
    if (baseColor.a < 0.1) discard;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(uViewPos - fragPosition);

    vec3 totalLight = uAmbientColor;

    if (uFlashlightIntensity > 0.001) {
        vec3 lightDir = normalize(uFlashlightPos - fragPosition);
        float dist = length(uFlashlightPos - fragPosition);
        float theta = dot(lightDir, normalize(-uFlashlightDir));
        float epsilon = uFlashlightCutoff - uFlashlightOuterCutoff;
        float spotIntensity = clamp((theta - uFlashlightOuterCutoff) / max(0.0001, epsilon), 0.0, 1.0);

        float attenuation = 1.0 / (1.0 + 0.12 * dist + 0.035 * dist * dist);
        float diff = max(dot(normal, lightDir), 0.0);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);

        totalLight += (diff * uFlashlightColor + spec * 0.25 * uFlashlightColor) * spotIntensity * attenuation * uFlashlightIntensity;
    }

    for (int i = 0; i < uNumPointLights; ++i) {
        if (uPointLightRadius[i] <= 0.0) continue;
        float dist = length(uPointLightPos[i] - fragPosition);
        if (dist > uPointLightRadius[i]) continue;

        vec3 lightDir = normalize(uPointLightPos[i] - fragPosition);
        float diff = max(dot(normal, lightDir), 0.0);
        float atten = clamp(1.0 - (dist / uPointLightRadius[i]), 0.0, 1.0);
        atten *= atten;

        totalLight += diff * uPointLightColor[i] * atten;
    }

    vec3 litColor = baseColor.rgb * totalLight;

    float distToCam = length(uViewPos - fragPosition);
    float fogFactor = clamp((distToCam - uFogStart) / max(0.001, (uFogEnd - uFogStart)), 0.0, 1.0);
    vec3 finalRGB = mix(litColor, uFogColor, fogFactor);

    finalColor = vec4(finalRGB, baseColor.a);
}
