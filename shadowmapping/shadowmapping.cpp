#include "graphicsApp.h"
#include "quadric/include/teapot.h"
#include "quadric/include/plane.h"

class ShadowMapping : public GraphicsApp
{
    std::unique_ptr<quadric::Teapot> teapot;
    std::unique_ptr<quadric::Plane> ground;
    std::shared_ptr<magma::GraphicsPipeline> shadowMapPipeline;
    std::shared_ptr<magma::GraphicsPipeline> diffuseShadowPipeline;
    std::shared_ptr<magma::aux::DepthFramebuffer> shadowMap;
    std::shared_ptr<magma::Sampler> shadowSampler;
    DescriptorSet smDescriptor;
    DescriptorSet descriptor;

    rapid::matrix objTransforms[2];
    bool showDepthMap = false;

public:
    explicit ShadowMapping(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Shadow mapping"), 1280, 720, true)
    {
        setupViewProjection();
        setupTransforms();
        createShadowMap();
        createMeshObjects();
        setupDescriptorSets();
        setupGraphicsPipelines();

        renderScene(drawCmdBuffer);
        blit(msaaFramebuffer->getColorView(), FrontBuffer);
        blit(msaaFramebuffer->getColorView(), BackBuffer);
    }

    virtual void render(uint32_t bufferIndex) override
    {
        updateTransforms();
        updateViewProjTransforms();
        submitCommandBuffers(bufferIndex);
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Cap fps
    }

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Left:
            if (lightViewProj->getPosition().x > -20.f)
                lightViewProj->translate(-0.5f, 0.f, 0.f);
            break;
        case AppKey::Right:
            if (lightViewProj->getPosition().x < 20.f)
                lightViewProj->translate(0.5f, 0.f, 0.f);
            break;
        case AppKey::Up:
            if (lightViewProj->getPosition().z < 20.f)
                lightViewProj->translate(0.f, 0.f, 0.5f);
            break;
        case AppKey::Down:
            if (lightViewProj->getPosition().z > -20.f)
                lightViewProj->translate(0.f, 0.f, -0.5f);
            break;
        case AppKey::Space:
            showDepthMap = !showDepthMap;
            renderScene(drawCmdBuffer);
            break;
        }
        lightViewProj->updateView();
        lightViewProj->updateProjection();
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    void setupViewProjection()
    {
        viewProj = std::make_unique<LeftHandedViewProjection>();
        viewProj->setPosition(0.f, 7.f, -12.f);
        viewProj->setFocus(0.f, 1.f, 0.f);
        viewProj->setFieldOfView(40.f);
        viewProj->setNearZ(1.f);
        viewProj->setFarZ(100.f);
        viewProj->setAspectRatio(width/(float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-5.f, 11.f, 3.f);
        lightViewProj->setFocus(0.f, 0.f, 0.f);
        lightViewProj->setFieldOfView(45.f);
        lightViewProj->setNearZ(5.f);
        lightViewProj->setFarZ(40.f);
        lightViewProj->setAspectRatio(1.f);
        lightViewProj->updateView();
        lightViewProj->updateProjection();

        updateViewProjTransforms();
    }

    void setupTransforms()
    {
        createTransformBuffer(2);
        objTransforms[0] = rapid::identity();
        objTransforms[1] = rapid::identity();
    }

    void updateTransforms()
    {
        const rapid::matrix rotation = rapid::rotationY(rapid::radians(-spinX/4.f));
        const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            objTransforms[0] * rotation, objTransforms[1]};
        updateObjectTransforms(transforms);
    }

    void createShadowMap()
    {
        constexpr VkFormat depthFormat = VK_FORMAT_D16_UNORM; // 16 bits of depth is enough for a tiny scene
        constexpr VkExtent2D extent{2048, 2048};
        shadowMap = std::make_shared<magma::aux::DepthFramebuffer>(device, depthFormat, extent);
        shadowSampler = std::make_shared<magma::DepthSampler>(device, magma::samplers::magMinNearestCompareLessOrEqual);
    }

