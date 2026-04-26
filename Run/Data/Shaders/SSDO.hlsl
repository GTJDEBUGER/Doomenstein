//------------------------------------------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D originalScreenColor : register(t1);
Texture2D depthTexture : register(t2);
Texture2D originalScreenNormal : register(t3);

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

//------------------------------------------------------------------------------------------------
float InterleavedGradientNoise(float2 pixelPos)
{
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(pixelPos, magic.xy)));
}

float3 GetRandomVec(float2 uv, float2 screenResolution)
{
    float2 pixelPos = uv * screenResolution;
    float noise = InterleavedGradientNoise(pixelPos);
    float angle = noise * 6.28318530718f;
    return float3(cos(angle), sin(angle), 0.0);
}

float3 GetViewPos(float2 uv)
{
    float depth = depthTexture.SampleLevel(screenSampler, uv, 0).r;
    
    float4 clipPos = float4(uv.x * 2.0 - 1.0, (1.0 - uv.y) * 2.0 - 1.0, depth, 1.0);
    float4 viewPos = mul(invProjectionMatrix, clipPos);
    return viewPos.xyz / viewPos.w;
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
    float3 viewPos = GetViewPos(input.uv);
    float3 worldNormal = originalScreenNormal.Sample(screenSampler, input.uv).xyz;
    float rawDepth = depthTexture.Sample(screenSampler, input.uv).r;
    
    if (rawDepth >= 0.9995f || length(worldNormal) < 0.1f) 
        return float4(0, 0, 0, 1.f);

    float3 viewNormal = normalize(mul((float3x3) worldToViewMatrix, worldNormal));
    
    float3 randomVec = GetRandomVec(input.uv, screenResolution);
    
    float3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    float3 bitangent = cross(viewNormal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, viewNormal);

    float ambientOcclusion = 0.0;
    float directionalOcclusion = 0.0;
    float3 colorBleeding = float3(0.0, 0.0, 0.0);
    
    float3 viewSunDir = normalize(mul((float3x3) worldToViewMatrix, -SunDirection));
    
    float dynamicBias = bias * max(0.5f, abs(viewPos.z) * 0.1f);
    
    float totalWeight = 0.0;

    for (int i = 0; i < 64; i++)
    {
        float3 sampleOffset = mul(samples[i].xyz, TBN);
        
        float3 samplePos = viewPos + (sampleOffset * radius) + (viewNormal * dynamicBias);
        
        float4 offsetProj = mul(projectionMatrix, float4(samplePos, 1.0));
        float2 offsetUV = (offsetProj.xy / offsetProj.w) * 0.5 + 0.5;
        offsetUV.y = 1.0 - offsetUV.y;
        
        float3 sceneViewPos = GetViewPos(offsetUV);
        float depthDiff = abs(viewPos.z - sceneViewPos.z);
        
        float rangeCheck = smoothstep(0.0, 1.0, 1.0 - min(1.0, (depthDiff * depthDiff) / (radius * radius)));

        if (abs(sceneViewPos.z) <= abs(samplePos.z) - dynamicBias)
        {
            float3 dirToOccluder = normalize(samplePos - viewPos);
            float3 dirFromOccluder = normalize(viewPos - sceneViewPos);
            
            float weight = max(dot(viewNormal, dirToOccluder), 0.0);
            
            float lightWeight = max(dot(dirToOccluder, viewSunDir), 0.0);
            
            ambientOcclusion += weight * rangeCheck;
            directionalOcclusion += weight * lightWeight * rangeCheck;
            totalWeight += weight;
            
            float distSq = dot(viewPos - sceneViewPos, viewPos - sceneViewPos);
            float bounceAtten = 1.0 / (1.0 + distSq * 5.0);
            
            float3 sceneWorldNormal = originalScreenNormal.SampleLevel(screenSampler, offsetUV, 0).xyz;
            float3 sceneViewNormal = normalize(mul((float3x3) worldToViewMatrix, sceneWorldNormal));
            
            float bounceFacing = saturate(dot(sceneViewNormal, dirFromOccluder));
            float3 bounceColor = originalScreenColor.SampleLevel(screenSampler, offsetUV, 0).rgb;
            
            colorBleeding += bounceColor * weight * bounceFacing * bounceAtten * rangeCheck;
        }
    }
    
    float normalization = max(totalWeight, 0.001f);
    
    float aoFactor = saturate(1.0 - (ambientOcclusion / 64.0) * 2.5f);
    float doFactor = saturate(1.0 - (directionalOcclusion / 64.0) * 5.0f);
    
    float finalOcclusion = pow(aoFactor * doFactor, 1.2f);
    
    colorBleeding = (colorBleeding / 64.0) * 2.0f;

    return float4(colorBleeding, finalOcclusion);
}