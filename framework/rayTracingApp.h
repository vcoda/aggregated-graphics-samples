#pragma once
#include "vulkanApp.h"
#include "common.h"
#include "arcball.h"
#include "timer.h"

class RayTracingApp : public VulkanApp
{
public:
    RayTracingApp(const AppEntry& entry, const core::tstring& caption,
        uint32_t width, uint32_t height, bool sRGB);
    virtual void onMouseMove(int x, int y) override;
    virtual void onMouseLButton(bool down, int x, int y) override;

protected:
    virtual void enableDeviceFeatures(VkPhysicalDeviceFeatures& features) const noexcept override;
    virtual void enableDeviceFeaturesExt(std::vector<void *>& features) const override;
    virtual void enableExtensions(std::vector<const char*>& extensionNames) const override;
    void createOutputImage(VkFormat colorFormat);
    std::shared_ptr<magma::ShaderModule> loadShader(const char *shaderFileName) const;
    magma::PipelineShaderStage loadShaderStage(const char *shaderFileName, VkShaderStageFlagBits stage,
        std::shared_ptr<magma::Specialization> specialization = nullptr) const;
    void resizeScratchBuffer(VkDeviceSize size);
    void blit(std::shared_ptr<const magma::ImageView> imageView, uint32_t bufferIndex);
    void submitCommandBuffers(uint32_t bufferIndex);

private:
    void createSamplers();

protected:
    std::shared_ptr<magma::StorageImage2D> outputImage;
    std::shared_ptr<magma::ImageView> outputImageView;
    std::shared_ptr<magma::CommandBuffer> buildCmdBuffer;
    std::shared_ptr<magma::CommandBuffer> rtCmdBuffer;
    std::shared_ptr<magma::Semaphore> rtSemaphore;
    std::shared_ptr<magma::DescriptorPool> descriptorPool;

    std::shared_ptr<magma::Sampler> nearestRepeat;
    std::shared_ptr<magma::Sampler> bilinearRepeat;
    std::shared_ptr<magma::Sampler> trilinearRepeat;
    std::shared_ptr<magma::Sampler> anisotropicRepeat;

    std::shared_ptr<magma::AccelerationStructure> tlas;
    std::shared_ptr<magma::Buffer> scratchBuffer;
    std::shared_ptr<magma::AccelerationStructureInstanceBuffer> instanceBuffer;
    std::shared_ptr<magma::RayTracingPipeline> rtPipeline;
    std::shared_ptr<magma::ShaderBindingTable> shaderBindingTable;

    std::shared_ptr<magma::UniformBuffer<LightSource>> lightSource;
    std::shared_ptr<magma::UniformBuffer<RtTransforms>> transforms;

    std::unique_ptr<magma::aux::BlitRectangle> bltRect;
    std::shared_ptr<Arcball> arcball;
    std::unique_ptr<Timer> timer;
    int mouseX = 0;
    int mouseY = 0;
};
