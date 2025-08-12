#version 330 core

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

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

// for shadows
uniform sampler2D shadowMap;
uniform samplerCube shadowCube2;
uniform float farPlane2;

uniform bool receiveShadows;

float shadowFactor1(vec3 fragPos, vec3 norm) {
    vec3 proj = FragPosLightSpace.xyz / FragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 0.0;

    vec3 L = normalize(lightPos1 - fragPos);
    float bias = max(0.001, 0.005 * (1.0 - dot(norm, L)));

    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float s = 0.0;
    for (int x=-1; x<=1; ++x)
        for (int y=-1; y<=1; ++y) {
            float closest = texture(shadowMap, proj.xy + vec2(x,y) * texel).r;
            s += (proj.z - bias > closest) ? 1.0 : 0.0;
        }
    return s / 9.0;
}

float shadowFactor2(vec3 lightPos, vec3 fragPos, vec3 N)
{
    vec3 fragToLight = fragPos - lightPos;
    float dist = length(fragToLight);
    if (dist >= farPlane2) return 0.0;

    float current = dist / farPlane2;

    float ndotl = max(dot(N, normalize(-fragToLight)), 0.0);
    float bias = max(0.002, 0.006 * (1.0 - ndotl));

    // small PCF around the actual vector direction
    float shadow = 0.0;
    int samples = 12;
    float diskRadius = 0.03 * (dist / farPlane2);

    for (int i = 0; i < samples; ++i) {
        // simple quasi-uniform offsets
        float a = 6.2831853 * (i / float(samples));
        vec3 offset = vec3(cos(a), sin(a), cos(a * 0.7));
        float closest = texture(shadowCube2, fragToLight + offset * diskRadius).r;
        shadow += (current - bias > closest) ? 1.0 : 0.0;
    }
    return shadow / float(samples);
}

void main()
{
    vec3 texCol = useTexture ? texture(texture1, TexCoord).rgb : vec3(1.0);

    if (!useLighting) {
        vec3 finalColor = useTexture ? texCol : objectColor;
        FragColor = vec4(finalColor, 1.0);
        return;
    }

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    float ambientStrength = 0.05;
    float specularStrength = 1.0;
    float shininess = 64.0;

    // Light 1
    vec3 L1 = normalize(lightPos1 - FragPos);
    vec3 H1 = normalize(L1 + V);
    float diff1 = max(dot(N, L1), 0.0);
    float spec1 = pow(max(dot(N, H1), 0.0), shininess);

    // Light 2
    vec3 L2 = normalize(lightPos2 - FragPos);
    vec3 H2 = normalize(L2 + V);
    float diff2 = max(dot(N, L2), 0.0);
    float spec2 = pow(max(dot(N, H2), 0.0), shininess);

    // Shadows
    float sh1 = receiveShadows ? shadowFactor1(FragPos, N) : 0.0;
    float sh2 = receiveShadows ? shadowFactor2(lightPos2, FragPos, N) : 0.0;

    vec3 ambient = ambientStrength * (lightColor1 + lightColor2);
    vec3 c1 = (1.0 - sh1) * (1.5 * diff1 * lightColor1 + specularStrength * spec1 * lightColor1);
    vec3 c2 = (1.0 - sh2) * (1.5 * diff2 * lightColor2 + specularStrength * spec2 * lightColor2);

    vec3 lighting = (ambient + c1 + c2) * (objectColor * texCol);
    FragColor = vec4(lighting, 1.0);
}
