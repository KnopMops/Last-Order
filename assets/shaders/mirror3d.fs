#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform vec2 uScreenSize;
uniform vec3 uViewPos;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uTime;
uniform float uDistort;

out vec4 finalColor;

void main() {
    vec2 uv = gl_FragCoord.xy / uScreenSize;
    uv.x = 1.0 - uv.x;
    uv.x += sin(uv.y * 60.0 + uTime * 3.0) * 0.002 * uDistort;
    vec3 reflection = texture(texture0, uv).rgb;
    vec3 grime = texture(texture1, fragTexCoord).rgb;
    vec3 color = reflection * vec3(0.86, 0.9, 0.92) + grime * 0.035;
    float distToCam = length(uViewPos - fragPosition);
    float fog = clamp((distToCam - uFogStart) / max(0.001, uFogEnd - uFogStart), 0.0, 1.0);
    color = mix(color, uFogColor, fog);
    finalColor = vec4(color, 1.0);
}
