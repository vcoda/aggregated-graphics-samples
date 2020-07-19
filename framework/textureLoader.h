#pragma once
#include <memory>
#include <string>

namespace magma
{
    class ImageView;
    class CommandBuffer;
}

std::shared_ptr<magma::ImageView> loadDxtTexture(std::shared_ptr<magma::CommandBuffer> device, const std::string& filename);
