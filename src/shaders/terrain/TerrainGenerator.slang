[[vk::binding(0, 0)]] RWTexture2D<float> heightmap;

[shader("compute")]
[numthreads(32, 32, 1)]
void cs_main(uint3 threadId : SV_DispatchThreadID) {
    heightmap[threadId.xy] = (float)threadId.x / 4096.0;
}