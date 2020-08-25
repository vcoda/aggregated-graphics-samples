#include <fstream>
#include "graphicsApp.h"
#include "utilities.h"

GraphicsApp::GraphicsApp(const AppEntry& entry, const core::tstring& caption, uint32_t width, uint32_t height, bool sRGB):
    VulkanApp(entry, caption, width, height, sRGB)
{
    initialize();
    try {
        // Use 10 bit framebuffer to avoid banding artefacts
        createMultisampleFramebuffer(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    } catch (...) {
        createMultisampleFramebuffer(VK_FORMAT_R8G8B8A8_UNORM);
    }
    createDrawCommandBuffer();
    createSamplers();
    allocateViewProjTransforms();

    sysUniforms = std::make_shared<magma::UniformBuffer<SysUniforms>>(device);
    lightSource = std::make_shared<magma::UniformBuffer<LightSource>>(device);

    constexpr uint32_t maxDescriptorSets = 20;
    descriptorPool = std::shared_ptr<magma::DescriptorPool>(new magma::DescriptorPool(device, maxDescriptorSets,
        {
            magma::descriptors::DynamicUniformBuffer(10),
            magma::descriptors::UniformBuffer(10),
            magma::descriptors::CombinedImageSampler(8),
            magma::descriptors::StorageBuffer(4),
            magma::descriptors::DynamicStorageBuffer(4)
        }));

    arcball = std::shared_ptr<Trackball>(new Trackball(rapid::vector2(width/2.f, height/2.f), 300.f, false));
    timer = std::make_unique<Timer>();
}

void GraphicsApp::onMouseMove(int x, int y)
{
    arcball->rotate(rapid::vector2((float)x, height - (float)y));
    VulkanApp::onMouseMove(x, y);
    mouseX = x;
    mouseY = y;
}

void GraphicsApp::onMouseLButton(bool down, int x, int y)
{
    if (down)
        arcball->touch(rapid::vector2((float)x, height - (float)y));
    else
        arcball->release();
    mouseX = x;
    mouseY = y;
    VulkanApp::onMouseLButton(down, x, y);
}

void GraphicsApp::createMultisampleFramebuffer(VkFormat colorFormat)
{
    const VkFormat depthFormat = utilities::getSupportedDepthFormat(physicalDevice, false, true);
    const uint32_t sampleCount = utilities::getSupportedMultisampleLevel(physicalDevice, colorFormat);
    if (sampleCount < 2)
        throw std::exception("framebuffer format doesn't support multisampling");
    msaaFramebuffer = std::make_unique<magma::aux::ColorMultisampleFramebuffer>(device,
        colorFormat, depthFormat, framebuffers[FrontBuffer]->getExtent(), sampleCount);
    msaaBltRect = std::make_unique<magma::aux::BlitRectangle>(renderPass);
}

void GraphicsApp::createDrawCommandBuffer()
{
    drawCmdBuffer = std::make_shared<magma::PrimaryCommandBuffer>(commandPools[0]);
    drawSemaphore = std::make_shared<magma::Semaphore>(device);
}

void GraphicsApp::createSamplers()
{
    nearestRepeat = std::make_shared<magma::Sampler>(device, magma::samplers::magMinMipNearestRepeat);
    bilinearRepeat = std::make_shared<magma::Sampler>(device, magma::samplers::magMinLinearMipNearestRepeat);
    trilinearRepeat = std::make_shared<magma::Sampler>(device, magma::samplers::magMinMipLinearRepeat);
    anisotropicRepeat = std::make_shared<magma::Sampler>(device, magma::samplers::magMinLinearMipAnisotropicRepeat);

    nearestClampToEdge = std::make_shared<magma::Sampler>(device, magma::samplers::magMinMipNearestClampToEdge);
    bilinearClampToEdge = std::make_shared<magma::Sampler>(device, magma::samplers::magMinLinearMipNearestClampToEdge);
    trilinearClampToEdge = std::make_shared<magma::Sampler>(device, magma::samplers::magMinMipLinearClampToEdge);
    anisotropicClampToEdge = std::make_shared<magma::Sampler>(device, magma::samplers::magMinLinearMipAnisotropicClampToEdge);
}

void GraphicsApp::allocateViewProjTransforms()
{
    viewProjTransforms = std::make_shared<magma::UniformBuffer<ViewProjTransforms>>(device);
}

void GraphicsApp::createTransformBuffer(uint32_t numObjects)
{
    transforms = std::make_shared<magma::DynamicUniformBuffer<Transforms>>(device, numObjects);
}

std::shared_ptr<magma::ShaderModule> GraphicsApp::loadShader(const char *shaderFileName) const
{
    std::ifstream file(shaderFileName, std::ios::in | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("file \"" + std::string(shaderFileName) + "\" not found");
    std::vector<char> bytecode((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytecode.size() % sizeof(magma::SpirvWord))
        throw std::runtime_error("size of \"" + std::string(shaderFileName) + "\" bytecode must be a multiple of SPIR-V word");
    return std::make_shared<magma::ShaderModule>(device, reinterpret_cast<const magma::SpirvWord *>(bytecode.data()), bytecode.size(),
        0, 0, true, device->getAllocator());
}

magma::PipelineShaderStage GraphicsApp::loadShaderStage(const char *shaderFileName,
    std::shared_ptr<magma::Specialization> specialization /* nullptr */) const
{
    std::shared_ptr<magma::ShaderModule> module = loadShader(shaderFileName);
    const VkShaderStageFlagBits stage = module->getReflection()->getShaderStage();
    const char *const entrypoint = module->getReflection()->getEntryPointName(0);
    return magma::PipelineShaderStage(stage, std::move(module), entrypoint, std::move(specialization));
}

std::shared_ptr<magma::GraphicsPipeline> GraphicsApp::createShadowMapPipeline(const char *vertexShaderFile,
     const magma::VertexInputState& vertexInputState,
     const magma::RasterizationState& rasterizationState,
     std::shared_ptr<magma::DescriptorSetLayout> setLayout,
     std::shared_ptr<magma::aux::DepthFramebuffer> framebuffer)
{
     auto pipelineLayout = std::make_shared<magma::PipelineLayout>(
         std::move(setLayout));
     return std::make_shared<magma::GraphicsPipeline>(device,
        std::vector<magma::PipelineShaderStage>{
            loadShaderStage(vertexShaderFile)
        }, vertexInputState,
        magma::renderstates::triangleList,
        magma::TesselationState(),
        magma::ViewportState(0.f, 0.f, framebuffer->getExtent()),
        rasterizationState,
        magma::renderstates::dontMultisample,
        magma::renderstates::depthLessOrEqual,
        magma::renderstates::dontWriteRgba,
        std::initializer_list<VkDynamicState>{},
        std::move(pipelineLayout),
        framebuffer->getRenderPass(), 0, // subpass
        pipelineCache,
        nullptr, nullptr, 0);
}

std::shared_ptr<magma::GraphicsPipeline> GraphicsApp::createDepthOnlyPipeline(const char *vertexShaderFile,
    const magma::VertexInputState& vertexInputState, std::shared_ptr<magma::DescriptorSetLayout> setLayout,
    std::shared_ptr<magma::aux::Framebuffer> framebuffer /* nullptr */)
{
    auto pipelineLayout = std::make_shared<magma::PipelineLayout>(
         std::move(setLayout));
    return std::make_shared<magma::GraphicsPipeline>(device,
        std::vector<magma::PipelineShaderStage>{
            loadShaderStage(vertexShaderFile)
        }, vertexInputState,
        magma::renderstates::triangleList,
        magma::TesselationState(),
        magma::ViewportState(0.f, 0.f, framebuffer ? framebuffer->getExtent() : msaaFramebuffer->getExtent()),
        magma::renderstates::fillCullBackCW,
        framebuffer ? framebuffer->getMultisampleState() : msaaFramebuffer->getMultisampleState(),
        magma::renderstates::depthLessOrEqual,
        magma::renderstates::dontWriteRgba,
        std::initializer_list<VkDynamicState>{},
        std::move(pipelineLayout),
        framebuffer ? framebuffer->getRenderPass() : msaaFramebuffer->getRenderPass(),
        0, // subpass
        pipelineCache,
        nullptr, nullptr, 0);
}

std::shared_ptr<magma::GraphicsPipeline> GraphicsApp::createDepthOnlyPipeline(const char *vertexShaderFile,
    const magma::VertexInputState& vertexInputState, std::shared_ptr<magma::DescriptorSetLayout> setLayout,
    std::shared_ptr<magma::aux::MultiAttachmentFramebuffer> mrtFramebuffer)
{
     auto pipelineLayout = std::make_shared<magma::PipelineLayout>(
         std::move(setLayout));
     return std::make_shared<magma::GraphicsPipeline>(device,
        std::vector<magma::PipelineShaderStage>{
            loadShaderStage(vertexShaderFile)
        }, vertexInputState,
        magma::renderstates::triangleList,
        magma::TesselationState(),
        magma::ViewportState(0.f, 0.f, mrtFramebuffer->getExtent()),
        magma::renderstates::fillCullBackCW,
        mrtFramebuffer->getMultisampleState(),
        magma::renderstates::depthLessOrEqual,
        magma::renderstates::dontWriteRgba,
        std::initializer_list<VkDynamicState>{},
        std::move(pipelineLayout),
        mrtFramebuffer->getDepthRenderPass(), // Define only depth clear/store
        0, // subpass
        pipelineCache,
        nullptr, nullptr, 0);
}

std::shared_ptr<magma::GraphicsPipeline> GraphicsApp::createCommonPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
    const magma::VertexInputState& vertexInputState, std::shared_ptr<magma::DescriptorSetLayout> setLayout)
{
    return createCommonSpecializedPipeline(vertexShaderFile, fragmentShaderFile, nullptr, vertexInputState, std::move(setLayout));
}

std::shared_ptr<magma::GraphicsPipeline> GraphicsApp::createCommonSpecializedPipeline(
    const char *vertexShaderFile, const char *fragmentShaderFile, std::shared_ptr<magma::Specialization> specialization,
    const magma::VertexInputState& vertexInputState, std::shared_ptr<magma::DescriptorSetLayout> setLayout)
{
    auto pipelineLayout = std::make_shared<magma::PipelineLayout>(
         std::move(setLayout));
    return std::make_shared<magma::GraphicsPipeline>(device,
        std::vector<magma::PipelineShaderStage>{
            loadShaderStage(vertexShaderFile),
            loadShaderStage(fragmentShaderFile, std::move(specialization))
        }, vertexInputState,
        magma::renderstates::triangleList,
        magma::TesselationState(),
        magma::ViewportState(0.f, 0.f, msaaFramebuffer->getExtent()),
        magma::renderstates::fillCullBackCW,
        msaaFramebuffer->getMultisampleState(),
        magma::renderstates::depthLessOrEqual,
        magma::renderstates::dontBlendRgb,
        std::initializer_list<VkDynamicState>{},
        std::move(pipelineLayout),
        msaaFramebuffer->getRenderPass(), 0, // subpass
        pipelineCache,
        nullptr, nullptr, 0);
}

std::shared_ptr<magma::GraphicsPipeline> GraphicsApp::createMrtPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
    const magma::VertexInputState& vertexInputState, const magma::MultiColorBlendState& mrtBlendState,
    std::shared_ptr<magma::aux::MultiAttachmentFramebuffer> mrtFramebuffer, std::shared_ptr<magma::DescriptorSetLayout> setLayout)
{
    auto pipelineLayout = std::make_shared<magma::PipelineLayout>(
         std::move(setLayout));
    return std::make_shared<magma::GraphicsPipeline>(device,
        std::vector<magma::PipelineShaderStage>{
            loadShaderStage(vertexShaderFile),
            loadShaderStage(fragmentShaderFile)
        }, vertexInputState,
        magma::renderstates::triangleList,
        magma::TesselationState(),
        magma::ViewportState(0.f, 0.f, mrtFramebuffer->getExtent()),
        magma::renderstates::fillCullBackCW,
        mrtFramebuffer->getMultisampleState(),
        mrtFramebuffer->getDepthRenderPass() ? magma::renderstates::depthEqualDontWrite : magma::renderstates::depthLessOrEqual,
        mrtBlendState,
        std::initializer_list<VkDynamicState>{},
        std::move(pipelineLayout),
        mrtFramebuffer->getRenderPass(), 0, // subpass
        pipelineCache,
        nullptr, nullptr, 0);
}

std::shared_ptr<magma::GraphicsPipeline> GraphicsApp::createFullscreenPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
    std::shared_ptr<magma::DescriptorSetLayout> setLayout)
{
    std::shared_ptr<magma::PipelineLayout> pipelineLayout;
    if (setLayout)
        pipelineLayout = std::make_shared<magma::PipelineLayout>(std::move(setLayout));
    return std::make_shared<magma::GraphicsPipeline>(device,
        std::vector<magma::PipelineShaderStage>{
            loadShaderStage(vertexShaderFile),
            loadShaderStage(fragmentShaderFile)
        }, magma::renderstates::nullVertexInput,
        magma::renderstates::triangleStrip,
        magma::TesselationState(),
        magma::ViewportState(0.f, 0.f, msaaFramebuffer->getExtent()),
        magma::renderstates::fillCullBackCW,
        msaaFramebuffer->getMultisampleState(),
        magma::renderstates::depthAlwaysDontWrite,
        magma::renderstates::dontBlendRgba,
        std::initializer_list<VkDynamicState>{},
        std::move(pipelineLayout),
        msaaFramebuffer->getRenderPass(), 0, // subpass
        pipelineCache,
        nullptr, nullptr, 0);
}

