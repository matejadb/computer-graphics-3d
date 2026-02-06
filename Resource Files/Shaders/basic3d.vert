#version 330 core

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inCol;
layout(location = 2) in vec2 inTex;
layout(location = 3) in vec3 inNormal;

uniform mat4 uM;
uniform mat4 uV;
uniform mat4 uP;

out vec4 channelCol;
out vec2 channelTex;
out vec3 channelNormal;
out vec3 channelFragPos;

void main()
{
    gl_Position = uP * uV * uM * vec4(inPos, 1.0);
    channelCol = inCol;
    channelTex = inTex;
    channelNormal = mat3(transpose(inverse(uM))) * inNormal;
    channelFragPos = vec3(uM * vec4(inPos, 1.0));
}
