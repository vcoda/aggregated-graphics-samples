#include "graphicsApp.h"
#include "colorTable.h"
#include "textureLoader.h"
#include "quadric/include/teapot.h"

class BasicBrdf : public GraphicsApp
{
    struct Constants
    {
        VkBool32 gammaCorrection = true;
    };

    enum Brdf : uint32_t
    {
        Lambert = 0, PhongMetallic, PhongPlastic,
        OrenNayar, BlinnPhongMetallic, BlinnPhongPlastic,
        Minnaert, Aniso, Ward,
        Max
    };

    std::unique_ptr<quadric::Teapot> teapot;
    std::shared_ptr<magma::DynamicUniformBuffer<PhongMaterial>> materials;
    std::shared_ptr<magma::ImageView> aniso;
    std::shared_ptr<magma::GraphicsPipeline> lambertPipeline;
    std::shared_ptr<magma::GraphicsPipeline> phongPipeline;
    std::shared_ptr<magma::GraphicsPipeline> blinnPhongPipeline;
    std::shared_ptr<magma::GraphicsPipeline> orenNayarPipeline;
    std::shared_ptr<magma::GraphicsPipeline> minnaertPipeline;
    std::shared_ptr<magma::GraphicsPipeline> anisoPipeline;
    std::shared_ptr<magma::GraphicsPipeline> wardPipeline;
    DescriptorSet descriptor;

