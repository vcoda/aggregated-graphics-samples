#include "graphicsApp.h"
#include "colorTable.h"
#include "textureLoader.h"
#include "utilities.h"

#include "quadric/include/cube.h"
#include "quadric/include/sphere.h"
#include "quadric/include/torus.h"
#include "quadric/include/teapot.h"
#include "quadric/include/plane.h"

class DeferredShading : public GraphicsApp
{
    enum {
        Normal = 0, Albedo, Specular, Shininess, Depth
    };

    enum {
        Cube = 0, Teapot, Sphere, Torus, Ground,
        MaxObjects
    };

    std::unique_ptr<quadric::Quadric> objects[MaxObjects];
    std::shared_ptr<magma::aux::MultiAttachmentFramebuffer> gbuffer;
    std::shared_ptr<magma::DynamicUniformBuffer<PhongMaterial>> materials;
    std::shared_ptr<magma::ImageView> normalMap;
    std::shared_ptr<magma::GraphicsPipeline> depthPipeline;
    std::shared_ptr<magma::GraphicsPipeline> gbufferPipeline;
    std::shared_ptr<magma::GraphicsPipeline> gbufferTexPipeline;
    std::shared_ptr<magma::GraphicsPipeline> deferredPipeline;
    DescriptorSet depthDescriptor;
    DescriptorSet gbDescriptor;
    DescriptorSet gbTexDescriptor;
    DescriptorSet dsDescriptor;

