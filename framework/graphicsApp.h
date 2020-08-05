#pragma once
#include "vulkanApp.h"
#include "core/alignedAllocator.h"
#include "common.h"
#include "viewProjection.h"
#include "arcball.h"
#include "timer.h"

class GraphicsApp : public VulkanApp
{
public:
    struct alignas(16) SysUniforms
    {
        float time;
        rapid::float4a screenSize; // x, y, 1/x, 1/y
        rapid::float2a mousePos;
    };

public:
    GraphicsApp(const AppEntry& entry, const core::tstring& caption,
        uint32_t width, uint32_t height, bool sRGB);
    virtual void onMouseMove(int x, int y) override;
    virtual void onMouseLButton(bool down, int x, int y) override;

protected:
    virtual void createMultisampleFramebuffer(VkFormat colorFormat);
    void createDrawCommandBuffer();
    void createSamplers();
    void allocateViewProjTransforms();
    void createTransformBuffer(uint32_t numObjects);

    std::shared_ptr<magma::ShaderModule> loadShader(const char *shaderFileName) const;
    magma::PipelineShaderStage loadShaderStage(const char *shaderFileName,
        std::shared_ptr<magma::Specialization> specialization = nullptr) const;
    std::shared_ptr<magma::GraphicsPipeline> createShadowMapPipeline(const char *vertexShaderFile,
        const magma::VertexInputState& vertexInputState, const magma::RasterizationState& rasterizationState,
        std::shared_ptr<magma::DescriptorSetLayout> setLayout, std::shared_ptr<magma::aux::DepthFramebuffer> framebuffer);

    std::shared_ptr<magma::GraphicsPipeline> createDepthOnlyPipeline(const char *vertexShaderFile,
        const magma::VertexInputState& vertexInputState, std::shared_ptr<magma::DescriptorSetLayout> setLayout,
        std::shared_ptr<magma::aux::MultiAttachmentFramebuffer> mrtFramebuffer);

    std::shared_ptr<magma::GraphicsPipeline> createCommonPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
        const magma::VertexInputState& vertexInputState, std::shared_ptr<magma::DescriptorSetLayout> setLayout);
    std::shared_ptr<magma::GraphicsPipeline> createCommonSpecializedPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
        std::shared_ptr<magma::Specialization> specialization, const magma::VertexInputState& vertexInputState,
        std::shared_ptr<magma::DescriptorSetLayout> setLayout);
    std::shared_ptr<magma::GraphicsPipeline> createMrtPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
        const magma::VertexInputState& vertexInputState, const magma::MultiColorBlendState& mrtBlendState,
        std::shared_ptr<magma::aux::MultiAttachmentFramebuffer> mrtFramebuffer, std::shared_ptr<magma::DescriptorSetLayout> setLayout);
    std::shared_ptr<magma::GraphicsPipeline> createFullscreenPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
        std::shared_ptr<magma::DescriptorSetLayout> setLayout);
    std::shared_ptr<magma::GraphicsPipeline> createFullscreenPipeline(const char *vertexShaderFile, const char *fragmentShaderFile,
        std::shared_ptr<magma::Specialization> specialization, std::shared_ptr<magma::DescriptorSetLayout> setLayout,
        std::shared_ptr<magma::aux::Framebuffer> framebuffer);

    void updateSysUniforms();
    void updateViewProjTransforms();
    void updateObjectTransforms(const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>>& worldTransforms);
    virtual void updateLightSource();

    void blit(std::shared_ptr<const magma::ImageView> imageView, uint32_t bufferIndex);
    void submitCommandBuffers(uint32_t bufferIndex);

protected:
    std::unique_ptr<magma::aux::ColorMultisampleFramebuffer> msaaFramebuffer;
    std::unique_ptr<magma::aux::BlitRectangle> msaaBltRect;
    std::shared_ptr<magma::CommandBuffer> drawCmdBuffer;
    std::shared_ptr<magma::Semaphore> drawSemaphore;

    std::unique_ptr<ViewProjection> viewProj;
    std::unique_ptr<ViewProjection> lightViewProj;
    std::unique_ptr<magma::aux::BlitRectangle> bltRect;

    std::shared_ptr<magma::UniformBuffer<SysUniforms>> sysUniforms;
    std::shared_ptr<magma::DynamicUniformBuffer<Transforms>> transforms;
    std::shared_ptr<magma::UniformBuffer<ViewProjTransforms>> viewProjTransforms;
    std::shared_ptr<magma::UniformBuffer<LightSource>> lightSource;
    std::shared_ptr<magma::DescriptorPool> descriptorPool;

    std::shared_ptr<magma::Sampler> nearestRepeat;
    std::shared_ptr<magma::Sampler> bilinearRepeat;
    std::shared_ptr<magma::Sampler> trilinearRepeat;
    std::shared_ptr<magma::Sampler> anisotropicRepeat;
    std::shared_ptr<magma::Sampler> nearestClampToEdge;
    std::shared_ptr<magma::Sampler> bilinearClampToEdge;
    std::shared_ptr<magma::Sampler> trilinearClampToEdge;
    std::shared_ptr<magma::Sampler> anisotropicClampToEdge;

    std::shared_ptr<Arcball> arcball;
    std::unique_ptr<Timer> timer;
    int mouseX = 0;
    int mouseY = 0;
};
