#version 330 core

in vec4 channelCol;
in vec2 channelTex;
in vec3 channelNormal;
in vec3 channelFragPos;

out vec4 outCol;

uniform sampler2D uTex;
uniform bool useTex;
uniform bool transparent;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uViewPos;
uniform int isInspector;
uniform vec3 uCustomColor;
uniform bool useCustomColor;

void main()
{
    vec3 norm = normalize(channelNormal);
    vec3 lightDir = normalize(uLightPos - channelFragPos);
    
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * uLightColor;
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;
    
    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(uViewPos - channelFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * uLightColor;
    
    vec3 lighting = ambient + diffuse + specular;
    
    if (!useTex) {
        vec3 color = channelCol.rgb;
        
        // Koristi custom boju ako je postavljena
        if (useCustomColor) {
            color = uCustomColor;
        }
        
        // Oboji crveno ako je inspektor (override)
        if (isInspector == 1) {
            color = mix(color, vec3(1.0, 0.0, 0.0), 0.5);
        }
        
        outCol = vec4(color * lighting, channelCol.a);
    }
    else {
        vec4 texColor = texture(uTex, channelTex);
        
        // Ako je transparent mode, koristi alpha kao što je
        // Ako NIJE transparent, tretiraj sve piksele kao neprozirne
        if (transparent) {
            outCol = vec4(texColor.rgb * lighting, texColor.a);
        } else {
            // Za non-transparent teksture, ignoriši alpha i prikaži punu boju
            outCol = vec4(texColor.rgb * lighting, 1.0);
        }
    }
}
