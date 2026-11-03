#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexTangent;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform vec2 uSnapResolution;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec3 fragPowerLight;

void main() {
    vec4 world = matModel * vec4(vertexPosition, 1.0);
    fragPosition = world.xyz;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    fragPowerLight = vertexTangent.rgb;

    vec4 clip = mvp * vec4(vertexPosition, 1.0);
    if (uSnapResolution.x > 0.0 && clip.w > 0.0) {
        vec2 ndc = clip.xy / clip.w;
        ndc = floor(ndc * uSnapResolution + 0.5) / uSnapResolution;
        clip.xy = ndc * clip.w;
    }
    gl_Position = clip;
}
