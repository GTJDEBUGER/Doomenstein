//------------------------------------------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D depthTexture : register(t2);
Texture2D originalScreenNormal : register(t3);
SamplerState screenSampler : register(s0);

//------------------------------------------------------------------------------------------------
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
    float4x4 SunViewProjMatrix;
};

cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
    float3 CameraWorldPosition;
    float4 viewportBoundsUV;
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
    
    if (length(normal.xyz) < 0.1f || rawDepth >= 1.0f) 
        return sceneColor;

    float2 viewportSize = viewportBoundsUV.zw - viewportBoundsUV.xy;
    float2 viewportUV = (input.uv - viewportBoundsUV.xy) / viewportSize;
    
    float4 clipPos = float4(viewportUV.x * 2.0 - 1.0, 1.0 - viewportUV.y * 2.0, rawDepth, 1.0);
    float4 viewPos = mul(invProjectionMatrix, clipPos);
    viewPos.xyz /= viewPos.w;

    float dist = length(viewPos.xyz);
    
    float3x3 viewToWorld = transpose((float3x3) worldToViewMatrix);
    float3 viewDir = normalize(mul(viewToWorld, viewPos.xyz));
    
    float3 sunDir = normalize(-SunDirection);
    float sunDot = dot(viewDir, sunDir);
    
    float stormActive = smoothstep(0.4, 0.3, SunIntensity);
    
    float dayFactor = smoothstep(-0.15, 0.2, sunDir.z);
    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 4.0)) * smoothstep(-0.1, 0.0, sunDir.z);
    sunsetFactor = lerp(sunsetFactor, dayFactor, stormActive);
    
    float viewHeight = saturate(viewDir.z);
    
    float3 zenithColor = float3(0.05, 0.2, 0.6);
    zenithColor = lerp(zenithColor, float3(0.3, 0.02, 0.02), stormActive);
    
    float3 horizonColorDay = float3(0.6, 0.7, 0.85);
    float3 sunsetColor = float3(1.0, 0.45, 0.1);
    float3 sunsetRed = float3(0.95, 0.3, 0.1);
    
    float3 nightSky = float3(0.02, 0.04, 0.2);
    nightSky = lerp(nightSky, float3(0.05, 0.01, 0.01), stormActive);
    nightSky = lerp(nightSky, float3(0.08, 0.02, 0.01), stormActive);
    
    float3 currentHorizon = lerp(horizonColorDay, sunsetColor, sunsetFactor);
    float3 daySky = lerp(currentHorizon, zenithColor, pow(viewHeight, 0.7));
    
    float glowDist = pow(saturate(sunDot), 4.0);
    float3 sunsetGlow = lerp(sunsetColor, sunsetRed, sunsetFactor);
    daySky = lerp(daySky, sunsetGlow, glowDist * sunsetFactor * (1.0 - viewHeight * 0.5));
    
    float3 finalSky = lerp(nightSky, daySky, dayFactor);
    
    float cosTheta = saturate(sunDot);
    float rayleigh = (1.0 + sunDot * sunDot) * 0.5;
    float3 mieColor = lerp(float3(1.0, 0.95, 0.8), sunsetRed, sunsetFactor);
    float distanceMask = smoothstep(50.0, 300.0, dist);
    float mie = pow(cosTheta, 8.0) * 0.5 * distanceMask;
    
    float3 atmosphereEffect = (daySky * rayleigh * 0.5 + mieColor * mie) * dayFactor;
    
    float3 fogTargetColor = (finalSky * AmbientIntensity) + (atmosphereEffect) * sqrt(SunIntensity);
    
    float FOG_DENSITY = 0.095f;
    float FOG_FALLOFF = 0.025f;
    float opticalDepth = CalculateHeightFog(CameraWorldPosition, viewDir, dist, FOG_DENSITY, FOG_FALLOFF);
    float transmittance = exp(-max(opticalDepth, 0.0));
    float fogFactor = saturate(1.0 - transmittance);
    
    float3 finalColor = lerp(sceneColor.rgb, fogTargetColor, fogFactor);
    
    return float4(finalColor, sceneColor.a);
}