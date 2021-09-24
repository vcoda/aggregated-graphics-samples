#include <fstream>
#include "rayTracingApp.h"

RayTracingApp::RayTracingApp(const AppEntry& entry, const core::tstring& caption, uint32_t width, uint32_t height, bool sRGB):
    VulkanApp(entry, caption, width, height, sRGB, false)
{
    initialize();
    try {
        // Try 10 bit output to avoid banding artefacts
        createOutputImage(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    } catch (...) {
        createOutputImage(VK_FORMAT_B8G8R8A8_UNORM);
    }
    // Create ray trace command buffer
    buildCmdBuffer = std::make_shared<magma::PrimaryCommandBuffer>(commandPools[0]);
    rtCmdBuffer = std::make_shared<magma::PrimaryCommandBuffer>(commandPools[0]);
    rtSemaphore = std::make_shared<magma::Semaphore>(device);
    // Create descriptor pool
    constexpr uint32_t maxDescriptorSets = 20;
    descriptorPool = std::shared_ptr<magma::DescriptorPool>(new magma::DescriptorPool(device, maxDescriptorSets,
        {
            magma::descriptors::UniformBuffer(10),
            magma::descriptors::AccelerationStructure(10),
            magma::descriptors::StorageBuffer(30),
            magma::descriptors::StorageImage(4)
        }));
    createSamplers();
    // Create uniform buffers
    lightSource = std::make_shared<magma::UniformBuffer<LightSource>>(device);
    viewProjTransforms = std::make_shared<magma::UniformBuffer<ViewProjTransforms>>(device);
    // Create common objects
    bltRect = std::make_unique<magma::aux::BlitRectangle>(renderPass);
    arcball = std::shared_ptr<Trackball>(new Trackball(rapid::vector2(width/2.f, height/2.f), 300.f, false));
    timer = std::make_unique<Timer>();
}

void RayTracingApp::enableDeviceFeatures(VkPhysicalDeviceFeatures& features) const noexcept
{
    features.samplerAnisotropy = VK_TRUE;
    features.textureCompressionBC = VK_TRUE;
    features.shaderStorageImageMultisample = VK_TRUE;
}

void RayTracingApp::enableDeviceFeaturesExt(std::vector<void *>& features) const
{
#ifdef VK_EXT_descriptor_indexing
    if (extensions->EXT_descriptor_indexing)
    {
        static VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing = {};
        descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
        descriptorIndexing.pNext = nullptr;
        descriptorIndexing.runtimeDescriptorArray = VK_TRUE;
        features.push_back(&descriptorIndexing);
    }
#endif // VK_EXT_descriptor_indexing
}

void RayTracingApp::enableExtensions(std::vector<const char*>& extensionNames) const
{
    VulkanApp::enableExtensions(extensionNames);
#ifdef VK_NV_ray_tracing
    if (extensions->NV_ray_tracing)
        extensionNames.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
#endif
}

void RayTracingApp::createOutputImage(VkFormat colorFormat)
{
    const VkExtent2D& extent = framebuffers[0]->getExtent();
    outputImage = std::make_shared<magma::StorageImage2D>(device, colorFormat, extent, 1);
    outputImageView = std::make_shared<magma::ImageView>(outputImage);
    magma::helpers::executeCommandBuffer(commandPools[0],
        [this](std::shared_ptr<magma::CommandBuffer> cmdBuffer)
        {   // Perform transition from undefined to general image layout
            const magma::ImageSubresourceRange subresourceRange(outputImage);
            cmdBuffer->pipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                magma::ImageMemoryBarrier(outputImage, VK_IMAGE_LAYOUT_GENERAL, subresourceRange));
        });
}

void RayTracingApp::createSamplers()
{
    nearestClampToEdge = std::make_shared<magma::Sampler>(device, magma::samplers::magMinMipNearestClampToEdge);
    nearestRepeat = std::make_shared<magma::Sampler>(device, magma::samplers::magMinMipNearestRepeat);
    bilinearRepeat = std::make_shared<magma::Sampler>(device, magma::samplers::magMinLinearMipNearestRepeat);
    trilinearRepeat = std::make_shared<magma::Sampler>(device, magma::samplers::magMinMipLinearRepeat);
    anisotropicRepeat = std::make_shared<magma::Sampler>(device, magma::samplers::magMinLinearMipAnisotropicRepeat);
}

