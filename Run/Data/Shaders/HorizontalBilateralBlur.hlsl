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
    float2 texelSize = float2(1.0f / 1600.0f, 0.0f);
    
    float4 centerColor = screenColor.Sample(screenSampler, input.uv);
    
    float sigmaS = 4.0f;
    float sigmaR = 0.3f;
    
    float4 resultColor = float4(0, 0, 0, 0);
    float totalWeight = 0.0f;
    
    int radius = 4;
    
    for (int i = -radius; i <= radius; i++)
    {
        float2 offset = float2((float) i, 0.0f) * texelSize;
        float4 sampleColor = screenColor.SampleLevel(screenSampler, input.uv + offset, 0);
        
        float spatialWeight = exp(-(float) (i * i) / (2.0f * sigmaS * sigmaS));
        
        float colorDiff = distance(centerColor.rgb, sampleColor.rgb);
        float rangeWeight = exp(-(colorDiff * colorDiff) / (2.0f * sigmaR * sigmaR));
        
        float weight = spatialWeight * rangeWeight;
        
        resultColor += sampleColor * weight;
        totalWeight += weight;
    }
    
    return resultColor / totalWeight;
}