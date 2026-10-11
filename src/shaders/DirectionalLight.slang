static const float SHADOW_NORMAL_BIAS = 0.002;
static const float SHADOW_MIN_BIAS = 0.0005;
static const float SHADOW_MAX_BIAS = 0.005;

static const float CASCADE_BLEND_FRACTION = 0.1;

static const bool SHOW_CASCADES = false;

struct DirectionalLightUniforms {
    float4x4 lightSpaceViewProjMats[4];
    float4 cascadeDistances;
    float3 direction;
    float3 color;
    float cameraZFar;
    uint32_t cascadeCount;
    uint32_t shadowMapSize;
}

float3 sampleShadowCascade(
    DirectionalLightUniforms directionalLight,
    DepthTexture2DArray cascadedShadowMap,
    SamplerComparisonState cascadedShadowMapSampler,
    int cascadeIndex,
    float3 position,
    float3 normal,
    float3 lightDir
) {
    float cascadeDistance = directionalLight.cascadeDistances[cascadeIndex];

    float3 offsetPos = position + normal * (SHADOW_NORMAL_BIAS * cascadeDistance);

    float4 lightClip = mul(
        directionalLight.lightSpaceViewProjMats[cascadeIndex],
        float4(offsetPos, 1.0)
    );

    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = float2(
        ndc.x * 0.5 + 0.5,
        0.5 - ndc.y * 0.5
    );

    if (ndc.z < 0.0 || ndc.z > 1.0) return 0.0;

    float slopeBias = SHADOW_MAX_BIAS * (1.0 - dot(normal, lightDir));
    float bias = clamp(slopeBias, SHADOW_MIN_BIAS, SHADOW_MAX_BIAS) / cascadeDistance;

    float2 texelSize = 1.0 / float2(directionalLight.shadowMapSize);

    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float2 sampleUV = uv + float2(x, y) * texelSize;

            float visibility = cascadedShadowMap.SampleCmpLevelZero(
                cascadedShadowMapSampler,
                float3(sampleUV, float(cascadeIndex)),
                ndc.z - bias
            );

            shadow += 1.0 - visibility;
        }
    }

    shadow /= 9.0;

    if (SHOW_CASCADES) {
        if (cascadeIndex == 0) {
            return shadow * float3(1.0, 0.0, 0.0);
        } else if (cascadeIndex == 1) {
            return shadow * float3(1.0, 1.0, 0.0);
        } else if (cascadeIndex == 2) {
            return shadow * float3(0.0, 1.0, 0.0);
        } else if (cascadeIndex == 3) {
            return shadow * float3(0.0, 1.0, 1.0);
        }
    }

    return shadow;
}

float3 CalculateDirectionalLightShadow(
    DirectionalLightUniforms directionalLight,
    DepthTexture2DArray cascadedShadowMap,
    SamplerComparisonState cascadedShadowMapSampler,
    float4x4 viewMat,
    float3 position,
    float3 normal,
    float3 lightDir
) {
    float4 viewSpacePos = mul(viewMat, float4(position, 1.0));
    float viewZ = abs(viewSpacePos.z);

    int cascadeIndex = -1;
    for (int i = 0; i < int(min(directionalLight.cascadeCount, 4u)); i++) {
        if (viewZ < directionalLight.cascadeDistances[i]) {
            cascadeIndex = i;
            break;
        }
    }

    if (cascadeIndex == -1) return 0.0;

    float3 shadow = sampleShadowCascade(
        directionalLight,
        cascadedShadowMap,
        cascadedShadowMapSampler,
        cascadeIndex,
        position,
        normal,
        lightDir
    );

    if (cascadeIndex < directionalLight.cascadeCount - 1) {
        float splitNear = cascadeIndex == 0
                              ? 0.0
                              : directionalLight.cascadeDistances[cascadeIndex - 1];

        float splitFar = directionalLight.cascadeDistances[cascadeIndex];
        float blendWidth = (splitFar - splitNear) * CASCADE_BLEND_FRACTION;
        float blendStart = splitFar - blendWidth;

        if (viewZ > blendStart && blendWidth > 0.0) {
            float blendFactor = smoothstep(
                blendStart,
                splitFar,
                viewZ
            );

            float3 nextShadow = sampleShadowCascade(
                directionalLight,
                cascadedShadowMap,
                cascadedShadowMapSampler,
                cascadeIndex + 1,
                position,
                normal,
                lightDir
            );

            shadow = lerp(shadow, nextShadow, blendFactor);
        }
    }

    return shadow;
}