std::shared_ptr<magma::GraphicsPipeline> GraphicsApp::createFullscreenPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
    std::shared_ptr<magma::Specialization> specialization, std::shared_ptr<magma::DescriptorSetLayout> setLayout,
    std::shared_ptr<magma::aux::Framebuffer> framebuffer)
{
    std::shared_ptr<magma::PipelineLayout> pipelineLayout;
    if (setLayout)
        pipelineLayout = std::make_shared<magma::PipelineLayout>(std::move(setLayout));
    return std::make_shared<magma::GraphicsPipeline>(device,
        std::vector<magma::PipelineShaderStage>{
            loadShaderStage(vertexShaderFile),
            loadShaderStage(fragmentShaderFile, std::move(specialization))
        }, magma::renderstates::nullVertexInput,
        magma::renderstates::triangleStrip,
        magma::TesselationState(),
        magma::ViewportState(0.f, 0.f, framebuffer->getExtent()),
        magma::renderstates::fillCullBackCW,
        framebuffer->getMultisampleState(),
        magma::renderstates::depthAlwaysDontWrite,
        magma::renderstates::dontBlendRgba,
        std::initializer_list<VkDynamicState>{},
        std::move(pipelineLayout),
        framebuffer->getRenderPass(), 0, // subpass
        pipelineCache,
        nullptr, nullptr, 0);
}

