//----------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D originalScreenColor : register(t1);
Texture2D depthTexture : register(t2);
Texture2D originalScreenNormal : register(t3);
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
    float2 texelSize = float2(0.0f, 1.0f / 800.0f);
    
    float centerDepth = depthTexture.SampleLevel(screenSampler, input.uv, 0).r;
    float3 centerNormal = originalScreenNormal.SampleLevel(screenSampler, input.uv, 0).xyz;
    centerNormal = normalize(centerNormal);
    
    float weights[7] = { 0.015625, 0.09375, 0.234375, 0.3125, 0.234375, 0.09375, 0.015625 };
    
    float4 finalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;
    
    float depthThreshold = 0.01f;
    float normalPower = 64.0f;
    
    for (int i = -3; i <= 3; i++)
    {
        float2 sampleUV = input.uv + texelSize * (float) i;
        
        float4 sampleColor = screenColor.Sample(screenSampler, sampleUV);
        float sampleDepth = depthTexture.SampleLevel(screenSampler, sampleUV, 0).r;
        float3 sampleNormal = originalScreenNormal.SampleLevel(screenSampler, sampleUV, 0).xyz;
        sampleNormal = normalize(sampleNormal);
        
        float depthDiff = abs(centerDepth - sampleDepth);
        float depthWeight = exp(-(depthDiff * depthDiff) / (depthThreshold * depthThreshold));
        
        float normalDot = saturate(dot(centerNormal, sampleNormal));
        float normalWeight = pow(normalDot, normalPower);
        
        float finalWeight = weights[i + 3] * depthWeight * normalWeight;
        
        finalColor += sampleColor * finalWeight;
        totalWeight += finalWeight;
    }
    
    return finalColor / max(totalWeight, 0.0001f);
}