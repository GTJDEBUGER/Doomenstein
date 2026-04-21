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
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
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

float fbm(float3 p, int octaves)
{
    float f = 0.0;
    float weight = 0.525;
    for (int i = 0; i < octaves; i++)
    {
        f += weight * noise(p);
        p *= 2.0;
        weight *= 0.4;
    }
    return f;
}

float2 RaySphereIntersection(float3 ro, float3 rd, float radius)
{
    float2 result;
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float h = b * b - c;
    
    if (h < 0.0)
    {
        result = float2(-1.0, -1.0);
    }
    else
    {
        h = sqrt(h);
        result = float2(-b - h, -b + h);
    }
    
    return result;
}

float GetCloudDensity(float3 p, float heightFrac, bool useDetail, float3 baseMove, float3 detailMove, float coverage)
{
    float result;
    
    float3 pBase = p + baseMove;
    float warp = noise(pBase * 0.00005);
    float3 pWarped = pBase + float3(warp, warp, warp) * 1500.0;
    int baseOctaves = useDetail ? 4 : 2;
    float baseNoise = fbm(pWarped * 0.0003, baseOctaves);
    float heightGradient = smoothstep(0.0, 0.075, heightFrac) * smoothstep(1.0, 0.4, heightFrac);
    
    float baseDensity = saturate((baseNoise - coverage) * 2.095) * heightGradient;
    
    if (!useDetail || baseDensity <= 0.0)
    {
        result = baseDensity * 6.0;
    }
    else
    {
        float3 pDetail = p + detailMove;
        float detailNoise = fbm(pDetail * 0.00275, 4);
        
        float erosionWeight = 1.0 - baseDensity;
        float erosionAmount = detailNoise * erosionWeight * 0.75;
        float finalDensity = saturate((baseDensity - erosionAmount) / (1.0 - erosionAmount));
        result = finalDensity * 6.0;
    }
    
    return result;
}

float PhaseHG(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159 * pow(max(1.0 + g2 - 2.0 * g * cosTheta, 0.001), 1.5));
}

