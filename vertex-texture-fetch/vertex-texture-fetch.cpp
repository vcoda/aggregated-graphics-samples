#include "graphicsApp.h"
#include "gridMesh.h"

class VertexTextureFetch : public GraphicsApp
{
    struct Constants
    {
        VkBool32 showNormals = false;
    };

    struct SeaConstants
    {
        float invWidth;
        float invHeight;
        float height;
        float choppy;
        float speed;
        float frequency;
    };

    const float seaHeight = 1.f;
    const float seaChoppy = 4.f;
    const float seaSpeed = 0.8f;
    const float seaFrequency = 0.16f;

    std::unique_ptr<GridMesh> grid;
    std::shared_ptr<magma::aux::ColorFramebuffer> heightMap;
    std::shared_ptr<magma::GraphicsPipeline> heightMapPipeline;
    std::shared_ptr<magma::GraphicsPipeline> vertexTextureFetchPipeline;
    DescriptorSet hmDescriptor;
    DescriptorSet vtfDescriptor;

    Constants constants;
    bool wireframe = false;

public:
    explicit VertexTextureFetch(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Vertex texture fetch"), 1280, 720, true)
    {
        createTransformBuffer(1);
        setupViewProjection();
        createHeightmapFramebuffer();
        createGridMesh();
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
        case AppKey::Enter:
            constants.showNormals = !constants.showNormals;
            setupGraphicsPipelines();
            renderScene(drawCmdBuffer);
            break;
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
            [this](auto *lightSource)
            {   // Directional light
                const rapid::matrix normalMatrix = rapid::transpose(rapid::inverse(viewProj->getView()));
                const rapid::vector3 lightViewDir = normalMatrix * lightViewProj->getPosition();
                lightSource->viewPosition = lightViewDir.normalized();
            });
    }

    void updateTransforms()
    {
        const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            rapid::rotationY(rapid::radians(-spinX/4.f))
        };
        updateObjectTransforms(transforms);
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

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-1.f, 1.5f, -1.f);

        updateViewProjTransforms();
    }

    void createHeightmapFramebuffer()
    {
        constexpr VkFormat format = VK_FORMAT_R16_SFLOAT;
        constexpr VkExtent2D extent{2048, 2048};
		constexpr bool clearOp = false;
        heightMap = std::make_shared<magma::aux::ColorFramebuffer>(device, format, extent, clearOp);
    }

    void createGridMesh()
    {
        grid = std::make_unique<GridMesh>(64, 64, 32.f, cmdCopyBuf);
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
        hmDescriptor.set->writeDescriptor(0, sysUniforms);
        // 2. Vertex texture fetch
        vtfDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexFragmentStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1)),
                VertexFragmentStageBinding(3, CombinedImageSampler(1))
            }));
        vtfDescriptor.set = descriptorPool->allocateDescriptorSet(vtfDescriptor.layout);
        vtfDescriptor.set->writeDescriptor(0, transforms);
        vtfDescriptor.set->writeDescriptor(1, viewProjTransforms);
        vtfDescriptor.set->writeDescriptor(2, lightSource);
        vtfDescriptor.set->writeDescriptor(3, heightMap->getColorView(), nearestClampToEdge);
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

        specialization = std::make_shared<magma::Specialization>(constants,
            magma::SpecializationEntry(0, &Constants::showNormals));
        auto pipelineLayout = std::make_shared<magma::PipelineLayout>(vtfDescriptor.layout);
        vertexTextureFetchPipeline = std::make_shared<magma::GraphicsPipeline>(device,
            std::vector<magma::PipelineShaderStage>{
                loadShaderStage("displace.o"),
                loadShaderStage("bump.o", std::move(specialization))
            },
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
            vertexTextureFetchPass(cmdBuffer);
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

    void vertexTextureFetchPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer(),
            {
                magma::ClearColor(0.1f, 0.243f, 0.448f, 1.f),
                magma::clears::depthOne
            });
        {
            cmdBuffer->bindPipeline(vertexTextureFetchPipeline);
            cmdBuffer->bindDescriptorSet(vertexTextureFetchPipeline, vtfDescriptor.set, transforms->getDynamicOffset(0));
            grid->draw(cmdBuffer);
        }
        cmdBuffer->endRenderPass();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<VertexTextureFetch>(new VertexTextureFetch(entry));
}
