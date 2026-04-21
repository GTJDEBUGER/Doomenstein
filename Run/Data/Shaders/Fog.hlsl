//------------------------------------------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D depthTexture : register(t2);
Texture2D originalScreenNormal : register(t3);
SamplerState screenSampler : register(s0);

//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b1)
{
    float3 SunDirection;
    float SunIntensity;
    float AmbientIntensity;
    float4x4 SunViewProjMatrix;
};

cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
    float3 CameraWorldPosition;
};

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
    float cameraNear;
    float cameraFar;
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
v2p_t VertexMain(vs_input_t input)
{
    v2p_t v2p;
    v2p.clipSpacePosition = float4(input.modelSpacePosition, 1.f);
    v2p.color = input.color;
    v2p.uv = input.uv;
    return v2p;
}

//------------------------------------------------------------------------------------------------
float CalculateHeightFog(float3 cameraPos, float3 viewDir, float dist, float baseDensity, float falloff)
{
    float dirZ = viewDir.z;
    if (abs(dirZ) < 0.001)
    {
        dirZ = 0.001 * sign(dirZ + 0.0001);
    }
    
    float fogIntegral = (1.0 - exp(-falloff * dirZ * dist)) / dirZ;
    return baseDensity * exp(-falloff * cameraPos.z) * fogIntegral;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float4 sceneColor = screenColor.Sample(screenSampler, input.uv);
    float rawDepth = depthTexture.Sample(screenSampler, input.uv).r;
    float4 normal = originalScreenNormal.Sample(screenSampler, input.uv);
    
    if (length(normal.xyz) < 0.1f) 
        return sceneColor;

    float4 clipPos = float4(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0, rawDepth, 1.0);
    float4 viewPos = mul(invProjectionMatrix, clipPos);
    viewPos.xyz /= viewPos.w;

    float dist = length(viewPos.xyz);
    
    float3x3 viewToWorld = transpose((float3x3) worldToViewMatrix);
    float3 viewDir = normalize(mul(viewToWorld, viewPos.xyz));
    float3 worldPos = CameraWorldPosition + viewDir * dist;
    
    float FOG_DENSITY = 0.075f;
    float FOG_FALLOFF = 0.05f;
    float opticalDepth = CalculateHeightFog(CameraWorldPosition, viewDir, dist, FOG_DENSITY, FOG_FALLOFF);
    float transmittance = exp(-max(opticalDepth, 0.0));
    float fogFactor = saturate(1.0 - transmittance);
    
    float3 sunDir = normalize(-SunDirection);
    float sunDot = dot(viewDir, sunDir);
    
    float dayFactor = smoothstep(-0.15, 0.2, sunDir.z);
    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 4.0));
    float viewHeight = saturate(viewDir.z);
    
    float3 horizonColorDay = float3(0.6, 0.7, 0.85);
    float3 sunsetColor = float3(1.0, 0.45, 0.1);
    float3 sunsetRed = float3(0.95, 0.3, 0.1);
    float3 nightSky = float3(0.02, 0.04, 0.2);
    
    float3 currentHorizon = lerp(horizonColorDay, sunsetColor, sunsetFactor);
    
    float glowDist = pow(saturate(sunDot), 4.0);
    float3 sunsetGlow = lerp(sunsetColor, sunsetRed, sunsetFactor);
    currentHorizon = lerp(currentHorizon, sunsetGlow, glowDist * sunsetFactor * (1.0 - viewHeight * 0.5));
    
    float3 ambientFogColor = lerp(nightSky, currentHorizon, dayFactor);
    
    float cosTheta = saturate(sunDot);
    float mieScattering = pow(cosTheta, 8.0) * 1.5 + pow(cosTheta, 4.0) * 0.2;
    
    float3 mieColor = lerp(float3(1.0, 0.95, 0.8), sunsetRed, sunsetFactor);
    
    float3 fogColor = ambientFogColor * AmbientIntensity + (mieColor * mieScattering * dayFactor * SunIntensity);
    
    float3 finalColor = lerp(sceneColor.rgb, fogColor, fogFactor);

    return float4(finalColor, sceneColor.a);
}