    void createMeshObjects()
    {
        teapot = std::make_unique<quadric::Teapot>(16, cmdCopyBuf);
        ground = std::make_unique<quadric::Plane>(100.f, 100.f, false, cmdCopyBuf);
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // Shadow map shader
        smDescriptor.layout = std::make_shared<magma::DescriptorSetLayout>(device,
            VertexStageBinding(0, magma::descriptors::DynamicUniformBuffer(1)));
        smDescriptor.set = descriptorPool->allocateDescriptorSet(smDescriptor.layout);
        smDescriptor.set->update(0, transforms);
        // Lighting shader
        descriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1)),
                FragmentStageBinding(3, CombinedImageSampler(1))
            }));
        descriptor.set = descriptorPool->allocateDescriptorSet(descriptor.layout);
        descriptor.set->update(0, transforms);
        descriptor.set->update(1, viewProjTransforms);
        descriptor.set->update(2, lightSource);
        descriptor.set->update(3, shadowMap->getDepthView(), shadowSampler);
    }

    void setupGraphicsPipelines()
    {
        shadowMapPipeline = createShadowMapPipeline(
            "shadowMap.o",
            teapot->getVertexInput(),
            magma::renderstates::fillCullFrontCW, // Draw only back faces to get rid of shadow acne
            smDescriptor.layout,
            shadowMap);
        diffuseShadowPipeline = createCommonPipeline(
            "transform.o", "shadow.o",
            teapot->getVertexInput(),
            descriptor.layout);
    }

    void renderScene(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->begin();
        {
            shadowMapPass(cmdBuffer);
            lightingPass(cmdBuffer);
        }
        cmdBuffer->end();
    }

    void shadowMapPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(shadowMap->getRenderPass(), shadowMap->getFramebuffer(),
            {
                magma::clears::depthOne
            });
        {
            cmdBuffer->setViewport(magma::Viewport(0, 0, shadowMap->getExtent()));
            cmdBuffer->setScissor(magma::Scissor(0, 0, shadowMap->getExtent()));
            cmdBuffer->bindPipeline(shadowMapPipeline);
            cmdBuffer->bindDescriptorSet(shadowMapPipeline, smDescriptor.set, transforms->getDynamicOffset(0));
            teapot->draw(cmdBuffer);
        }
        cmdBuffer->endRenderPass();
    }

    void lightingPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer(),
            {
                magma::ClearColor(0.35f, 0.53f, 0.7f, 1.0f),
                magma::clears::depthOne
            });
        {
            cmdBuffer->setViewport(magma::Viewport(0, 0, msaaFramebuffer->getExtent()));
            cmdBuffer->setScissor(magma::Scissor(0, 0, msaaFramebuffer->getExtent()));
            cmdBuffer->bindPipeline(diffuseShadowPipeline);
            cmdBuffer->bindDescriptorSet(diffuseShadowPipeline, descriptor.set, transforms->getDynamicOffset(0));
            teapot->draw(cmdBuffer);
            cmdBuffer->bindDescriptorSet(diffuseShadowPipeline, descriptor.set, transforms->getDynamicOffset(1));
            ground->draw(cmdBuffer);
            if (showDepthMap)
                drawDepthMap(cmdBuffer);
        }
        cmdBuffer->endRenderPass();
    }

    void drawDepthMap(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        constexpr VkRect2D rect = VkRect2D{0, 0, 256U, 256U};
        if (!bltRect) bltRect = std::make_unique<magma::aux::BlitRectangle>(renderPass, loadShader("linearizeDepth.o"));
        bltRect->blit(std::move(cmdBuffer), shadowMap->getDepthView(), VK_FILTER_NEAREST, rect);
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::make_unique<ShadowMapping>(entry);
}
