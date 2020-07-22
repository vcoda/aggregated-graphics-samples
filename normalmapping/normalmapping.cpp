#include "graphicsApp.h"
#include "textureLoader.h"

#include "quadric/include/torus.h"
#include "quadric/include/sphere.h"

class NormalMapping : public GraphicsApp
{
    struct Constants
    {
        VkBool32 showNormals;
    };

    std::unique_ptr<quadric::Torus> torus;
    std::unique_ptr<quadric::Sphere> sphere;
    std::shared_ptr<magma::ImageView> normalMap;
    std::shared_ptr<magma::GraphicsPipeline> bumpPipeline;
    std::shared_ptr<magma::GraphicsPipeline> spherePipeline;
    DescriptorSet bumpDescriptor;
    DescriptorSet fillDescriptor;

    Constants constants = {false};

public:
    explicit NormalMapping(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Normal mapping"), 1280, 720, true)
    {
        setupViewProjection();
        createTransformBuffer(2);
        createMeshObjects();
        loadTexture();
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
        constexpr float step = 0.1f;
        switch (key)
        {
        case AppKey::Left:
            lightViewProj->translate(-step, 0.f, 0.f);
            break;
        case AppKey::Right:
            lightViewProj->translate(step, 0.f, 0.f);
            break;
        case AppKey::Up:
            lightViewProj->translate(0.f, step, 0.f);
            break;
        case AppKey::Down:
            lightViewProj->translate(0.f, -step, 0.f);
            break;
        case AppKey::PgUp:
            lightViewProj->translate(0.f, 0.f, step);
            break;
        case AppKey::PgDn:
            lightViewProj->translate(0.f, 0.f, -step);
            break;
        case AppKey::Space:
            constants.showNormals = !constants.showNormals;
            setupGraphicsPipelines();
            renderScene(drawCmdBuffer);
            break;
        }
        updateTransforms();
        updateLightSource();
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    virtual void onMouseWheel(float distance) override
    {
        float step = distance * 0.2f;
        float z = viewProj->getPosition().z;
        if ((z + step < -2.5f) && (z + step > -15.f))
            viewProj->translate(0.f, 0.f, step);
        updateViewProjTransforms();
    }

    void setupViewProjection()
    {
        viewProj = std::make_unique<LeftHandedViewProjection>();
        viewProj->setPosition(0.f, 0.f, -4.f);
        viewProj->setFocus(0.f, 0.f, 0.f);
        viewProj->setFieldOfView(60.f);
        viewProj->setNearZ(1.f);
        viewProj->setFarZ(100.f);
        viewProj->setAspectRatio(width / (float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-1., 1.f, -1.f);

        updateViewProjTransforms();
    }

    void updateTransforms()
    {
        rapid::matrix lightTransform = rapid::translation(rapid::vector3(lightViewProj->getPosition()));
        const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            arcball->transform(),
            lightTransform
        };
        updateObjectTransforms(transforms);
    }

    void createMeshObjects()
    {
        torus = std::make_unique<quadric::Torus>(0.4f, 1.0f, 64, 128, true, cmdCopyBuf);
        sphere = std::make_unique<quadric::Sphere>(0.02f, 8, 8, true, cmdCopyBuf);
    }

    void loadTexture()
    {   // https://freepbr.com/materials/steel-plate1/
        normalMap = loadDxtTexture(cmdCopyImg, "steelplate1_normal-dx.dds");
    }

    void setupDescriptorSets()
    {
        bumpDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                magma::bindings::VertexFragmentStageBinding(0, magma::descriptors::DynamicUniformBuffer(1)),
                magma::bindings::FragmentStageBinding(1, magma::descriptors::UniformBuffer(1)),
                magma::bindings::FragmentStageBinding(2, magma::descriptors::UniformBuffer(1)),
                magma::bindings::FragmentStageBinding(3, magma::descriptors::CombinedImageSampler(1))
            }));
        bumpDescriptor.set = descriptorPool->allocateDescriptorSet(bumpDescriptor.layout);
        bumpDescriptor.set->update(0, transforms);
        bumpDescriptor.set->update(1, viewProjTransforms);
        bumpDescriptor.set->update(2, lightSource);
        bumpDescriptor.set->update(3, normalMap, anisotropicClampToEdge);

        fillDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                magma::bindings::VertexStageBinding(0, magma::descriptors::DynamicUniformBuffer(1))
            }));
        fillDescriptor.set = descriptorPool->allocateDescriptorSet(fillDescriptor.layout);
        fillDescriptor.set->update(0, transforms);
    }

    void setupGraphicsPipelines()
    {
        std::shared_ptr<magma::Specialization> specialization(new magma::Specialization(constants,
            {
                magma::SpecializationEntry(0, &Constants::showNormals)
            }
        ));
        bumpPipeline = createCommonSpecializedPipeline(
            "transform.o", "bump.o",
            std::move(specialization),
            torus->getVertexInput(),
            bumpDescriptor.layout);
        if (!spherePipeline)
        {
            spherePipeline = createCommonPipeline(
                "transform.o", "fill.o",
                sphere->getVertexInput(),
                fillDescriptor.layout);
        }
    }

    void renderScene(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->begin();
        {
            cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer(),
                {
                    magma::ClearColor(0.1f, 0.243f, 0.448f, 1.f),
                    magma::clears::depthOne
                });
            {
                cmdBuffer->bindPipeline(bumpPipeline);
                cmdBuffer->bindDescriptorSet(bumpPipeline, bumpDescriptor.set, transforms->getDynamicOffset(0));
                torus->draw(cmdBuffer);
                cmdBuffer->bindPipeline(spherePipeline);
                cmdBuffer->bindDescriptorSet(spherePipeline, fillDescriptor.set, transforms->getDynamicOffset(1));
                sphere->draw(cmdBuffer);
            }
            cmdBuffer->endRenderPass();
        }
        cmdBuffer->end();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<NormalMapping>(new NormalMapping(entry));
}
