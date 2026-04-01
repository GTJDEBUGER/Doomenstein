//------------------------------------------------------------------------------------------------
struct vs_input_t
{
    float3 modelPosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float3 modelTangent : TANGENT;
    float3 modelBitangent : BITANGENT;
    float3 modelNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
    float4 worldPosition : WORLDPOSITION;
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float4 worldTangent : TANGENT;
    float4 worldBitangent : BITANGENT;
    float4 worldNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct p_out
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
};

//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b1)
{
    float3 SunDirection;
    float SunIntensity;
    float AmbientIntensity;
    float4x4 SunViewProjMatrix;
};

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
    float3 CameraWorldPosition;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
    float4x4 ModelToWorldTransform;
    float4 ModelColor;
};

//------------------------------------------------------------------------------------------------
Texture2D diffuseTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D aoTexture : register(t2);
Texture2D displacementTexture : register(t3);
Texture2D roughnessTexture : register(t4);
Texture2D metallicTexture : register(t5);
Texture2D shadowMapTexture : register(t6);

//------------------------------------------------------------------------------------------------
SamplerState pointSampler : register(s0);
SamplerState anisoSampler : register(s1);
SamplerComparisonState shadowMapSampler : register(s2);

//------------------------------------------------------------------------------------------------
static const float2 PoissonDisk[32] =
{
    float2(-0.613392, 0.617481), float2(0.170019, -0.040254),
    float2(-0.299417, 0.791925), float2(0.645680, 0.493210),
    float2(-0.651784, -0.714526), float2(0.421346, 0.366170),
    float2(-0.247105, -0.928423), float2(-0.054923, 0.296984),
    float2(-0.971525, -0.131598), float2(0.934009, -0.005362),
    float2(-0.469429, -0.409139), float2(-0.733896, 0.212753),
    float2(0.854958, -0.446563), float2(0.528849, -0.471003),
    float2(-0.150991, 0.621332), float2(0.338152, -0.932601),
    float2(0.174820, 0.974050), float2(0.544934, 0.801111),
    float2(-0.357750, -0.009632), float2(0.106403, 0.621215),
    float2(0.117770, -0.355299), float2(-0.487472, 0.283303),
    float2(0.491502, -0.073147), float2(-0.841098, 0.548071),
    float2(-0.144536, -0.420659), float2(0.791925, 0.178817),
    float2(-0.814494, -0.347094), float2(0.218840, -0.713513),
    float2(0.588394, -0.211532), float2(-0.117965, 0.100866),
    float2(0.083754, -0.137378), float2(0.444026, 0.134181)
};

static const float LIGHT_SIZE = 0.04;
static const float NEAR_PLANE = -0.001;

float FindBlockerDistance(float2 uv, float zReceiver, float searchRadius)
{
    float avgBlockerDepth = 0.0;
    int blockers = 0;

    [unroll]
    for (int i = 0; i < 16; i++)
    {
        float2 sampleUV = uv + PoissonDisk[i] * searchRadius;
        float shadowMapDepth = shadowMapTexture.SampleLevel(pointSampler, sampleUV, 0).r;
        
        if (shadowMapDepth < zReceiver)
        {
            avgBlockerDepth += shadowMapDepth;
            blockers++;
        }
    }

    if (blockers == 0)
        return -1.0;
    return avgBlockerDepth / (float) blockers;
}

float CalculateShadow(float4 lightSpacePos, float depthBias)
{
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = -projCoords.y * 0.5 + 0.5;

    if (projCoords.x < 0 || projCoords.x > 1 || projCoords.y < 0 || projCoords.y > 1)
        return 1.0;

    float zReceiver = projCoords.z - depthBias;
    
    float searchRadius = 0.005;
    float avgBlockerDepth = FindBlockerDistance(projCoords.xy, zReceiver, searchRadius);

    if (avgBlockerDepth == -1.0)
        return 1.0;
    
    float penumbra = ((zReceiver - avgBlockerDepth) * LIGHT_SIZE) / avgBlockerDepth;
    
    float spread = clamp(penumbra, 0.0, 0.01);

    if (zReceiver - avgBlockerDepth < 0.001)
    {
        spread = 0.0;
    }
    
    float shadow = 0.0;
    [unroll]
    for (int i = 0; i < 32; i++)
    {
        float2 sampleUV = projCoords.xy + PoissonDisk[i] * spread;
        shadow += shadowMapTexture.SampleCmp(shadowMapSampler, sampleUV, zReceiver);
    }
    
    return shadow / 32.0;
}

//------------------------------------------------------------------------------------------------
float3 GetProceduralAmbient(float3 worldNormal, float3 sunDir)
{
    float dayFactor = smoothstep(-0.1, 0.2, sunDir.z);
    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 3.0));
    
    float3 zenithColor = float3(0.05, 0.2, 0.6);
    float3 horizonColorDay = float3(0.6, 0.7, 0.85);
    float3 sunsetColor = float3(1.0, 0.4, 0.1);
    float3 nightSky = float3(0.02, 0.05, 0.2);
    
    float viewHeight = saturate(worldNormal.z);
    float3 currentHorizon = lerp(horizonColorDay, sunsetColor, sunsetFactor);
    float3 daySky = lerp(currentHorizon, zenithColor, pow(viewHeight, 0.8));
    
    return lerp(nightSky, daySky, dayFactor) * AmbientIntensity;
}

