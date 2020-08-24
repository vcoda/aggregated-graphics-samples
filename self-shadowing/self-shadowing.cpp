#include "graphicsApp.h"
#include "colorTable.h"
#include "textureLoader.h"
#include "quadric/include/cube.h"
#include "quadric/include/sphere.h"

class SelfShadowing : public GraphicsApp
{
    struct Constants
    {
        VkBool32 selfShadowing = false;
    };

    std::unique_ptr<quadric::Cube> box;
    std::unique_ptr<quadric::Sphere> sphere;
    std::shared_ptr<magma::ImageView> diffuseMap;
    std::shared_ptr<magma::ImageView> normalMap;
    std::shared_ptr<magma::ImageView> specularMap;
    std::shared_ptr<magma::GraphicsPipeline> phongPipeline;
    std::shared_ptr<magma::GraphicsPipeline> spherePipeline;
    DescriptorSet phongDescriptor;
    DescriptorSet fillDescriptor;

    Constants constants;

public:
    explicit SelfShadowing(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Self-shadowing"), 1280, 720, true)
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
        switch (key)
        {
        case AppKey::Space:
            constants.selfShadowing = !constants.selfShadowing;
            setupGraphicsPipelines();
            renderScene(drawCmdBuffer);
            break;
        }
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    virtual void onMouseWheel(float distance) override
    {
        float step = distance * 0.2f;
        float z = viewProj->getPosition().z;
        if ((z + step < -3.3f) && (z + step > -15.f))
            viewProj->translate(0.f, 0.f, step);
        updateViewProjTransforms();
    }

    virtual void updateLightSource()
    {
        magma::helpers::mapScoped<LightSource>(lightSource,
            [this](auto *light)
            {
                constexpr float ambientFactor = 0.2f;
                light->viewPosition = viewProj->getView() * lightViewProj->getPosition();
                light->ambient = ivory * ambientFactor;
                light->diffuse = ivory;
                light->specular = old_lace;
            });
    }

    void setupViewProjection()
    {
        viewProj = std::make_unique<LeftHandedViewProjection>();
        viewProj->setPosition(0.f, 0.f, -5.f);
        viewProj->setFocus(0.f, 0.f, 0.f);
        viewProj->setFieldOfView(45.f);
        viewProj->setNearZ(1.f);
        viewProj->setFarZ(100.f);
        viewProj->setAspectRatio(width / (float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-1.f, 1.f, -2.f);

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
        box = std::make_unique<quadric::Cube>(cmdCopyBuf);
        sphere = std::make_unique<quadric::Sphere>(0.02f, 8, 8, true, cmdCopyBuf);
    }

    void loadTexture()
    {   // https://freepbr.com/materials/storage-container2/
        diffuseMap = loadDxtTexture(cmdCopyImg, "storage-container2/storage-container2-albedo.dds", true);
        normalMap = loadDxtTexture(cmdCopyImg, "storage-container2/storage-container2-normal-dx.dds");
        specularMap = loadDxtTexture(cmdCopyImg, "storage-container2/storage-container2-roughness.dds");
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // Bump shader
        phongDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexFragmentStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1)),
                FragmentStageBinding(3, CombinedImageSampler(1)),
                FragmentStageBinding(4, CombinedImageSampler(1)),
                FragmentStageBinding(5, CombinedImageSampler(1))
            }));
        phongDescriptor.set = descriptorPool->allocateDescriptorSet(phongDescriptor.layout);
        phongDescriptor.set->update(0, transforms);
        phongDescriptor.set->update(1, viewProjTransforms);
        phongDescriptor.set->update(2, lightSource);
        phongDescriptor.set->update(3, diffuseMap, anisotropicClampToEdge);
        phongDescriptor.set->update(4, normalMap, anisotropicClampToEdge);
        phongDescriptor.set->update(5, specularMap, anisotropicClampToEdge);
        // Fill shader
        fillDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexStageBinding(0, DynamicUniformBuffer(1))
            }));
        fillDescriptor.set = descriptorPool->allocateDescriptorSet(fillDescriptor.layout);
        fillDescriptor.set->update(0, transforms);
    }

    void setupGraphicsPipelines()
    {
        auto specialization(std::make_shared<magma::Specialization>(constants,
            magma::SpecializationEntry(0, &Constants::selfShadowing)));
        phongPipeline = createCommonSpecializedPipeline(
            "transform.o", "phong.o",
            std::move(specialization),
            box->getVertexInput(),
            phongDescriptor.layout);
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
                cmdBuffer->bindPipeline(phongPipeline);
                cmdBuffer->bindDescriptorSet(phongPipeline, phongDescriptor.set, transforms->getDynamicOffset(0));
                box->draw(cmdBuffer);
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
    return std::unique_ptr<SelfShadowing>(new SelfShadowing(entry));
}
