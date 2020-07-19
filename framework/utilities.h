#pragma once
#include <memory>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#endif
#include <vulkan/vulkan.h>
#include "core/alignedAllocator.h"

namespace magma
{
    class PhysicalDevice;
}

namespace gliml
{
    class context;
}

namespace utilities
{
    VkFormat getBlockCompressedFormat(const gliml::context& ctx);
    VkFormat getSupportedDepthFormat(std::shared_ptr<magma::PhysicalDevice> physicalDevice, bool hasStencil, bool optimalTiling);
    uint32_t getSupportedMultisampleLevel(std::shared_ptr<magma::PhysicalDevice> physicalDevice, VkFormat format);
    std::vector<char, core::aligned_allocator<char>> loadBinaryFile(const std::string& filename);
    VkBool32 VKAPI_PTR reportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
        uint64_t object, size_t location, int32_t messageCode,
        const char *pLayerPrefix, const char *pMessage, void *pUserData);
} // namespace utilities
