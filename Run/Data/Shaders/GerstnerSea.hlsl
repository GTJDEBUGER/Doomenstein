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
    float2 uv : TEXCOORD0;
    float waveHeight : TEXCOORD1;
    float foamMask : TEXCOORD2;
    float2 gridPos : TEXCOORD3;
    float4 projPos : TEXCOORD4;
    float displacementWeight : TEXCOORD5;
    float rawModelZ : TEXCOORD6;
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
#define MAX_POINT_LIGHTS 256

struct PointLight
{
    float3 Position;
    float Range;
    float4 Color;
    float Intensity;
    int Volumetric;
    float2 padding;
};

cbuffer GameConstants : register(b0)
{
    float GameRunTime;
    float IsStencilPass;
    
    // --- Weather ---
    float WeatherCoverage;
    float WeatherDensity;
    float WeatherAbsorption;
    float WeatherDarkness;
    float WeatherCloudMinY;
    float WeatherCloudMaxY;
    
    float2 StormCenter;
    float StormRadius;
    float StormTwistStrength;
    float StormFunnelDepth;
    float StormEyeRadius;
};

cbuffer LightConstants : register(b1)
{
    float3 SunDirection;
    float SunIntensity;
    
    float AmbientIntensity;
    float3 _Padding1;
    
    float4x4 SunViewProjMatrix;
    
    uint NumPointLights;
    float3 _Padding2;
    
    PointLight PointLights[MAX_POINT_LIGHTS];
    float4 _Padding3;
};

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
    float3 CameraWorldPosition;
    float4 ViewportBoundsUV;
    float CameraNear;
    float CameraFar;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
    float4x4 ModelToWorldTransform;
    float4 ModelColor;
};

//------------------------------------------------------------------------------------------------
#define MAX_WAVES 16
#define MAX_SPIRALS 16

cbuffer WaveConstants : register(b5)
{
    float4 Waves_DirK_Speed_Phase[MAX_WAVES];
    float4 Waves_Steep_A_Dx_Dy[MAX_WAVES];
    float4 Spirals_Center_Radius_Intensity[MAX_SPIRALS];
};

//------------------------------------------------------------------------------------------------
Texture2D seaNormalTexture : register(t0);
Texture2D seaFoamTexture : register(t1);
Texture2D seaFoamNormalTexture : register(t2);
Texture2D sceneColor : register(t3);
Texture2D sceneNormal : register(t4);
Texture2D sceneDepth : register(t5);

//------------------------------------------------------------------------------------------------
SamplerState pointSampler : register(s0);
SamplerState texSampler : register(s1);

//------------------------------------------------------------------------------------------------
float LinearizeDepth(float ndcDepth, float4x4 projMat)
{
    float p22 = projMat[2][2];
    float p32_val1 = projMat[2][3];
    float p32_val2 = projMat[3][2];
    
    float p32 = lerp(p32_val2, p32_val1, step(0.000001f, abs(p32_val1)));
    
    float p32Mask = step(abs(p32), 0.0001f);
    p32 = lerp(p32, -0.1f, p32Mask);
    
    float denom = ndcDepth - p22;
    float denomMask = step(abs(denom), 0.00001f);
    float safeDenom = step(0.0f, denom) * 0.00002f - 0.00001f;
    denom = lerp(denom, safeDenom, denomMask);
    
    return abs(p32 / denom);
}

//------------------------------------------------------------------------------------------------
float3 GetProceduralAmbient(float3 worldNormal, float3 sunDir)
{
    float sunDot = dot(worldNormal, sunDir);
    float dayFactor = smoothstep(-0.15, 0.2, sunDir.z);
    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 4.0)) * smoothstep(-0.1, 0.0, sunDir.z);
    float viewHeight = saturate(worldNormal.z);
    float3 zenithColor = float3(0.05, 0.2, 0.6);
    float3 horizonColorDay = float3(0.6, 0.7, 0.85);
    float3 sunsetColor = float3(1.0, 0.45, 0.1);
    float3 sunsetRed = float3(0.95, 0.3, 0.1);
    float3 nightSky = float3(0.02, 0.04, 0.2);
    
    float3 currentHorizon = lerp(horizonColorDay, sunsetColor, sunsetFactor);
    float3 daySky = lerp(currentHorizon, zenithColor, pow(viewHeight, 0.7));
    
    float glowDist = pow(saturate(sunDot), 4.0);
    float3 sunsetGlow = lerp(sunsetColor, sunsetRed, sunsetFactor);
    daySky = lerp(daySky, sunsetGlow, glowDist * sunsetFactor * (1.0 - viewHeight * 0.5));
    
    float cosTheta = saturate(sunDot);
    float mie = pow(cosTheta, 256.0) * 2.0 + pow(cosTheta, 8.0) * 0.5;
    float3 mieColor = lerp(float3(1.0, 0.95, 0.8), sunsetRed, sunsetFactor);
    float3 fakeSunAndHalo = mieColor * mie * dayFactor;
    
    float normSun = SunIntensity / 0.85f;
    return (lerp(nightSky, daySky, dayFactor) + fakeSunAndHalo * normSun) * AmbientIntensity;
}

