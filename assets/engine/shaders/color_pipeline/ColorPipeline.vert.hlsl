struct VSIn {
    float3 position : POSITION0;
};

struct VSOut {
    float4 position : SV_POSITION;
};

struct ViewProjectionBuffer {
    float4x4 viewProjection;
};

ConstantBuffer <ViewProjectionBuffer> vpMatrix: register(b0, space0);

struct PushConsts
{    
    float4x4 model;
    float4 color;
    float placeholder0;
    float placeholder1;
    float placeholder2;
    float placeholder3;
};

[[vk::push_constant]]
cbuffer {
    PushConsts pushConsts;
};

VSOut main(VSIn input) {
    VSOut output;

    float4x4 mvpMatrix = mul(vpMatrix.viewProjection, pushConsts.model);
    output.position = mul(mvpMatrix, float4(input.position, 1.0));
    
    return output;
}