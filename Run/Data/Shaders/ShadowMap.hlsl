//------------------------------------------------------------------------------------------------
struct vs_input_t
{
    float3 modelPosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float3 modelTangent : TANGENT;
    float3 modelBitangent : BITANGENT;
    float3 modelNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
    float4 worldPosition : WORLDPOSITION;
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float4 worldTangent : TANGENT;
    float4 worldBitangent : BITANGENT;
    float4 worldNormal : NORMAL;
};

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
    float4 ViewportBoundsUV;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
    float4x4 ModelToWorldTransform;
    float4 ModelColor;
};

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    float4 modelPosition = float4(input.modelPosition.xyz, 1.0f);
    float4 worldPosition = mul(ModelToWorldTransform, modelPosition);
    float4 cameraPosition = mul(WorldToCameraTransform, worldPosition);
    float4 renderPosition = mul(CameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(RenderToClipTransform, renderPosition);
    
    float3 worldTangentXYZ = mul((float3x3) ModelToWorldTransform, input.modelTangent.xyz);
    float3 worldBitangentXYZ = mul((float3x3) ModelToWorldTransform, input.modelBitangent.xyz);
    float3 worldNormalXYZ = mul((float3x3) ModelToWorldTransform, input.modelNormal.xyz);
    
    v2p_t v2p;
    
    v2p.worldPosition = float4(worldPosition.xyz, 1.0f);
    v2p.clipPosition = clipPosition;
    v2p.color = float4(input.color.rgba);
    v2p.uv = float2(input.uv.xy);
    
    v2p.worldTangent = float4(worldTangentXYZ.xyz, 0.0f);
    v2p.worldBitangent = float4(worldBitangentXYZ.xyz, 0.0f);
    v2p.worldNormal = float4(worldNormalXYZ.xyz, 0.0f);

    return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    return float4(0, 0, 0, 1);
}