    rapid::matrix objTransforms[MaxObjects];

public:
    DeferredShading(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Deferred shading"), 1280, 720, true)
    {
        setupViewProjection();
        setupTransforms();
        setupMaterials();
        createGbuffer();
        createMeshObjects();
        loadTextures();
        setupDescriptorSets();
        setupGraphicsPipelines();

        renderScene(FrontBuffer);
        renderScene(BackBuffer);

        timer->run();
    }

    virtual void render(uint32_t bufferIndex) override
    {
        updateTransforms();
        queue->submit(commandBuffers[bufferIndex],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            presentFinished,
            renderFinished,
            waitFences[bufferIndex]);
    }

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Left:
            lightViewProj->translate(-0.5f, 0.f, 0.f);
            break;
        case AppKey::Right:
            lightViewProj->translate(0.5f, 0.f, 0.f);
            break;
        case AppKey::Down:
            lightViewProj->translate(0.f, 0.f, -0.5f);
            break;
        case AppKey::Up:
            lightViewProj->translate(0.f, 0.f, 0.5f);
            break;
        case AppKey::PgDn:
            lightViewProj->translate(0.f, -0.5f, 0.f);
            break;
        case AppKey::PgUp:
            lightViewProj->translate(0.f, 0.5f, 0.f);
            break;
        }
        lightViewProj->updateView();
        lightViewProj->updateProjection();
        updateLightSource();
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    void updateTransforms()
    {
        constexpr float speed = 0.05f;
        static float angle = 0.f;
        angle += timer->millisecondsElapsed() * speed;
        const float radians = rapid::radians(angle);
        const rapid::matrix pitch = rapid::rotationX(radians);
        const rapid::matrix yaw = rapid::rotationY(radians);
        const rapid::matrix roll = rapid::rotationZ(radians);
        const rapid::matrix torusRotation = pitch * yaw * roll;
        const rapid::matrix rotation = rapid::rotationY(rapid::radians(-spinX / 4.f));
        const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            objTransforms[Cube] * rotation,
            objTransforms[Teapot] * rotation,
            objTransforms[Sphere] * rotation,
            torusRotation * objTransforms[Torus] * rotation,
            objTransforms[Ground] * rotation };
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
        viewProj->updateView();
        viewProj->updateProjection();

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-5.f, 8.f, -5.f);
        lightViewProj->setFocus(0.f, 0.f, 0.f);
        lightViewProj->setFieldOfView(60.f);
        lightViewProj->setNearZ(1.f);
        lightViewProj->setFarZ(20.f);
        lightViewProj->setAspectRatio(1.f);
        lightViewProj->updateView();
        lightViewProj->updateProjection();

        updateViewProjTransforms();
    }

    void setupTransforms()
    {
        createTransformBuffer(MaxObjects);
        objTransforms[Cube] = rapid::translation(0.f, 1.f,-10.f);
        objTransforms[Teapot] = rapid::translation(-10.f, 0.f, 0.f);
        objTransforms[Sphere] = rapid::translation(0.f, 1.7f, 10.f);
        objTransforms[Torus] = rapid::translation(10.f, 2.7f, 0.f);
        objTransforms[Ground] = rapid::identity();
    }

    void setupMaterials()
    {   // https://www.rapidtables.com/web/color/RGB_Color.html
        materials = std::make_shared<magma::DynamicUniformBuffer<PhongMaterial>>(device, 5);
        magma::helpers::mapScoped<PhongMaterial>(materials,
            [this](magma::helpers::AlignedUniformArray<PhongMaterial>& materials)
            {
                constexpr float ambientFactor = 0.4f;

                materials[Cube].diffuse = sRGBColor(pale_golden_rod, ambientFactor);
                materials[Cube].specular = pale_golden_rod;
                materials[Cube].shininess = 4.f;

                materials[Teapot].diffuse = sRGBColor(hot_pink, ambientFactor);
                materials[Teapot].specular = pink * 1.2f;
                materials[Teapot].shininess = 128.f;

                materials[Sphere].diffuse = sRGBColor(corn_flower_blue, ambientFactor);
                materials[Sphere].specular = corn_flower_blue * 1.2f;
                materials[Sphere].shininess = 16.f;

                materials[Torus].diffuse = sRGBColor(spring_green, ambientFactor);
                materials[Torus].specular = pale_green;
                materials[Torus].shininess = 64.f;

                materials[Ground].diffuse = sRGBColor(slate_gray, ambientFactor);
                materials[Ground].specular = slate_gray * 1.4f;
                materials[Ground].shininess = 8.f;
            });
    }

    void createGbuffer()
    {
        constexpr bool depthSampled = true;
        constexpr bool separateDepthPass = true;
        const VkFormat depthFormat = utilities::getSupportedDepthFormat(physicalDevice, false, true);
        gbuffer = std::make_shared<magma::aux::MultiAttachmentFramebuffer>(device,
            std::initializer_list<VkFormat>{
                VK_FORMAT_R16G16_SFLOAT, // Normal
                VK_FORMAT_R8G8B8A8_UNORM, // Albedo
                VK_FORMAT_R8G8B8A8_UNORM}, // Specular
            depthFormat, framebuffers[FrontBuffer]->getExtent(),
            depthSampled, // Reconstruct position from depth
            separateDepthPass); // Depth pre-pass for zero overdraw
    }

    void createMeshObjects()
    {
        objects[Cube] = std::make_unique<quadric::Cube>(cmdCopyBuf);
        objects[Teapot] = std::make_unique<quadric::Teapot>(16, cmdCopyBuf);
        objects[Sphere] = std::make_unique<quadric::Sphere>(1.7f, 64, 64, false, cmdCopyBuf);
        objects[Torus] = std::make_unique<quadric::Torus>(0.5f, 2.0f, 32, 128, true, cmdCopyBuf);
        objects[Ground] = std::make_unique<quadric::Plane>(25.f, 25.f, true, cmdCopyBuf);
    }

    void loadTextures()
    {
        normalMap = loadDxtTexture(cmdCopyImg, "sand.dds");
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // 1. Depth pre-pass
        depthDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexFragmentStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1))
            }));
        depthDescriptor.set = descriptorPool->allocateDescriptorSet(depthDescriptor.layout);
        depthDescriptor.set->update(0, transforms);
        depthDescriptor.set->update(1, viewProjTransforms);
        // 2. G-buffer fill shader
        gbDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexFragmentStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, DynamicUniformBuffer(1))
            }));
        gbDescriptor.set = descriptorPool->allocateDescriptorSet(gbDescriptor.layout);
        gbDescriptor.set->update(0, transforms);
        gbDescriptor.set->update(1, viewProjTransforms);
        gbDescriptor.set->update(2, materials);
        // 3. G-buffer fill texture shader
        gbTexDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexFragmentStageBinding(0, magma::descriptors::DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, DynamicUniformBuffer(1)),
                FragmentStageBinding(3, CombinedImageSampler(1))
            }));
        gbTexDescriptor.set = descriptorPool->allocateDescriptorSet(gbTexDescriptor.layout);
        gbTexDescriptor.set->update(0, transforms);
        gbTexDescriptor.set->update(1, viewProjTransforms);
        gbTexDescriptor.set->update(2, materials);
        gbTexDescriptor.set->update(3, normalMap, anisotropicClampToEdge);
        // 4. Deferred shading
        dsDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexStageBinding(1, magma::descriptors::UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1)), // Light source
                FragmentStageBinding(3, CombinedImageSampler(1)), // Normal
                FragmentStageBinding(4, CombinedImageSampler(1)), // Albedo
                FragmentStageBinding(5, CombinedImageSampler(1)), // Specular
                FragmentStageBinding(6, CombinedImageSampler(1))  // Depth
            }));
        dsDescriptor.set = descriptorPool->allocateDescriptorSet(dsDescriptor.layout);
        dsDescriptor.set->update(0, viewProjTransforms);
        dsDescriptor.set->update(1, lightSource);
        dsDescriptor.set->update(2, gbuffer->getAttachmentView(0), nearestClampToEdge);
        dsDescriptor.set->update(3, gbuffer->getAttachmentView(1), nearestClampToEdge);
        dsDescriptor.set->update(4, gbuffer->getAttachmentView(2), nearestClampToEdge);
        dsDescriptor.set->update(5, gbuffer->getDepthStencilView(), nearestClampToEdge);
    }

    void setupGraphicsPipelines()
    {
        depthPipeline = createDepthOnlyPipeline("transform.o",
            objects[0]->getVertexInput(),
            depthDescriptor.layout,
            gbuffer);
        const magma::MultiColorBlendState gbufferBlendState(
            {
                magma::blendstates::writeRg, // Normal
                magma::blendstates::writeRgba, // Albedo
                magma::blendstates::writeRgba  // Specular
            });
        gbufferPipeline = createMrtPipeline("transform.o", "fillGbuffer.o",
            objects[0]->getVertexInput(),
            gbufferBlendState,
            gbuffer,
            gbDescriptor.layout);
        gbufferTexPipeline = createMrtPipeline("transform.o", "fillGbufferTex.o",
            objects[0]->getVertexInput(),
            gbufferBlendState,
            gbuffer,
            gbTexDescriptor.layout);
        deferredPipeline = createFullscreenPipeline("quad.o", "deferred.o", dsDescriptor.layout);
    }

    void renderScene(uint32_t bufferIndex)
    {
        std::shared_ptr<magma::CommandBuffer> cmdBuffer = commandBuffers[bufferIndex];
        cmdBuffer->begin();
        {
            if (FrontBuffer == bufferIndex)
            {   // Draw once
                depthPrePass(cmdBuffer);
                gbufferPass(cmdBuffer);
            }
            deferredPass(cmdBuffer, bufferIndex);
        }
        cmdBuffer->end();
    }

    void depthPrePass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(gbuffer->getDepthRenderPass(), gbuffer->getDepthFramebuffer(),
            {   // Clear only depth attachment
                magma::clears::depthOne
            });
        {
            cmdBuffer->bindPipeline(depthPipeline);
            for (uint32_t i = Cube; i < MaxObjects; ++i)
            {
                cmdBuffer->bindDescriptorSet(depthPipeline, depthDescriptor.set,
                    transforms->getDynamicOffset(i));
                objects[i]->draw(cmdBuffer);
            }
        }
        cmdBuffer->endRenderPass();
    }

    void gbufferPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(gbuffer->getRenderPass(), gbuffer->getFramebuffer(),
            {   // Clear only color attachments
                magma::clears::blackColor,
                magma::ClearColor(0.35f, 0.53f, 0.7f, 1.0f),
                magma::ClearColor(0.35f, 0.53f, 0.7f, 1.0f)
            });
        {   // 1. Draw objects
            cmdBuffer->bindPipeline(gbufferPipeline);
            for (uint32_t i = Cube; i < Ground; ++i)
            {
                cmdBuffer->bindDescriptorSet(gbufferPipeline, gbDescriptor.set,
                    {
                        transforms->getDynamicOffset(i),
                        materials->getDynamicOffset(i)
                    });
                objects[i]->draw(cmdBuffer);
            }
            // 2. Draw textured ground
            cmdBuffer->bindPipeline(gbufferTexPipeline);
            cmdBuffer->bindDescriptorSet(gbufferTexPipeline, gbTexDescriptor.set,
                {
                    transforms->getDynamicOffset(Ground),
                    materials->getDynamicOffset(Ground)
                });
            objects[Ground]->draw(cmdBuffer);
        }
        cmdBuffer->endRenderPass();
    }

    void deferredPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer, uint32_t bufferIndex)
    {
        cmdBuffer->beginRenderPass(renderPass, framebuffers[bufferIndex],
            {
                magma::ClearColor(0.1f, 0.243f, 0.448f, 1.f)
            });
        {
            cmdBuffer->bindPipeline(deferredPipeline);
            cmdBuffer->bindDescriptorSet(deferredPipeline, dsDescriptor.set);
            cmdBuffer->draw(4, 0);
        }
        cmdBuffer->endRenderPass();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::make_unique<DeferredShading>(entry);
}
