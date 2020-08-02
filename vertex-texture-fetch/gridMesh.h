#pragma once
#include <cstdint>
#include <memory>
#include "core/noncopyable.h"

namespace magma
{
    class CommandBuffer;
    class VertexBuffer;
    class IndexBuffer;
}

class GridMesh : public core::NonCopyable
{
public:
    explicit GridMesh(uint16_t rows, uint16_t columns, float scale,
        std::shared_ptr<magma::CommandBuffer> cmdBuffer);
    void draw(std::shared_ptr<magma::CommandBuffer> cmdBuffer);

private:
    std::shared_ptr<magma::VertexBuffer> vertexBuffer;
    std::shared_ptr<magma::IndexBuffer> indexBuffer;
};
