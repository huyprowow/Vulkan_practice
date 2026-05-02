// ======================== GRAPHICS (vẽ particles) ========================

struct VSInput
{
    float2 inPosition;
    float4 inColor;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float pointSize : SV_PointSize;
    float3 fragColor : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 fragColor : COLOR0;
    float2 pointCoord : SV_PointCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.pointSize = 14.0;
    output.pos = float4(input.inPosition, 1.0, 1.0);
    output.fragColor = input.inColor.rgb;
    return output;
}

[shader("fragment")]
float4 fragMain(PSInput input) : SV_TARGET
{
    float2 coord = input.pointCoord - float2(0.5);
    return float4(input.fragColor, 0.5 - length(coord));
}

// ======================== COMPUTE (cập nhật particles) ========================

struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
};

struct UniformBuffer
{
    float deltaTime;
};
ConstantBuffer<UniformBuffer> ubo;
// Push constants — main thread inject startIndex + count cho mỗi worker
struct PushConstants
{
    uint startIndex;
    uint count;
};
[[vk::push_constant]] PushConstants pushConstants;

struct ParticleSSBO
{
    Particle particles;
};
StructuredBuffer<ParticleSSBO> particlesIn;
RWStructuredBuffer<ParticleSSBO> particlesOut;

[shader("compute")]
[numthreads(256, 1, 1)] // invocations trong 1 work group (256 x, 1 y, 1 z) vi particle mang 1 chieu
void compMain(uint3 threadId: SV_DispatchThreadID)
{
    // localIndex = vi tri trong range cua thread, KHONG phai global index
    uint localIndex = threadId.x;
    // Bound check theo COUNT thread nay nhan, khong phai PARTICLE_COUNT toan cuc
    // (multithreading: moi worker chi xu li 1 phan particle)
    if (localIndex >= pushConstants.count)
        return;
    // global index = offset thread + local index
    uint index = pushConstants.startIndex + localIndex;

    Particle pIn = particlesIn[index].particles;

    particlesOut[index].particles.position = pIn.position + pIn.velocity * ubo.deltaTime;
    particlesOut[index].particles.velocity = pIn.velocity;
    particlesOut[index].particles.color = pIn.color;
    // Normalized Device Coordinates [-1,1] (trai-phai, tren-duoi)
    // kiem tra va flip velocity khi va cham bien (nay nguoc lai)
    if (particlesOut[index].particles.position.x <= -1.0 ||
        particlesOut[index].particles.position.x >= 1.0)
    {
        particlesOut[index].particles.velocity.x *= -1.0;
    }
    if (particlesOut[index].particles.position.y <= -1.0 ||
        particlesOut[index].particles.position.y >= 1.0)
    {
        particlesOut[index].particles.velocity.y *= -1.0;
    }
}
