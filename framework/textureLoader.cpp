#include <fstream>
#include "magma/magma.h"
#include "gliml/gliml.h"

static VkFormat blockCompressedFormat(const gliml::context& ctx, bool sRGB)
{
    const int internalFormat = ctx.image_internal_format();
    switch (internalFormat)
    {
    case GLIML_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        return sRGB ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case GLIML_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        return sRGB ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
    case GLIML_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return sRGB ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

static VkFormat blockCompressedExtFormat(unsigned int fourCC)
{
    switch (fourCC)
    {
    // ATI1
    case MAKEFOURCC('B', 'C', '4', 'U'):
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case MAKEFOURCC('B', 'C', '4', 'S'):
        return VK_FORMAT_BC4_SNORM_BLOCK;
    // ATI2
    case MAKEFOURCC('B', 'C', '5', 'U'):
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case MAKEFOURCC('B', 'C', '5', 'S'):
        return VK_FORMAT_BC5_SNORM_BLOCK;
    default:
        throw std::invalid_argument("unknown block compressed format");
        return VK_FORMAT_UNDEFINED;
    }
}

static VkFormat uncompressedRgbaFormat(const gliml::context& ctx, bool sRGB)
{
    const int internalFormat = ctx.image_internal_format();
    switch (internalFormat)
    {
    case GLIML_GL_RGBA:
        return sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    case GLIML_GL_BGRA:
        return sRGB ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
    default:
        throw std::invalid_argument("unsupported format");
        return VK_FORMAT_UNDEFINED;
    }
}

static VkFormat loadDxtTextureExt(const gliml::dds_header *hdr, VkExtent2D& extent, magma::Image::MipmapLayout& mipOffsets, VkDeviceSize& baseMipOffset)
{
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

    const unsigned char* dataBytePtr = (const unsigned char *)hdr;
    dataBytePtr += sizeof(gliml::dds_header);
    const VkFormat format = blockCompressedExtFormat(hdr->ddspf.dwFourCC);
    const int bytesPerElement = static_cast<int>(magma::Format(format).blockCompressedSize());
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
    return format;
}

std::shared_ptr<magma::ImageView> loadDxtTexture(std::shared_ptr<magma::CommandBuffer> cmdCopy, const std::string& filename,
    bool sRGB /* false */)
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
        ctx.enable_bgra(true);
        if (ctx.load(data, static_cast<unsigned>(size)))
        {   // Setup texture data description
            format = blockCompressedFormat(ctx, sRGB);
            if (VK_FORMAT_UNDEFINED == format)
                format = uncompressedRgbaFormat(ctx, sRGB);
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
            case MAKEFOURCC('B', 'C', '4', 'U'): // ATI1
            case MAKEFOURCC('B', 'C', '4', 'S'): // ATI1
            case MAKEFOURCC('B', 'C', '5', 'U'): // ATI2
            case MAKEFOURCC('B', 'C', '5', 'S'): // ATI2
                format = loadDxtTextureExt(hdr, extent, mipOffsets, baseMipOffset);
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

std::shared_ptr<magma::ImageView> loadDxtCubeTexture(const std::string& filename, std::shared_ptr<magma::CommandBuffer> cmdCopy,
    bool sRGB /* false */)
{
    std::ifstream file("../assets/textures/" + filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("failed to open file \"../assets/textures/" + filename + "\"");
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    gliml::context ctx;
    VkDeviceSize baseMipOffset = 0;
    std::shared_ptr<magma::SrcTransferBuffer> buffer = std::make_shared<magma::SrcTransferBuffer>(cmdCopy->getDevice(), size);
    magma::helpers::mapScoped<uint8_t>(buffer, [&](uint8_t *data)
    {   // Read data from file
        file.read(reinterpret_cast<char *>(data), size);
        ctx.enable_dxt(true);
        if (!ctx.load(data, static_cast<unsigned>(size)))
            throw std::runtime_error("failed to load DDS texture");
        // Skip DDS header
        baseMipOffset = reinterpret_cast<const uint8_t *>(ctx.image_data(0, 0)) - data;
    });
    // Setup texture data description
    const VkFormat format = blockCompressedFormat(ctx, sRGB);
    const uint32_t dimension = ctx.image_width(0, 0);
    magma::Image::MipmapLayout mipOffsets;
    VkDeviceSize lastMipSize = 0;
    for (int face = 0; face < ctx.num_faces(); ++face)
    {
        mipOffsets.push_back(lastMipSize);
        const int mipLevels = ctx.num_mipmaps(face);
        for (int level = 1; level < mipLevels; ++level)
        {   // Compute relative offset
            const intptr_t mipOffset = (const uint8_t *)ctx.image_data(face, level) - (const uint8_t *)ctx.image_data(face, level - 1);
            mipOffsets.push_back(mipOffset);
        }
        lastMipSize = ctx.image_size(face, mipLevels - 1);
    }
    // Upload texture data from buffer
    magma::Image::CopyLayout bufferLayout{baseMipOffset, 0, 0};
    std::shared_ptr<magma::ImageCube> image = std::make_shared<magma::ImageCube>(cmdCopy, format, dimension, ctx.num_mipmaps(0), buffer, mipOffsets, bufferLayout);
    // Create image view
    return std::make_shared<magma::ImageView>(std::move(image));
}
