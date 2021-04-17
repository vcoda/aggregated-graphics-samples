#pragma once
#include "rtMesh.h"

#ifdef VK_NV_ray_tracing
class CornellBox
{
public:
    std::shared_ptr<RtMesh<RtColorVertex>> box;
    std::shared_ptr<RtMesh<RtColorVertex>> block;
    std::shared_ptr<RtMesh<RtColorVertex>> quad;

    explicit CornellBox(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        constexpr alignas(16) float cornellBoxVertices[] = {
#include "cornellBox.inl"
        };
        constexpr alignas(16) float blockVertices[] = {
#include "block.inl"
        };
        constexpr alignas(16) uint32_t cubeFaces[] = {
#include "faces.inl"
        };
        constexpr alignas(16) float quadVertices[] = {
#include "quad.inl"
        };
        constexpr alignas(16) uint32_t quadFaces[] = {
            0, 1, 2, 2, 1, 3
        };
        box = std::make_shared<RtMesh<RtColorVertex>>(cmdBuffer, cornellBoxVertices, cubeFaces);
        block = std::make_shared<RtMesh<RtColorVertex>>(cmdBuffer, blockVertices, cubeFaces);
        quad = std::make_shared<RtMesh<RtColorVertex>>(std::move(cmdBuffer), quadVertices, quadFaces);
    }
};
#endif // VK_NV_ray_tracing
