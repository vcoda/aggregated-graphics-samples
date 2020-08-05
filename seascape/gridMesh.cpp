#include "gridMesh.h"
#include "magma/magma.h"
#include "rapid/DirectXMath/Inc/DirectXPackedVector.h"
using namespace DirectX;

GridMesh::GridMesh(uint16_t rows, uint16_t cols, float scale,
    std::shared_ptr<magma::CommandBuffer> cmdBuffer)
{
    const float dx = scale/cols;
    const float dz = scale/rows;
    const float o = -scale * .5f;
    std::vector<XMFLOAT2> vertices;
    vertices.reserve((rows + 1) * (cols + 1));
    // Setup X, Z coordinates
    float z = o;
    for (uint16_t i = 0, n = rows + 1; i < n; ++i, z += dz)
    {
        float x = o;
        for (uint16_t j = 0, m = cols + 1; j < m; ++j, x += dx)
            vertices.emplace_back(x, z);
    }
    std::vector<uint16_t> indices;
    indices.reserve(rows * (3 + cols * 2));
    const uint16_t stride = cols + 1;
    // Generate indices of triangle strip
    for (uint16_t i = 0; i < rows; ++i)
    {
        const uint16_t first = i * stride;
        indices.push_back(first);
        indices.push_back(first + stride);
        for (uint16_t j = 0, k = 1; j < cols; ++j, ++k)
        {
            indices.push_back(first + k);
            indices.push_back(first + k + stride);
            assert(indices.back() <= std::numeric_limits<uint16_t>::max());
        }
        // Restart strip
        indices.push_back(std::numeric_limits<uint16_t>::max());
    }
    // Quantize floats to halves
    std::vector<PackedVector::HALF> halfVertices(vertices.size() * 2);
    PackedVector::XMConvertFloatToHalfStream(halfVertices.data(), sizeof(PackedVector::HALF),
        (const float *)vertices.data(), sizeof(float), vertices.size() * 2);
    // Create vertex and index buffers
    vertexBuffer = std::make_shared<magma::VertexBuffer>(cmdBuffer, halfVertices);
    indexBuffer = std::make_shared<magma::IndexBuffer>(cmdBuffer, indices);
}

void GridMesh::draw(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
{
    cmdBuffer->bindVertexBuffer(0, vertexBuffer);
    cmdBuffer->bindIndexBuffer(indexBuffer);
    cmdBuffer->drawIndexed(indexBuffer->getIndexCount());
}