std::shared_ptr<magma::ShaderModule> RayTracingApp::loadShader(const char *shaderFileName, bool reflect /* false */) const
{
    std::ifstream file(shaderFileName, std::ios::in | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("file \"" + std::string(shaderFileName) + "\" not found");
    std::vector<char> bytecode((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytecode.size() % sizeof(magma::SpirvWord))
        throw std::runtime_error("size of \"" + std::string(shaderFileName) + "\" bytecode must be a multiple of SPIR-V word");
    return std::make_shared<magma::ShaderModule>(device, reinterpret_cast<const magma::SpirvWord *>(bytecode.data()), bytecode.size(),
        0, 0, reflect, device->getAllocator());
}

magma::PipelineShaderStage RayTracingApp::loadShaderStage(const char *shaderFileName,
    std::shared_ptr<magma::Specialization> specialization /* nullptr */) const
{
    std::shared_ptr<magma::ShaderModule> module = loadShader(shaderFileName, true);
    const VkShaderStageFlagBits stage = module->getReflection()->getShaderStage();
    const char *const entrypoint = module->getReflection()->getEntryPointName(0);
    return magma::PipelineShaderStage(stage, std::move(module), entrypoint, std::move(specialization));
}

magma::PipelineShaderStage RayTracingApp::loadShaderStage(const char *shaderFileName, VkShaderStageFlagBits stage,
    std::shared_ptr<magma::Specialization> specialization /* nullptr */) const
{
    std::shared_ptr<magma::ShaderModule> module = loadShader(shaderFileName, false);
    return magma::PipelineShaderStage(stage, std::move(module), "main", std::move(specialization));
}

void RayTracingApp::resizeScratchBuffer(VkDeviceSize size)
{
    if (!scratchBuffer || scratchBuffer->getSize() < size)
        scratchBuffer = std::make_shared<magma::RayTracingBuffer>(device, size);
}

void RayTracingApp::updateViewProjTransforms()
{
    viewProj->updateView();
    viewProj->updateProjection();
    magma::helpers::mapScoped(viewProjTransforms,
        [this](auto* transforms)
        {
            transforms->view = viewProj->getView();
            transforms->viewInv = viewProj->getViewInv();
            transforms->proj = viewProj->getProj();
            transforms->projInv = viewProj->getProjInv();
            transforms->viewProj = viewProj->getViewProj();
            transforms->viewProjInv = rapid::inverse(viewProj->getViewProj());
            transforms->shadowProj = rapid::identity();
        });
}

void RayTracingApp::blit(std::shared_ptr<const magma::ImageView> imageView, uint32_t bufferIndex)
{
    std::shared_ptr<magma::CommandBuffer> cmdBuffer = commandBuffers[bufferIndex];
    cmdBuffer->begin();
    cmdBuffer->beginRenderPass(renderPass, framebuffers[bufferIndex]);
    {
        const VkRect2D rc = VkRect2D{0, 0, width, height};
        bltRect->blit(cmdBuffer, imageView, VK_FILTER_NEAREST, rc);
    }
    cmdBuffer->endRenderPass();
    cmdBuffer->end();
}

void RayTracingApp::submitCommandBuffers(uint32_t bufferIndex)
{
    queue->submit(rtCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        presentFinished, // Wait for swapchain
        rtSemaphore,
        nullptr);
    queue->submit(commandBuffers[bufferIndex],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        rtSemaphore, // Wait for scene rendering
        renderFinished,
        waitFences[bufferIndex]);
}

void RayTracingApp::onMouseMove(int x, int y)
{
    arcball->rotate(rapid::vector2((float)x, height - (float)y));
    VulkanApp::onMouseMove(x, y);
    mouseX = x;
    mouseY = y;
}

void RayTracingApp::onMouseLButton(bool down, int x, int y)
{
    if (down)
        arcball->touch(rapid::vector2((float)x, height - (float)y));
    else
        arcball->release();
    mouseX = x;
    mouseY = y;
    VulkanApp::onMouseLButton(down, x, y);
}
