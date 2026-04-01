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
    float4 color = screenColor.Sample(screenSampler, input.uv);
    float rawDepth = depthTexture.Sample(screenSampler, input.uv).r;
    
    if (rawDepth >= 0.9995f) 
        return float4(0, 0, 0, color.a);
    
    float near = 0.1f;
    float far = 1000.f;
    float linearDepth = (2.0f * near * far) / (far + near - (rawDepth * 2.0f - 1.0f) * (far - near));
    float normalizedDepth = saturate(linearDepth / far);
    
    float luma = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float threshold = 0.4f;
    float knee = 0.2f;
    float soft = luma - threshold + knee;
    soft = clamp(soft, 0, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 0.00001f);
    float contribution = max(soft, luma - threshold) / max(luma, 0.00001f);
    
    float bloomScale = saturate(1.0f - normalizedDepth);
    
    bloomScale = pow(bloomScale, 3.0f);

    return float4(color.rgb * contribution * bloomScale, color.a);
}