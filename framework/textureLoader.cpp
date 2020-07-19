#include <fstream>
#include "magma/magma.h"
#include "gliml/gliml.h"

static VkFormat blockCompressedFormat(const gliml::context& ctx)
{
    const int internalFormat = ctx.image_internal_format();
    switch (internalFormat)
    {
    case GLIML_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case GLIML_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case GLIML_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return VK_FORMAT_BC3_UNORM_BLOCK;
    default:
        throw std::invalid_argument("unknown block compressed format");
        return VK_FORMAT_UNDEFINED;
    }
}

std::shared_ptr<magma::ImageView> loadDxtTexture(std::shared_ptr<magma::CommandBuffer> cmdCopy, const std::string& filename)
{
    std::ifstream file("../assets/textures/" + filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("failed to open file ../assets/textures/\"" + filename + "\"");
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    gliml::context ctx;
    VkDeviceSize baseMipOffset = 0;
    std::shared_ptr<magma::SrcTransferBuffer> buffer = std::make_shared<magma::SrcTransferBuffer>(cmdCopy->getDevice(), size);
    magma::helpers::mapScoped<uint8_t>(buffer, [&](uint8_t *data)
    {   // Read data to buffer
        file.read(reinterpret_cast<char *>(data), size);
        file.close();
        ctx.enable_dxt(true);
        if (!ctx.load(data, static_cast<unsigned>(size)))
            throw std::runtime_error("failed to load DDS texture");
        // Skip DDS header
        baseMipOffset = reinterpret_cast<const uint8_t *>(ctx.image_data(0, 0)) - data;
    });
    // Setup texture data description
    const VkFormat format = blockCompressedFormat(ctx);
    const VkExtent2D extent = {
        static_cast<uint32_t>(ctx.image_width(0, 0)),
        static_cast<uint32_t>(ctx.image_height(0, 0))
    };
    magma::Image::MipmapLayout mipOffsets(1, 0);
    for (int level = 1; level < ctx.num_mipmaps(0); ++level)
    {   // Compute relative offset
        const intptr_t mipOffset = (const uint8_t *)ctx.image_data(0, level) - (const uint8_t *)ctx.image_data(0, level - 1);
        mipOffsets.push_back(mipOffset);
    }
    // Upload texture data from buffer
    magma::Image::CopyLayout bufferLayout{baseMipOffset, 0, 0};
    std::shared_ptr<magma::Image2D> image = std::make_shared<magma::Image2D>(cmdCopy,
        format, extent, std::move(buffer), mipOffsets, bufferLayout);
    // Create image view
    return std::make_shared<magma::ImageView>(std::move(image));
}
