#include "graphicsApp.h"
#include "colorTable.h"
#include "quadric/include/teapot.h"

class CookTorrance : public GraphicsApp
{
    struct Constants
    {
        VkBool32 gammaCorrection = true;
    };

    struct alignas(16) RefractiveIndices
    {
        float rF0;
        float gF0;
        float bF0;
    };

    struct alignas(16) Roughness
    {
        float roughness;
    };

    struct alignas(16) DirectionalLightSource
    {
        rapid::float3 direction;
        LinearColor diffuse;
    };

    enum Metal : uint32_t
    {
        Silver = 0, Aluminium, Gold,
        Copper, Chromium, Nickel,
        Titanium, Cobalt, Platinum,
        Max
    };

    std::unique_ptr<quadric::Teapot> teapot;
    std::shared_ptr<magma::StorageBuffer> lightSources;
    std::shared_ptr<magma::UniformBuffer<Roughness>> roughnessUniform;
    std::shared_ptr<magma::DynamicUniformBuffer<RefractiveIndices>> refractiveIndices;
    std::shared_ptr<magma::DynamicUniformBuffer<LinearColor>> albedoColors;
    std::shared_ptr<magma::GraphicsPipeline> cookTorrancePipeline;
    DescriptorSet descriptor;

    Constants constants;
    float roughness = 0.27f;

public:
    explicit CookTorrance(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Cook-Torrance BRDF"), 1280, 720, true)
    {
        createTransformBuffer(Metal::Max);
        setupViewProjection();
        setupRefractiveIndices();
        setupAlbedo();
        setupDirectionalLights();
        updateRoughness(roughness);
        createMesh();
        setupDescriptorSets();
        setupGraphicsPipeline();

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

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        constexpr float step = 0.002f;
        switch (key)
        {
        case AppKey::PgUp:
            if (roughness < 0.4001f - step)
                updateRoughness(roughness += step);
            break;
        case AppKey::PgDn:
            if (roughness > 0.0999f + step)
                updateRoughness(roughness -= step);
            break;
        case AppKey::Space:
            constants.gammaCorrection = !constants.gammaCorrection;
            setupGraphicsPipeline();
            renderScene(drawCmdBuffer);
            break;
        }
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    void updateTransforms()
    {
        constexpr float dx = 8.f, dy = 5.f;
        static const rapid::matrix offset = rapid::translation(0.f, -1.5f, 0.f);
        static const rapid::matrix grid3x3[Metal::Max] = {
            rapid::translation(-dx, dy),
            rapid::translation(0.f, dy),
            rapid::translation( dx, dy),
            rapid::translation(-dx, 0.f),
            rapid::translation(0.f, 0.f),
            rapid::translation( dx, 0.f),
            rapid::translation(-dx, -dy),
            rapid::translation(0.f, -dy),
            rapid::translation( dx, -dy)
        };
        const rapid::matrix rotation = arcball->transform();
        std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms(Metal::Max);
        for (uint32_t i = 0; i < Metal::Max; ++i)
            transforms[i] = offset * rotation * grid3x3[i];
        updateObjectTransforms(transforms);
    }

    void updateRoughness(float roughness)
    {
        if (!roughnessUniform)
            roughnessUniform = std::make_shared<magma::UniformBuffer<Roughness>>(device);
        magma::helpers::mapScoped<Roughness>(roughnessUniform,
            [roughness](auto *param)
            {
                param->roughness = roughness;
            });
        std::cout << "roughness: " << roughness << std::endl;
    }

    void setupViewProjection()
    {
        viewProj = std::make_unique<LeftHandedViewProjection>();
        viewProj->setPosition(0.f, 0.f, -100.0f);
        viewProj->setFocus(0.f, 0.f, 0.f);
        viewProj->setFieldOfView(9.f);
        viewProj->setNearZ(90.f);
        viewProj->setFarZ(110.f);
        viewProj->setAspectRatio(width/(float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        updateViewProjTransforms();
    }

    void setupRefractiveIndices()
    {
        refractiveIndices = std::make_shared<magma::DynamicUniformBuffer<RefractiveIndices>>(device, Metal::Max);
        magma::helpers::mapScoped<RefractiveIndices>(refractiveIndices,
            [this](magma::helpers::AlignedUniformArray<RefractiveIndices>& indices)
            {   // https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/
                indices[Silver] = {0.971519f, 0.959915f, 0.915324f};
                indices[Aluminium] = {0.913183f, 0.921494f, 0.924524f};
                indices[Gold] = {1.0f, 0.765557f, 0.336057f};
                indices[Copper] = {0.955008f, 0.637427f, 0.538163f};
                indices[Chromium] = {0.549585f, 0.556114f, 0.554256f};
                indices[Nickel] = {0.659777f, 0.608679f, 0.525649f};
                indices[Titanium] = {0.541931f, 0.496791f, 0.449419f};
                indices[Cobalt] = {0.662124f, 0.654864f, 0.633732f};
                indices[Platinum] = {0.672411f, 0.637331f, 0.585456f};
            });
    }

    void setupAlbedo()
    {
        albedoColors = std::make_shared<magma::DynamicUniformBuffer<LinearColor>>(device, Metal::Max);
        magma::helpers::mapScoped<LinearColor>(albedoColors,
            [this](magma::helpers::AlignedUniformArray<LinearColor>& albedo)
            {   // https://www.chaosgroup.com/blog/understanding-metalness
                albedo[Silver] = LinearColor(252, 250, 249);
                albedo[Aluminium] = LinearColor(230, 233, 235);
                albedo[Gold] = LinearColor(243, 201, 104);
                albedo[Copper] = LinearColor(238, 158, 137);
                albedo[Chromium] = LinearColor(141, 141, 141);
                albedo[Nickel] = LinearColor(226, 219, 192);
                albedo[Titanium] = LinearColor(246, 239, 208);
                albedo[Cobalt] = LinearColor(174, 167, 157);
                albedo[Platinum] = LinearColor(243, 238, 216);
            });
    }

    void setupDirectionalLights()
    {
        DirectionalLightSource lights[2];
        rapid::vector3(-3.f, -2.7f, -1.8f).normalized().store(&lights[0].direction);
        lights[0].diffuse = alice_blue;
        rapid::vector3(0.3f, 0.7f, -0.3f).normalized().store(&lights[1].direction);
        lights[1].diffuse = lavender_blush;
        lightSources = std::make_shared<magma::StorageBuffer>(cmdCopyBuf, lights, sizeof(lights));
    }

    void createMesh()
    {
        teapot = std::make_unique<quadric::Teapot>(16, cmdCopyBuf);
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        descriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, StorageBuffer(1)),
                FragmentStageBinding(3, UniformBuffer(1)),
                FragmentStageBinding(4, DynamicUniformBuffer(1)),
                FragmentStageBinding(5, DynamicUniformBuffer(1))
            }));
        descriptor.set = descriptorPool->allocateDescriptorSet(descriptor.layout);
        descriptor.set->writeDescriptor(0, transforms);
        descriptor.set->writeDescriptor(1, viewProjTransforms);
        descriptor.set->writeDescriptor(2, lightSources);
        descriptor.set->writeDescriptor(3, roughnessUniform);
        descriptor.set->writeDescriptor(4, refractiveIndices);
        descriptor.set->writeDescriptor(5, albedoColors);
    }

