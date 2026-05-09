//------------------------------------------------------------------------------------------------
// Textures & Samplers
//------------------------------------------------------------------------------------------------
Texture2D screenColor : register(t0);
Texture2D originalScreenColor : register(t1);
Texture2D depthTexture : register(t2);
Texture2D originalScreenNormal : register(t3);
Texture2DMS<uint4> depthStencilTexture : register(t4);
Texture2D seaNormalTexture : register(t5);
Texture2D sunShadowTexture : register(t6);

SamplerState screenSampler : register(s0);
SamplerState texSampler : register(s1);

//------------------------------------------------------------------------------------------------
// Constant Buffers
//------------------------------------------------------------------------------------------------
cbuffer GameConstants : register(b0)
{
    float GameRunTime;
    float GridUVSize;
    float _GamePadding[6];
};

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
    float3 _LightPadding1;
    float4x4 SunViewProjMatrix;
    uint NumPointLights;
    float3 _LightPadding2;
    PointLight PointLights[MAX_POINT_LIGHTS];
};

cbuffer CameraConstants : register(b2)
{
    float4x4 WorldToCameraTransform;
    float4x4 CameraToRenderTransform;
    float4x4 RenderToClipTransform;
    float3 CameraWorldPosition;
    float _CameraPadding;
    float4 ViewportBoundsUV;
};

cbuffer PostProcessingBuffer : register(b4)
{
    float4x4 projectionMatrix;
    float4x4 invProjectionMatrix;
    float4x4 worldToViewMatrix;
};

#define MAX_WAVES 16
cbuffer WaveConstants : register(b5)
{
    float4 Waves_DirK_Speed_Phase[MAX_WAVES];
    float4 Waves_Steep_A_Dx_Dy[MAX_WAVES];
};

//------------------------------------------------------------------------------------------------
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
// Helper Functions
//------------------------------------------------------------------------------------------------
float LinearizeDepth(float ndcDepth, float4x4 projMat)
{
    float p22 = projMat[2][2];
    float p32_val1 = projMat[2][3];
    float p32_val2 = projMat[3][2];
    
    float p32 = lerp(p32_val2, p32_val1, step(0.000001f, abs(p32_val1)));
    float p32Mask = step(abs(p32), 0.0001f);
    p32 = lerp(p32, -0.1f, p32Mask);
    
    float denom = ndcDepth - p22;
    float denomMask = step(abs(denom), 0.00001f);
    float safeDenom = step(0.0f, denom) * 0.00002f - 0.00001f;
    denom = lerp(denom, safeDenom, denomMask);
    
    return abs(p32 / denom);
}

float3 CalculateGerstnerWave(int waveIndex, float3 samplePoint, inout float3 tangent, inout float3 bitangent, float waveActiveMask)
{
    float4 dirKSpeedPhase = Waves_DirK_Speed_Phase[waveIndex];
    float4 steepADxDy = Waves_Steep_A_Dx_Dy[waveIndex];
    
    float f = dirKSpeedPhase.x * samplePoint.x +
              dirKSpeedPhase.y * samplePoint.y -
              dirKSpeedPhase.z * GameRunTime +
              dirKSpeedPhase.w;
    float sinf, cosf;
    sincos(f, sinf, cosf);
    
    float steepness = steepADxDy.x;
    float a = steepADxDy.y;
    float dx = steepADxDy.z;
    float dy = steepADxDy.w;
    float choppiness = 1.2f;
    
    float steepSin = steepness * sinf * choppiness;
    float steepCos = steepness * cosf;
    
    tangent += float3(
        -dx * dx * steepSin,
        -dx * dy * steepSin,
        dx * steepCos
    ) * waveActiveMask;
    bitangent += float3(
        -dx * dy * steepSin,
        -dy * dy * steepSin,
        dy * steepCos
    ) * waveActiveMask;
    return float3(
        dx * (a * cosf) * choppiness,
        dy * (a * cosf) * choppiness,
        a * sinf
    ) * waveActiveMask;
}

