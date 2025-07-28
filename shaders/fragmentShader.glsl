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

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    float ambientStrength = 0.05;
    float specularStrength = 1.0;
    float shininess = 64.0;
    
    // ---- LIGHT 1 ----
    vec3 lightDir1 = normalize(lightPos1 - FragPos);
    vec3 halfDir1 = normalize(lightDir1 + viewDir);
    float spec1 = pow(max(dot(norm, halfDir1), 0.0), shininess);

    // ---- LIGHT 2 ----
    vec3 lightDir2 = normalize(lightPos2 - FragPos);
    vec3 halfDir2 = normalize(lightDir2 + viewDir);
    float spec2 = pow(max(dot(norm, halfDir2), 0.0), shininess);

    vec3 ambient = ambientStrength * (lightColor1 + lightColor2);
    vec3 specular          = specularStrength * (spec1 * lightColor1 + spec2 * lightColor2);
    float diff1            = max(dot(norm, lightDir1), 0.0);
    float diff2            = max(dot(norm, lightDir2), 0.0);
    float diffuseBoost     = 1.5;
    vec3 diffuse           = diffuseBoost * (diff1 * lightColor1 + diff2 * lightColor2);

    vec3 lightResult = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(lightResult, 1.0);
}