#include "graphicsApp.h"

class PingPongBlur : public GraphicsApp
{
    static constexpr uint32_t numPasses = 32;

    struct Constants
    {
        VkBool32 ping;
    };

    struct Pass
    {
        std::shared_ptr<magma::aux::ColorFramebuffer> framebuffer;
        std::shared_ptr<magma::DescriptorSet> set;
        std::shared_ptr<magma::GraphicsPipeline> pipeline;
    } ping, pong;

    std::shared_ptr<magma::GraphicsPipeline> checkerboardPipeline;

    bool blurImage = true;

public:
    explicit PingPongBlur(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Ping-pong blur"), 1280, 32 * 22, false)
    {
        createPingPongPasses();
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

    Pass createPass(std::shared_ptr<magma::DescriptorSetLayout> layout, bool ping)
    {
        Pass pass;
        pass.framebuffer = std::make_shared<magma::aux::ColorFramebuffer>(device,
            VK_FORMAT_R8G8B8A8_UNORM, msaaFramebuffer->getExtent(), false);
        pass.set = descriptorPool->allocateDescriptorSet(layout);
        const Constants constant = {MAGMA_BOOLEAN(ping)};
        const magma::SpecializationEntry entry(0, &Constants::ping);
        auto specialization = std::make_shared<magma::Specialization>(constant, entry);
        pass.pipeline = createFullscreenPipeline("quadoff.o", "bilerp.o", std::move(specialization), std::move(layout), pass.framebuffer);
        return pass;
    }

    void createPingPongPasses()
    {
        auto layout = std::make_shared<magma::DescriptorSetLayout>(device,
            magma::bindings::VertexFragmentStageBinding(0, magma::descriptors::CombinedImageSampler(1)));
        ping = createPass(layout, true);
        pong = createPass(layout, false);
        ping.set->writeDescriptor(0, pong.framebuffer->getColorView(), bilinearRepeat); // Use hardware bilinear filtering
        pong.set->writeDescriptor(0, ping.framebuffer->getColorView(), bilinearRepeat); // Use hardware bilinear filtering
    }

    void setupGraphicsPipelines()
    {
        checkerboardPipeline = createFullscreenPipeline("quad.o", "checkerboard.o", nullptr, nullptr, pong.framebuffer);
        bltRect = std::make_unique<magma::aux::BlitRectangle>(renderPass);
    }

    void renderScene(uint32_t bufferIndex)
    {
        std::shared_ptr<magma::CommandBuffer> cmdBuffer = commandBuffers[bufferIndex];
        cmdBuffer->begin();
        {
            if (!blurImage)
                checkerboardPass(cmdBuffer, bufferIndex);
            else
            {
                if (FrontBuffer == bufferIndex)
                {   // Draw once
                    checkerboardPass(cmdBuffer, bufferIndex);
                    blurPass(cmdBuffer);
                }
                blitPass(cmdBuffer, bufferIndex);
            }
        }
        cmdBuffer->end();
    }

    void checkerboardPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer, uint32_t bufferIndex)
    {
        if (blurImage)
            cmdBuffer->beginRenderPass(pong.framebuffer->getRenderPass(), pong.framebuffer->getFramebuffer());
        else // Draw to swapchain
            cmdBuffer->beginRenderPass(renderPass, framebuffers[bufferIndex]);
        {
            cmdBuffer->bindPipeline(checkerboardPipeline);
            cmdBuffer->draw(4, 0);
        }
        cmdBuffer->endRenderPass();
    }

    void blurPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        for (uint32_t i = 0; i < numPasses; ++i)
        {
            const Pass& pass = i % 2 ? pong : ping;
            cmdBuffer->beginRenderPass(pass.framebuffer->getRenderPass(), pass.framebuffer->getFramebuffer());
            {
                cmdBuffer->bindPipeline(pass.pipeline);
                cmdBuffer->bindDescriptorSet(pass.pipeline, pass.set);
                cmdBuffer->draw(4, 0);
            }
            cmdBuffer->endRenderPass();
        }
    }

    void blitPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer, uint32_t bufferIndex)
    {
        cmdBuffer->beginRenderPass(renderPass, framebuffers[bufferIndex]);
        {
            const VkRect2D rc{0, 0, width, height};
            bltRect->blit(cmdBuffer, pong.framebuffer->getColorView(), VK_FILTER_NEAREST, rc);
        }
        cmdBuffer->endRenderPass();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<PingPongBlur>(new PingPongBlur(entry));
}
