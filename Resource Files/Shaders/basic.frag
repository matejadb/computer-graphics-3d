#version 330 core

in vec2 chTex;
out vec4 outCol;

uniform sampler2D uTex;
uniform float uAlpha;
uniform int uUseColor;
uniform vec3 uColor;

void main()
{
	if (uUseColor == 1) {
		outCol = vec4(uColor, uAlpha);
	} else {
		vec4 texColor = texture(uTex, chTex);
		outCol = vec4(texColor.rgb, texColor.a * uAlpha);
	}
}