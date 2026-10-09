static const float DENSITY = 0.002;
// static const float HEIGHT_FALLOFF = 0.01;
// static const float BASE_HEIGHT = 0.0;

float3 ApplyDistanceFog(
    float3 surfaceColor,
    float3 worldPos,
    float3 viewPos,
    SamplerState environmentSampler,
    TextureCube<float4> environmentMap,
) {
    float3 toFragment = worldPos - viewPos;
    float distance = length(toFragment);
    if (distance < 0.0001) return surfaceColor;

    float3 rayDir = toFragment / distance;

    float heightFactor = 1.0; //exp(-max(0.0, worldPos.y - BASE_HEIGHT) * HEIGHT_FALLOFF);
    float fogFactor = 1.0 - exp(-distance * DENSITY * heightFactor);
    fogFactor = saturate(fogFactor);

    float3 environmentColor = environmentMap.Sample(environmentSampler, rayDir).rgb;

    return lerp(surfaceColor, environmentColor, fogFactor);
}