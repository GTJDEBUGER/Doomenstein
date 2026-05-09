//----------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D originalScreenColor : register(t1);
Texture2D depthTexture : register(t2);
SamplerState screenSampler : register(s0);

//----------------------------------------------------------------
struct vs_input_t
{
    float3 modelSpacePosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

//----------------------------------------------------------------
struct v2p_t
{
    float4 clipSpacePosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

//----------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
    float3 CameraWorldPosition;
    float4 ViewportBoundsUV;
};

//----------------------------------------------------------------
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
    float4 vignetteColor;
    float vignetteIntensity;
    float vignetteSmoothness;
    float vignetteRoudness;
    float vignettePixelized;
};

//----------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    v2p_t v2p;
    v2p.clipSpacePosition = float4(input.modelSpacePosition, 1.f);
    v2p.color = input.color;
    v2p.uv = input.uv;
    return v2p;
}

//----------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float2 viewportSize = ViewportBoundsUV.zw - ViewportBoundsUV.xy;
    float2 localUV = (input.uv - ViewportBoundsUV.xy) / viewportSize;
    float aspect = (screenResolution.x * viewportSize.x) / (screenResolution.y * viewportSize.y);
    
    float2 workingLocalUV = localUV;
    
    if (vignettePixelized > 0.0f)
    {
        float baseResY = screenResolution.y * viewportSize.y;
        
        float maxPixelSize = max(baseResY / 24.0f, 1.0f);
        float currentPixelSize = lerp(1.0f, maxPixelSize, vignettePixelized);
        
        float targetBlocksY = max(baseResY / currentPixelSize, 2.0f);
        float2 gridSize = float2(targetBlocksY * aspect, targetBlocksY);
        
        workingLocalUV = floor(localUV * gridSize) / gridSize;
        workingLocalUV += 0.5f / gridSize;
        workingLocalUV = saturate(workingLocalUV);
    }
    
    float2 sampleUV = workingLocalUV * viewportSize + ViewportBoundsUV.xy;
    float4 baseColor = screenColor.Sample(screenSampler, sampleUV);
    
    float2 delta = workingLocalUV - 0.5f;
    delta.x *= lerp(1.0f, aspect, vignetteRoudness);
    
    float dist = length(delta);
    
    float maxDist = length(float2(0.5f * lerp(1.0f, aspect, vignetteRoudness), 0.5f));
    float outerRadius = lerp(maxDist * 2.0f, 0.0f, vignetteIntensity);
    float innerRadius = outerRadius - vignetteSmoothness * maxDist;
    float vignetteWeight = smoothstep(innerRadius, outerRadius, dist);
    
    float3 finalColor = lerp(baseColor.rgb, vignetteColor.rgb, vignetteWeight * vignetteColor.a);
    
    return float4(finalColor, baseColor.a);
}