//------------------------------------------------------------------------------------------------
static const float PI = 3.14159265359;

float3 CalculateGerstnerWave(int waveIndex, float3 samplePoint, inout float3 tangent, inout float3 bitangent, float waveActiveMask)
{
    float4 dirKSpeedPhase = Waves_DirK_Speed_Phase[waveIndex];
    float4 steepADxDy = Waves_Steep_A_Dx_Dy[waveIndex];
    
    float f = dirKSpeedPhase.x * samplePoint.x +
              dirKSpeedPhase.y * samplePoint.y -
              dirKSpeedPhase.z * GameRunTime +
              dirKSpeedPhase.w;
    
    float sinf, cosf;
    sincos(f, sinf, cosf);
    
    float steepness = steepADxDy.x;
    float a = steepADxDy.y;
    float dx = steepADxDy.z;
    float dy = steepADxDy.w;
    float choppiness = 1.2f;
    
    float steepSin = steepness * sinf * choppiness;
    float steepCos = steepness * cosf;
    
    tangent += float3(
        -dx * dx * steepSin,
        -dx * dy * steepSin,
        dx * steepCos
    ) * waveActiveMask;
    
    bitangent += float3(
        -dx * dy * steepSin,
        -dy * dy * steepSin,
        dy * steepCos
    ) * waveActiveMask;
    
    return float3(
        dx * (a * cosf) * choppiness,
        dy * (a * cosf) * choppiness,
        a * sinf
    ) * waveActiveMask;
}

