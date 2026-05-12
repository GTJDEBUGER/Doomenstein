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
    float IsStencilPass;
    
    // --- Weather ---
    float WeatherCoverage; // 0.45
    float WeatherDensity; // 6.0
    float WeatherAbsorption; // 2.0
    
    float WeatherDarkness; // 0.4
    float WeatherCloudMinY; // 5000.0
    float WeatherCloudMaxY; // 15000.0
    
    float2 StormCenter;
    float StormRadius;
    float StormTwistStrength;
    float StormFunnelDepth;
    float StormEyeRadius;
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
                     lerp(hash(i + float3(0, 1, 1)), hash(i + float3(1, 1, 1)),
f.x), f.y), f.z);
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

float3 ApplyStormVortex(float3 p, float3 earthCenter, out float eyeMask, out float stormWeight)
{
    eyeMask = 1.0f;
    stormWeight = 0.0f;
    
    if (StormRadius <= 0.01f)
        return p;

    float2 offset = p.xy - StormCenter;
    float distSq = dot(offset, offset);
    float radSq = StormRadius * StormRadius;
    
    if (distSq > radSq)
        return p;
    
    float dist = sqrt(distSq);
    float r_norm = saturate(dist / StormRadius);
    
    float falloff = smoothstep(1.0, 0.0, r_norm);
    stormWeight = falloff;
    
    float rigidSpin = GameRunTime * 0.15f;
    float spatialTwist = StormTwistStrength * pow(falloff, 1.5f);
    float twist = spatialTwist + rigidSpin;
    
    float sinT, cosT;
    sincos(twist, sinT, cosT);
    
    float2 rotatedXY = float2(
        offset.x * cosT - offset.y * sinT,
        offset.x * sinT + offset.y * cosT
    );
    
    float3 warpedP = p;
    warpedP.xy = StormCenter + rotatedXY;
    
    float3 upDir = normalize(p - earthCenter);
    warpedP += upDir * (pow(falloff, 2.0f) * StormFunnelDepth);
    
    float heightEst = length(p - earthCenter) - 48000.0;
    float hFrac = saturate((heightEst - WeatherCloudMinY) / (WeatherCloudMaxY - WeatherCloudMinY));
    
    float dynamicEyeRadius = StormEyeRadius * lerp(0.3f, 1.6f, hFrac);
    eyeMask = smoothstep(dynamicEyeRadius * 0.4f, dynamicEyeRadius * 1.2f, dist);
    
    return warpedP;
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
        result = baseDensity * WeatherDensity;
    }
    else
    {
        float3 pDetail = p + detailMove;
        float detailNoise = fbm(pDetail * 0.00250, 3);
        
        float erosionWeight = 1.0 - baseDensity;
        float erosionAmount = detailNoise * erosionWeight * 0.75;
        float finalDensity = saturate((baseDensity - erosionAmount) / (1.0 - erosionAmount));
        result = finalDensity * WeatherDensity;
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
    float earthRadius = 48000.0;
    
    float cloudMinHeight = WeatherCloudMinY;
    float cloudMaxHeight = WeatherCloudMaxY;
    
    float actualMinHeight = cloudMinHeight;
    if (StormRadius > 0.01f)
    {
        actualMinHeight = max(100.0, cloudMinHeight - StormFunnelDepth);
    }
    
    float3 earthCenter = float3(0.0, 0.0, -earthRadius);
    float3 localRo = ro - earthCenter;
    
    float2 tBottom = RaySphereIntersection(localRo, rd, earthRadius + actualMinHeight);
    float2 tTop = RaySphereIntersection(localRo, rd, earthRadius + cloudMaxHeight);
    
    if (tTop.y >= 0.0)
    {
        float startDist = max(0.0, tBottom.y > 0.0 ? tBottom.y : tTop.x);
        float endDist = tTop.y;
    
        float maxRenderDist = 12000.0;
        endDist = min(endDist, startDist + maxRenderDist);
        int steps = (int) lerp(48.0, 64.0, saturate(rd.z * 2.0));
        float stepSize = (endDist - startDist) / float(steps);
        float3 seed = float3(screenPos, GameRunTime * 10.0);
        float jitter = hash(seed) * stepSize;
        float currentDist = startDist + jitter;
        float3 activeLightDir = lerp(moonDir, sunDir, smoothstep(0.0, 0.2, dayFactor));
        float cosTheta = dot(rd, activeLightDir);
        float phase = lerp(PhaseHG(cosTheta, 0.4), PhaseHG(cosTheta, 0.7), 0.5);
    
        float stormActive = smoothstep(0.0, 5000.0, StormRadius);

        float3 ambientDay = float3(0.55, 0.6, 0.7);
        float3 ambientNight = float3(0.05, 0.08, 0.15);
    
        ambientDay = lerp(ambientDay, float3(0.6, 0.3, 0.15), stormActive);
        
        ambientNight = lerp(ambientNight, float3(0.35, 0.08, 0.05), stormActive);
        
        float3 baseColor = lerp(ambientNight, ambientDay, dayFactor);

        float3 lightDay = float3(1.0, 1.0, 1.0);
        float3 lightNight = float3(0.15, 0.25, 0.4);
        
        lightNight = lerp(lightNight, float3(0.7, 0.15, 0.08), stormActive);
    
        float3 highlightColor = lerp(lightNight, lightDay, dayFactor);

        float sunsetFactor = saturate(1.0 - abs(sunDir.z * 4.0)) * smoothstep(-0.1, 0.0, sunDir.z);
        sunsetFactor = lerp(sunsetFactor, dayFactor, stormActive);
        highlightColor = lerp(highlightColor, sunsetColor * 1.5, sunsetFactor);
        baseColor = lerp(baseColor, sunsetColor * 0.35, sunsetFactor);
        float scatteringBoost = lerp(1.2, 1.8, dayFactor * (1.0 - sunsetFactor));
    
        float4 cloudResult = float4(0.0, 0.0, 0.0, 0.0);
        float3 initialMove = float3(12000.0, 18000.0, 0.0);
        float3 baseMove = initialMove + float3(GameRunTime * 120.0, GameRunTime * 180.0, 0.0);
        float3 detailMove = baseMove * 1.1 + float3(0.0, 0.0, GameRunTime * -80.0);
        
        float currentCoverage = lerp(WeatherCoverage, WeatherCoverage - 0.0375, dayFactor);
        float distFade = smoothstep(maxRenderDist, maxRenderDist * 0.7, currentDist - startDist);
        
        float tBolt = -1.0;
        float3 lightningColorToInject = float3(0.0, 0.0, 0.0);
        float globalFlashEnergy = 0.0;

        if (StormRadius > 0.01f)
        {
            float strikeTime = GameRunTime * 1.5;
            float strikeBeat = floor(strikeTime);
            float strikeFract = frac(strikeTime);
            float strikeChance = hash(float3(strikeBeat, 13.0, 27.0));
    
            float doStrike = step(0.4, strikeChance);
            float strobe = pow(sin(strikeFract * 40.0) * 0.5 + 0.5, 3.0);
            float suddenFlash = exp(-strikeFract * 6.0) * strobe * doStrike;
    
            globalFlashEnergy = suddenFlash;
            
            float2 dirToStorm = normalize(StormCenter - ro.xy);
            float denom = dot(rd.xy, dirToStorm);
    
            if (denom > 0.001 && suddenFlash > 0.001)
            {
                float t = length(StormCenter - ro.xy) / denom;
        
                if (t > startDist && t < endDist)
                {
                    tBolt = t;
                    float3 hitPos = ro + rd * tBolt;
            
                    float2 planeRight = float2(-dirToStorm.y, dirToStorm.x);
                    float u = dot(hitPos.xy - StormCenter, planeRight);
                    float v = hitPos.z;
            
                    float zFrac = saturate((cloudMaxHeight - v) / (cloudMaxHeight - cloudMinHeight));
            
                    float spread = lerp(150.0, 5000.0, zFrac);
            
                    float nv = v * 0.0002;
                    float tNoise = GameRunTime * 4.0;
            
                    float warp1 = noise(float3(0.0, nv * 8.0, tNoise)) * 2.0 - 1.0;
                    float warp2 = noise(float3(0.0, nv * 22.0, tNoise * 1.3)) * 2.0 - 1.0;
                    float trunkU = u + (warp1 + warp2 * 0.4) * spread;
                    float distToTrunk = abs(trunkU);
            
                    float core = smoothstep(30.0, 0.0, distToTrunk);
                    float halo = smoothstep(500.0, 0.0, distToTrunk);
            
                    float branchNoise = noise(float3(trunkU * 0.0008, nv * 12.0, tNoise * 0.8)) * 2.0 - 1.0;
                    float branchDist = abs(trunkU - branchNoise * spread * 1.5);
            
                    float branchMask = smoothstep(2500.0, 0.0, distToTrunk) * smoothstep(0.1, 0.6, zFrac);
            
                    float branchCore = smoothstep(20.0, 0.0, branchDist) * branchMask;
                    float branchHalo = smoothstep(250.0, 0.0, branchDist) * branchMask;
            
                    float finalCore = max(core, branchCore);
                    float finalHalo = max(halo, branchHalo);
            
                    lightningColorToInject = finalCore * float3(1.0, 0.9, 0.6) * 150.0 + finalHalo * float3(1.0, 0.05, 0.01) * 35.0;
                    lightningColorToInject *= suddenFlash;
            
                    lightningColorToInject *= smoothstep(0.0, 0.2, zFrac);
                }
            }
        }
        
        for (int i = 0; i < steps; i++)
        {
            float3 p = ro + rd * currentDist;
            float eyeMask = 1.0f;
            float stormWeight = 0.0f;
            p = ApplyStormVortex(p, earthCenter, eyeMask, stormWeight);
            
            float3 localBaseMove = lerp(baseMove, float3(0.0, 0.0, GameRunTime * -250.0), stormWeight);
            float3 localDetailMove = lerp(detailMove, float3(0.0, 0.0, GameRunTime * -500.0), stormWeight);

            float heightFrac = (length(p - earthCenter) - (earthRadius + cloudMinHeight)) / (cloudMaxHeight - cloudMinHeight);
    
            float density = GetCloudDensity(p, saturate(heightFrac), true, localBaseMove, localDetailMove, currentCoverage);
            density *= eyeMask;

            if (density > 0.01)
            {
                float shadowDist = 300.0;
                float shadowHeightFrac = saturate(heightFrac + (activeLightDir.z * shadowDist) / (cloudMaxHeight - cloudMinHeight));
                float shadowDensity = GetCloudDensity(p + activeLightDir * shadowDist, shadowHeightFrac, false, localBaseMove, localDetailMove, currentCoverage);
                shadowDensity *= eyeMask;
                shadowDensity *= eyeMask;
                
                float primaryLight = exp(-(density * 0.6 + shadowDensity) * WeatherAbsorption);
                float scatterLight = exp(-(density * 0.1 + shadowDensity * 0.2)) * 0.065;
                float transmittance = primaryLight + scatterLight;
                float powder = 1.0 - exp(-density * 1.5);
                float dynamicPhase = lerp(1.0, phase, transmittance);
            
                float ambientHeightGradient = lerp(0.5, 1.0, saturate(heightFrac));
                
                float ambientOcclusion = lerp(WeatherDarkness, 1.0, exp(-density * 0.5));
                
                float3 localAmbient = baseColor * ambientHeightGradient * ambientOcclusion * lerp(1.0, 1.75, dayFactor);
                float3 localDirect = highlightColor * transmittance * powder * dynamicPhase * scatteringBoost;
                float stepAlpha = saturate(density * stepSize * 0.005 * distFade);
                float directWeight = density * stepSize * 0.01 * distFade;
                float3 particleColor = localAmbient * stepAlpha + localDirect * directWeight;
                if (StormRadius > 0.01f)
                {
                    float3 warp = float3(noise(p * 0.0001), noise(p * 0.0001 + 13.0), noise(p * 0.0001 + 27.0));
                    float3 cellPos = p * 0.0003 + warp * 1.5;
                    float3 cell = floor(cellPos);
                    float3 localPos = frac(cellPos);
            
                    float3 edgeFade = sin(localPos * 3.1415926);
                    float cellMask = pow(edgeFade.x * edgeFade.y * edgeFade.z, 3.0);
                    
                    float cellHash = hash(cell);
                    float timeScale = GameRunTime * 2.0 + cellHash * 100.0;
                    float currentBeat = floor(timeScale);
                    float beatFract = frac(timeScale);
            
                    float strikeChance = hash(cell + currentBeat * 13.0);
                    float doStrike = step(0.4, strikeChance);
            
                    float strobe = sin(beatFract * 25.0) * 0.5 + 0.5;
                    float suddenFlash = exp(-beatFract * 5.0) * strobe * doStrike;
            
                    float spatialGlow = smoothstep(0.4, 0.8, noise(p * 0.0005));
            
                    float flashEnergy = suddenFlash * spatialGlow * cellMask;
            
                    if (flashEnergy > 0.001)
                    {
                        float scatteringVolume = density * exp(-density * 3.5);
                        float volumetricMask = scatteringVolume * 18.0;
                        float profile = flashEnergy * volumetricMask;
                        
                        float absorbWeight = smoothstep(0.0, 0.3, flashEnergy * density);
                        particleColor.g *= lerp(1.0, 0.05, absorbWeight);
                        particleColor.b *= lerp(1.0, 0.0, absorbWeight);
                        particleColor.r *= lerp(1.0, 1.2, absorbWeight);
                        
                        float r = profile;
                
                        float g = pow(profile, 2.5) * 0.12;
                
                        float b = 0.0;
                
                        float3 emissionColor = float3(r, g, b) * 20.0;
                
                        emissionColor.g = min(emissionColor.g, 0.35);
                        
                        particleColor += emissionColor * stepSize * 0.01 * distFade;
                    }
                }
            
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
        //float horizonFade = smoothstep(0.08, 0.25, rd.z + 0.02);
        float horizonFade = smoothstep(0.01, 0.15, rd.z);
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
    float stormActive = smoothstep(0.0, 5000.0, StormRadius);

    float sunsetFactor = saturate(1.0 - abs(sunDir.z * 4.0)) * smoothstep(-0.1, 0.0, sunDir.z);
    sunsetFactor = lerp(sunsetFactor, dayFactor, stormActive);

    float viewHeight = saturate(viewDir.z);
    float3 zenithColor = float3(0.05, 0.2, 0.6);
    float3 horizonColorDay = float3(0.6, 0.7, 0.85);
    float3 sunsetColor = float3(1.0, 0.45, 0.1);
    float3 sunsetRed = float3(0.95, 0.3, 0.1);
    float3 nightSky = float3(0.02, 0.04, 0.2);
    
    zenithColor = lerp(zenithColor, float3(0.4, 0.08, 0.02), stormActive);
    nightSky = lerp(nightSky, float3(0.08, 0.02, 0.01), stormActive);
    
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