    void setupGraphicsPipeline()
    {
        auto specialization(std::make_shared<magma::Specialization>(constants,
            magma::SpecializationEntry(0, &Constants::gammaCorrection)));
        cookTorrancePipeline = createCommonSpecializedPipeline(
            "transform.o", "cookTorrance.o",
            std::move(specialization),
            teapot->getVertexInput(),
            descriptor.layout);
    }

    void renderScene(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->begin();
        {
            cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer(),
                {
                    magma::ClearColor(0.349f, 0.289f, 0.255f, 1.f),
                    magma::clears::depthOne
                });
            {
                cmdBuffer->setViewport(magma::Viewport(0, 0, msaaFramebuffer->getExtent()));
                cmdBuffer->setScissor(magma::Scissor(0, 0, msaaFramebuffer->getExtent()));
                cmdBuffer->bindPipeline(cookTorrancePipeline);
                for (uint32_t i = 0; i < Metal::Max; ++i)
                {
                    cmdBuffer->bindDescriptorSet(cookTorrancePipeline, descriptor.set, {
                        transforms->getDynamicOffset(i),
                        refractiveIndices->getDynamicOffset(i),
                        albedoColors->getDynamicOffset(i)
                    });
                    teapot->draw(cmdBuffer);
                }
            }
            cmdBuffer->endRenderPass();
        }
        cmdBuffer->end();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<CookTorrance>(new CookTorrance(entry));
}
