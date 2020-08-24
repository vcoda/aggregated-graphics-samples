#include "graphicsApp.h"
#include "colorTable.h"
#include "textureLoader.h"
#include "utilities.h"

#include "quadric/include/cube.h"
#include "quadric/include/sphere.h"
#include "quadric/include/torus.h"
#include "quadric/include/teapot.h"
#include "quadric/include/plane.h"

class Gbuffer : public GraphicsApp
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
    std::shared_ptr<magma::GraphicsPipeline> gbufferPipeline;
    std::shared_ptr<magma::GraphicsPipeline> gbufferTexPipeline;
    std::unique_ptr<magma::aux::BlitRectangle> layers[5];
    DescriptorSet descriptor;
    DescriptorSet texDescriptor;

    rapid::matrix objTransforms[MaxObjects];
    uint32_t currLayer = 0;

public:
    Gbuffer(const AppEntry& entry):
        GraphicsApp(entry, TEXT("G-buffer"), 1280, 720, false)
    {
        setupViewProjection();
        setupTransforms();
        setupMaterials();
        createGbuffer();
        createMeshObjects();
        loadTextures();
        setupDescriptorSets();
        setupGraphicsPipelines();
        setupDrawLayers();

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

    virtual void onMouseLButton(bool down, int x, int y) override
    {
        if (x < (int)width/5)
        {
            const uint32_t layer = (uint32_t )floor(y/(float)height * 5);
            if (layer != currLayer)
            {
                currLayer = layer;
                renderScene(FrontBuffer);
                renderScene(BackBuffer);
            }
        }
        return GraphicsApp::onMouseLButton(down, x, y);
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
        const rapid::matrix rotation = rapid::rotationY(rapid::radians(-spinX/4.f));
        const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            objTransforms[Cube] * rotation,
            objTransforms[Teapot] * rotation,
            objTransforms[Sphere] * rotation,
            torusRotation * objTransforms[Torus] * rotation,
            objTransforms[Ground] * rotation};
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
        constexpr bool noDepthPass = false;
        const VkFormat depthFormat = utilities::getSupportedDepthFormat(physicalDevice, false, true);
        gbuffer = std::make_shared<magma::aux::MultiAttachmentFramebuffer>(device,
            std::initializer_list<VkFormat>{
                VK_FORMAT_R16G16_SFLOAT, // Normal
                VK_FORMAT_R8G8B8A8_UNORM, // Albedo
                VK_FORMAT_R8G8B8A8_UNORM}, // Specular
            depthFormat, framebuffers[FrontBuffer]->getExtent(),
            depthSampled, // Reconstruct position from depth
            noDepthPass);
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
    {   // 1. G-buffer fill shader
        descriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                magma::bindings::VertexStageBinding(0, magma::descriptors::DynamicUniformBuffer(1)),
                magma::bindings::FragmentStageBinding(1, magma::descriptors::UniformBuffer(1)),
                magma::bindings::FragmentStageBinding(2, magma::descriptors::DynamicUniformBuffer(1)),
            }));
        descriptor.set = descriptorPool->allocateDescriptorSet(descriptor.layout);
        descriptor.set->update(0, transforms);
        descriptor.set->update(1, viewProjTransforms);
        descriptor.set->update(2, materials);
        // 2. G-buffer fill texture shader
        texDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                magma::bindings::VertexFragmentStageBinding(0, magma::descriptors::DynamicUniformBuffer(1)),
                magma::bindings::FragmentStageBinding(1, magma::descriptors::UniformBuffer(1)),
                magma::bindings::FragmentStageBinding(2, magma::descriptors::DynamicUniformBuffer(1)),
                magma::bindings::FragmentStageBinding(3, magma::descriptors::CombinedImageSampler(1))
            }));
        texDescriptor.set = descriptorPool->allocateDescriptorSet(texDescriptor.layout);
        texDescriptor.set->update(0, transforms);
        texDescriptor.set->update(1, viewProjTransforms);
        texDescriptor.set->update(2, materials);
        texDescriptor.set->update(3, normalMap, anisotropicClampToEdge);
    }

    void setupGraphicsPipelines()
    {
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
            descriptor.layout);
        gbufferTexPipeline = createMrtPipeline("transform.o", "fillGbufferTex.o",
            objects[0]->getVertexInput(),
            gbufferBlendState,
            gbuffer,
            texDescriptor.layout);
    }

    void setupDrawLayers()
    {
        layers[Normal] = std::make_unique<magma::aux::BlitRectangle>(renderPass, loadShader("decodeNormal.o"));
        layers[Albedo] = std::make_unique<magma::aux::BlitRectangle>(renderPass, loadShader("loadColor.o"));
        layers[Specular] = std::make_unique<magma::aux::BlitRectangle>(renderPass, loadShader("loadColor.o"));
        layers[Shininess] = std::make_unique<magma::aux::BlitRectangle>(renderPass, loadShader("loadShininess.o"));
        layers[Depth] = std::make_unique<magma::aux::BlitRectangle>(renderPass, loadShader("linearizeDepth.o"));
    }

    void renderScene(uint32_t bufferIndex)
    {
        std::shared_ptr<magma::CommandBuffer> cmdBuffer = commandBuffers[bufferIndex];
        cmdBuffer->begin();
        {
            if (FrontBuffer == bufferIndex)
                gbufferPass(cmdBuffer); // Draw once
            attributePass(cmdBuffer, bufferIndex);
        }
        cmdBuffer->end();
    }

    void gbufferPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(gbuffer->getRenderPass(), gbuffer->getFramebuffer(),
            {
                magma::clears::blackColor,
                magma::ClearColor(0.35f, 0.53f, 0.7f, 1.f),
                magma::ClearColor(0.35f, 0.53f, 0.7f, 1.f),
                magma::clears::depthOne
            });
        {   // 1. Draw objects
            cmdBuffer->bindPipeline(gbufferPipeline);
            for (uint32_t i = Cube; i < Ground; ++i)
            {
                cmdBuffer->bindDescriptorSet(gbufferPipeline, descriptor.set,
                    {
                        transforms->getDynamicOffset(i),
                        materials->getDynamicOffset(i)
                    });
                objects[i]->draw(cmdBuffer);
            }
            // 2. Draw textured ground
            cmdBuffer->bindPipeline(gbufferTexPipeline);
            cmdBuffer->bindDescriptorSet(gbufferTexPipeline, texDescriptor.set,
                {
                    transforms->getDynamicOffset(Ground),
                    materials->getDynamicOffset(Ground)
                });
            objects[Ground]->draw(cmdBuffer);
        }
        cmdBuffer->endRenderPass();
    }

    void attributePass(std::shared_ptr<magma::CommandBuffer> cmdBuffer, uint32_t bufferIndex)
    {
        cmdBuffer->beginRenderPass(renderPass, framebuffers[bufferIndex]);
        {
            std::shared_ptr<const magma::ImageView> imageView;
            if (currLayer < Depth)
                imageView = gbuffer->getAttachmentView(std::min(currLayer, 2U));
            else
                imageView = gbuffer->getDepthStencilView();
            const VkRect2D rect{0, 0, gbuffer->getExtent()};
            layers[currLayer]->blit(cmdBuffer, imageView, VK_FILTER_NEAREST, rect);
            drawDecodedLayers(cmdBuffer);
        }
        cmdBuffer->endRenderPass();
    }

    void drawDecodedLayers(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        constexpr VkFilter filter = VK_FILTER_NEAREST;
        const VkExtent2D extent{width/5, height/5};
        const int32_t h = (int32_t)extent.height;
        layers[Normal]->blit(cmdBuffer, gbuffer->getAttachmentView(0), filter, VkRect2D{0, 0, extent});
        layers[Albedo]->blit(cmdBuffer, gbuffer->getAttachmentView(1), filter, VkRect2D{0, h, extent});
        layers[Specular]->blit(cmdBuffer, gbuffer->getAttachmentView(2), filter, VkRect2D{0, h * 2, extent});
        layers[Shininess]->blit(cmdBuffer, gbuffer->getAttachmentView(2), filter, VkRect2D{0, h * 3, extent});
        layers[Depth]->blit(cmdBuffer, gbuffer->getDepthStencilView(), filter, VkRect2D{0, h * 4, extent});
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::make_unique<Gbuffer>(entry);
}
