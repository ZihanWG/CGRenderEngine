#version 330 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;
layout (location = 2) out vec4 AlbedoColor;
layout (location = 3) out vec4 NormalColor;
layout (location = 4) out vec4 MaterialColor;

in VS_OUT
{
    vec3 WorldPos;
    vec3 Normal;
    vec3 WorldTangent;
    float TangentSign;
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
    float normalScale;
    float occlusionStrength;
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
uniform sampler2D uBaseColorTexture;
uniform sampler2D uMetallicRoughnessTexture;
uniform sampler2D uNormalTexture;
uniform sampler2D uOcclusionTexture;
uniform sampler2D uEmissiveTexture;
uniform sampler2D uEnvironmentMap;
uniform sampler2D uBrdfLut;
uniform int uEnableShadows;
uniform int uEnableIBL;
uniform int uHasBaseColorTexture;
uniform int uHasMetallicRoughnessTexture;
uniform int uHasNormalTexture;
uniform int uHasOcclusionTexture;
uniform int uHasEmissiveTexture;
uniform float uEnvironmentIntensity;
uniform float uEnvironmentMaxLod;

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

vec2 DirectionToLatLong(vec3 direction)
{
    vec3 dir = normalize(direction);
    float phi = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * PI) + 0.5, theta / PI);
}

vec3 SampleEnvironment(vec3 direction, float lod)
{
    float mipLevel = clamp(lod, 0.0, uEnvironmentMaxLod);
    return textureLod(uEnvironmentMap, DirectionToLatLong(direction), mipLevel).rgb * uEnvironmentIntensity;
}

vec3 SampleBaseColor()
{
    vec3 baseColor = uMaterial.albedo;
    if (uHasBaseColorTexture == 1)
    {
        baseColor *= texture(uBaseColorTexture, fs_in.TexCoord).rgb;
    }

    return max(baseColor, vec3(0.0));
}

vec2 SampleMetallicRoughness()
{
    float metallic = uMaterial.metallic;
    float roughness = uMaterial.roughness;

    if (uHasMetallicRoughnessTexture == 1)
    {
        vec4 metallicRoughness = texture(uMetallicRoughnessTexture, fs_in.TexCoord);
        roughness *= metallicRoughness.g;
        metallic *= metallicRoughness.b;
    }

    return vec2(clamp(metallic, 0.0, 1.0), clamp(roughness, 0.05, 1.0));
}

float SampleAmbientOcclusion()
{
    float occlusion = uMaterial.ao;
    if (uHasOcclusionTexture == 1)
    {
        float textureOcclusion = texture(uOcclusionTexture, fs_in.TexCoord).r;
        occlusion *= mix(1.0, textureOcclusion, clamp(uMaterial.occlusionStrength, 0.0, 1.0));
    }

    return clamp(occlusion, 0.0, 1.0);
}

vec3 SampleEmissive()
{
    vec3 emissive = uMaterial.emissive;
    if (uHasEmissiveTexture == 1)
    {
        emissive *= texture(uEmissiveTexture, fs_in.TexCoord).rgb;
    }

    return max(emissive, vec3(0.0));
}