float GetTextureCaustic(float2 uv, float time, Texture2D normalTex, SamplerState samp)
{
    float2 p = uv * 0.18f;
    
    float intensity = 0.0f;
    float amp = 1.0f;
    
    for (int i = 0; i < 3; i++)
    {
        float3 n = normalTex.SampleLevel(samp, p, 1).rgb * 2.0f - 1.0f;
        
        float ridgeX = 1.0f - abs(n.x * 2.0f);
        float ridgeY = 1.0f - abs(n.y * 2.0f);
        
        float ridge = max(ridgeX, ridgeY);
        
        intensity += pow(saturate(ridge), 4.0f) * amp;
        
        float2 nextP;
        nextP.x = p.x * 0.866f - p.y * 0.5f;
        nextP.y = p.x * 0.5f + p.y * 0.866f;
        
        p = nextP * 1.8f + float2(0.015f, -0.01f) * time;
        
        amp *= 0.5f;
    }
    
    return pow(intensity * 0.8f, 1.5f);
}

//------------------------------------------------------------------------------------------------
// Vertex Shader
//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    v2p_t v2p;
    v2p.clipSpacePosition = float4(input.modelSpacePosition, 1.0f);
    v2p.color = input.color;
    v2p.uv = input.uv;
    return v2p;
}

