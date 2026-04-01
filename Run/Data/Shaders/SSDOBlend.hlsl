//----------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D originalScreenColor : register(t1);
Texture2D depthTexture : register(t2);
SamplerState screenSampler : register(s0);

//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b1)
{
	float3 SunDirection;
	float SunIntensity;
	float AmbientIntensity;
	float4x4 SunViewProjMatrix;
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
    
    float4 SSDOColor = screenColor.Sample(screenSampler, input.uv);
    
    float giIntensity = 2.5f;
    
    float3 indirectIllumination = SSDOColor.xyz * giIntensity;
    
    float3 finalColor = baseColor * (SSDOColor.a + indirectIllumination * AmbientIntensity);
    
    return float4(finalColor, 1.0f);
}