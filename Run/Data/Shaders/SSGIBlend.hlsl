//----------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D originalScreenColor : register(t1);
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

float4 PixelMain(v2p_t input) : SV_Target0
{
    float3 baseColor = originalScreenColor.Sample(screenSampler, input.uv).rgb;
    float3 SSGIColor = screenColor.Sample(screenSampler, input.uv).rgb;
    
    return float4(baseColor + SSGIColor, 1.0f);
}