Texture2D colorTexture : register(t0);
Texture2D depthTexture : register(t1);
Texture2D normalTexture : register(t2);
SamplerState screenSampler : register(s0);

//----------------------------------------------------------------
static const float GaussianWeights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

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
    float2 texelSize = float2(1.0 / 1600.0, 1.0/800.0);
    float depthTolerance = 0.001;
    float normalTolerance = 0.8;
    
    float2 uv = input.uv;
    
    float3 centerColor = colorTexture.SampleLevel(screenSampler, uv, 0).rgb;
    float centerDepth = depthTexture.SampleLevel(screenSampler, uv, 0).r;
    float3 centerNormal = normalTexture.SampleLevel(screenSampler, uv, 0).xyz;
    
    float3 finalColor = centerColor * GaussianWeights[0];
    float totalWeight = GaussianWeights[0];
    
    for (int i = 1; i < 5; ++i)
    {
        float2 offsetUV_Right = uv + float2(texelSize.x * i, 0.0);
        float sampleDepth_R = depthTexture.SampleLevel(screenSampler, offsetUV_Right, 0).r;
        float3 sampleNormal_R = normalTexture.SampleLevel(screenSampler, offsetUV_Right, 0).xyz;
        
        float depthWeight_R = exp(-abs(centerDepth - sampleDepth_R) / depthTolerance);
        float normalWeight_R = pow(max(dot(centerNormal, sampleNormal_R), 0.0), 32.0f);
        float weight_R = GaussianWeights[i] * depthWeight_R * normalWeight_R;
        
        finalColor += colorTexture.SampleLevel(screenSampler, offsetUV_Right, 0).rgb * weight_R;
        totalWeight += weight_R;
        
        float2 offsetUV_Left = uv - float2(texelSize.x * i, 0.0);
        float sampleDepth_L = depthTexture.SampleLevel(screenSampler, offsetUV_Left, 0).r;
        float3 sampleNormal_L = normalTexture.SampleLevel(screenSampler, offsetUV_Left, 0).xyz;
        
        float depthWeight_L = exp(-abs(centerDepth - sampleDepth_L) / depthTolerance);
        float normalWeight_L = pow(max(dot(centerNormal, sampleNormal_L), 0.0), 32.0f);
        float weight_L = GaussianWeights[i] * depthWeight_L * normalWeight_L;
        
        finalColor += colorTexture.SampleLevel(screenSampler, offsetUV_Left, 0).rgb * weight_L;
        totalWeight += weight_L;
    }
    
    return float4(finalColor / max(totalWeight, 0.0001f), 1.0);
}