vec3 GetShadingNormal()
{
    vec3 normal = normalize(fs_in.Normal);
    if (uHasNormalTexture == 0)
    {
        return normal;
    }

    vec3 tangent = fs_in.WorldTangent;
    vec3 bitangent = vec3(0.0);

    if (length(tangent) > 1e-6 && abs(fs_in.TangentSign) > 0.5)
    {
        tangent = normalize(tangent - normal * dot(normal, tangent));
        bitangent = normalize(cross(normal, tangent)) * sign(fs_in.TangentSign);
    }
    else
    {
        vec3 q1 = dFdx(fs_in.WorldPos);
        vec3 q2 = dFdy(fs_in.WorldPos);
        vec2 st1 = dFdx(fs_in.TexCoord);
        vec2 st2 = dFdy(fs_in.TexCoord);

        tangent = q1 * st2.y - q2 * st1.y;
        bitangent = -q1 * st2.x + q2 * st1.x;

        if (length(tangent) < 1e-6 || length(bitangent) < 1e-6)
        {
            return normal;
        }

        tangent = normalize(tangent - normal * dot(normal, tangent));
        bitangent = normalize(bitangent - normal * dot(normal, bitangent));

        if (dot(cross(tangent, bitangent), normal) < 0.0)
        {
            bitangent = -bitangent;
        }

        if (length(tangent) < 1e-6 || length(bitangent) < 1e-6)
        {
            return normal;
        }
    }

    vec3 tangentNormal = texture(uNormalTexture, fs_in.TexCoord).xyz * 2.0 - 1.0;
    tangentNormal.xy *= uMaterial.normalScale;

    mat3 tbn = mat3(tangent, bitangent, normal);
    return normalize(tbn * tangentNormal);
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

vec3 EvaluateLight(
    vec3 normal,
    vec3 viewDirection,
    vec3 lightDirection,
    vec3 radiance,
    vec3 materialAlbedo,
    float materialMetallic,
    float materialRoughness
)
{
    vec3 halfway = normalize(viewDirection + lightDirection);
    vec3 f0 = mix(vec3(0.04), materialAlbedo, materialMetallic);

    float ndf = DistributionGGX(normal, halfway, materialRoughness);
    float geometry = GeometrySmith(normal, viewDirection, lightDirection, materialRoughness);
    vec3 fresnel = FresnelSchlick(max(dot(halfway, viewDirection), 0.0), f0);

    vec3 numerator = ndf * geometry * fresnel;
    float denominator = 4.0 * max(dot(normal, viewDirection), 0.0) * max(dot(normal, lightDirection), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = fresnel;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - materialMetallic);
    float nDotL = max(dot(normal, lightDirection), 0.0);

    return (kD * materialAlbedo / PI + specular) * radiance * nDotL;
}

vec3 EvaluateIBL(
    vec3 normal,
    vec3 viewDirection,
    vec3 materialAlbedo,
    float materialMetallic,
    float materialRoughness,
    float ambientOcclusion
)
{
    vec3 reflected = reflect(-viewDirection, normal);
    float nDotV = max(dot(normal, viewDirection), 0.0);
    vec3 f0 = mix(vec3(0.04), materialAlbedo, materialMetallic);
    vec3 fresnel = FresnelSchlick(nDotV, f0);
    vec3 kS = fresnel;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - materialMetallic);

    vec3 diffuseEnvironment = SampleEnvironment(normal, max(uEnvironmentMaxLod - 3.0, 0.0));
    vec3 prefilteredEnvironment = SampleEnvironment(reflected, materialRoughness * uEnvironmentMaxLod);
    vec2 envBrdf = texture(uBrdfLut, vec2(nDotV, materialRoughness)).rg;
    vec3 diffuse = diffuseEnvironment * materialAlbedo;
    vec3 specular = prefilteredEnvironment * (fresnel * envBrdf.x + envBrdf.y);

    return (kD * diffuse + specular) * ambientOcclusion;
}

void main()
{
    vec3 normal = GetShadingNormal();
    vec3 viewDirection = normalize(uCameraPosition - fs_in.WorldPos);
    vec3 materialAlbedo = SampleBaseColor();
    vec2 metallicRoughness = SampleMetallicRoughness();
    float materialMetallic = metallicRoughness.x;
    float materialRoughness = metallicRoughness.y;
    float ambientOcclusion = SampleAmbientOcclusion();
    vec3 emissive = SampleEmissive();

    vec3 lightDirection = normalize(-uDirectionalLight.direction);
    float shadow = uEnableShadows == 1
        ? ShadowCalculation(fs_in.FragPosLightSpace, normal, lightDirection)
        : 0.0;
    vec3 directionalRadiance = uDirectionalLight.color * uDirectionalLight.intensity;

    vec3 color = (1.0 - shadow) * EvaluateLight(
        normal,
        viewDirection,
        lightDirection,
        directionalRadiance,
        materialAlbedo,
        materialMetallic,
        materialRoughness
    );

    vec3 toPointLight = uPointLight.position - fs_in.WorldPos;
    float pointDistance = length(toPointLight);
    if (pointDistance < uPointLight.range)
    {
        vec3 pointLightDirection = normalize(toPointLight);
        float normalizedDistance = pointDistance / uPointLight.range;
        float falloff = clamp(1.0 - normalizedDistance * normalizedDistance, 0.0, 1.0);
        float attenuation = (falloff * falloff) / (1.0 + pointDistance * pointDistance);
        vec3 pointRadiance = uPointLight.color * uPointLight.intensity * attenuation;
        color += EvaluateLight(
            normal,
            viewDirection,
            pointLightDirection,
            pointRadiance,
            materialAlbedo,
            materialMetallic,
            materialRoughness
        );
    }

    if (uEnableIBL == 1)
    {
        color += EvaluateIBL(
            normal,
            viewDirection,
            materialAlbedo,
            materialMetallic,
            materialRoughness,
            ambientOcclusion
        );
    }
    else
    {
        color += materialAlbedo * 0.03 * ambientOcclusion * (1.0 - materialMetallic);
    }
    color += emissive;
    color = max(color, vec3(0.0));

    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 brightValue = brightness > 1.0 ? color : emissive;

    FragColor = vec4(color, 1.0);
    BrightColor = vec4(brightValue, 1.0);
    AlbedoColor = vec4(materialAlbedo, 1.0);
    NormalColor = vec4(normal * 0.5 + 0.5, 1.0);
    MaterialColor = vec4(materialMetallic, materialRoughness, ambientOcclusion, 1.0);
}
