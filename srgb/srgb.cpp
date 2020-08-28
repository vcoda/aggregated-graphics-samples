#include "graphicsApp.h"
#include "colorTable.h"
#include "quadric/include/sphere.h"

class GammaCorrection : public GraphicsApp
{
    std::unique_ptr<quadric::Sphere> sphere;
    std::shared_ptr<magma::GraphicsPipeline> rgbPipeline;
    std::shared_ptr<magma::GraphicsPipeline> srgbPipeline;
    DescriptorSet descriptor;

public:
    explicit GammaCorrection(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Gamma correction"), 1280, 720, false)
    {
        createTransformBuffer(2);
        setupViewProjection();
        createMesh();
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
        sleep(2); // Cap fps
    }

    virtual void updateLightSource() override
    {
        magma::helpers::mapScoped(lightSource,
            [this](auto *light)
            {
                const rapid::matrix normalMatrix = viewProj->calculateNormal(rapid::identity());
                const rapid::vector3 lightViewDir = normalMatrix * lightViewProj->getPosition();
                light->viewDirection = lightViewDir.normalized();
            });
    }

    void updateTransforms()
    {
        std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            rapid::translation(-1.5f, 0.f),
            rapid::translation(1.5f, 0.f),
        };
        updateObjectTransforms(transforms);
    }

    void setupViewProjection()
    {
        viewProj = std::make_unique<LeftHandedViewProjection>();
        viewProj->setPosition(0.f, 0.f, -100.f);
        viewProj->setFocus(0.f, 0.f, 0.f);
        viewProj->setFieldOfView(2.f);
        viewProj->setNearZ(90.f);
        viewProj->setFarZ(110.f);
        viewProj->setAspectRatio(width/(float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(1.f, 1.f, -0.4f);
        lightViewProj->setFocus(0.f, 0.f, 0.f);
        lightViewProj->setFieldOfView(70.f);
        lightViewProj->setNearZ(5.f);
        lightViewProj->setFarZ(30.f);
        lightViewProj->setAspectRatio(1.f);
        lightViewProj->updateView();
        lightViewProj->updateProjection();

        updateViewProjTransforms();
    }

    void createMesh()
    {
        sphere = std::make_unique<quadric::Sphere>(1.f, 64, 64, false, cmdCopyBuf);
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        descriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1))
            }));
        descriptor.set = descriptorPool->allocateDescriptorSet(descriptor.layout);
        descriptor.set->update(0, transforms);
        descriptor.set->update(1, viewProjTransforms);
        descriptor.set->update(2, lightSource);
    }

    void setupGraphicsPipelines()
    {
        struct Constants
        {
            VkBool32 sRGB;
        } constants;
        constants.sRGB = false;
        auto rgb(std::make_shared<magma::Specialization>(constants,
            magma::SpecializationEntry(0, &Constants::sRGB)));
        rgbPipeline = createCommonSpecializedPipeline(
            "transform.o", "phong.o", std::move(rgb),
            sphere->getVertexInput(),
            descriptor.layout);
        constants.sRGB = true;
        auto sRGB(std::make_shared<magma::Specialization>(constants,
            magma::SpecializationEntry(0, &Constants::sRGB)));
        srgbPipeline = createCommonSpecializedPipeline(
            "transform.o", "phong.o", std::move(sRGB),
            sphere->getVertexInput(),
            descriptor.layout);
    }

    void renderScene(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->begin();
        {
            cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer(),
                {
                    magma::ClearColor(0.35f, 0.53f, 0.7f, 1.f),
                    magma::clears::depthOne
                });
            {
                cmdBuffer->bindPipeline(rgbPipeline);
                cmdBuffer->bindDescriptorSet(rgbPipeline, descriptor.set, transforms->getDynamicOffset(0));
                sphere->draw(cmdBuffer);
                cmdBuffer->bindPipeline(srgbPipeline);
                cmdBuffer->bindDescriptorSet(srgbPipeline, descriptor.set, transforms->getDynamicOffset(1));
                sphere->draw(cmdBuffer);
            }
            cmdBuffer->endRenderPass();
        }
        cmdBuffer->end();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<GammaCorrection>(new GammaCorrection(entry));
}
