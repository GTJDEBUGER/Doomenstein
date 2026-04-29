Texture2D screenColor : register(t0);
SamplerState screenSampler : register(s0);

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
    float cameraNear;
    float cameraFar;
    float4 viewportBoundsUV;
};

//----------------------------------------------------------------
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

//----------------------------------------------------------------
float GetLuma(float3 rgb)
{
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

//----------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    v2p_t v2p;
    v2p.clipSpacePosition = float4(input.modelSpacePosition, 1.f);
    v2p.color = input.color;
    v2p.uv = input.uv;
    return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float2 texelSize = 1.0 / screenResolution;
    
    float3 rgbNW = screenColor.Sample(screenSampler, input.uv + float2(-1, -1) * texelSize).rgb;
    float3 rgbNE = screenColor.Sample(screenSampler, input.uv + float2(1, -1) * texelSize).rgb;
    float3 rgbSW = screenColor.Sample(screenSampler, input.uv + float2(-1, 1) * texelSize).rgb;
    float3 rgbSE = screenColor.Sample(screenSampler, input.uv + float2(1, 1) * texelSize).rgb;
    float3 rgbM = screenColor.Sample(screenSampler, input.uv).rgb;

    float lumaNW = GetLuma(rgbNW);
    float lumaNE = GetLuma(rgbNE);
    float lumaSW = GetLuma(rgbSW);
    float lumaSE = GetLuma(rgbSE);
    float lumaM = GetLuma(rgbM);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(float2(8.0, 8.0), max(float2(-8.0, -8.0), dir * rcpDirMin)) * texelSize;

    float3 rgbA = 0.5 * (
        screenColor.Sample(screenSampler, input.uv + dir * (1.0 / 3.0 - 0.5)).rgb +
        screenColor.Sample(screenSampler, input.uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    float3 rgbB = rgbA * 0.5 + 0.25 * (
        screenColor.Sample(screenSampler, input.uv + dir * -0.5).rgb +
        screenColor.Sample(screenSampler, input.uv + dir * 0.5).rgb);

    float lumaB = GetLuma(rgbB);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        return float4(rgbA, 1.0);
    return float4(rgbB, 1.0);
}