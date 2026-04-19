//------------------------------------------------------------------------------------------------
Texture2D screenColor : register(t0);
SamplerState screenSampler : register(s0);

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

struct vs_input_t
{
    float3 modelSpacePosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct v2p_t
{
    float4 clipSpacePosition : SV_POSITION;
    float2 uv : TEXCOORD;
};

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    v2p_t v2p;
    v2p.clipSpacePosition = float4(input.modelSpacePosition, 1.0f);
    v2p.uv = input.uv;
    return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float3 sunWorldPos = CameraWorldPosition - SunDirection * 100000.0f;
    
    float4 viewPos = mul(worldToViewMatrix, float4(sunWorldPos, 1.0f));
    float4 clipPos = mul(projectionMatrix, viewPos);
    
    if (clipPos.w < -0.5f)
    {
        return screenColor.Sample(screenSampler, input.uv);
    }
    
    clipPos.xyz /= clipPos.w;
    float2 sunScreenPos = clipPos.xy * float2(0.5f, -0.5f) + 0.5f;
    
    float2 texCoord = input.uv;
    float3 originalColor = screenColor.Sample(screenSampler, texCoord).rgb;
    
    float behindCameraFade = saturate(clipPos.w * 2.0f);
    float distFromCenter = distance(sunScreenPos, float2(0.5f, 0.5f));
    float offScreenFade = 1.0f - saturate((distFromCenter - 0.8f) * 1.5f);
    
    float globalGodRayWeight = behindCameraFade * offScreenFade;
    
    if (globalGodRayWeight <= 0.001f)
    {
        return float4(originalColor, 1.0f);
    }
    
    float Density = 1.2f;
    float Weight = 0.01f;
    float Decay = 0.975f;
    float Exposure = 0.65f;
    float BrightnessThreshold = 0.99f;

    float2 deltaTexCoord = (texCoord - sunScreenPos);
    deltaTexCoord *= 1.0f / 64.0f * Density;

    float3 godRays = float3(0.0f, 0.0f, 0.0f);
    float illuminationDecay = 1.0f;
    
    [unroll(64)]
    for (int i = 0; i < 64; i++)
    {
        texCoord -= deltaTexCoord;
        
        float3 sampleColor = screenColor.SampleLevel(screenSampler, texCoord, 0).rgb;
        
        float brightness = dot(sampleColor, float3(0.2126f, 0.7152f, 0.0722f));
        float brightnessWeight = smoothstep(BrightnessThreshold - 0.1f, BrightnessThreshold + 0.02f, brightness);
        sampleColor *= brightnessWeight;
        
        godRays += sampleColor * illuminationDecay * Weight;
        illuminationDecay *= Decay;
    }
    
    float3 finalColor = originalColor + (godRays * Exposure);

    return float4(finalColor, 1.0f);
}