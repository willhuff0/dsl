struct VertexOutput
{
    float4 position : SV_Position;
    float2 texCoords : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vs_main(uint vertexIndex : SV_VertexID)
{
    static const float2 positions[] = {
        // 1st triangle
        float2(0.0,  0.0),  // center
        float2(1.0,  0.0),  // right, center
        float2(0.0,  1.0),  // center, top

        // 2nd triangle
        float2(0.0,  1.0),  // center, top
        float2(1.0,  0.0),  // right, center
        float2(1.0,  1.0),  // right, top
    };

    VertexOutput output;
    float2 xy = positions[vertexIndex];
    output.position = float4(xy * 2.0 - 1.0, 0.0, 1.0);
    output.texCoords = float2(xy.x, xy.y);
    return output;
}