void GraphicsApp::updateViewProjTransforms()
{
    viewProj->updateView();
    viewProj->updateProjection();
    magma::helpers::mapScoped(viewProjTransforms,
        [this](auto *transforms)
        {
            transforms->view = viewProj->getView();
            transforms->viewInv = viewProj->getViewInv();
            transforms->proj = viewProj->getProj();
            transforms->projInv = viewProj->getProjInv();
            transforms->viewProj = viewProj->getViewProj();
            transforms->viewProjInv = rapid::inverse(viewProj->getViewProj());
            transforms->shadowProj = lightViewProj ? lightViewProj->calculateShadowProj() : rapid::identity();
        });
    updateLightSource(); // View-dependent
}

void GraphicsApp::updateSysUniforms()
{
    magma::helpers::mapScoped(sysUniforms,
        [this](auto *sys)
        {
            static float time = 0.0f;
            time += timer->secondsElapsed();

            sys->time = time;
            sys->screenSize = rapid::float4a(float(width), float(height), 1.f/width, 1.f/height);
            sys->mousePos.x = (float)mouseX;
            sys->mousePos.y = (float)mouseY;
        });
}

void GraphicsApp::updateObjectTransforms(const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>>& worldTransforms)
{
    assert(worldTransforms.size() == transforms->getArraySize());
    magma::helpers::mapScoped<Transforms>(transforms,
        [this, &worldTransforms](magma::helpers::AlignedUniformArray<Transforms>& array)
        {
            std::size_t i = 0;
            for (auto& it : array)
            {
                const rapid::matrix world = worldTransforms[i++];
                it.world = world;
                it.worldInv = rapid::inverse(world);
                it.worldView = world * viewProj->getView();
                it.worldViewProj = world * viewProj->getViewProj();
                it.worldLightProj = lightViewProj ? world * lightViewProj->getViewProj() : rapid::identity();
                it.normal = viewProj->calculateNormal(world);
            }
        });
}

