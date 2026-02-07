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
    
    // ========== PHONG LIGHTING MODEL ==========
    
    // 1. AMBIENT - osnovno osvetljenje (pove?ano za svetliju unutrašnjost)
    float ambientStrength = 0.35;  // Pove?ano sa 0.15 na 0.35 za svetliju unutrašnjost
    vec3 ambient = ambientStrength * uLightColor;
    
    // 2. DIFFUSE - difuzno rasejanje svetla (glavni izvor)
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor * 0.8;  // 0.8 faktor za prirodniji izgled
    
    // 3. SPECULAR - odsjaj (Phong refleksioni model)
    float specularStrength = 0.7;  // Pove?ano sa 0.5 na 0.7 za ja?i odsjaj
    vec3 viewDir = normalize(uViewPos - channelFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    
    // Shininess = 64 (srednja vrednost, daje lep odsjaj)
    // Ve?e vrednosti (128, 256) = sjajnije površine (metal, staklo)
    // Niže vrednosti (16, 32) = matnije površine (drvo, guma)
    float shininess = 64.0;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * uLightColor;
    
    // Kombinuj sve komponente
    vec3 lighting = ambient + diffuse + specular;
    
    if (!useTex) {
        vec3 color;
        
        // PRVO proveri useCustomColor - ignorise vertex boju potpuno
        if (useCustomColor) {
            color = uCustomColor;  // Koristi uniform boju (ignorise vertex boju)
        } else {
            color = channelCol.rgb;  // Koristi vertex boju samo ako useCustomColor je false
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
