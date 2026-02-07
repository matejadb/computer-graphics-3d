#version 330 core

// ========== PHONG LIGHTING STRUCTS (iz V10 vežbi) ==========
struct Light {
    vec3 pos;       // Pozicija svetla
    vec3 kA;        // Ambijentalna komponenta
    vec3 kD;        // Difuzna komponenta
    vec3 kS;        // Spekularna komponenta
};

struct Material {
    vec3 kA;        // Ambijentalna refleksija materijala
    vec3 kD;        // Difuzna refleksija materijala
    vec3 kS;        // Spekularna refleksija materijala
    float shine;    // Shininess (ugla?anost)
};

in vec4 channelCol;
in vec2 channelTex;
in vec3 channelNormal;
in vec3 channelFragPos;

out vec4 outCol;

uniform sampler2D uTex;
uniform bool useTex;
uniform bool transparent;

// Phong lighting uniforms (strukture)
uniform Light uLight;
uniform Material uMaterial;
uniform vec3 uViewPos;

// Custom uniforms
uniform int isInspector;
uniform vec3 uCustomColor;
uniform bool useCustomColor;

void main()
{
    vec3 norm = normalize(channelNormal);
    vec3 lightDir = normalize(uLight.pos - channelFragPos);
    
    // ========== PHONG LIGHTING MODEL (identi?no V10 vežbama) ==========
    
    // 1. AMBIENT - ambijentalna komponenta (osnovno osvetljenje)
    vec3 ambient = uLight.kA * uMaterial.kA;
    
    // 2. DIFFUSE - difuzna komponenta (direktno svetlo)
    float nD = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = uLight.kD * (nD * uMaterial.kD);
    
    // 3. SPECULAR - spekularna komponenta (odsjaj)
    vec3 viewDir = normalize(uViewPos - channelFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float s = pow(max(dot(viewDir, reflectDir), 0.0), uMaterial.shine);
    vec3 specular = uLight.kS * (s * uMaterial.kS);
    
    // Kombinuj sve komponente (Phong model)
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