float4 RenderClouds(float3 ro, float3 rd, float3 sunDir, float3 moonDir, float dayFactor, float3 sunsetColor, float2 screenPos)
{
    float4 finalResult = float4(0.0, 0.0, 0.0, 0.0);
    
    float earthRadius = 600000.0;
    float cloudMinHeight = 5000.0;
    float cloudMaxHeight = 15000.0;
    
    float3 earthCenter = float3(0.0, 0.0, -earthRadius);
    float3 localRo = ro - earthCenter;
    float2 tBottom = RaySphereIntersection(localRo, rd, earthRadius + cloudMinHeight);
    float2 tTop = RaySphereIntersection(localRo, rd, earthRadius + cloudMaxHeight);
    
    if (tTop.y >= 0.0)
    {
        float startDist = max(0.0, tBottom.y > 0.0 ? tBottom.y : tTop.x);
        float endDist = tTop.y;
    
        float maxRenderDist = 12000.0;
        endDist = min(endDist, startDist + maxRenderDist);
   
        int steps = (int) lerp(48.0, 72.0, saturate(rd.z * 2.0));
        float stepSize = (endDist - startDist) / float(steps);
    
        float3 seed = float3(screenPos, GameRunTime * 10.0);
        float jitter = hash(seed) * stepSize;
        float currentDist = startDist + jitter;
    
        float3 activeLightDir = lerp(moonDir, sunDir, smoothstep(0.0, 0.2, dayFactor));
        float cosTheta = dot(rd, activeLightDir);
        float phase = lerp(PhaseHG(cosTheta, 0.4), PhaseHG(cosTheta, 0.7), 0.5);
    
        float3 ambientDay = float3(0.55, 0.6, 0.7);
        float3 ambientNight = float3(0.05, 0.08, 0.15);
        float3 baseColor = lerp(ambientNight, ambientDay, dayFactor);
    
        float3 lightDay = float3(1.0, 1.0, 1.0);
        float3 lightNight = float3(0.15, 0.25, 0.4);
        float3 highlightColor = lerp(lightNight, lightDay, dayFactor);
    
        float sunsetFactor = saturate(1.0 - abs(sunDir.z * 4.0)) * smoothstep(-0.1, 0.0, sunDir.z);
        highlightColor = lerp(highlightColor, sunsetColor * 1.5, sunsetFactor);
        baseColor = lerp(baseColor, sunsetColor * 0.35, sunsetFactor);
        float scatteringBoost = lerp(1.2, 1.8, dayFactor * (1.0 - sunsetFactor));
    
        float4 cloudResult = float4(0.0, 0.0, 0.0, 0.0);
    
        float3 initialMove = float3(12000.0, 18000.0, 0.0);
        float3 baseMove = initialMove + float3(GameRunTime * 120.0, GameRunTime * 180.0, 0.0);
        float3 detailMove = baseMove * 1.1 + float3(0.0, 0.0, GameRunTime * -80.0);

        float currentCoverage = lerp(0.45, 0.4125, dayFactor);
        for (int i = 0; i < steps; i++)
        {
            float3 p = ro + rd * currentDist;
            float heightFrac = (length(p - earthCenter) - (earthRadius + cloudMinHeight)) / (cloudMaxHeight - cloudMinHeight);
        
            float density = GetCloudDensity(p, saturate(heightFrac), true, baseMove, detailMove, currentCoverage);
        
            if (density > 0.01)
            {
                float shadowDist = 300.0;
                float shadowHeightFrac = saturate(heightFrac + (activeLightDir.z * shadowDist) / (cloudMaxHeight - cloudMinHeight));
                float shadowDensity = GetCloudDensity(p + activeLightDir * shadowDist, shadowHeightFrac, false, baseMove, detailMove, currentCoverage);
            
                float primaryLight = exp(-(density * 0.6 + shadowDensity) * 2.0);
                float scatterLight = exp(-(density * 0.1 + shadowDensity * 0.2)) * 0.065;
                float transmittance = primaryLight + scatterLight;
            
                float powder = 1.0 - exp(-density * 1.5);
                float distFade = smoothstep(maxRenderDist, maxRenderDist * 0.4, currentDist - startDist);
                float dynamicPhase = lerp(1.0, phase, transmittance);
            
                float ambientHeightGradient = lerp(0.5, 1.0, saturate(heightFrac));
                float ambientOcclusion = lerp(0.4, 1.0, exp(-density * 0.5));
            
                float3 localAmbient = baseColor * ambientHeightGradient * ambientOcclusion * lerp(1.0, 1.75, dayFactor);
                float3 localDirect = highlightColor * transmittance * powder * dynamicPhase * scatteringBoost;
            
                float stepAlpha = saturate(density * stepSize * 0.005 * distFade);
                float directWeight = density * stepSize * 0.01 * distFade;
            
                float3 particleColor = localAmbient * stepAlpha + localDirect * directWeight;
            
                cloudResult.rgb += particleColor * (1.0 - cloudResult.a);
                cloudResult.a += stepAlpha * (1.0 - cloudResult.a);
            
                if (cloudResult.a > 0.95)
                    break;
            }
            currentDist += stepSize;
        }
    
        float viewSunDot = dot(rd, sunDir);
        float directionalGlow = pow(saturate(viewSunDot), 3.0) * sunsetFactor;
        cloudResult.rgb = lerp(cloudResult.rgb, sunsetColor * 1.2, directionalGlow * 0.6 * cloudResult.a);
    
        float horizonFade = smoothstep(0.08, 0.25, rd.z + 0.02);
        cloudResult.rgb *= horizonFade;

        finalResult = float4(cloudResult.rgb, saturate(cloudResult.a * horizonFade));
    }
    
    return finalResult;
}
//------------------------------------------------------------------------------------------------
// Pixel Shader
//------------------------------------------------------------------------------------------------
p_out PixelMain(v2p_t input)
{
    p_out output;
    //Sky background
    float3 viewDir = normalize(input.worldNormal.xyz);
    float3 sunDir = normalize(-SunDirection);
    float3 moonDir = -sunDir;
    
    float sunDot = dot(viewDir, sunDir);
    float dayFactor = smoothstep(-0.15, 0.2, sunDir.z);
    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 4.0)) * smoothstep(-0.1, 0.0, sunDir.z);
    float viewHeight = saturate(viewDir.z);
    
    float3 zenithColor = float3(0.05, 0.2, 0.6);
    float3 horizonColorDay = float3(0.6, 0.7, 0.85);
    float3 sunsetColor = float3(1.0, 0.45, 0.1);
    float3 sunsetRed = float3(0.95, 0.3, 0.1);
    float3 nightSky = float3(0.02, 0.04, 0.2);
    
    float3 currentHorizon = lerp(horizonColorDay, sunsetColor, sunsetFactor);
    float3 daySky = lerp(currentHorizon, zenithColor, pow(viewHeight, 0.7));
    
    float glowDist = pow(saturate(sunDot), 4.0);
    float3 sunsetGlow = lerp(sunsetColor, sunsetRed, sunsetFactor);
    daySky = lerp(daySky, sunsetGlow, glowDist * sunsetFactor * (1.0 - viewHeight * 0.5));
    
    float3 finalSky = lerp(nightSky, daySky, dayFactor);
    
    // Procedural Stars
    float3 starPos = viewDir * 300.0;
    float3 starCell = floor(starPos);
    float3 starLocal = frac(starPos) - 0.5;
    
    float rand = hash(starCell);
    
    float isStar = step(0.985, rand);
    
    float starShape = smoothstep(0.25, 0.0, length(starLocal));
    
    float starIntensity = isStar * starShape * (rand * 3.0 + 1.0);
    float3 stars = float3(starIntensity, starIntensity, starIntensity);
    
    stars *= saturate(1.0 - dayFactor);
    stars *= smoothstep(0.0, 0.2, viewDir.z);

    finalSky += stars;
    
    // Atmospheric Scattering
    float cosTheta = saturate(sunDot);
    float rayleigh = (1.0 + sunDot * sunDot) * 0.5;
    
    float3 mieColor = lerp(float3(1.0, 0.95, 0.8), sunsetRed, sunsetFactor);
    float mie = (pow(cosTheta, 4096) * 4.0 + pow(cosTheta, 8.0) * 0.5);
    float3 atmosphereEffect = (daySky * rayleigh * 0.5 + mieColor * mie) * dayFactor;
    
    // Sun and Moon Disks
    float sunDisk = smoothstep(0.9999, 0.99993, sunDot);
    float3 sunBody = sunDisk * float3(1.0, 0.9, 0.7) * 3.0 * dayFactor;

    float moonDot = dot(viewDir, moonDir);
    float moonDisk = smoothstep(0.9998, 0.99994, moonDot);
    float3 moonBody = moonDisk * float3(0.75, 0.75, 0.85) * saturate(1.0 - dayFactor);
    
    float3 finalRGB = (finalSky * AmbientIntensity) + (sunBody + atmosphereEffect) * sqrt(SunIntensity) + moonBody;
    
    // Procedural Clouds
    float4 cloudData = float4(0.0, 0.0, 0.0, 0.0);
    
    if (viewDir.z > -0.02)
    {
        cloudData = RenderClouds(CameraWorldPosition, viewDir, sunDir, moonDir, dayFactor, sunsetGlow, input.clipPosition.xy);
    }

    finalRGB = finalRGB * (1.0 - cloudData.a) + cloudData.rgb;
    
    float4 color = float4(finalRGB, 1.0f) * ModelColor * input.color;
    color.rgb = saturate(color.rgb);
    
    output.color = color;
    output.normal = float4(0.f, 0.f, 0.0001f, 1.0f);
    return output;
}