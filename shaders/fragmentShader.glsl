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
    float ambientStrength = 0.1;
    float specularStrength = 0.5;
    float shininess = 32.0;
    
    // ---- LIGHT 1 ----
    vec3 lightDir1 = normalize(lightPos1 - FragPos);
    float diff1 = max(dot(norm, lightDir1), 0.0);
    vec3 reflectDir1 = reflect(-lightDir1, norm);
    float spec1 = pow(max(dot(viewDir, reflectDir1), 0.0), shininess);

    vec3 ambient1 = ambientStrength * lightColor1;
    vec3 diffuse1 = diff1 * lightColor1;
    vec3 specular1 = specularStrength * spec1 * lightColor1;

    // ---- LIGHT 2 ----
    vec3 lightDir2 = normalize(lightPos2 - FragPos);
    float diff2 = max(dot(norm, lightDir2), 0.0);
    vec3 reflectDir2 = reflect(-lightDir2, norm);
    float spec2 = pow(max(dot(viewDir, reflectDir2), 0.0), shininess);

    vec3 ambient2 = ambientStrength * lightColor2;
    vec3 diffuse2 = diff2 * lightColor2;
    vec3 specular2 = specularStrength * spec2 * lightColor2;

    vec3 lighting = (ambient1 + diffuse1 + specular1) + (ambient2 + diffuse2 + specular2);
    FragColor = vec4(lighting * objectColor, 1.0);
}