    Constants constants;

public:
    explicit BasicBrdf(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Basic BRDFs"), 1280, 720, true)
    {
        createTransformBuffer(9);
        setupViewProjection();
        setupMaterials();
        createMesh();
        loadAnisoTexture();
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

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Space:
            constants.gammaCorrection = !constants.gammaCorrection;
            setupGraphicsPipelines();
            renderScene(drawCmdBuffer);
            break;
        }
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    virtual void updateLightSource() override
    {
        magma::helpers::mapScoped(lightSource,
            [this](auto *light)
            {   // Directional light
                constexpr float ambientFactor = 0.4f;
                const rapid::matrix normalMatrix = rapid::transpose(rapid::inverse(viewProj->getView()));
                const rapid::vector3 lightViewDir = normalMatrix * lightViewProj->getPosition();
                light->viewPosition = lightViewDir.normalized();
                light->ambient = white * ambientFactor;
                light->diffuse = white;
                light->specular = white;
            });
    }

    void updateTransforms()
    {
        constexpr float dx = 8.f, dy = 5.f;
        static const rapid::matrix offset = rapid::translation(0.f, -1.5f, 0.f);
        static const rapid::matrix grid3x3[Brdf::Max] = {
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
        std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms(Brdf::Max);
        for (uint32_t i = 0; i < Brdf::Max; ++i)
            transforms[i] = offset * rotation * grid3x3[i];
        updateObjectTransforms(transforms);
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

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(0.3f, 0.7f, -0.3f);
        lightViewProj->setFocus(0.f, 0.f, 0.f);
        lightViewProj->setFieldOfView(70.f);
        lightViewProj->setNearZ(5.f);
        lightViewProj->setFarZ(30.f);
        lightViewProj->setAspectRatio(1.f);
        lightViewProj->updateView();
        lightViewProj->updateProjection();

        updateViewProjTransforms();
    }

    void setupMaterials()
    {
        materials = std::make_shared<magma::DynamicUniformBuffer<PhongMaterial>>(device, Brdf::Max);
        magma::helpers::mapScoped<PhongMaterial>(materials,
            [this](magma::helpers::AlignedUniformArray<PhongMaterial>& materials)
            {
                constexpr float ambientFactor = 0.4f;

                materials[Lambert].diffuse = white;
                materials[OrenNayar].diffuse = white;
                materials[Minnaert].diffuse = white;

                materials[PhongMetallic].ambient = medium_blue * ambientFactor;
                materials[PhongMetallic].diffuse = medium_blue;
                materials[PhongMetallic].specular = deep_sky_blue;
                materials[PhongMetallic].shininess = 2.f; // High roughness for metal look like

                materials[PhongPlastic].ambient = royal_blue * ambientFactor;
                materials[PhongPlastic].diffuse = royal_blue;
                materials[PhongPlastic].specular = light_blue;
                materials[PhongPlastic].shininess = 64.f; // Low roughness for plastic look like

                materials[BlinnPhongMetallic].ambient = coral * ambientFactor;
                materials[BlinnPhongMetallic].diffuse = coral;
                materials[BlinnPhongMetallic].specular = light_coral;
                materials[BlinnPhongMetallic].shininess = materials[PhongMetallic].shininess * 4.f;

                materials[BlinnPhongPlastic].ambient = medium_sea_green * ambientFactor;
                materials[BlinnPhongPlastic].diffuse = medium_sea_green;
                materials[BlinnPhongPlastic].specular = light_green;
                materials[BlinnPhongPlastic].shininess = materials[PhongPlastic].shininess * 4.f;
            });
    }

    void createMesh()
    {
        teapot = std::make_unique<quadric::Teapot>(16, cmdCopyBuf);
    }

    void loadAnisoTexture()
    {
        aniso = loadDxtTexture(cmdCopyImg, "aniso2.dds");
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        descriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1)),
                FragmentStageBinding(3, DynamicUniformBuffer(1)),
                FragmentStageBinding(4, CombinedImageSampler(1))
            }));
        descriptor.set = descriptorPool->allocateDescriptorSet(descriptor.layout);
        descriptor.set->update(0, transforms);
        descriptor.set->update(1, viewProjTransforms);
        descriptor.set->update(2, lightSource);
        descriptor.set->update(3, materials);
        descriptor.set->update(4, aniso, anisotropicClampToEdge);
    }

    void setupGraphicsPipelines()
    {
        auto specialization(std::make_shared<magma::Specialization>(constants,
            magma::SpecializationEntry(0, &Constants::gammaCorrection)));
        lambertPipeline = createCommonSpecializedPipeline(
            "transform.o", "lambert.o", specialization,
            teapot->getVertexInput(), descriptor.layout);
        phongPipeline = createCommonSpecializedPipeline(
            "transform.o", "phong.o", specialization,
            teapot->getVertexInput(), descriptor.layout);
        blinnPhongPipeline = createCommonSpecializedPipeline(
            "transform.o", "blinnPhong.o", specialization,
            teapot->getVertexInput(), descriptor.layout);
        orenNayarPipeline = createCommonSpecializedPipeline(
            "transform.o", "orenNayar.o", specialization,
            teapot->getVertexInput(), descriptor.layout);
        minnaertPipeline = createCommonSpecializedPipeline(
            "transform.o", "minnaert.o", specialization,
            teapot->getVertexInput(), descriptor.layout);
        anisoPipeline = createCommonSpecializedPipeline(
            "transform.o", "aniso.o", specialization,
            teapot->getVertexInput(), descriptor.layout);
        wardPipeline = createCommonSpecializedPipeline(
            "transform.o", "ward.o", specialization,
            teapot->getVertexInput(), descriptor.layout);
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
                std::shared_ptr<magma::GraphicsPipeline> pipelines[] = {
                    lambertPipeline,
                    phongPipeline,
                    phongPipeline,
                    orenNayarPipeline,
                    blinnPhongPipeline,
                    blinnPhongPipeline,
                    minnaertPipeline,
                    anisoPipeline,
                    wardPipeline
                };
                for (uint32_t i = 0; i < Brdf::Max; ++i)
                {
                    cmdBuffer->bindPipeline(pipelines[i]);
                    cmdBuffer->bindDescriptorSet(pipelines[i], descriptor.set, {
                        transforms->getDynamicOffset(i),
                        materials->getDynamicOffset(i)
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
    return std::unique_ptr<BasicBrdf>(new BasicBrdf(entry));
}
