#version 330 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in VS_OUT
{
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoord;
    vec4 FragPosLightSpace;
} fs_in;

struct MaterialInfo
{
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    vec3 emissive;
};

struct DirectionalLight
{
    vec3 direction;
    vec3 color;
    float intensity;
};

struct PointLight
{
    vec3 position;
    vec3 color;
    float intensity;
    float range;
};

uniform MaterialInfo uMaterial;
uniform DirectionalLight uDirectionalLight;
uniform PointLight uPointLight;
uniform vec3 uCameraPosition;
uniform sampler2D uShadowMap;

const float PI = 3.14159265359;

float DistributionGGX(vec3 n, vec3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(n, h), 0.0);
    float nDotH2 = nDotH * nDotH;
    float denominator = (nDotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denominator * denominator, 0.0001);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotV / max(nDotV * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(vec3 n, vec3 v, vec3 l, float roughness)
{
    float ggx2 = GeometrySchlickGGX(max(dot(n, v), 0.0), roughness);
    float ggx1 = GeometrySchlickGGX(max(dot(n, l), 0.0), roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDirection)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
    {
        return 0.0;
    }

    float bias = max(0.0035 * (1.0 - dot(normal, lightDirection)), 0.0008);
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow = 0.0;

    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > closestDepth ? 1.0 : 0.0;
        }
    }

    return shadow / 9.0;
}

vec3 EvaluateLight(vec3 normal, vec3 viewDirection, vec3 lightDirection, vec3 radiance)
{
    float roughness = clamp(uMaterial.roughness, 0.05, 1.0);
    vec3 halfway = normalize(viewDirection + lightDirection);
    vec3 f0 = mix(vec3(0.04), uMaterial.albedo, uMaterial.metallic);

    float ndf = DistributionGGX(normal, halfway, roughness);
    float geometry = GeometrySmith(normal, viewDirection, lightDirection, roughness);
    vec3 fresnel = FresnelSchlick(max(dot(halfway, viewDirection), 0.0), f0);

    vec3 numerator = ndf * geometry * fresnel;
    float denominator = 4.0 * max(dot(normal, viewDirection), 0.0) * max(dot(normal, lightDirection), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = fresnel;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - uMaterial.metallic);
    float nDotL = max(dot(normal, lightDirection), 0.0);

    return (kD * uMaterial.albedo / PI + specular) * radiance * nDotL;
}

void main()
{
    vec3 normal = normalize(fs_in.Normal);
    vec3 viewDirection = normalize(uCameraPosition - fs_in.WorldPos);

    vec3 lightDirection = normalize(-uDirectionalLight.direction);
    float shadow = ShadowCalculation(fs_in.FragPosLightSpace, normal, lightDirection);
    vec3 directionalRadiance = uDirectionalLight.color * uDirectionalLight.intensity;

    vec3 color = (1.0 - shadow) * EvaluateLight(normal, viewDirection, lightDirection, directionalRadiance);

    vec3 toPointLight = uPointLight.position - fs_in.WorldPos;
    float pointDistance = length(toPointLight);
    if (pointDistance < uPointLight.range)
    {
        vec3 pointLightDirection = normalize(toPointLight);
        float normalizedDistance = pointDistance / uPointLight.range;
        float falloff = clamp(1.0 - normalizedDistance * normalizedDistance, 0.0, 1.0);
        float attenuation = (falloff * falloff) / (1.0 + pointDistance * pointDistance);
        vec3 pointRadiance = uPointLight.color * uPointLight.intensity * attenuation;
        color += EvaluateLight(normal, viewDirection, pointLightDirection, pointRadiance);
    }

    color += uMaterial.albedo * 0.03 * uMaterial.ao * (1.0 - uMaterial.metallic);
    color += uMaterial.emissive;
    color = max(color, vec3(0.0));

    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 brightValue = brightness > 1.0 ? color : uMaterial.emissive;

    FragColor = vec4(color, 1.0);
    BrightColor = vec4(brightValue, 1.0);
}
