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
struct p_out
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 ambient : SV_Target2;
};

//------------------------------------------------------------------------------------------------
cbuffer GameConstants : register(b0)
{
    float GameRunTime;
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
// Procedural Cloud Functions
//------------------------------------------------------------------------------------------------
float hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(hash(i + float3(0, 0, 0)), hash(i + float3(1, 0, 0)), f.x),
                     lerp(hash(i + float3(0, 1, 0)), hash(i + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(hash(i + float3(0, 0, 1)), hash(i + float3(1, 0, 1)), f.x),
                     lerp(hash(i + float3(0, 1, 1)), hash(i + float3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(float3 p)
{
    float f = 0.0;
    float weight = 0.5;
    for (int i = 0; i < 4; i++)
    {
        f += weight * noise(p);
        p *= 2.0;
        weight *= 0.4;
    }
    return f;
}

float GetCloudDensity(float3 p)
{
    p += float3(GameRunTime * 100.0, GameRunTime * 200.0, 0.0);
    float d = fbm(p * 0.0008);
    
    return smoothstep(0.3, 0.9, d);
}

float4 RenderClouds(float3 ro, float3 rd, float3 sunDir, float3 moonDir, float dayFactor, float3 sunsetColor)
{
    if (rd.z <= 0.02)
        return float4(0, 0, 0, 0);
    
    float cloudHeight = 1500.0;
    float dist = cloudHeight / max(rd.z, 0.001);
    float3 startPos = ro + rd * dist;
    
    float3 stepDir = rd * 250.0;
    float totalDensity = 0.0;
    float lightScattering = 0.0;
    
    float3 activeLightDir = lerp(moonDir, sunDir, smoothstep(0.0, 0.2, dayFactor));
    
    for (int i = 0; i < 8; i++)
    {
        float3 p = startPos + stepDir * i;
        float density = GetCloudDensity(p);
        
        if (density > 0.01)
        {
            float shadowDensity = GetCloudDensity(p + activeLightDir * 150.0)
                                + GetCloudDensity(p + activeLightDir * 300.0);
            
            float transmittance = exp(-shadowDensity * 0.5);
            
            totalDensity += density * (1.0 - totalDensity);
            
            lightScattering += density * transmittance * (1.0 - totalDensity) * 1.5;
            
            if (totalDensity > 0.99)
                break;
        }
    }
    
    float3 baseColor = lerp(float3(0.2, 0.25, 0.35), float3(0.85, 0.9, 0.95), dayFactor);
    float3 highlightColor = lerp(float3(0.5, 0.6, 0.8), float3(1.0, 1.0, 1.0), dayFactor);
    
    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 3.0));
    highlightColor = lerp(highlightColor, sunsetColor, sunsetFactor * dayFactor);
    
    float3 finalCloudColor = lerp(baseColor, highlightColor, lightScattering);
    float horizonFade = smoothstep(0.02, 0.15, rd.z);
    
    return float4(finalCloudColor, saturate(totalDensity * horizonFade));
}

//------------------------------------------------------------------------------------------------
// Pixel Shader
//------------------------------------------------------------------------------------------------
p_out PixelMain(v2p_t input)
{
    p_out output;
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
    
    float4 cloudData = RenderClouds(CameraWorldPosition, viewDir, sunDir, moonDir, dayFactor, sunsetColor);
    finalRGB = lerp(finalRGB, cloudData.rgb, cloudData.a);

    float4 color = float4(finalRGB, 1.0f) * ModelColor * input.color;
    color.rgb = saturate(color.rgb);
    
    clip(color.a - 0.01f);
    output.color = color;
    output.normal = float4(0.f, 0.f, 0.f, 1.0f);
    output.ambient = float4(0.f, 0.f, 0.f, 1.0f);
    return output;
}