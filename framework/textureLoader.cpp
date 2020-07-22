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

static void loadBC5SignedNorm(const gliml::dds_header *hdr, VkExtent2D& extent, magma::Image::MipmapLayout& mipOffsets, VkDeviceSize& baseMipOffset)
{
    const unsigned char* dataBytePtr = (const unsigned char *)hdr;
    dataBytePtr += sizeof(gliml::dds_header);

    constexpr int MaxNumMipmaps = 16;
    struct Face {
        int numMipmaps;
        struct mipmap {
            int width;
            int height;
            int depth;
            int size;
            const void* data;
        } mipmaps[MaxNumMipmaps];
    } face;

    face.numMipmaps = (hdr->dwMipMapCount == 0) ? 1 : hdr->dwMipMapCount;
    // For each mipmap
    for (int mipIndex = 0; mipIndex < face.numMipmaps; mipIndex++)
    {
        Face::mipmap& curMip = face.mipmaps[mipIndex];
        // mipmap dimensions
        int w = hdr->dwWidth >> mipIndex;
        if (w <= 0) w = 1;
        int h = hdr->dwHeight >> mipIndex;
        if (h <= 0) h = 1;
        int d = hdr->dwDepth >> mipIndex;
        if (d <= 0) d = 1;
        curMip.width = w;
        curMip.height = h;
        curMip.depth = d;
        // Mipmap byte size
        constexpr int bytesPerElement = 16;
        curMip.size = ((w + 3) / 4) * ((h + 3) / 4) * d * bytesPerElement;
        // Set and advance surface data pointer
        curMip.data = dataBytePtr;
        dataBytePtr += curMip.size;
    }

    extent.width = static_cast<uint32_t>(face.mipmaps[0].width);
    extent.height = static_cast<uint32_t>(face.mipmaps[0].height);
    for (int level = 1; level < face.numMipmaps; ++level)
    {   // Compute relative offset
        const intptr_t mipOffset = (const uint8_t *)face.mipmaps[level].data - (const uint8_t *)face.mipmaps[level - 1].data;
        mipOffsets.push_back(mipOffset);
    }
    // Skip DDS header
    baseMipOffset = (const uint8_t *)face.mipmaps[0].data - (const uint8_t *)hdr;
}

std::shared_ptr<magma::ImageView> loadDxtTexture(std::shared_ptr<magma::CommandBuffer> cmdCopy, const std::string& filename)
{
    std::ifstream file("../assets/textures/" + filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("failed to open file \"../assets/textures/" + filename + "\"");
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    gliml::context ctx;
    VkFormat format;
    VkExtent2D extent;
    VkDeviceSize baseMipOffset = 0;
    magma::Image::MipmapLayout mipOffsets(1, 0);
    std::shared_ptr<magma::SrcTransferBuffer> buffer = std::make_shared<magma::SrcTransferBuffer>(cmdCopy->getDevice(), size);
    magma::helpers::mapScoped<uint8_t>(buffer, [&](uint8_t *data)
    {   // Read data to buffer
        file.read(reinterpret_cast<char *>(data), size);
        file.close();
        ctx.enable_dxt(true);
        if (ctx.load(data, static_cast<unsigned>(size)))
        {   // Setup texture data description
            format = blockCompressedFormat(ctx);
            extent.width = static_cast<uint32_t>(ctx.image_width(0, 0));
            extent.height = static_cast<uint32_t>(ctx.image_height(0, 0));
            for (int level = 1; level < ctx.num_mipmaps(0); ++level)
            {   // Compute relative offset
                const intptr_t mipOffset = (const uint8_t *)ctx.image_data(0, level) - (const uint8_t *)ctx.image_data(0, level - 1);
                mipOffsets.push_back(mipOffset);
            }
            // Skip DDS header
            baseMipOffset = (const uint8_t *)ctx.image_data(0, 0) - data;
        }
        else if (ctx.error() == GLIML_ERROR_INVALID_COMPRESSED_FORMAT)
        {   // Not supported, proceed ourself
            const gliml::dds_header *hdr = (const gliml::dds_header *)data;
            switch (hdr->ddspf.dwFourCC)
            {
            case MAKEFOURCC('B', 'C', '5', 'S'): // ATI1
                format = VK_FORMAT_BC5_SNORM_BLOCK;
                loadBC5SignedNorm(hdr, extent, mipOffsets, baseMipOffset);
                break;
            default:
                throw std::runtime_error("unknown compressed format");
            }
        }
        else
        {
            throw std::runtime_error("failed to load DDS texture");
        }
    });
    // Upload texture data from buffer
    magma::Image::CopyLayout bufferLayout{baseMipOffset, 0, 0};
    std::shared_ptr<magma::Image2D> image = std::make_shared<magma::Image2D>(cmdCopy,
        format, extent, std::move(buffer), mipOffsets, bufferLayout);
    // Create image view
    return std::make_shared<magma::ImageView>(std::move(image));
}
