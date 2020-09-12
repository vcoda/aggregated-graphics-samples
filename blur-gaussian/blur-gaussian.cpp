#include "graphicsApp.h"

class GaussianBlur : public GraphicsApp
{
    static constexpr uint32_t samples = 15; // odd

    struct Constants
    {
        VkBool32 horzPass;
    };

    struct alignas(16) Weights
    {
        float weights[samples + 1];
    };

    std::shared_ptr<magma::aux::ColorFramebuffer> inputFramebuffer;
    std::shared_ptr<magma::aux::ColorFramebuffer> tempFramebuffer;
    std::shared_ptr<magma::StorageBuffer> weightsBuffer;
    std::shared_ptr<magma::GraphicsPipeline> checkerboardPipeline;
    std::shared_ptr<magma::GraphicsPipeline> horzPassPipeline;
    std::shared_ptr<magma::GraphicsPipeline> vertPassPipeline;
    DescriptorSet horzDescriptor;
    DescriptorSet vertDescriptor;

    bool blurImage = true;

public:
    explicit GaussianBlur(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Gaussian blur"), 1280, 32 * 22, false)
    {
        createFramebuffers();
        precomputeGaussianWeights();
        setupDescriptorSets();
        setupGraphicsPipelines();
        renderScene(FrontBuffer);
        renderScene(BackBuffer);
    }

    virtual void render(uint32_t bufferIndex) override
    {
        queue->submit(commandBuffers[bufferIndex],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            presentFinished, // Wait for swapchain
            renderFinished,
            waitFences[bufferIndex]);
        sleep(2); // Cap fps
    }

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Space:
            blurImage = !blurImage;
            renderScene(FrontBuffer);
            renderScene(BackBuffer);
            break;
        }
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    void createFramebuffers()
    {
        constexpr bool clearOp = false;
        inputFramebuffer = std::make_shared<magma::aux::ColorFramebuffer>(device,
            VK_FORMAT_R8G8B8A8_UNORM,
            msaaFramebuffer->getExtent(),
            clearOp);
        tempFramebuffer = std::make_shared<magma::aux::ColorFramebuffer>(device,
            VK_FORMAT_R8G8B8A8_UNORM,
            msaaFramebuffer->getExtent(),
            clearOp);
    }

    // https://en.wikipedia.org/wiki/Normal_distribution
    float normalPdf(int x, float sigma)
    {
        const float invSqrtTwoPi = 1.f/sqrtf(rapid::constants::twoPi); // 0.398942
        return invSqrtTwoPi * expf(-0.5f * x * x/(sigma * sigma))/sigma;
    }

    void precomputeGaussianWeights()
    {
        constexpr int n = samples >> 1;
        constexpr float sigma = 7.f;
        float alignas(16) weights[samples + 1];
        float sum = 0.f;
        for (int x = -n, i = 0; x <= n; ++x, ++i)
        {
            weights[i] = normalPdf(x, sigma);
            sum += weights[i];
        }
        weights[samples] = 1.f/sum; // Normalization factor
        weightsBuffer = std::make_shared<magma::StorageBuffer>(cmdCopyBuf, weights, sizeof(weights));
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // 1. Horizontal pass
        horzDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                FragmentStageBinding(0, CombinedImageSampler(1)),
                FragmentStageBinding(1, StorageBuffer(1)),
            }));
        horzDescriptor.set = descriptorPool->allocateDescriptorSet(horzDescriptor.layout);
        horzDescriptor.set->update(0, inputFramebuffer->getColorView(), nearestRepeat);
        horzDescriptor.set->update(1, weightsBuffer);
        // 2. Vertical pass
        vertDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                FragmentStageBinding(0, CombinedImageSampler(1)),
                FragmentStageBinding(1, StorageBuffer(1)),
            }));
        vertDescriptor.set = descriptorPool->allocateDescriptorSet(vertDescriptor.layout);
        vertDescriptor.set->update(0, tempFramebuffer->getColorView(), nearestRepeat);
        vertDescriptor.set->update(1, weightsBuffer);
    }

    void setupGraphicsPipelines()
    {
        checkerboardPipeline = createFullscreenPipeline("quad.o", "checkerboard.o", nullptr, nullptr, inputFramebuffer);
        const magma::SpecializationEntry entry(0, &Constants::horzPass);
        // 1. Horizontal pass
        Constants constants;
        constants.horzPass = VK_TRUE;
        auto specialization = std::make_shared<magma::Specialization>(constants, entry);
        horzPassPipeline = createFullscreenPipeline("quad.o", "blur.o", std::move(specialization), horzDescriptor.layout, tempFramebuffer);
        // 2. Vertical pass
        constants.horzPass = VK_FALSE;
        specialization = std::make_shared<magma::Specialization>(constants, entry);
        vertPassPipeline = createFullscreenPipeline("quad.o", "blur.o", std::move(specialization), vertDescriptor.layout, framebuffers[0]);
    }

    void renderScene(uint32_t bufferIndex)
    {
        std::shared_ptr<magma::CommandBuffer> cmdBuffer = commandBuffers[bufferIndex];
        cmdBuffer->begin();
        {
            checkerboardPass(cmdBuffer, bufferIndex);
            if (blurImage)
                blurPass(cmdBuffer, bufferIndex);
        }
        cmdBuffer->end();
    }

    void checkerboardPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer, uint32_t bufferIndex)
    {
        if (blurImage)
            cmdBuffer->beginRenderPass(inputFramebuffer->getRenderPass(), inputFramebuffer->getFramebuffer());
        else
            cmdBuffer->beginRenderPass(renderPass, framebuffers[bufferIndex]); // Draw to swapchain
        {
            cmdBuffer->bindPipeline(checkerboardPipeline);
            cmdBuffer->draw(4, 0);
        }
        cmdBuffer->endRenderPass();
    }

    void blurPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer, uint32_t bufferIndex)
    {   // 1. Horizontal pass
        cmdBuffer->beginRenderPass(tempFramebuffer->getRenderPass(), tempFramebuffer->getFramebuffer());
        {
            cmdBuffer->bindPipeline(horzPassPipeline);
            cmdBuffer->bindDescriptorSet(horzPassPipeline, horzDescriptor.set);
            cmdBuffer->draw(4, 0);
        }
        cmdBuffer->endRenderPass();
        // 2. Vertical pass
        cmdBuffer->beginRenderPass(renderPass, framebuffers[bufferIndex]);
        {
            cmdBuffer->bindPipeline(vertPassPipeline);
            cmdBuffer->bindDescriptorSet(vertPassPipeline, vertDescriptor.set);
            cmdBuffer->draw(4, 0);
        }
        cmdBuffer->endRenderPass();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<GaussianBlur>(new GaussianBlur(entry));
}
