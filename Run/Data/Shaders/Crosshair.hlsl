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
    float4 sceneColor = screenColor.Sample(screenSampler, input.uv);
    
    float aspectRatio = 2.0f;
    float2 center = float2(0.5f, 0.5f);
    float2 offset = (input.uv - center) * float2(aspectRatio, 1.0f);
    
    float thickness = 0.0015f;
    float size = 0.02f;
    float4 crosshairColor = float4(0.5f, 0.5f, 0.5f, 0.5f);
    
    float absX = abs(offset.x);
    float absY = abs(offset.y);
    bool isInside = (absY < thickness && absX < size) || (absX < thickness && absY < size);
    
    if (isInside)
    {
        float3 finalRGB = lerp(sceneColor.rgb, crosshairColor.rgb, crosshairColor.a);
        return float4(finalRGB, 1.0f);
    }

    return sceneColor;
}