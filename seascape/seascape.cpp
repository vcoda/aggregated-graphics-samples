#include "graphicsApp.h"
#include "gridMesh.h"
#include "textureLoader.h"
#include "colorTable.h"

class Seascape : public GraphicsApp
{
    struct SeaConstants
    {
        float invWidth;
        float invHeight;
        float height;
        float choppy;
        float speed;
        float frequency;
    };

    struct alignas(16) Seabed
    {
        rapid::float4a viewPlane;
    };

    const float seaHeight = 1.f;
    const float seaChoppy = 4.f;
    const float seaSpeed = 0.8f;
    const float seaFrequency = 0.16f;

    std::unique_ptr<GridMesh> grid;
    std::shared_ptr<magma::UniformBuffer<Seabed>> seabed;
    std::shared_ptr<magma::DynamicUniformBuffer<PhongMaterial>> material;
    std::shared_ptr<magma::aux::ColorFramebuffer> heightMap;
    std::shared_ptr<magma::ImageView> envMap;
    std::shared_ptr<magma::GraphicsPipeline> heightMapPipeline;
    std::shared_ptr<magma::GraphicsPipeline> marinePipeline;
    DescriptorSet hmDescriptor;
    DescriptorSet vtfDescriptor;

    const float seaDepth = -5.;
    const rapid::plane seabedPlane = rapid::plane(0.f, 1.f, 0.f, -seaDepth);
    bool wireframe = false;

public:
    explicit Seascape(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Seascape"), 1280, 720, true)
    {
        createTransformBuffer(1);
        setupViewProjection();
        setupMaterials();
        createHeightmapFramebuffer();
        createUniformBuffer();
        createGridMesh();
        loadEnvMap();
        setupDescriptorSets();
        setupGraphicsPipelines();

        renderScene(drawCmdBuffer);
        blit(msaaFramebuffer->getColorView(), FrontBuffer);
        blit(msaaFramebuffer->getColorView(), BackBuffer);

        timer->run();
    }

    virtual void render(uint32_t bufferIndex) override
    {
        updateSysUniforms();
        updateTransforms();
        submitCommandBuffers(bufferIndex);
    }

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Space:
            wireframe = !wireframe;
            setupGraphicsPipelines();
            renderScene(drawCmdBuffer);
            break;
        }
        GraphicsApp::onKeyDown(key, repeat, flags);
    }

    virtual void updateLightSource() override
    {
        magma::helpers::mapScoped<LightSource>(lightSource,
            [this](auto *light)
            {   // Directional light
                const rapid::matrix normalMatrix = rapid::transpose(rapid::inverse(viewProj->getView()));
                const rapid::vector3 lightViewDir = normalMatrix * lightViewProj->getPosition();
                light->viewPosition = lightViewDir.normalized();
                //light->ambient =
                //light->diffuse =
                light->specular = papaya_whip;
            });
    }

    void updateTransforms()
    {
        const rapid::matrix world = rapid::rotationY(rapid::radians(-spinX/4.f));
        const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            world
        };
        updateObjectTransforms(transforms);

        magma::helpers::mapScoped<Seabed>(seabed,
            [this, &world](auto *seabed)
            {   // Transform seabed plane to view space
                const rapid::matrix normalMatrix = viewProj->calculateNormal(world);
                const rapid::plane viewPlane = seabedPlane * normalMatrix;
                viewPlane.store(&seabed->viewPlane);
            });
    }

    void setupViewProjection()
    {
        viewProj = std::make_unique<LeftHandedViewProjection>();
        viewProj->setPosition(0.f, 25.f, -25.f - 2.f);
        viewProj->setFocus(0.f, 0.f, -2.f);
        viewProj->setFieldOfView(45.f);
        viewProj->setNearZ(1.f);
        viewProj->setFarZ(100.f);
        viewProj->setAspectRatio(width/(float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-1.f, 1.5f, -1.f);

        updateViewProjTransforms();
    }

    void setupMaterials()
    {
        material = std::make_shared<magma::DynamicUniformBuffer<PhongMaterial>>(device, 1);
        magma::helpers::mapScoped<PhongMaterial>(material,
            [this](magma::helpers::AlignedUniformArray<PhongMaterial>& materials)
            {
                constexpr float ambientFactor = 0.4f;
                materials[0].ambient = sRGBColor(0.f);
                materials[0].diffuse = sRGBColor(0.2f, 0.6f, 0.8f);
                materials[0].specular = sRGBColor(0.6f, 0.8f, 0.9f);
                materials[0].shininess = 16.f;
            });
    }

    void createHeightmapFramebuffer()
    {
        constexpr VkFormat format = VK_FORMAT_R16_SFLOAT;
        constexpr VkExtent2D extent{2048, 2048};
        heightMap = std::make_shared<magma::aux::ColorFramebuffer>(device, format, VK_FORMAT_UNDEFINED, extent, false);
    }

    void createUniformBuffer()
    {
        seabed = std::make_shared<magma::UniformBuffer<Seabed>>(device);
    }

    void createGridMesh()
    {
        grid = std::make_unique<GridMesh>(255, 255, 32.f, cmdCopyBuf);
    }

    void loadEnvMap()
    {
        envMap = loadDxtCubeTexture("cubemaps/san_fr.dds", cmdCopyImg);
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // 1. Heightmap pass
        hmDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                FragmentStageBinding(0, UniformBuffer(1))
            }));
        hmDescriptor.set = descriptorPool->allocateDescriptorSet(hmDescriptor.layout);
        hmDescriptor.set->update(0, sysUniforms);
        // 2. Seascape pass
        vtfDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexFragmentStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1)),
                FragmentStageBinding(3, DynamicUniformBuffer(1)), // material
                FragmentStageBinding(4, UniformBuffer(1)), // seabed
                VertexFragmentStageBinding(5, CombinedImageSampler(1)), // heightmap
                FragmentStageBinding(6, CombinedImageSampler(1)) // envmap
            }));
        vtfDescriptor.set = descriptorPool->allocateDescriptorSet(vtfDescriptor.layout);
        vtfDescriptor.set->update(0, transforms);
        vtfDescriptor.set->update(1, viewProjTransforms);
        vtfDescriptor.set->update(2, lightSource);
        vtfDescriptor.set->update(3, material);
        vtfDescriptor.set->update(4, seabed);
        vtfDescriptor.set->update(5, heightMap->getColorView(), nearestClampToEdge);
        vtfDescriptor.set->update(6, envMap, anisotropicClampToEdge);
    }

    std::shared_ptr<magma::Specialization> createSpecialization()
    {
        const float ratio = heightMap->getExtent().width/64.f;
        const SeaConstants constants = {
            1.f/heightMap->getExtent().width,
            1.f/heightMap->getExtent().height,
            seaHeight,
            seaChoppy,
            seaSpeed/ratio,
            seaFrequency * ratio
        };
        return std::make_shared<magma::Specialization>(constants,
            std::initializer_list<magma::SpecializationEntry>
            {
                {0, &SeaConstants::invWidth},
                {1, &SeaConstants::invHeight},
                {2, &SeaConstants::height},
                {3, &SeaConstants::choppy},
                {4, &SeaConstants::speed},
                {5, &SeaConstants::frequency}
            });
    }

    void setupGraphicsPipelines()
    {
        auto specialization = createSpecialization();
        heightMapPipeline = createFullscreenPipeline("quad.o", "heightmap.o", std::move(specialization),
            hmDescriptor.layout, heightMap);

        auto pipelineLayout = std::make_shared<magma::PipelineLayout>(vtfDescriptor.layout);
        marinePipeline = std::make_shared<magma::GraphicsPipeline>(device,
            std::vector<magma::PipelineShaderStage>{
                loadShaderStage("displace.o"),
                loadShaderStage("marine.o")},
            magma::renderstates::pos2h,
            magma::renderstates::triangleStripRestart,
            magma::TesselationState(),
            magma::ViewportState(0.f, 0.f, msaaFramebuffer->getExtent()),
            wireframe ? magma::renderstates::lineCullBackCW : magma::renderstates::fillCullBackCW,
            msaaFramebuffer->getMultisampleState(),
            magma::renderstates::depthLessOrEqual,
            magma::renderstates::dontBlendRgb,
            std::initializer_list<VkDynamicState>{},
            std::move(pipelineLayout),
            msaaFramebuffer->getRenderPass(), 0,
            pipelineCache,
            nullptr, nullptr, 0);
    }

    void renderScene(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->begin();
        {
            heightMapPass(cmdBuffer);
            marinePass(cmdBuffer);
        }
        cmdBuffer->end();
    }

    void heightMapPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(heightMap->getRenderPass(), heightMap->getFramebuffer());
        {
            cmdBuffer->bindPipeline(heightMapPipeline);
            cmdBuffer->bindDescriptorSet(heightMapPipeline, hmDescriptor.set);
            cmdBuffer->draw(4, 0);
        }
        cmdBuffer->endRenderPass();
    }

    void marinePass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer(),
            {
                magma::clears::whiteColor,
                magma::clears::depthOne
            });
        {
            cmdBuffer->bindPipeline(marinePipeline);
            cmdBuffer->bindDescriptorSet(marinePipeline, vtfDescriptor.set,
                {
                    transforms->getDynamicOffset(0),
                    material->getDynamicOffset(0)
                });
            grid->draw(cmdBuffer);
        }
        cmdBuffer->endRenderPass();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<Seascape>(new Seascape(entry));
}