void GraphicsApp::updateLightSource()
{
    if (lightViewProj)
    {
        lightViewProj->updateView();
        lightViewProj->updateProjection();
    }
    magma::helpers::mapScoped(lightSource,
        [this](auto *light)
        {
            if (lightViewProj)
                light->viewPosition = viewProj->getView() * lightViewProj->getPosition();
            else
                light->viewPosition = rapid::vector3(0.f);
            light->ambient = sRGBColor(0.4f, 0.4f, 0.4f);
            light->diffuse = sRGBColor(1.f, 1.f, 1.f);
            light->specular = light->diffuse;
        });
}

void GraphicsApp::blit(std::shared_ptr<const magma::ImageView> imageView, uint32_t bufferIndex)
{
    std::shared_ptr<magma::CommandBuffer> cmdBuffer = commandBuffers[bufferIndex];
    cmdBuffer->begin();
    cmdBuffer->beginRenderPass(renderPass, framebuffers[bufferIndex]);
    {
        const VkRect2D rc = VkRect2D{0, 0, width, height};
        msaaBltRect->blit(cmdBuffer, imageView, VK_FILTER_NEAREST, rc);
    }
    cmdBuffer->endRenderPass();
    cmdBuffer->end();
}

void GraphicsApp::submitCommandBuffers(uint32_t bufferIndex)
{
    queue->submit(drawCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        presentFinished, // Wait for swapchain
        drawSemaphore,
        nullptr);
    queue->submit(commandBuffers[bufferIndex],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        drawSemaphore, // Wait for scene rendering
        renderFinished,
        waitFences[bufferIndex]);
}

void GraphicsApp::sleep(long ms) noexcept
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
