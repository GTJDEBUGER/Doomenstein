//------------------------------------------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D originalScreenColor : register(t1);
Texture2D depthTexture : register(t2);
Texture2D originalScreenNormal : register(t3);

SamplerState screenSampler : register(s0);

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

cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
    float3 CameraWorldPosition;
    float _PaddingCam1;
    float4 ViewportBoundsUV;
    float CameraNear;
    float CameraFar;
};

//------------------------------------------------------------------------------------------------
cbuffer PostProcessingBuffer : register(b4)
{
    float4x4 projectionMatrix;
    float4x4 invProjectionMatrix;
    float4x4 worldToViewMatrix;
    float4 samples[64];
    float2 noiseScale;
    float radius;
    float bias;
    float2 screenResolution;
    float4 viewportBoundsUV;
};

//------------------------------------------------------------------------------------------------
struct vs_input_t
{
    float3 modelSpacePosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct v2p_t
{
    float4 clipSpacePosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

//------------------------------------------------------------------------------------------------
float GetViewZFromDepth(float depth)
{
    return (CameraNear * CameraFar) / (CameraFar - depth * (CameraFar - CameraNear));
}

float3 GetViewPos(float2 uv)
{
    float depth = depthTexture.Sample(screenSampler, uv).r;
    float4 clipPos = float4(uv.x * 2.0 - 1.0, (1.0 - uv.y) * 2.0 - 1.0, depth, 1.0);
    float4 viewPos = mul(invProjectionMatrix, clipPos);
    return viewPos.xyz / viewPos.w;
}

float ScreenSpaceRayMarch(float3 startViewPos, float3 rayViewDir, out float2 outHitUV)
{
    int maxSteps = 128;
    float maxRayDistance = 20.0f;
    float thickness = 0.5f;
    
    float3 endViewPos = startViewPos + rayViewDir * maxRayDistance;
    
    if (endViewPos.z <= CameraNear)
    {
        float t = (CameraNear - startViewPos.z) / rayViewDir.z;
        endViewPos = startViewPos + rayViewDir * t;
    }
    
    float4 startClip = mul(projectionMatrix, float4(startViewPos, 1.0));
    float4 endClip = mul(projectionMatrix, float4(endViewPos, 1.0));

    float2 startUV = float2(startClip.x / startClip.w * 0.5 + 0.5, 1.0 - (startClip.y / startClip.w * 0.5 + 0.5));
    float2 endUV = float2(endClip.x / endClip.w * 0.5 + 0.5, 1.0 - (endClip.y / endClip.w * 0.5 + 0.5));
    
    float2 deltaUV = endUV - startUV;
    float2 pixelDelta = abs(deltaUV * screenResolution);
    float numSteps = max(pixelDelta.x, pixelDelta.y);
    
    numSteps = min(numSteps, (float) maxSteps);
    if (numSteps <= 1.0)
        return 0.0;

    float2 stepUV = deltaUV / numSteps;
    
    float startInvZ = 1.0 / startViewPos.z;
    float endInvZ = 1.0 / endViewPos.z;
    float stepInvZ = (endInvZ - startInvZ) / numSteps;

    float2 currentUV = startUV + stepUV;
    float currentInvZ = startInvZ + stepInvZ;
    
    [loop]
    for (int i = 0; i < maxSteps; i++)
    {
        if ((float) i >= numSteps)
            break;
        
        if (currentUV.x < 0.0 || currentUV.x > 1.0 || currentUV.y < 0.0 || currentUV.y > 1.0)
            break;
        
        float sampledDepth = depthTexture.SampleLevel(screenSampler, currentUV, 0).r;
        float sceneZ = GetViewZFromDepth(sampledDepth);
        
        float currentRayZ = 1.0 / currentInvZ;
        
        float depthDiff = currentRayZ - sceneZ;
        
        if (depthDiff > 0.0 && depthDiff < thickness)
        {
            outHitUV = currentUV;
            
            float2 edgeDist = abs(currentUV * 2.0 - 1.0);
            float edgeFade = saturate(1.0 - max(edgeDist.x, edgeDist.y) * 1.2);
            return edgeFade;
        }
        
        currentUV += stepUV;
        currentInvZ += stepInvZ;
    }
    
    return 0.0;
}

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    v2p_t v2p;
    v2p.clipSpacePosition = float4(input.modelSpacePosition, 1.f);
    v2p.color = input.color;
    v2p.uv = input.uv;
    return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float3 viewPos = GetViewPos(input.uv);
    float3 worldNormal = originalScreenNormal.Sample(screenSampler, input.uv).xyz;
    float3 viewNormal = normalize(mul((float3x3) worldToViewMatrix, worldNormal));
    
    float baseRandom = frac(sin(dot(input.uv * noiseScale, float2(12.9898, 78.233))) * 43758.5453);
    float3 totalIndirectLight = float3(0, 0, 0);
    
    int RAY_COUNT = 4;
    float maxRayDistance = 20.0f;
    float giIntensity = 2.0f;

    [unroll]
    for (int r = 0; r < RAY_COUNT; r++)
    {
        float currentRandom = frac(baseRandom + (float) r * 0.6180339f);
        int sampleIndex = (int) (currentRandom * 63.99f);
        
        float3 randomHemisphereDir = normalize(samples[sampleIndex].xyz);
        float3 randomRotVec = normalize(float3(cos(currentRandom * 6.283f), sin(currentRandom * 6.283f), 0.0f));
        
        float3 tangent = normalize(randomRotVec - viewNormal * dot(randomRotVec, viewNormal));
        float3 bitangent = cross(viewNormal, tangent);
        float3x3 TBN = float3x3(tangent, bitangent, viewNormal);
        
        float3 rayViewDir = normalize(mul(randomHemisphereDir, TBN));
        
        float2 hitUV = 0.0;
        float hitConfidence = ScreenSpaceRayMarch(viewPos, rayViewDir, hitUV);
        
        if (hitConfidence > 0.0)
        {
            float3 bounceColor = originalScreenColor.SampleLevel(screenSampler, hitUV, 0).rgb;
            float NdotL = max(dot(viewNormal, rayViewDir), 0.0);
            
            float hitDepth = depthTexture.SampleLevel(screenSampler, hitUV, 0).r;
            float hitSceneZ = GetViewZFromDepth(hitDepth);
            float rayDistance = abs(hitSceneZ - viewPos.z);
            
            float distanceFade = saturate(1.0f - (rayDistance / maxRayDistance));
            distanceFade = distanceFade * distanceFade;
            
            totalIndirectLight += bounceColor * NdotL * hitConfidence * distanceFade * giIntensity;
        }
    }
    
    return float4(totalIndirectLight / (float) RAY_COUNT, 1.0);
}