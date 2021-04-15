#include "graphicsApp.h"
#include "colorTable.h"
#include "quadric/include/cube.h"
#include "quadric/include/sphere.h"
#include "quadric/include/teapot.h"

class EdgeDetection : public GraphicsApp
{
    enum {
        Cube = 0, Teapot, Sphere,
        MaxObjects
    };

    struct Constants
    {
        VkBool32 sobelFilter = true;
    };

    std::unique_ptr<quadric::Quadric> objects[MaxObjects];
    std::shared_ptr<magma::GraphicsPipeline> depthPipeline;
    std::shared_ptr<magma::aux::DepthFramebuffer> depthFramebuffer;
    DescriptorSet descriptor;

    rapid::matrix objTransforms[MaxObjects];
    Constants constants;

public:
    explicit EdgeDetection(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Edge detection"), 1280, 720, false)
    {
        setupViewProjection();
        setupTransforms();
        createDepthFramebuffer();
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
        sleep(2); // Cap fps
    }

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Space:
            constants.sobelFilter = !constants.sobelFilter;
            setupGraphicsPipelines();
            renderScene(drawCmdBuffer);
            break;
        }
        VulkanApp::onKeyDown(key, repeat, flags);
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

        updateViewProjTransforms();
    }

    void setupTransforms()
    {
        createTransformBuffer(MaxObjects);
        constexpr float radius = 4.f;
        objTransforms[Cube] = rapid::translation(radius, 1.f, 0.f) * rapid::rotationY(rapid::radians(30.f));
        objTransforms[Teapot] = rapid::translation(radius, 0.f, 0.f) * rapid::rotationY(rapid::radians(150.f));
        objTransforms[Sphere] = rapid::translation(radius, 1.5f, 0.f) * rapid::rotationY(rapid::radians(270.f));
    }

    void updateTransforms()
    {
        const rapid::matrix rotation = rapid::rotationY(rapid::radians(-spinX/4.f));
        std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms(MaxObjects);
        transforms[Cube] = objTransforms[Cube] * rotation,
        transforms[Teapot] = objTransforms[Teapot] * rotation,
        transforms[Sphere] = objTransforms[Sphere] * rotation,
        updateObjectTransforms(transforms);
    }

    void createDepthFramebuffer()
    {
        depthFramebuffer = std::make_shared<magma::aux::DepthFramebuffer>(device, VK_FORMAT_D16_UNORM, msaaFramebuffer->getExtent());
    }

    void createMeshObjects()
    {
        objects[Cube] = std::make_unique<quadric::Cube>(cmdCopyBuf);
        objects[Teapot] = std::make_unique<quadric::Teapot>(16, cmdCopyBuf);
        objects[Sphere] = std::make_unique<quadric::Sphere>(1.5f, 64, 64, false, cmdCopyBuf);
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        descriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            VertexStageBinding(0, DynamicUniformBuffer(1))));
        descriptor.set = descriptorPool->allocateDescriptorSet(descriptor.layout);
        descriptor.set->writeDescriptor(0, transforms);
    }

    void setupGraphicsPipelines()
    {
        depthPipeline = createDepthOnlyPipeline(
            "transform.o",
            objects[0]->getVertexInput(),
            descriptor.layout,
            depthFramebuffer);
        auto specialization(std::make_shared<magma::Specialization>(constants,
            magma::SpecializationEntry(0, &Constants::sobelFilter)));
        bltRect = std::make_unique<magma::aux::BlitRectangle>(renderPass,
            loadShader("edgeDetect.o"), std::move(specialization));
    }

    void renderScene(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->begin();
        {
            depthPass(cmdBuffer);
            edgeDetectPass(cmdBuffer);
        }
        cmdBuffer->end();
    }

    void depthPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(depthFramebuffer->getRenderPass(), depthFramebuffer->getFramebuffer(),
            {
                magma::clears::depthOne
            });
        {
            cmdBuffer->setViewport(magma::Viewport(0, 0, depthFramebuffer->getExtent()));
            cmdBuffer->setScissor(magma::Scissor(0, 0, depthFramebuffer->getExtent()));
            cmdBuffer->bindPipeline(depthPipeline);
            for (uint32_t i = Cube; i < MaxObjects; ++i)
            {
                cmdBuffer->bindDescriptorSet(depthPipeline, descriptor.set, transforms->getDynamicOffset(i));
                objects[i]->draw(cmdBuffer);
            }
        }
        cmdBuffer->endRenderPass();
    }

    void edgeDetectPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer());
        {
            bltRect->blit(cmdBuffer,
                depthFramebuffer->getDepthView(),
                VK_FILTER_NEAREST,
                VkRect2D{0, 0, msaaFramebuffer->getExtent()});
        }
        cmdBuffer->endRenderPass();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<EdgeDetection>(new EdgeDetection(entry));
}