//------------------------------------------------------------------------------------------------
// Pixel Shader
//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    int2 pixelCoord = int2(input.clipSpacePosition.xy);
    uint stencilValue = depthStencilTexture.Load(pixelCoord, 0).g;
    
    if (stencilValue == 0)
        return originalScreenColor.Sample(screenSampler, input.uv);

    // -------------------------------------------------------------------------
    // Split-Screen Viewport Fix
    // -------------------------------------------------------------------------
    float2 viewportSize = ViewportBoundsUV.zw - ViewportBoundsUV.xy;
    float2 localUV = (input.uv - ViewportBoundsUV.xy) / viewportSize;
    float aspect = projectionMatrix[1][1] / projectionMatrix[0][0];

    // -------------------------------------------------------------------------
    // Reconstruct Scene Information
    // -------------------------------------------------------------------------
    float waveSpeed = GameRunTime * 1.5f;
    
    float2 distortionOffset = float2(
        sin(localUV.y * 15.0f + waveSpeed) * 0.003f + cos(localUV.y * 35.0f - waveSpeed * 1.2f) * 0.001f,
        cos(localUV.x * 12.0f + waveSpeed * 0.8f) * 0.003f
    );
    
    float2 distortedUV = clamp(input.uv + distortionOffset, ViewportBoundsUV.xy, ViewportBoundsUV.zw);
    
    float2 distortedLocalUV = (distortedUV - ViewportBoundsUV.xy) / viewportSize;
    
    float r = screenColor.Sample(screenSampler, distortedUV + float2(0.002f, 0.0f)).r;
    float g = screenColor.Sample(screenSampler, distortedUV).g;
    float b = screenColor.Sample(screenSampler, distortedUV - float2(0.002f, 0.0f)).b;
    float3 baseColor = float3(r, g, b);
    
    float rawDepth = depthTexture.Sample(screenSampler, distortedUV).r;
    float linearZ = LinearizeDepth(rawDepth, projectionMatrix);
    
    float scaleX = projectionMatrix[0][0];
    float scaleY = projectionMatrix[1][1];
    
    float depthViewX = (distortedLocalUV.x * 2.0f - 1.0f) / scaleX;
    float depthViewY = -(distortedLocalUV.y * 2.0f - 1.0f) / scaleY;
    float3 sceneRendererPos = float3(depthViewX * linearZ, depthViewY * linearZ, linearZ);
    
    float2 ndc = localUV * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    float viewX = ndc.x / scaleX;
    float viewY = ndc.y / scaleY; 
    
    float3 camForward = normalize(WorldToCameraTransform[0].xyz); 
    float3 camLeft = normalize(WorldToCameraTransform[1].xyz);
    float3 camUp = normalize(WorldToCameraTransform[2].xyz); 
    float3 camRight = -camLeft; 
    float3 unnormalizedViewRay = viewX * camRight + viewY * camUp + 1.0f * camForward;
    float3 worldPos = CameraWorldPosition + unnormalizedViewRay * linearZ;
    
    float distToCamera = length(unnormalizedViewRay * linearZ);
    float3 viewDirWorld = normalize(unnormalizedViewRay);

    // -------------------------------------------------------------------------
    // Absorption & Specular Highlight Preservation
    // -------------------------------------------------------------------------
    float3 absorptionCoeff = float3(0.75f, 0.15f, 0.02f) * 0.06f;
    float3 transmittance = exp(-absorptionCoeff * distToCamera);
    float3 absorbedColor = baseColor * transmittance;
    
    float baseLuminance = dot(baseColor, float3(0.299f, 0.587f, 0.114f));
    float highlightMask = smoothstep(0.6f, 1.5f, baseLuminance);
    float highlightPreservation = highlightMask * exp(-distToCamera * 0.015f);

    float desaturateFactor = saturate(distToCamera * 0.005f);
    absorbedColor = lerp(absorbedColor, float3(baseLuminance, baseLuminance, baseLuminance), desaturateFactor * 0.4f);
    
    // -------------------------------------------------------------------------
    // Color Grading & Depth-Based Fog 
    // -------------------------------------------------------------------------
    float3 surfaceColor = float3(0.05f, 0.45f, 0.55f);
    float3 midColor = float3(0.01f, 0.08f, 0.18f);
    float3 abyssColor = float3(0.00f, 0.005f, 0.01f);
    
    float camZ = min(0.0f, CameraWorldPosition.z);
    float depthScale = 0.2f;
    float depthAmbient = exp(camZ * 0.03f * depthScale);
    
    float lookUpWeight = smoothstep(0.0f, 0.8f, viewDirWorld.z);
    float rayExtinction = exp(-distToCamera * 0.015f * depthScale);
    float surfaceGlowWeight = depthAmbient * lerp(rayExtinction, 1.0f, lookUpWeight);
    float abyssWeight = exp(camZ * 0.01f * depthScale);
    
    float3 fogColor = lerp(midColor, surfaceColor, surfaceGlowWeight);
    fogColor = lerp(abyssColor, fogColor, abyssWeight);
    
    float scatteringDensity = 0.005f;
    float baseScattering = saturate(1.0f - exp(-scatteringDensity * distToCamera));
    float scatteringFactor = lerp(baseScattering, 0.0f, highlightPreservation);
    
    if (rawDepth >= 0.9999f)
        scatteringFactor = 1.0f;
    
    float3 underwaterColor = lerp(absorbedColor, fogColor, scatteringFactor);

    // -------------------------------------------------------------------------
    // Physically Accurate Caustics
    // -------------------------------------------------------------------------
    float3 incidentSun = normalize(SunDirection);
    float3 flatWaterNormal = float3(0.0f, 0.0f, 1.0f);
    float3 refractedSunProp = refract(incidentSun, flatWaterNormal, 1.0f / 1.333f);
    
    float3 underwaterSunDir = normalize(-refractedSunProp);
    float3 originalSunDir = normalize(-SunDirection);
    float dayMask = saturate(originalSunDir.z * 10.0f);
    float sunDot = saturate(dot(viewDirWorld, underwaterSunDir));
    
    float2 stableCausticUV = worldPos.xy - underwaterSunDir.xy * worldPos.z;

    float3 tangent = float3(1.0, 0.0, 0.0);
    float3 bitangent = float3(0.0, 1.0, 0.0);
    float3 waveOffset = float3(0, 0, 0);
    
    [unroll]
    for (int i = 0; i < 16; i++)
    {
        float waveActiveMask = step(0.0001f, Waves_Steep_A_Dx_Dy[i].x);
        waveOffset += CalculateGerstnerWave(i, worldPos, tangent, bitangent, waveActiveMask);
    }
    
    float2 baseCausticUV = (stableCausticUV + waveOffset.xy) * 0.02f;
    
    float2 pan = float2(0.02f, 0.03f) * GameRunTime;
    float3 nPerturb = seaNormalTexture.SampleLevel(texSampler, baseCausticUV + pan, 0).rgb * 2.0f - 1.0f;
    baseCausticUV += nPerturb.xy * 0.01f;
    
    float chromaticOffset = 0.0015f;
    float causticTime = GameRunTime * 1.5f;
    
    float focusR = GetTextureCaustic(baseCausticUV + float2(chromaticOffset, 0.0f), causticTime, seaNormalTexture, texSampler);
    float focusG = GetTextureCaustic(baseCausticUV, causticTime, seaNormalTexture, texSampler);
    float focusB = GetTextureCaustic(baseCausticUV - float2(chromaticOffset, 0.0f), causticTime, seaNormalTexture, texSampler);
    
    float3 chromaticCaustic = float3(focusR, focusG, focusB);
    
    chromaticCaustic = pow(max(chromaticCaustic, 0.0f), 1.0f) * 1.5f;
    
    float waterDepthExtinction = exp(worldPos.z * 0.12f * depthScale);
    float viewDistExtinction = exp(-distToCamera * 0.035f * depthScale);
    
    float3 sceneNormal = originalScreenNormal.Sample(screenSampler, distortedUV).xyz;
    sceneNormal = normalize(sceneNormal);
    
    float nDotL = saturate(dot(sceneNormal, underwaterSunDir));
    float surfaceLightFade = nDotL;
    
    float shadowMask = 1.0f;
    
    float3 causticColor = float3(0.85f, 0.95f, 1.0f) * chromaticCaustic * waterDepthExtinction * viewDistExtinction * dayMask * SunIntensity * surfaceLightFade * shadowMask;
    
    if (rawDepth < 0.9999f)
    {
        underwaterColor += causticColor * absorbedColor;
    }
    
    // -------------------------------------------------------------------------
    // Volume God Rays (Ray marching)
    // -------------------------------------------------------------------------
    float HG_g = 0.65f;
    float phaseFunction = (1.0f - HG_g * HG_g) / pow(abs(1.0f + HG_g * HG_g - 2.0f * HG_g * sunDot), 1.5f);
    phaseFunction = clamp(phaseFunction, 0.0f, 3.5f);
    
    float3 volumetricGodRays = float3(0, 0, 0);
    float2 pixelPos = input.clipSpacePosition.xy;
    float jitter = frac(sin(dot(pixelPos, float2(12.9898f, 78.233f))) * 43758.5453123f);

    int marchSteps = 16;
    float marchDist = min(distToCamera, 20.0f);
    float stepSize = marchDist / (float) marchSteps;
    
    float currentDist = stepSize * jitter;
    float accumulatedShafts = 0.0f;
    float illuminationDecay = 1.0f;

    for (int j = 0; j < marchSteps; j++)
    {
        float3 marchPos = CameraWorldPosition + viewDirWorld * currentDist;
        if (marchPos.z > 0.0f)
            break;
            
        float4 stepSunClip = mul(SunViewProjMatrix, float4(marchPos, 1.0f));
        float3 stepSunNdc = stepSunClip.xyz / stepSunClip.w;
        float2 stepShadowUV = stepSunNdc.xy * float2(0.5f, -0.5f) + 0.5f;
        float stepShadowMask = 1.0f;
        
        if (all(stepShadowUV >= 0.0f && stepShadowUV <= 1.0f))
        {
            float stepShadowDepth = sunShadowTexture.SampleLevel(texSampler, stepShadowUV, 0).r;
            stepShadowMask = (stepSunNdc.z <= stepShadowDepth + 0.002f) ? 1.0f : 0.0f;
        }
        
        if (stepShadowMask > 0.0f)
        {
            float distToSurfStep = max(0.0f, (0.0f - marchPos.z) / max(underwaterSunDir.z, 0.001f));
            float3 surfPos = marchPos + underwaterSunDir * distToSurfStep;
            float2 sUV = surfPos.xy * 0.015f;
            
            float3 nStep = seaNormalTexture.SampleLevel(texSampler, sUV, 2.0f).rgb * 2.0f - 1.0f;
            float3 marchNormal = normalize(float3(nStep.x, nStep.y, 1.0f));
            
            float baseFocusStep = saturate(dot(marchNormal, underwaterSunDir));
            float shaftFocus = pow(baseFocusStep, 3.5f) * 2.0f;
            
            float depthFade = exp(marchPos.z * 0.06f * depthScale);
            accumulatedShafts += shaftFocus * depthFade * illuminationDecay * stepSize;
        }
        
        illuminationDecay *= 0.92f;
        currentDist += stepSize;
    }
    
    float godRayIntensity = 0.015f;
    volumetricGodRays = float3(accumulatedShafts, accumulatedShafts, accumulatedShafts);
    volumetricGodRays *= phaseFunction * dayMask * SunIntensity * godRayIntensity;
    
    // -------------------------------------------------------------------------
    // Bloom & Vignette
    // -------------------------------------------------------------------------
    float viewTransition = smoothstep(-0.2f, 0.8f, viewDirWorld.z);
    float3 volumeLightColor = float3(0.55f, 0.8f, 0.95f);
    
    underwaterColor += volumetricGodRays * volumeLightColor * viewTransition;
    
    float2 centerOffset = localUV - 0.5f;
    
    float vignette = smoothstep(0.85f, 0.35f, length(centerOffset));
    underwaterColor *= vignette;
    
    return float4(underwaterColor, 1.0f);
}