float3 CalculateWhirlpool(int index, float3 samplePoint, inout float3 tangent, inout float3 bitangent, float activeMask, out float2 uvSwirl, out float foamAccum)
{
    float2 center = Spirals_Center_Radius_Intensity[index].xy;
    float radius = Spirals_Center_Radius_Intensity[index].z;
    float intensity = Spirals_Center_Radius_Intensity[index].w;

    float2 offset = samplePoint.xy - center;
    float distSq = dot(offset, offset);
    float radSq = max(radius * radius, 0.0001f);
    
    float insideMask = step(distSq, radSq) * activeMask;
    
    float dist = sqrt(distSq);
    float r_norm = saturate(dist / max(radius, 0.001f));
    
    float f = 1.0f - r_norm;
    float falloff = f * f;
    float zOffset = -intensity * falloff;
    
    float dz_dx = 8.0f * intensity * f * offset.x / radSq;
    float dz_dy = 8.0f * intensity * f * offset.y / radSq;
    tangent += float3(0.0f, 0.0f, dz_dx) * insideMask;
    bitangent += float3(0.0f, 0.0f, dz_dy) * insideMask;
    
    float safe_r = max(r_norm, 0.01f);
    
    float densityScale = 1.0f / pow(safe_r, 0.4f);
    
    float twistStrength = 1.2f;
    float staticTwist = twistStrength * (1.0f - r_norm) / (r_norm + 0.2f);
    
    float2 scaledUV = offset * densityScale;
    
    float sinT, cosT;
    sincos(staticTwist, sinT, cosT);
    
    float2 rotatedUV = float2(
        scaledUV.x * cosT - scaledUV.y * sinT,
        scaledUV.x * sinT + scaledUV.y * cosT
    );
    
    uvSwirl = (rotatedUV - offset) * insideMask;
    
    foamAccum = smoothstep(0.2f, 1.0f, falloff) * insideMask;

    return float3(0.0f, 0.0f, zOffset) * insideMask;
}

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    float depthWeight = smoothstep(-10.0f, 0.0f, input.modelPosition.z);
    
    float distX = abs(input.modelPosition.x);
    float distY = abs(input.modelPosition.y);
    float maxDist = max(distX, distY);
    float edgeWeight = 1.0f - smoothstep(1000.0f, 1500.0f, maxDist);
    
    float finalWaveWeight = depthWeight * edgeWeight;
    
    float4 modelPosition = float4(input.modelPosition.xyz, 1.0f);
    float4 worldPosition = mul(ModelToWorldTransform, modelPosition);
    float3 baseGridPoint = worldPosition.xyz;
    
    float2 spatialWarp = float2(
        sin(baseGridPoint.y * 0.002f + GameRunTime * 0.1f),
        cos(baseGridPoint.x * 0.002f - GameRunTime * 0.1f)
    ) * 15.0f;
    float3 samplePoint = baseGridPoint;
    samplePoint.xy += spatialWarp;
    
    float3 tangent = float3(1.0, 0.0, 0.0);
    float3 bitangent = float3(0.0, 1.0, 0.0);
    float3 gerstnerOffset = float3(0, 0, 0);
    float3 whirlpoolOffset = float3(0, 0, 0);
    float maxWhirlpoolFalloff = 0.0f;
    float2 totalUVSwirl = float2(0.0f, 0.0f);
    float totalWhirlpoolFoam = 0.0f;
    
    [unroll]
    for (int i = 0; i < MAX_WAVES; i++)
    {
        float waveActiveMask = step(0.0001f, Waves_Steep_A_Dx_Dy[i].x);
        gerstnerOffset += CalculateGerstnerWave(i, samplePoint, tangent, bitangent, waveActiveMask);
    }
    
    [unroll]
    for (int j = 0; j < MAX_SPIRALS; j++)
    {
        float spiralActiveMask = step(0.001f, abs(Spirals_Center_Radius_Intensity[j].w));
        float2 uvSwirl = float2(0.0f, 0.0f);
        float foamAccum = 0.0f;
        
        whirlpoolOffset += CalculateWhirlpool(j, baseGridPoint, tangent, bitangent, spiralActiveMask, uvSwirl, foamAccum);
        totalUVSwirl += uvSwirl;
        totalWhirlpoolFoam += foamAccum;
        
        float2 offset = baseGridPoint.xy - Spirals_Center_Radius_Intensity[j].xy;
        float distSq = dot(offset, offset);
        float radSq = max(Spirals_Center_Radius_Intensity[j].z * Spirals_Center_Radius_Intensity[j].z, 0.0001f);
        float f = max(0.0f, 1.0f - distSq / radSq);
        maxWhirlpoolFalloff = max(maxWhirlpoolFalloff, f * f * spiralActiveMask);
    }
    
    float waveSuppression = lerp(1.0f, 0.2f, maxWhirlpoolFalloff);
    float3 waveOffset = gerstnerOffset * waveSuppression + whirlpoolOffset;
    
    float localWaveZ = waveOffset.z;
    float3 mainWindDir = float3(0.877f, 0.479f, 0.0f);
    float curlStrength = 2.5f;
    float crestMask = smoothstep(0.5f, 3.0f, localWaveZ);
    float safeJacobian = tangent.x * bitangent.y - tangent.y * bitangent.x;
    float adaptiveCurl = curlStrength * saturate(safeJacobian);
    
    float curlWhirlpoolMask = saturate(1.0f - maxWhirlpoolFalloff * 1.5f);
    
    waveOffset.xy += mainWindDir.xy * crestMask * adaptiveCurl * curlWhirlpoolMask;
    
    worldPosition.xyz = baseGridPoint + waveOffset * finalWaveWeight;
    
    float3 normal = normalize(cross(tangent, bitangent));
    normal.z = max(normal.z, 0.1f);
    
    float4 cameraPosition = mul(WorldToCameraTransform, float4(worldPosition.xyz, 1.0f));
    float4 renderPosition = mul(CameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(RenderToClipTransform, renderPosition);
    float jacobian = tangent.x * bitangent.y - tangent.y * bitangent.x;
    
    v2p_t v2p;
    v2p.worldPosition = float4(worldPosition.xyz, 1.0f);
    v2p.waveHeight = localWaveZ;
    
    v2p.foamMask = saturate(1.0f - jacobian + totalWhirlpoolFoam * 2.0f);
    
    float warpSuppression = lerp(1.0f, 0.0f, maxWhirlpoolFalloff);
    v2p.gridPos = baseGridPoint.xy + spatialWarp * warpSuppression + totalUVSwirl;
    
    v2p.clipPosition = clipPosition;
    v2p.projPos = clipPosition;
    v2p.color = float4(input.color.rgba);
    v2p.uv = float2(input.uv.xy);
    v2p.worldTangent = float4(tangent, 0.0f);
    v2p.worldBitangent = float4(bitangent, 0.0f);
    v2p.worldNormal = float4(normalize(normal), 0.0f);
    
    v2p.displacementWeight = finalWaveWeight;
    v2p.rawModelZ = input.modelPosition.z;

    return v2p;
}

