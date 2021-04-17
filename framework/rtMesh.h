#pragma once

// Each attribute should have 16-byte alignment

struct RtVertex
{   
    rapid::float3a pos;
    rapid::float3a normal;
};

struct RtColorVertex
{
    rapid::float3a pos;
    rapid::float3a normal;
    rapid::float4a color;
};

struct RtTexVertex
{
    rapid::float3 pos;
    float u;
    rapid::float3 normal;
    float v;
};

#ifdef VK_NV_ray_tracing
template<typename VertexType>
class RtMesh
{
public:
    template<size_t vertexArraySize, size_t indexArraySize>
    explicit RtMesh(std::shared_ptr<magma::CommandBuffer> cmdBuffer,
        const float(&vertices)[vertexArraySize],
        const uint32_t(&indices)[indexArraySize])
    {
        static_assert(sizeof(RtVertex) == sizeof(rapid::float4) * 2, "RtVertex: each attribute should have 16-byte alignment");
        static_assert(sizeof(RtColorVertex) == sizeof(rapid::float4) * 3, "RtColorVertex: each attribute should have 16-byte alignment");
        static_assert(sizeof(RtTexVertex) == sizeof(rapid::float4) * 2, "RtTexVertex: each attribute should have 16-byte alignment");
        // Create vertex buffer
        vertexBuffer = std::make_shared<magma::AccelerationStructureVertexBuffer>(cmdBuffer,
            sizeof(float) * vertexArraySize, vertices);
        vertexBuffer->setVertexCount(sizeof(float) * vertexArraySize / sizeof(VertexType));
        // Create index buffer
        indexBuffer = std::make_shared<magma::AccelerationStructureIndexBuffer>(cmdBuffer,
            sizeof(uint32_t) * indexArraySize, indices, VK_INDEX_TYPE_UINT32);
        // Setup geometry description
        triangles = std::make_shared<magma::GeometryTriangles>(
            vertexBuffer, sizeof(VertexType), VK_FORMAT_R32G32B32_SFLOAT, indexBuffer);
        // Allocate bottom level acceleration structure
        blas = std::make_shared<magma::BottomLevelAccelerationStructure>(cmdBuffer->getDevice(),
            std::list<magma::Geometry>{*triangles},
            VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV);
    }

    std::shared_ptr<magma::AccelerationStructureVertexBuffer> getVertexBuffer() const noexcept { return vertexBuffer; }
    std::shared_ptr<magma::AccelerationStructureIndexBuffer> getIndexBuffer() const noexcept { return indexBuffer; }
    std::shared_ptr<magma::AccelerationStructure> getBlas() const noexcept { return blas; }

private:
    std::shared_ptr<magma::AccelerationStructureVertexBuffer> vertexBuffer;
    std::shared_ptr<magma::AccelerationStructureIndexBuffer> indexBuffer;
    std::shared_ptr<magma::GeometryTriangles> triangles;
    std::shared_ptr<magma::AccelerationStructure> blas;
};
#endif //  VK_NV_ray_tracing
