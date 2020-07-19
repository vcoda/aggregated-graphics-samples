#include "graphicsApp.h"
#include "colorTable.h"
#include "quadric/include/cube.h"
#include "quadric/include/sphere.h"
#include "quadric/include/teapot.h"
#include "quadric/include/plane.h"

class PcfShadowMapping : public GraphicsApp
{
    enum {
        Cube = 0, Teapot, Sphere, Ground,
        MaxObjects
    };

    struct alignas(16) SpecializationConstants
    {
        VkBool32 filterPoisson;
    };

    std::unique_ptr<quadric::Quadric> objects[MaxObjects];
    std::shared_ptr<magma::GraphicsPipeline> shadowMapPipeline;
    std::shared_ptr<magma::GraphicsPipeline> pcfShadowPipeline;
    std::shared_ptr<magma::aux::DepthFramebuffer> shadowMap;
    std::shared_ptr<magma::Sampler> shadowSampler;
    DescriptorSet smDescriptor;
    DescriptorSet descriptor;

    rapid::matrix objTransforms[MaxObjects];
    bool filterPoisson = false;
    bool showDepthMap = false;

public:
    explicit PcfShadowMapping(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Percentage closer filtering"), 1280, 720, true)
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
        submitCommandBuffers(bufferIndex);
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Cap fps
    }

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Enter:
            filterPoisson = !filterPoisson;
            setupGraphicsPipelines();
            renderScene(drawCmdBuffer);
            break;
        case AppKey::Space:
            showDepthMap = !showDepthMap;
            renderScene(drawCmdBuffer);
            break;
        }
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    virtual void updateLightSource()
    {
        magma::helpers::mapScoped<LightSource>(lightSource,
            [this](auto *light)
            {
                constexpr float ambientFactor = 0.4f;
                light->viewPosition = viewProj->getView() * lightViewProj->getPosition();
                light->ambient = ghost_white * ambientFactor;
                light->diffuse = ghost_white;
                light->specular = ghost_white;
            });
    }

    void setupViewProjection()
    {
        viewProj = std::make_unique<LeftHandedViewProjection>();
        viewProj->setPosition(0.f, 11.f, -18.f);
        viewProj->setFocus(0.f, 0.f, -1.f);
        viewProj->setFieldOfView(45.f);
        viewProj->setNearZ(1.f);
        viewProj->setFarZ(100.f);
        viewProj->setAspectRatio(width/(float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-8.f, 12.f, 1.f);
        lightViewProj->setFocus(0.f, 0.f, 0.f);
        lightViewProj->setFieldOfView(70.f);
        lightViewProj->setNearZ(5.f);
        lightViewProj->setFarZ(30.f);
        lightViewProj->setAspectRatio(1.f);
        lightViewProj->updateView();
        lightViewProj->updateProjection();

        updateViewProjTransforms();
    }

    void setupTransforms()
    {
        createTransformBuffer(MaxObjects);
        constexpr float radius = 4.f;
        objTransforms[Cube] = rapid::translation(radius, 1.f, 0.f) * rapid::rotationY(rapid::radians(30.f));
        objTransforms[Teapot] = rapid::translation(radius, 0.f, 0.f) * rapid::rotationY(rapid::radians(150.f));
        objTransforms[Sphere] = rapid::translation(radius, 1.5f, 0.f) * rapid::rotationY(rapid::radians(270.f));
        constexpr float bias = -0.05f; // Shift slightly down to get rid of shadow leakage
        objTransforms[Ground] = rapid::translation(0.f, bias, 0.f);
    }

    void updateTransforms()
    {
        const rapid::matrix rotation = rapid::rotationY(rapid::radians(-spinX/4.f));
        std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms(MaxObjects);
        transforms[Cube] = objTransforms[Cube] * rotation,
        transforms[Teapot] = objTransforms[Teapot] * rotation,
        transforms[Sphere] = objTransforms[Sphere] * rotation,
        transforms[Ground] = objTransforms[Ground];
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
        objects[Cube] = std::make_unique<quadric::Cube>(cmdCopyBuf);
        objects[Teapot] = std::make_unique<quadric::Teapot>(16, cmdCopyBuf);
        objects[Sphere] = std::make_unique<quadric::Sphere>(1.5f, 64, 64, false, cmdCopyBuf);
        objects[Ground] = std::make_unique<quadric::Plane>(100.f, 100.f, false, cmdCopyBuf);
    }

    void setupDescriptorSets()
    {   // Shadow map shader
        smDescriptor.layout = std::make_shared<magma::DescriptorSetLayout>(device,
            magma::bindings::VertexStageBinding(0, magma::descriptors::DynamicUniformBuffer(1)));
        smDescriptor.set = descriptorPool->allocateDescriptorSet(smDescriptor.layout);
        smDescriptor.set->update(0, transforms);
        // Lighting shader
        descriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                magma::bindings::VertexStageBinding(0, magma::descriptors::DynamicUniformBuffer(1)),
                magma::bindings::FragmentStageBinding(1, magma::descriptors::UniformBuffer(1)),
                magma::bindings::FragmentStageBinding(2, magma::descriptors::UniformBuffer(1)),
                magma::bindings::FragmentStageBinding(3, magma::descriptors::CombinedImageSampler(1))
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
            objects[0]->getVertexInput(),
            magma::renderstates::fillCullFrontCW, // Draw only back faces to get rid of shadow acne
            smDescriptor.layout,
            shadowMap);

        SpecializationConstants constants;
        constants.filterPoisson = filterPoisson;
        std::shared_ptr<magma::Specialization> specialization(new magma::Specialization(constants,
            {
                magma::SpecializationEntry(0, &SpecializationConstants::filterPoisson)
            }
        ));

        pcfShadowPipeline = createCommonSpecializedPipeline(
            "transform.o", "shadow.o",
            std::move(specialization),
            objects[0]->getVertexInput(),
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
            for (uint32_t i = Cube; i < Ground; ++i)
            {
                cmdBuffer->bindDescriptorSet(shadowMapPipeline, smDescriptor.set, transforms->getDynamicOffset(i));
                objects[i]->draw(cmdBuffer);
            }
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
            cmdBuffer->bindPipeline(pcfShadowPipeline);
            for (uint32_t i = Cube; i < MaxObjects; ++i)
            {
                cmdBuffer->bindDescriptorSet(pcfShadowPipeline, descriptor.set, transforms->getDynamicOffset(i));
                objects[i]->draw(cmdBuffer);
            }
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
    return std::make_unique<PcfShadowMapping>(entry);
}