// =================================================================================
// PBR Functions
// =================================================================================
float ComputeGGX(float3 N, float3 V, float3 L, float roughness)
{
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.005f);
    float NdotV = max(dot(N, V), 0.005f);
    float NdotH = max(dot(N, H), 0.0f);
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    
    float numNDF = alpha2;
    float denomNDF = (NdotH * NdotH * (alpha2 - 1.0f) + 1.0f);
    denomNDF = PI * denomNDF * denomNDF;
    float D = numNDF / max(denomNDF, 0.000001f);
    
    float lambdaV = NdotL * sqrt(NdotV * NdotV * (1.0f - alpha2) + alpha2);
    float lambdaL = NdotV * sqrt(NdotL * NdotL * (1.0f - alpha2) + alpha2);
    float Vis = 0.5f / max(lambdaV + lambdaL, 0.00001f);
    
    float horizonFade = smoothstep(-0.02f, 0.1f, L.z);
    return D * Vis * horizonFade;
}

float ComputeSSSIntensity(float3 N, float3 V, float3 L, float tipThickness)
{
    float3 sssLightDir = normalize(-L + N * 0.15f);
    float VoL = dot(V, sssLightDir);
    float scatterView = saturate(VoL);
    
    float g = 0.5f;
    float g2 = g * g;
    float phaseFunction = (1.0f - g2) / pow(abs(1.0f + g2 - 2.0f * g * scatterView), 1.5f);
    phaseFunction = min(phaseFunction, 3.5f);
    
    float backFacePenetration = smoothstep(0.2f, -0.8f, dot(N, L));
    return phaseFunction * saturate(tipThickness * backFacePenetration) * 0.8f;
}

