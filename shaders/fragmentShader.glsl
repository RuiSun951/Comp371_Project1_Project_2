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

// for shadows
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;
uniform samplerCube shadowCube2;
uniform float farPlane2;

float shadowFactor1(vec3 fragPos, vec3 norm) {
    vec4 lpos = lightSpaceMatrix * vec4(fragPos, 1.0);
    vec3 proj = lpos.xyz / lpos.w; proj = proj * 0.5 + 0.5;
    if (proj.x<0.0||proj.x>1.0||proj.y<0.0||proj.y>1.0||proj.z>1.0) return 0.0;
    float bias = max(0.001, 0.005 * (1.0 - dot(norm, normalize(lightPos1 - fragPos))));
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float s=0.0; for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y){
        float p = texture(shadowMap, proj.xy + vec2(x,y)*texel).r;
        s += (proj.z - bias > p) ? 1.0 : 0.0;
    }
    return s/9.0;
}

float shadowFactor2(vec3 lightPos, vec3 fragPos, vec3 N) {
    vec3 fragToLight = fragPos - lightPos;
    float current = length(fragToLight);

    if (current >= farPlane2) return 0.0;

    vec3 dir = normalize(fragToLight);
    float ndotl = max(dot(N, -dir), 0.0);
    float bias = max(0.002, 0.006 * (1.0 - ndotl));

    float shadow = 0.0;
    int samples = 20;
    float diskRadius = 0.05 * (current / farPlane2);
    diskRadius = min(diskRadius, 0.1);

    float currentNDC = current / farPlane2;        // normalize current depth

    for (int i = 0; i < samples; ++i) {
        float fi = float(i);
        vec3 offset = normalize(vec3(cos(fi*3.11), sin(fi*2.66), cos(fi*1.37)));
        vec3 sampleDir = normalize(dir + offset * diskRadius);

        float closestNDC = texture(shadowCube2, sampleDir).r;  // already [0,1]
        shadow += (currentNDC - bias / farPlane2 > closestNDC) ? 1.0 : 0.0;
    }
    return shadow / float(samples);
}

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

    float shadow1 = shadowFactor1(FragPos, norm);
    float shadow2 = shadowFactor2(lightPos2, FragPos, norm);


    // before adding shadows
    vec3 L1 = 1.5 * diff1 * lightColor1 + specularStrength * spec1 * lightColor1;
    vec3 L2 = 1.5 * diff2 * lightColor2 + specularStrength * spec2 * lightColor2;

    // add shadow
    vec3 lighting = ambient + (1.0 - shadow1) * L1 + (1.0 - shadow2) * L2;
    lighting *= objectColor * textureColor;
    FragColor = vec4(lighting, 1.0);
}
