#version 330 core

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos1;
uniform vec3 lightColor1;

uniform vec3 lightPos2;
uniform vec3 lightColor2;

uniform vec3 viewPos;
uniform vec3 objectColor;

uniform sampler2D texture1;
uniform bool useLighting;
uniform bool useTexture;

void main()
{
    vec3 textureColor = useTexture ? texture(texture1, TexCoord).rgb : vec3(1.0); 

    if (!useLighting) {
        vec3 finalColor = useTexture ? textureColor : objectColor;
        FragColor = vec4(finalColor, 1.0);
        return;
    }

    // Lighting calculations
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    float ambientStrength = 0.05;
    float specularStrength = 1.0;
    float shininess = 64.0;

    // Light 1
    vec3 lightDir1 = normalize(lightPos1 - FragPos);
    vec3 halfDir1 = normalize(lightDir1 + viewDir);
    float spec1 = pow(max(dot(norm, halfDir1), 0.0), shininess);

    // Light 2
    vec3 lightDir2 = normalize(lightPos2 - FragPos);
    vec3 halfDir2 = normalize(lightDir2 + viewDir);
    float spec2 = pow(max(dot(norm, halfDir2), 0.0), shininess);

    float diff1 = max(dot(norm, lightDir1), 0.0);
    float diff2 = max(dot(norm, lightDir2), 0.0);

    vec3 ambient  = ambientStrength * (lightColor1 + lightColor2);
    vec3 diffuse  = 1.5 * (diff1 * lightColor1 + diff2 * lightColor2);
    vec3 specular = specularStrength * (spec1 * lightColor1 + spec2 * lightColor2);

    vec3 lighting = (ambient + diffuse + specular) * objectColor * textureColor;
    FragColor = vec4(lighting, 1.0);
}
