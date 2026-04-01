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
cbuffer LightConstants : register(b1)
{
    float3 SunDirection;
    float SunIntensity;
    float AmbientIntensity;
    float4x4 SunViewProjMatrix;
};

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
    float3 CameraWorldPosition;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
    float4x4 ModelToWorldTransform;
    float4 ModelColor;
};

//------------------------------------------------------------------------------------------------
// Vertex Shader
//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    float4 modelPosition = float4(input.modelPosition, 1);
    float4 worldPosition = mul(ModelToWorldTransform, modelPosition);
    float4 cameraPosition = mul(WorldToCameraTransform, worldPosition);
    float4 renderPosition = mul(CameraToRenderTransform, cameraPosition);
    float4 clipPosition = mul(RenderToClipTransform, renderPosition);

    float4 worldNormal = mul(ModelToWorldTransform, float4(input.modelNormal, 0.0f));

    v2p_t v2p;
    v2p.worldPosition = worldPosition;
    v2p.clipPosition = clipPosition;
    v2p.color = input.color;
    v2p.uv = input.uv;
    v2p.worldTangent = mul(ModelToWorldTransform, float4(input.modelTangent, 0.0f));
    v2p.worldBitangent = mul(ModelToWorldTransform, float4(input.modelBitangent, 0.0f));
    v2p.worldNormal = worldNormal;
    
    return v2p;
}

//------------------------------------------------------------------------------------------------
// Pixel Shader
//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    float3 viewDir = normalize(input.worldNormal.xyz);
    float3 sunDir = normalize(-SunDirection);
    float3 moonDir = -sunDir;
    
    float dayFactor = smoothstep(-0.1, 0.2, sunDir.z);
    
    float moonVisibility = smoothstep(0.5, 0.0, dayFactor);
    
    float3 zenithColor = float3(0.05, 0.2, 0.6);
    float3 horizonColorDay = float3(0.6, 0.7, 0.85);
    float3 sunsetColor = float3(1.0, 0.4, 0.1);
    float3 nightSky = float3(0.02, 0.05, 0.2); 
    
    float viewHeight = saturate(viewDir.z);
    
    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 3.0));
    float3 currentHorizon = lerp(horizonColorDay, sunsetColor, sunsetFactor);
    
    float3 daySky = lerp(currentHorizon, zenithColor, pow(viewHeight, 0.8));
    
    float3 finalSky = lerp(nightSky, daySky, dayFactor);
    
    float sunDot = dot(viewDir, sunDir);
    float cosTheta = saturate(sunDot);
    
    float rayleigh = (1.0 + sunDot * sunDot) * 0.5;
    
    float mie = pow(cosTheta, 2048.0) * 2.0 + pow(cosTheta, 8.0) * 0.3;
    
    float3 atmosphereEffect = (daySky * rayleigh + float3(1.0, 0.9, 0.7) * mie) * dayFactor;

    
    float sunDisk = smoothstep(0.9998, 0.99995, sunDot);
    float3 sunBody = sunDisk * float3(1.0, 1.0, 0.9) * 2.0 * dayFactor;

    float moonDot = dot(viewDir, moonDir);
    float moonDisk = smoothstep(0.9998, 0.99995, moonDot);
    float3 moonBody = moonDisk * float3(0.7, 0.7, 0.8) * moonVisibility;

    
    float3 finalRGB = (finalSky * AmbientIntensity) + (sunBody + atmosphereEffect) * sqrt(SunIntensity) + moonBody;

    float4 color = float4(finalRGB, 1.0f) * ModelColor * input.color;
    color.rgb = saturate(color.rgb);
    
    clip(color.a - 0.01f);
    
    return color;
}