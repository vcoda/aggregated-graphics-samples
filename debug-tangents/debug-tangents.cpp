#include "graphicsApp.h"
#include "quadric/include/torus.h"

class DebugTangents : public GraphicsApp
{
    struct alignas(16) Wireframe
    {
        rapid::float3 color;
        float lineWidth;
    };

    std::unique_ptr<quadric::Torus> torus;
    std::shared_ptr<magma::UniformBuffer<Wireframe>> wireframe;
    std::shared_ptr<magma::GraphicsPipeline> depthPipeline;
    std::shared_ptr<magma::GraphicsPipeline> wireframePipeline;
    std::shared_ptr<magma::GraphicsPipeline> tangentsPipeline;
    DescriptorSet wireframeDescriptor;
    DescriptorSet transformDescriptor;

    bool showTangents = true;

public:
    explicit DebugTangents(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Debug tangents"), 1280, 720, false)
    {
        setupViewProjection();
        createMesh();
        createTransformBuffer(1);
        createUniformBuffer();
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
        constexpr float step = 0.1f;
        switch (key)
        {
        case AppKey::Space:
            showTangents = !showTangents;
            renderScene(drawCmdBuffer);
            break;
        }
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
        viewProj->setAspectRatio(width/(float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        updateViewProjTransforms();
    }

    void updateTransforms()
    {
        const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            arcball->transform()
        };
        updateObjectTransforms(transforms);
    }

    void createMesh()
    {
        torus = std::make_unique<quadric::Torus>(0.4f, 1.0f, 16, 32, true, cmdCopyBuf);
    }

    void createUniformBuffer()
    {
        wireframe = std::make_shared<magma::UniformBuffer<Wireframe>>(device);
        magma::helpers::mapScoped(wireframe,
            [](auto *wireframe)
            {
                wireframe->color = rapid::float3(0.4f, 0.4f, 0.4f);
                wireframe->lineWidth = 1.2f;
            });
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // 1. Wireframe pass
        wireframeDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
            }));
        wireframeDescriptor.set = descriptorPool->allocateDescriptorSet(wireframeDescriptor.layout);
        wireframeDescriptor.set->update(0, transforms);
        wireframeDescriptor.set->update(1, wireframe);
        // 2. Tangents pass
        transformDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            VertexGeometryStageBinding(0, DynamicUniformBuffer(1))));
        transformDescriptor.set = descriptorPool->allocateDescriptorSet(transformDescriptor.layout);
        transformDescriptor.set->update(0, transforms);
    }

    void setupGraphicsPipelines()
    {
        depthPipeline = createDepthOnlyPipeline("transform.o",
            torus->getVertexInput(),
            transformDescriptor.layout);
        auto pipelineLayout = std::make_shared<magma::PipelineLayout>(wireframeDescriptor.layout);
        wireframePipeline = std::make_shared<magma::GraphicsPipeline>(device,
            std::vector<magma::PipelineShaderStage>{
                loadShaderStage("transform.o"),
                loadShaderStage("barycentric.o"),
                loadShaderStage("wireframe.o")
            },
            torus->getVertexInput(),
            magma::renderstates::triangleList,
            magma::TesselationState(),
            magma::ViewportState(0.f, 0.f, msaaFramebuffer->getExtent()),
            magma::renderstates::fillCullBackCW,
            msaaFramebuffer->getMultisampleState(),
            magma::renderstates::depthEqual,
            magma::renderstates::blendNormalRgb,
            std::initializer_list<VkDynamicState>{},
            std::move(pipelineLayout),
            msaaFramebuffer->getRenderPass(),
            0,
            pipelineCache,
            nullptr, nullptr, 0);
        pipelineLayout = std::make_shared<magma::PipelineLayout>(transformDescriptor.layout);
        tangentsPipeline = std::make_shared<magma::GraphicsPipeline>(device,
            std::vector<magma::PipelineShaderStage>{
                loadShaderStage("transform.o"),
                loadShaderStage("tangents.o"),
                loadShaderStage("fill.o")
            },
            torus->getVertexInput(),
            magma::renderstates::triangleList,
            magma::TesselationState(),
            magma::ViewportState(0.f, 0.f, msaaFramebuffer->getExtent()),
            magma::renderstates::fillCullBackCW,
            msaaFramebuffer->getMultisampleState(),
            magma::renderstates::depthLessOrEqual,
            magma::renderstates::dontBlendRgb,
            std::initializer_list<VkDynamicState>{VK_DYNAMIC_STATE_LINE_WIDTH},
            std::move(pipelineLayout),
            msaaFramebuffer->getRenderPass(),
            0,
            pipelineCache,
            nullptr, nullptr, 0);
    }

    void renderScene(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->begin();
        {
            cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer(),
                {
                    magma::clears::blackColor,
                    magma::clears::depthOne
                });
            {   // 1. Depth pre-pass
                cmdBuffer->bindPipeline(depthPipeline);
                cmdBuffer->bindDescriptorSet(depthPipeline, transformDescriptor.set, transforms->getDynamicOffset(0));
                torus->draw(cmdBuffer);
                // 2. Wireframe mesh
                cmdBuffer->bindPipeline(wireframePipeline);
                cmdBuffer->bindDescriptorSet(wireframePipeline, wireframeDescriptor.set, transforms->getDynamicOffset(0));
                torus->draw(cmdBuffer);
                // 3. Debug tangents
                if (showTangents)
                {
                    cmdBuffer->bindPipeline(tangentsPipeline);
                    cmdBuffer->bindDescriptorSet(tangentsPipeline, transformDescriptor.set, transforms->getDynamicOffset(0));
                    cmdBuffer->setLineWidth(3.f);
                    torus->draw(cmdBuffer);
                }
            }
            cmdBuffer->endRenderPass();
        }
        cmdBuffer->end();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<DebugTangents>(new DebugTangents(entry));
}
