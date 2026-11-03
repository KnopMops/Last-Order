#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec3 uViewPos;

uniform vec3 uFlashlightPos;
uniform vec3 uFlashlightDir;
uniform vec3 uFlashlightColor;
uniform float uFlashlightCutoff;
uniform float uFlashlightOuterCutoff;
uniform float uFlashlightIntensity;

uniform vec3 uAmbientColor;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

out vec4 finalColor;

void main() {
    vec4 texColor = texture(texture0, fragTexCoord);
    vec4 baseColor = texColor * fragColor;
    if (baseColor.a < 0.05) discard;

    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing) normal = -normal;

    vec3 viewDir = normalize(uViewPos - fragPosition);

    vec3 ambient = uAmbientColor;

    vec3 spotLight = vec3(0.0);
    if (uFlashlightIntensity > 0.01) {
        vec3 toLight = uFlashlightPos - fragPosition;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 0.001);

        float theta = dot(lightDir, -normalize(uFlashlightDir));
        float epsilon = uFlashlightCutoff - uFlashlightOuterCutoff;
        float spotFactor = clamp((theta - uFlashlightOuterCutoff) / max(0.0001, epsilon), 0.0, 1.0);

        float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.022 * dist * dist);
        float diff = max(dot(normal, lightDir), 0.0);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

        spotLight = (diff * uFlashlightColor + spec * 0.35 * uFlashlightColor) * spotFactor * attenuation * uFlashlightIntensity;
    }

    vec3 litColor = baseColor.rgb * (ambient + spotLight);

    float distToCam = length(uViewPos - fragPosition);
    float fogFactor = clamp((distToCam - uFogStart) / max(0.001, (uFogEnd - uFogStart)), 0.0, 1.0);
    vec3 finalRGB = mix(litColor, uFogColor, fogFactor);

    finalColor = vec4(finalRGB, baseColor.a);
}
