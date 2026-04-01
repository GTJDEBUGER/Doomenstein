//------------------------------------------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D originalScreenColor : register(t1);
Texture2D depthTexture : register(t2);
Texture2D originalScreenNormal : register(t3);

SamplerState screenSampler : register(s0);

//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b1)
{
    float3 SunDirection;
    float SunIntensity;
    float AmbientIntensity;
    float4x4 SunViewProjMatrix;
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
float3 GetRandomVec(float2 uv)
{
    float angle = frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453) * 6.283185;
    return float3(cos(angle), sin(angle), 0.0);
}

float3 GetViewPos(float2 uv)
{
    float depth = depthTexture.Sample(screenSampler, uv).r;
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
    
    if (rawDepth >= 0.9995f) 
        return float4(0, 0, 0, 1.f);
    
    if (length(worldNormal) < 0.1)
        return originalScreenColor.Sample(screenSampler, input.uv);
    
    float3 viewNormal = normalize(mul((float3x3) worldToViewMatrix, worldNormal));
    
    float3 randomVec = GetRandomVec(viewPos.xy * noiseScale);
    float3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    float3 bitangent = cross(viewNormal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, viewNormal);

    float occlusion = 0.0;
    float3 directOcclusion = 0.0;
    float3 colorBleeding = 0.0;
    
    float3 viewSunDir = normalize(mul((float3x3) worldToViewMatrix, -SunDirection));

    for (int i = 0; i < 64; i++)
    {
        float3 sampleOffset = mul(samples[i].xyz, TBN) * radius;
        float3 samplePos = viewPos + sampleOffset;
        
        float4 offsetProj = mul(projectionMatrix, float4(samplePos, 1.0));
        float2 offsetUV = (offsetProj.xy / offsetProj.w) * 0.5 + 0.5;
        offsetUV.y = 1.0 - offsetUV.y;
        
        float3 sceneViewPos = GetViewPos(offsetUV);
        
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(viewPos.z - sceneViewPos.z));
        
        if (sceneViewPos.z <= samplePos.z - bias)
        {
            float3 dirToOccluder = normalize(sampleOffset);
            float3 dirFromOccluder = normalize(viewPos - sceneViewPos);

            float weight = max(dot(viewNormal, dirToOccluder), 0.0);
            float lightWeight = max(dot(dirToOccluder, viewSunDir), 0.0);
            
            float dist = length(viewPos - sceneViewPos);
            float bounceAtten = saturate(1.0 - (dist / radius));
            bounceAtten = bounceAtten * bounceAtten;
    
            float3 sceneWorldNormal = originalScreenNormal.SampleLevel(screenSampler, offsetUV, 0).xyz;
            float3 sceneViewNormal = normalize(mul((float3x3) worldToViewMatrix, sceneWorldNormal));
            float bounceFacing = max(dot(sceneViewNormal, dirFromOccluder), 0.0);
    
            directOcclusion += weight * lightWeight * rangeCheck;
    
            float3 bounceColor = originalScreenColor.SampleLevel(screenSampler, offsetUV, 0).rgb;
            colorBleeding += bounceColor * weight * bounceFacing * bounceAtten * rangeCheck;
        }
    }
    
    float rawOcclusion = directOcclusion / 64.0;
    float occlusionIntensity = 8.0f;
    float occlusionFactor = saturate(1.0 - (rawOcclusion * occlusionIntensity));
    occlusionFactor = pow(occlusionFactor, 2.2f);
    
    colorBleeding /= 64.0;
    
    return float4(colorBleeding, occlusionFactor);
}