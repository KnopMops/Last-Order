#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float uTime;
uniform float uInvert;
uniform float uDistortion;
uniform float uScanline;
uniform float uChroma;
uniform float uBrightness;
uniform float uSanityStress;

void main() {
    vec2 uv = fragTexCoord * 2.0 - 1.0;
    vec2 offset = uv.yx / 6.5;
    float distStrength = 0.12 * uDistortion + 0.04 * uSanityStress * sin(uTime * 2.7);
    uv = uv + uv * offset * offset * distStrength;
    uv = uv * 0.5 + 0.5;

    if (uSanityStress > 0.05) {
        float wave = sin(uv.y * 42.0 + uTime * 9.0) * 0.0025 * uSanityStress;
        uv.x += wave;
    }

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        finalColor = vec4(0.039, 0.051, 0.102, 1.0);
        return;
    }

    float edgeDist = length(fragTexCoord - vec2(0.5));
    float chromaBoost = uChroma + uSanityStress * 1.65 * (0.6 + 0.4 * sin(uTime * 4.2));
    float shift = 0.0038 * chromaBoost * edgeDist;
    float r = texture(texture0, vec2(uv.x + shift, uv.y)).r;
    float g = texture(texture0, uv).g;
    float b = texture(texture0, vec2(uv.x - shift, uv.y)).b;
    vec3 col = vec3(r, g, b);

    if (uInvert > 0.5) {
        col = vec3(1.0) - col;
    }

    float scanline = mod(floor(uv.y * 180.0), 2.0);
    col *= (1.0 - scanline * 0.18 * uScanline);

    float vhsBar = smoothstep(0.02, 0.0, abs(fract(uv.y - uTime * 0.07) - 0.5));
    col += vec3(0.025, 0.02, 0.018) * vhsBar * uScanline;

    float pulse = 0.5 + 0.5 * sin(uTime * (3.2 + uSanityStress * 4.0));
    float vigStrength = 0.45 + uSanityStress * (0.65 + 0.25 * pulse);
    float vig = 1.0 - edgeDist * edgeDist * vigStrength;
    col *= clamp(vig, 0.18, 1.0);

    if (uSanityStress > 0.15) {
        vec3 stressTint = vec3(0.66, 0.14, 0.14) * (edgeDist * uSanityStress * 0.28 * pulse);
        col = mix(col, col + stressTint, uSanityStress * 0.5);
    }

    col *= uBrightness;

    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0) * fragColor;
}
