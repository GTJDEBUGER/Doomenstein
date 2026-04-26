//------------------------------------------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D depthTexture : register(t2);
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
    float2 texCoord = input.uv;
    float4 originalSample = screenColor.Sample(screenSampler, texCoord);
    float3 originalColor = originalSample.rgb;
    
    float3 totalGodRays = float3(0.0f, 0.0f, 0.0f);
    
    float Density = 1.2f;
    float Weight = 0.01f;
    float Decay = 0.975f;
    float Exposure = 0.65f;
    float BrightnessThreshold = 0.99f;

    // ==========================================
    // Directional Light
    // ==========================================
    float3 sunWorldPos = CameraWorldPosition - SunDirection * 100000.0f;
    float4 sunViewPos = mul(worldToViewMatrix, float4(sunWorldPos, 1.0f));
    float4 sunClipPos = mul(projectionMatrix, sunViewPos);
    
    if (sunClipPos.w >= -0.5f)
    {
        sunClipPos.xyz /= sunClipPos.w;
        float2 sunScreenPos = sunClipPos.xy * float2(0.5f, -0.5f) + 0.5f;
        
        float behindCameraFade = saturate(sunClipPos.w * 2.0f);
        float distFromCenter = distance(sunScreenPos, float2(0.5f, 0.5f));
        float offScreenFade = 1.0f - saturate((distFromCenter - 0.8f) * 1.5f);
        float globalGodRayWeight = behindCameraFade * offScreenFade;
        
        if (globalGodRayWeight > 0.001f)
        {
            float2 deltaTexCoord = (texCoord - sunScreenPos);
            deltaTexCoord *= 1.0f / 64.0f * Density;

            float3 sunGodRays = float3(0.0f, 0.0f, 0.0f);
            float illuminationDecay = 1.0f;
            float2 currentUV = texCoord;
            
            [unroll(64)]
            for (int i = 0; i < 64; i++) 
            {
                currentUV -= deltaTexCoord; 
                float3 sampleColor = screenColor.SampleLevel(screenSampler, currentUV, 0).rgb; 
                
                float brightness = dot(sampleColor, float3(0.2126f, 0.7152f, 0.0722f)); 
                float brightnessWeight = smoothstep(BrightnessThreshold - 0.1f, BrightnessThreshold + 0.02f, brightness);
                sampleColor *= brightnessWeight;
                
                sunGodRays += sampleColor * illuminationDecay * Weight;
                illuminationDecay *= Decay;
            }
            totalGodRays += sunGodRays;
        }
    }

    // ==========================================
    // Point Lights
    // ==========================================
    float depthVal = depthTexture.SampleLevel(screenSampler, texCoord, 0).r;
    
    float4 ndcPixelPos = float4(texCoord.x * 2.0f - 1.0f, 1.0f - texCoord.y * 2.0f, depthVal, 1.0f);
    float4 viewPixelPos = mul(invProjectionMatrix, ndcPixelPos);
    viewPixelPos.xyz /= viewPixelPos.w;
    
    float pixelRayLength = length(viewPixelPos.xyz);
    
    float3 viewDir = normalize(viewPixelPos.xyz);

    float3 pointLightScattering = float3(0.0f, 0.0f, 0.0f);
    float BaseScatteringDensity = 0.0075f;
    
    for (uint j = 0; j < NumPointLights; j++)
    {
        float3 lightWorldPos = PointLights[j].Position;
        float radius = PointLights[j].Range;
        
        float3 lightViewPos = mul(worldToViewMatrix, float4(lightWorldPos, 1.0f)).xyz;
        
        float3 L = lightViewPos;
        float tca = dot(L, viewDir);
        float d2 = dot(L, L) - tca * tca;

        if (d2 > radius * radius)
            continue;

        float thc = sqrt(radius * radius - d2);
        
        float t0 = tca - thc;
        float t1 = tca + thc;

        if (t1 < 0.0f)
            continue;
        
        t1 = min(t1, pixelRayLength);
        
        t0 = max(0.0f, t0);
        
        if (t0 >= t1)
            continue;
        
        float d = max(sqrt(d2), 0.01f);
        
        float x0 = t0 - tca;
        float x1 = t1 - tca;
        
        float integral = (atan(x1 / d) - atan(x0 / d)) / d;

        float edgeFade = saturate(1.0f - (d / radius));
        
        pointLightScattering += PointLights[j].Color.rgb * PointLights[j].Intensity * integral * BaseScatteringDensity * edgeFade * PointLights[j].Volumetric;
    }

    totalGodRays += pointLightScattering;

    // ==========================================
    // Final Composition
    // ==========================================
    float3 finalColor = originalColor + (totalGodRays * Exposure);
    return float4(finalColor, 1.0f);
}