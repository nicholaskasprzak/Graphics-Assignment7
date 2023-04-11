#version 450                          
layout (location = 0) out vec4 FragColor;

in struct Vertex
{
    vec3 worldNormal;
    vec3 worldPosition;
    vec2 uv;
}vertexOutput;

in mat3 TBN;
in vec4 lightSpacePos;

struct Material
{
    vec3 color;
    float ambientK, diffuseK, specularK;
    float shininess;
    float normalIntensity;
};

struct Light
{
    vec3 color;
    float intensity;
};

struct DirectionalLight
{
    vec3 direction;
    Light light;
};

struct PointLight
{
    vec3 position;
    Light light;

    float constK, linearK, quadraticK;
};

struct SpotLight
{
    vec3 position;
    vec3 direction;
    Light light;

    float range;
    float innerAngle;
    float outerAngle;
    float angleFalloff;
};

uniform DirectionalLight _DirectionalLight;
uniform Material _Material;
uniform vec3 _CameraPosition;

uniform sampler2D _Texture1;
uniform sampler2D _Texture2;
uniform sampler2D _ShadowMap;
uniform sampler2D _Normal;

uniform float time;
uniform float _MinBias;
uniform float _MaxBias;

float calcAmbient(float ambientCoefficient)
{
    float ambientRet;

    ambientRet = ambientCoefficient;

    return ambientRet;
}

float calcDiffuse(float diffuseCoefficient, vec3 lightDirection, vec3 vertexNormal)
{
    float diffuseRet;

    float cosAngle = dot(normalize(lightDirection), normalize(vertexNormal));
    cosAngle = clamp(cosAngle, 0, cosAngle);

    diffuseRet = diffuseCoefficient * cosAngle;

    return diffuseRet;
};

float calcSpecular(float specularCoefficient, vec3 lightDirection, vec3 vertexPosition, vec3 vertexNormal, float shininess, vec3 cameraPosition)
{
    float specularRet;

    vec3 reflectDir = reflect(-lightDirection, vertexNormal);
    vec3 cameraDir = cameraPosition - vertexPosition;
    float cosAngle = dot(normalize(reflectDir), normalize(cameraDir));
    cosAngle = clamp(cosAngle, 0, cosAngle);

    specularRet = specularCoefficient * pow(cosAngle, shininess);

    return specularRet;
};

vec3 calcPhong(Vertex vertex, Material material, Light light, vec3 lightDirection, vec3 cameraPosition)
{
    vec3 phongRet;

    vec3 lightColor = light.intensity * light.color;

    float ambient = calcAmbient(material.ambientK);
    float diffuse = calcDiffuse(material.diffuseK, lightDirection, vertex.worldNormal);
    float specular = calcSpecular(material.specularK, lightDirection, vertex.worldPosition, vertex.worldNormal, material.shininess, cameraPosition);

    phongRet = (ambient + diffuse + specular) * lightColor;

    return phongRet;
}

float calcGLAttenuation(PointLight light, vec3 vertPos)
{
    float attenuation;
    float dist = distance(light.position, vertPos);

    attenuation = 1 / (light.constK + light.linearK + (light.quadraticK * dist));

    return attenuation;
}

float calcAngularAttenuation(SpotLight light, vec3 vertPos)
{
    float attenuation;

    vec3 dir = (vertPos - light.position) / length(vertPos - light.position);
    float cosAngle = dot(dir, normalize(-light.direction));

    float maxAngle = cos(radians(light.outerAngle));
    float minAngle = cos(radians(light.innerAngle));

    attenuation = (cosAngle - maxAngle) / (minAngle - maxAngle);
    attenuation = pow(attenuation, light.angleFalloff);
    attenuation = clamp(attenuation, 0, 1);

    return attenuation;
}

float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
    vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
    sampleCoord = sampleCoord * 0.5 + 0.5;

    float shadowMapDepth = texture(shadowMap, sampleCoord.xy).r;

    float minBias = _MinBias;//0.005f;
    float maxBias = _MaxBias;//0.015f;

    float bias = max(maxBias * (1.0f - dot(normal, lightDir)), minBias);
    float depth = sampleCoord.z - bias;

    float shadow = 0.0f;

    vec2 texelOffset = 1.0 / textureSize(shadowMap, 0);

    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
            shadow += step(texture(shadowMap, uv).r, depth);
        }
    }
    shadow /= 10f;

    return shadow;
}

void main(){ 
    vec3 normal = texture(_Normal, vertexOutput.uv).rgb;
    normal = (normal * 2.0f) - 1.0f;
    normal = normalize(normal * TBN);

    Vertex newVertex = vertexOutput;
    newVertex.worldNormal = normal;

    vec3 lightCol;
    float shadow = calcShadow(_ShadowMap, lightSpacePos, vertexOutput.worldNormal, _DirectionalLight.direction);

    lightCol += calcPhong(newVertex, _Material, _DirectionalLight.light, _DirectionalLight.direction, _CameraPosition) * (1.0 - shadow);

    vec2 modifiedUV = vertexOutput.uv;

    FragColor = texture(_Texture1, vertexOutput.uv) * vec4(lightCol * _Material.color, 1.0f);
}