//------------------------------------------------------------------------------------------------
// PBR Functions
//------------------------------------------------------------------------------------------------
static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    return a2 / (PI * pow(NdotH2 * (a2 - 1.0) + 1.0, 2.0));
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    float4 modelPosition = float4(input.modelPosition.xyz, 1.0f);
    float4 worldPosition = mul(ModelToWorldTransform, modelPosition);
    float4 cameraPosition = mul(WorldToCameraTransform, worldPosition);
    float4 renderPosition = mul(CameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(RenderToClipTransform, renderPosition);
    
    float3 worldTangentXYZ = mul((float3x3) ModelToWorldTransform, input.modelTangent.xyz);
    float3 worldBitangentXYZ = mul((float3x3) ModelToWorldTransform, input.modelBitangent.xyz);
    float3 worldNormalXYZ = mul((float3x3) ModelToWorldTransform, input.modelNormal.xyz);
    
    v2p_t v2p;
    
    v2p.worldPosition = float4(worldPosition.xyz, 1.0f);
    v2p.clipPosition = clipPosition;
    v2p.color = float4(input.color.rgba);
    v2p.uv = float2(input.uv.xy);
    
    v2p.worldTangent = float4(worldTangentXYZ.xyz, 0.0f);
    v2p.worldBitangent = float4(worldBitangentXYZ.xyz, 0.0f);
    v2p.worldNormal = float4(worldNormalXYZ.xyz, 0.0f);

    return v2p;
}

// ------------------------------------------------------------------------------------------------
p_out PixelMain(v2p_t input)
{
    p_out output;
    // --- Sampler Blend Factor ---
    float2 dx = ddx(input.uv);
    float2 dy = ddy(input.uv);
    float delta = max(dot(dx, dx), dot(dy, dy));
    float blendFactor = smoothstep(0.00001, 0.0000675, delta);

    // --- Sample Textures (Albedo, Normal, AO) ---
    float4 albedoTex = lerp(diffuseTexture.Sample(pointSampler, input.uv), diffuseTexture.Sample(anisoSampler, input.uv), blendFactor);
    float3 albedo = pow(albedoTex.rgb * ModelColor.rgb * input.color.rgb, 2.2);
    
    float3 nSample = lerp(normalTexture.Sample(pointSampler, input.uv).rgb, normalTexture.Sample(anisoSampler, input.uv).rgb, blendFactor);
    float3 tangentNormal = nSample * 2.0f - 1.0f;
    float3x3 TBN = float3x3(normalize(input.worldTangent.xyz), normalize(input.worldBitangent.xyz), normalize(input.worldNormal.xyz));
    float3 N = normalize(mul(tangentNormal, TBN));

    float ao = lerp(aoTexture.Sample(pointSampler, input.uv).r, aoTexture.Sample(anisoSampler, input.uv).r, blendFactor);
    float roughness = lerp(roughnessTexture.Sample(pointSampler, input.uv).r, roughnessTexture.Sample(anisoSampler, input.uv).r, blendFactor);
    float metallic = lerp(metallicTexture.Sample(pointSampler, input.uv).r, metallicTexture.Sample(anisoSampler, input.uv).r, blendFactor);

    // --- PBR Help Vectors ---
    float3 V = normalize(CameraWorldPosition - input.worldPosition.xyz);
    float3 L = normalize(-SunDirection);
    float3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotL = max(dot(N, L), 0.0);

    // --- Cook-Torrance Directional Light ---
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - metallic);

    float3 worldNormal = normalize(input.worldNormal.xyz);
    float slope = tan(acos(NdotL));
    slope = clamp(slope, 0.0, 5.0);
    float depthBiasScale = 0.001;
    float normalBiasScale = 0.015;
    float currentDepthBias = depthBiasScale * slope;
    float currentNormalBias = normalBiasScale * (1.0 - NdotL);
    float texelSize = 1.0 / 2048.0;
    float3 normalOffset = worldNormal * currentNormalBias * texelSize * 2.0;
    float3 biasedWorldPos = input.worldPosition.xyz + normalOffset;
    float4 lightSpacePos = mul(SunViewProjMatrix, float4(biasedWorldPos, 1.0));
    float shadow = CalculateShadow(lightSpacePos, currentDepthBias);
    float3 directLight = (kD * albedo / PI + specular) * SunIntensity * NdotL * shadow;

    // --- Calculate Ambient Light ---
    float3 skyAmbient = GetProceduralAmbient(N, L);
    float3 ambient = skyAmbient * albedo * ao;

    // --- Final Blend ---
    float3 finalRgb = ambient + directLight;
    
    // Exposure Tone Mapping
    finalRgb = finalRgb / (finalRgb + float3(1.0, 1.0, 1.0));
    // Gamma Correction
    finalRgb = pow(finalRgb, 1.0 / 2.5);
    
    clip(albedoTex.a - 0.01f);
    output.color = float4(finalRgb, albedoTex.a);
    output.normal = float4(N, 1.0f);
    
    return output;
}