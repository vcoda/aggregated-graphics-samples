#include "gridMesh.h"
#include "magma/magma.h"
#include "rapid/rapid.h"

GridMesh::GridMesh(uint16_t rows, uint16_t cols, float scale,
    std::shared_ptr<magma::CommandBuffer> cmdBuffer)
{
    const float dx = scale/cols;
    const float dz = scale/rows;
    const float o = -scale * .5f;
    const std::size_t vertexBufferSize = (rows + 1) * (cols + 1) * sizeof(rapid::half2);
    const std::size_t indexBufferSize = rows * (3 + cols * 2) * sizeof(uint16_t);
    std::shared_ptr<magma::SrcTransferBuffer> stagingBuffer(std::make_shared<magma::SrcTransferBuffer>(
        cmdBuffer->getDevice(), vertexBufferSize + indexBufferSize));
    void *data = stagingBuffer->getMemory()->map();
    rapid::half2 *vert = (rapid::half2 *)data;
    // Setup X, Z coordinates
    float z = o;
    for (uint16_t i = 0, n = rows + 1; i < n; ++i, z += dz)
    {
        float x = o;
        for (uint16_t j = 0, m = cols + 1; j < m; ++j, x += dx)
        {   // Quantize floats to halves
            vert->x = rapid::ftoh(x);
            vert->y = rapid::ftoh(z);
            ++vert;
        }
    }
    uint16_t *idx = (uint16_t *)((char *)data + vertexBufferSize);
    const uint16_t stride = cols + 1;
    // Generate indices of triangle strip
    for (uint16_t i = 0; i < rows; ++i)
    {
        const uint16_t first = i * stride;
        *idx++ = first;
        *idx++ = first + stride;
        for (uint16_t j = 0, k = 1; j < cols; ++j, ++k)
        {
            *idx++ = first + k;
            *idx++ = first + k + stride;
            assert(first + k + stride <= std::numeric_limits<uint16_t>::max());
        }
        // Restart strip
        *idx++ = std::numeric_limits<uint16_t>::max();
    }
    // Create vertex and index buffers
    vertexBuffer = std::make_shared<magma::VertexBuffer>(cmdBuffer,
        stagingBuffer, vertexBufferSize, 0);
    indexBuffer = std::make_shared<magma::IndexBuffer>(std::move(cmdBuffer),
        stagingBuffer, VK_INDEX_TYPE_UINT16, indexBufferSize, vertexBufferSize);
    stagingBuffer->getMemory()->unmap();
}

void GridMesh::draw(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
{
    cmdBuffer->bindVertexBuffer(0, vertexBuffer);
    cmdBuffer->bindIndexBuffer(indexBuffer);
    cmdBuffer->drawIndexed(indexBuffer->getIndexCount());
}