// ------------------------------------------------------------------------------------------------
p_out PixelMain(v2p_t input, bool isFrontFace : SV_IsFrontFace)
{
    if (IsStencilPass < 0.5f && input.rawModelZ < -1.0f)
    {
        discard;
    }
    
    p_out output;
    
    // =================================================================================
    // View Direction & Underwater Detection Masks
    // =================================================================================
    float3 viewDir = normalize(CameraWorldPosition - input.worldPosition.xyz);
    float3 macroTangent = normalize(input.worldTangent.xyz);
    float3 macroBitangent = normalize(input.worldBitangent.xyz);
    float3 macroNormal = normalize(input.worldNormal.xyz);
    
    float isUnderwaterMask = isFrontFace ? 0.0f : 1.0f;
    float notUnderwaterMask = 1.0f - isUnderwaterMask;
    
    float3 workingNormal = lerp(macroNormal, -macroNormal, isUnderwaterMask);
    float3x3 TBN = float3x3(macroTangent, macroBitangent, workingNormal);
    
    // =================================================================================
    // Normal Mapping Setup
    // =================================================================================
    float2 worldUV = input.gridPos;
    float2 uvScale1 = worldUV * 0.025f;
    float2 uvScale2 = worldUV * 0.0125f;
    float2 pan1 = float2(0.04f, 0.02f) * GameRunTime;
    float2 pan2 = float2(-0.02f, 0.05f) * GameRunTime;
    
    float3 n1 = seaNormalTexture.Sample(texSampler, uvScale1 + pan1).rgb * 2.0f - 1.0f;
    float3 n2 = seaNormalTexture.Sample(texSampler, uvScale2 + pan2).rgb * 2.0f - 1.0f;
    
    float normalIntensity = 1.2f;
    float3 tangentNormal = normalize(n1 + n2);
    tangentNormal.xy *= normalIntensity;
    
    tangentNormal.xy *= lerp(1.0f, -1.0f, isUnderwaterMask);
    tangentNormal = normalize(tangentNormal);
    
    float3 N = normalize(mul(tangentNormal, TBN));
    float NdotV = max(dot(N, viewDir), 0.0001f);

    // =================================================================================
    // Screen UV, Snell's Law Refraction & Real Depth
    // =================================================================================
    float2 localScreenUV = (input.projPos.xy / input.projPos.w) * 0.5f + 0.5f;
    localScreenUV.y = 1.0f - localScreenUV.y;
    
    float underwaterIOR = 1.15f;
    float eta = lerp(1.0f / 1.333f, underwaterIOR, isUnderwaterMask);
    
    float3 baseNormal = lerp(macroNormal, -macroNormal, isUnderwaterMask);
    float3 refractN = normalize(lerp(baseNormal, N, 0.35f));
    
    float3 incident = -viewDir;
    float3 refractDir = refract(incident, refractN, eta);
    
    float isTIRMask = isUnderwaterMask * (1.0f - smoothstep(0.0f, 0.05f, length(refractDir)));
    
    float2 viewportSize = ViewportBoundsUV.zw - ViewportBoundsUV.xy;
    float2 realScreenUV = localScreenUV * viewportSize + ViewportBoundsUV.xy;
    float waterViewZ = input.projPos.w;
    
    float distortionStrength = lerp(0.05f, 0.15f, isUnderwaterMask);
    float2 refractedLocalUV = saturate(localScreenUV + tangentNormal.xy * distortionStrength);
    float2 refractedRealUV = refractedLocalUV * viewportSize + ViewportBoundsUV.xy;
    
    float bgDepthRaw = sceneDepth.SampleLevel(pointSampler, refractedRealUV, 0).r;
    float bgViewZ = LinearizeDepth(bgDepthRaw, RenderToClipTransform);
    float depthCmpMask = 1.0f - step(waterViewZ, bgViewZ);
    
    refractedRealUV = lerp(refractedRealUV, realScreenUV, depthCmpMask);
    float altBgDepthRaw = sceneDepth.SampleLevel(pointSampler, realScreenUV, 0).r;
    bgDepthRaw = lerp(bgDepthRaw, altBgDepthRaw, depthCmpMask);
    bgViewZ = lerp(bgViewZ, LinearizeDepth(altBgDepthRaw, RenderToClipTransform), depthCmpMask);
    
    float3 refractColor = sceneColor.SampleLevel(pointSampler, refractedRealUV, 0).rgb;
    float3 refractAbove = pow(max(refractColor, 0.0f), 2.25f);
    float3 refractBelow = min(refractColor * 1.25f, float3(2.0f, 2.0f, 2.0f));
    refractColor = lerp(refractAbove, refractBelow, isUnderwaterMask);
    
    float distToCam = length(CameraWorldPosition - input.worldPosition.xyz);
    float thicknessAbove = max(0.001f, bgViewZ - waterViewZ);
    float thicknessBelow = distToCam;
    float realWaterThickness = lerp(thicknessAbove, thicknessBelow, isUnderwaterMask);

    // =================================================================================
    // View & Sun/Moon Light Setup
    // =================================================================================
    float3 sunDir = normalize(-SunDirection);
    float3 moonDir = normalize(SunDirection);
    
    float normSun = SunIntensity / 0.85f;
    float normAmbient = AmbientIntensity / 0.35f;
    float sunMask = smoothstep(0.02f, 0.1f, sunDir.z);
    float moonMask = smoothstep(0.02f, 0.1f, moonDir.z);
    
    float dayFactor = smoothstep(-0.15, 0.2, sunDir.z);
    float stormActive = smoothstep(0.4, 0.3, SunIntensity);
    
    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 4.0)) * smoothstep(-0.1, 0.0, sunDir.z);
    sunsetFactor = lerp(sunsetFactor, dayFactor, stormActive);
    
    float3 sunColorVal = lerp(float3(1.0, 0.95, 0.9), float3(1.0, 0.45, 0.1), sunsetFactor);
    sunColorVal = lerp(sunColorVal, float3(0.7, 0.05, 0.05), stormActive);
    
    float3 sunLightColor = sunColorVal * SunIntensity * sunMask;
    float3 moonLightColor = float3(0.2f, 0.3f, 0.45f) * 0.4f * moonMask * normAmbient;

    // =================================================================================
    // Aeration, Wave Crests & Foam Masks
    // =================================================================================
    float totalSurfaceFoam = 0.0f;
    float foamCore = 0.0f;
    float foamGlow = 0.0f;
    float currentRoughness = lerp(0.15f, 0.03f, isUnderwaterMask);
    float combinedFoamPattern = 0.0f;
    
    float2 baseUV = input.gridPos;
    float2 mainWindDirFlow = float2(0.877f, 0.479f);
    float2 waveFlow = mainWindDirFlow * GameRunTime * 0.4f;
    float2 slopeSlide = input.worldNormal.xy * 0.15f;
    
    float2 foamUV1 = baseUV * 0.15f - waveFlow + slopeSlide;
    float2 foamUV2 = baseUV * 0.22f - waveFlow * 1.2f + slopeSlide * 1.2f;
    
    foamUV1 += tangentNormal.xy * distortionStrength;
    foamUV2 -= tangentNormal.xy * distortionStrength * 0.5f;
    
    float foamTex1 = seaFoamTexture.Sample(texSampler, foamUV1).r;
    float foamTex2 = seaFoamTexture.Sample(texSampler, foamUV2).r;
    combinedFoamPattern = saturate(foamTex1 * foamTex2 * 2.5f);
    
    float crestMask2 = smoothstep(0.5f, 3.5f, input.waveHeight);
    float kinematicFoam = saturate(input.foamMask * crestMask2 * 2.0f);
    
    float subsurfaceAeration = saturate((kinematicFoam + crestMask2 * 0.6f) * combinedFoamPattern);
    float surfaceFoam = saturate((kinematicFoam * 1.5f + crestMask2 * 0.2f) * smoothstep(0.1f, 0.8f, combinedFoamPattern));
    
    float baseContactRadius = 6.f;
    float waveImpact = smoothstep(0.0f, 2.5f, input.waveHeight);
    float dynamicRadius = baseContactRadius + waveImpact * 1.5f;
    
    float2 sloshUV = baseUV * 0.2f - waveFlow * 1.5f + slopeSlide * 1.6f;
    float sloshNoise = seaFoamTexture.SampleLevel(texSampler, sloshUV, 0).r;
    sloshNoise = pow(sloshNoise, 1.5f);
    
    float finalCollisionRadius = dynamicRadius + sloshNoise * 1.2f;
    float collisionFade = saturate(1.0f - (realWaterThickness / max(finalCollisionRadius, 0.001f)));
    
    float rootFoam = smoothstep(0.4f, 0.95f, collisionFade);
    float scatteredFoam = smoothstep(0.0f, 0.7f, collisionFade) * sloshNoise * combinedFoamPattern;
    float hardContact = smoothstep(0.15f, 0.0f, realWaterThickness);
    
    float platformFoamMask = saturate(rootFoam * 1.5f + scatteredFoam + hardContact);
    
    float platformGlow = rootFoam * (waveImpact + 0.8f);
    
    float calcTotalSurfaceFoam = saturate(surfaceFoam + platformFoamMask);
    float calcFoamCore = smoothstep(0.0f, 0.5f, calcTotalSurfaceFoam);
    
    float calcFoamGlow = saturate(subsurfaceAeration + platformGlow * 1.5f);
    
    float3 fn1 = seaFoamNormalTexture.Sample(texSampler, foamUV1).rgb * 2.0f - 1.0f;
    float3 fn2 = seaFoamNormalTexture.Sample(texSampler, foamUV2).rgb * 2.0f - 1.0f;
    float3 combinedFoamTangentNormal = normalize(normalize(fn1 + fn2) * float3(1.2f, 1.2f, 1.0f));
    
    float3 platformBumpNormal = normalize(float3(tangentNormal.xy * 4.0f + (sloshNoise - 0.5f) * 3.0f, 0.3f));
    
    combinedFoamTangentNormal = normalize(lerp(combinedFoamTangentNormal, platformBumpNormal, platformFoamMask * 0.85f));
    
    float3 foamWorldNormal = normalize(mul(combinedFoamTangentNormal, TBN));
    
    float3 calcFoamNormal = lerp(N, normalize(N + foamWorldNormal), calcFoamCore * 0.6f);
    
    float calcCurrentRoughness = lerp(0.15f, 0.85f, calcFoamCore);
    
    totalSurfaceFoam = calcTotalSurfaceFoam * notUnderwaterMask;
    foamCore = calcFoamCore * notUnderwaterMask;
    foamGlow = calcFoamGlow * notUnderwaterMask;
    N = normalize(lerp(N, calcFoamNormal, notUnderwaterMask));
    currentRoughness = lerp(currentRoughness, calcCurrentRoughness, notUnderwaterMask);

    // =================================================================================
    // Wave Crest SSS & Volume Scattering
    // =================================================================================
    float worldScale = 0.45f;
    
    float3 waterAbsorption = float3(0.65f, 0.24f, 0.05f) * worldScale;
    waterAbsorption = lerp(waterAbsorption, float3(0.05f, 0.85f, 0.95f) * worldScale, stormActive);
    
    float3 baseWaterScattering = float3(0.01f, 0.08f, 0.18f) * worldScale;
    baseWaterScattering = lerp(baseWaterScattering, float3(0.25f, 0.01f, 0.01f) * worldScale, stormActive);

    float crestGradient = smoothstep(-7.5f, 5.0f, input.waveHeight);
    
    float3 midWaveColor = float3(0.15f, 0.55f, 0.75f);
    midWaveColor = lerp(midWaveColor, float3(0.35f, 0.01f, 0.01f), stormActive);
    
    float3 peakBubbleColor = float3(0.85f, 0.95f, 1.0f);
    peakBubbleColor = lerp(peakBubbleColor, float3(0.7f, 0.05f, 0.02f), stormActive);

    float3 dynamicBubbleColor = lerp(midWaveColor, peakBubbleColor, smoothstep(0.4f, 1.0f, crestGradient));
    
    float bubbleIntensity = foamGlow * lerp(1.5f, 4.0f, crestGradient + 0.5f);
    float3 totalScattering = baseWaterScattering + dynamicBubbleColor * bubbleIntensity;
    float3 extinction = waterAbsorption + totalScattering;
    float3 baseVolumeColor = totalScattering / max(extinction, 0.00001f);
    
    float clarityFactor = lerp(1.0f, 0.6f, isUnderwaterMask);
    float opticalDepth = (realWaterThickness * clarityFactor) + (foamGlow * 1.5f);
    
    float3 lightTransmittance = exp(-extinction * opticalDepth);
    
    float crestThinness = smoothstep(-2.5f, 10.f, input.waveHeight);
    float slopeThinness = smoothstep(0.95f, 0.2f, N.z) * smoothstep(-0.2f, 0.2f, N.z);
    
    float waveTranslucency = crestThinness * (0.4f + slopeThinness * 0.8f);
    float kinematicFoamToPass = lerp(saturate(input.foamMask * smoothstep(0.5f, 3.5f, input.waveHeight) * 2.0f), 0.0f, isUnderwaterMask);
    waveTranslucency *= (1.0f - kinematicFoamToPass);
    waveTranslucency += foamGlow * 0.3f;
    
    float sssSun = ComputeSSSIntensity(N, viewDir, sunDir, waveTranslucency);
    float sssMoon = ComputeSSSIntensity(N, viewDir, moonDir, waveTranslucency);
    
    float sunLowAngleAttenuation = smoothstep(-0.05f, 0.25f, sunDir.z);
    float3 sssColor = lerp(midWaveColor, dynamicBubbleColor, 0.5f) * 0.8f;
    float3 crestSSSLight = (sunLightColor * sssSun * sunMask * sunLowAngleAttenuation + moonLightColor * sssMoon * moonMask) * sssColor;
    
    float dayEnvFactor = smoothstep(-0.15f, 0.2f, sunDir.z);
    
    float3 ambientVolumeDay = float3(1.0f, 1.0f, 1.0f);
    float3 ambientVolumeNight = float3(0.002f, 0.005f, 0.01f);
    
    ambientVolumeDay = lerp(ambientVolumeDay, float3(0.4f, 0.02f, 0.01f), stormActive);
    ambientVolumeNight = lerp(ambientVolumeNight, float3(0.04f, 0.002f, 0.001f), stormActive);
    
    float3 ambientVolumeLight = lerp(ambientVolumeNight, ambientVolumeDay, dayEnvFactor) * 0.2f * normAmbient;
    
    float3 foamGlowDay = float3(0.8f, 0.9f, 1.0f);
    float3 foamGlowNight = float3(0.001f, 0.003f, 0.005f);
    
    foamGlowDay = lerp(foamGlowDay, float3(0.5f, 0.05f, 0.02f), stormActive);
    foamGlowNight = lerp(foamGlowNight, float3(0.05f, 0.005f, 0.002f), stormActive);
    
    ambientVolumeLight += lerp(foamGlowNight, foamGlowDay, dayEnvFactor) * foamGlow * 0.8f * normAmbient;
    
    ambientVolumeLight *= lerp(0.15f, 1.0f, saturate(exp(-realWaterThickness * 0.4f)));
    float3 internalLight = ambientVolumeLight + crestSSSLight;
                           
    float3 opticalTransmittanceVolume = exp(-extinction * realWaterThickness);
    float3 transmittedBackground = refractColor * opticalTransmittanceVolume;
    float3 scatteredVolume = baseVolumeColor * internalLight * (1.0f - opticalTransmittanceVolume);
    
    float3 illuminatedVolume = transmittedBackground + scatteredVolume + crestSSSLight * 0.25f;

    // =================================================================================
    // Reflection & Water Surface Color (TIR Mixing)
    // =================================================================================
    float3 R = normalize(reflect(-viewDir, N));
    R.z = max(R.z, 0.0f);

    float F0 = 0.02f;
    float f_base = 1.0f - NdotV;
    float f5 = pow(f_base, 5.0f);
    float baseFresnel = F0 + (1.0f - currentRoughness - F0) * f5;
    float fresnelAbove = lerp(baseFresnel * 0.6f, baseFresnel, smoothstep(0.0f, 0.4f, NdotV));
    
    float criticalFade = smoothstep(0.f, 0.05f, refractDir.z);
    float fresnelBelow = lerp(1.0f, baseFresnel, criticalFade);
    
    float3 proceduralAmbient = GetProceduralAmbient(R, sunDir);
    float3 colorAbove = lerp(illuminatedVolume, proceduralAmbient, fresnelAbove * (1.0f - foamCore));
    
    float3 deepSeaReflect = float3(0.005f, 0.02f, 0.05f) * AmbientIntensity;
    deepSeaReflect = lerp(deepSeaReflect, float3(0.06f, 0.002f, 0.001f) * AmbientIntensity, stormActive);
    
    float3 snellWindowColor = lerp(illuminatedVolume, deepSeaReflect + scatteredVolume, fresnelBelow);
    float3 colorBelow = lerp(snellWindowColor, deepSeaReflect + scatteredVolume, isTIRMask);
    float3 waterSurfaceColor = lerp(colorAbove, colorBelow, isUnderwaterMask);

    // =================================================================================
    // Physically Based Foam
    // =================================================================================
    float foamApplyMask = notUnderwaterMask * step(0.001f, totalSurfaceFoam);
    
    float3 foamBaseAlbedo = float3(0.9f, 0.95f, 1.0f);
    foamBaseAlbedo = lerp(foamBaseAlbedo, float3(0.6f, 0.05f, 0.02f), stormActive);
    
    float3 foamAlbedo = lerp(foamBaseAlbedo, dynamicBubbleColor, 0.6f);
    float foamDetail = lerp(0.2f, 1.0f, combinedFoamPattern);
    
    float wrap = 0.3f;
    float NdotL_Sun_Wrap = saturate((dot(N, sunDir) + wrap) / (1.0f + wrap));
    float NdotL_Moon_Wrap = saturate((dot(N, moonDir) + wrap) / (1.0f + wrap));
    
    float3 foamAmbient = GetProceduralAmbient(N, sunDir) * 0.3f + min(crestSSSLight * 0.8f, float3(1.5f, 1.5f, 1.5f));
    float3 foamDirect = sunLightColor * NdotL_Sun_Wrap * sunMask * 0.6f + moonLightColor * NdotL_Moon_Wrap * moonMask * 0.4f;
    
    float3 illuminatedFoam = foamAlbedo * foamDetail * (foamAmbient + foamDirect);
    float3 foamyWater = lerp(waterSurfaceColor, illuminatedFoam, totalSurfaceFoam * 0.75f);
    float3 waterAndFoam = lerp(waterSurfaceColor, foamyWater, foamApplyMask);

    // =================================================================================
    // GGX Specular Overlay & Underwater Transmission Glint
    // =================================================================================
    float specSun = ComputeGGX(N, viewDir, sunDir, currentRoughness);
    float specMoon = ComputeGGX(N, viewDir, moonDir, currentRoughness);
    float specIntensity = lerp(1.0f, 0.3f, foamCore);
    float3 specularLightAbove = (sunLightColor * specSun * sunMask + moonLightColor * specMoon * moonMask) * fresnelAbove * specIntensity;
    
    float3 specularLight = specularLightAbove * notUnderwaterMask;
    
    float glintDotSun = max(dot(refractDir, sunDir), 0.0f);
    float glintDotMoon = max(dot(refractDir, moonDir), 0.0f);
    
    float glintSpread = lerp(400.0f, 80.0f, currentRoughness);
    
    float sunGlintCore = pow(glintDotSun, glintSpread) * 20.0f;
    float sunGlintHalo = pow(glintDotSun, glintSpread * 0.1f) * 6.0f;
    
    float2 sparklePan = input.worldPosition.xy * 7.5f + GameRunTime * float2(1.2f, 0.8f);
    float sparkleMask = saturate(sin(sparklePan.x) * cos(sparklePan.y));
    sparkleMask = pow(sparkleMask, 40.0f);
    
    float microSparkle = pow(glintDotSun, glintSpread * 3.5f) * 25.0f * sparkleMask;
    
    float sunGlint = sunGlintCore + sunGlintHalo + microSparkle;
    
    float moonGlint = pow(glintDotMoon, glintSpread) * 10.0f + pow(glintDotMoon, glintSpread * 0.1f) * 2.0f;
    
    float snellTransmissionMask = isUnderwaterMask * (1.0f - isTIRMask);
    
    float3 brightWhiteSun = lerp(sunLightColor, float3(1.0f, 1.0f, 1.0f) * normSun, saturate(sunGlint * 0.15f));
    float3 underwaterTransmissionGlint = (brightWhiteSun * sunGlint * sunMask + moonLightColor * moonGlint * moonMask) * snellTransmissionMask;
    
    float3 upwellingDir = normalize(float3(-sunDir.xy * 0.5f, -1.0f));
    float internalSpec = ComputeGGX(N, viewDir, upwellingDir, currentRoughness * 1.5f);
    float3 internalSpecularLight = baseVolumeColor * internalSpec * isTIRMask * isUnderwaterMask * 0.75f;
    
    float3 underwaterSparkle = underwaterTransmissionGlint + internalSpecularLight;
    
    float3 finalRgb = waterAndFoam + specularLight + underwaterSparkle;
    
    // =================================================================================
    // Finalization
    // =================================================================================
    finalRgb = pow(max(finalRgb, 0.0f), 0.3f);
    float waterAlpha = 1.0f - smoothstep(500.0f, 725.0f, distToCam);
    
    output.color = float4(finalRgb, waterAlpha);
    output.normal = float4(N, 1.0f);

    return output;
}