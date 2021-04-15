#include "graphicsApp.h"
#include "colorTable.h"
#include "quadric/include/cube.h"
#include "quadric/include/sphere.h"
#include "quadric/include/teapot.h"
#include "quadric/include/plane.h"

class PcfPoissonShadowMapping : public GraphicsApp
{
    enum {
        Cube = 0, Teapot, Sphere, Ground,
        MaxObjects
    };

    struct alignas(16) Parameters
    {
        rapid::float4a screenSize; // x, y, 1/x, 1/y
        float radius;
        float zbias;
    };

    std::unique_ptr<quadric::Quadric> objects[MaxObjects];
    std::shared_ptr<magma::DynamicUniformBuffer<PhongMaterial>> materials;
    std::shared_ptr<magma::UniformBuffer<Parameters>> parameters;
    std::shared_ptr<magma::GraphicsPipeline> shadowMapPipeline;
    std::shared_ptr<magma::GraphicsPipeline> phongShadowPipeline;
    std::shared_ptr<magma::aux::DepthFramebuffer> shadowMap;
    std::shared_ptr<magma::Sampler> shadowSampler;
    DescriptorSet smDescriptor;
    DescriptorSet descriptor;

    rapid::matrix objTransforms[MaxObjects];
    float radius = 10.f;
    bool showDepthMap = false;

public:
    explicit PcfPoissonShadowMapping(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Poisson percentage closer filtering"), 1280, 720, true)
    {
        setupViewProjection();
        setupTransforms();
        setupMaterials();
        updateParameters();
        createShadowMap();
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
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Cap fps
    }

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::PgUp:
            if (radius <= 19.5f)
            {
                radius += 0.5f;
                updateParameters();
            }
            break;
        case AppKey::PgDn:
            if (radius >= 1.5f)
            {
                radius -= 0.5f;
                updateParameters();
            }
            break;
        case AppKey::Space:
            showDepthMap = !showDepthMap;
            renderScene(drawCmdBuffer);
            break;
        }
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    virtual void updateLightSource()
    {
        magma::helpers::mapScoped<LightSource>(lightSource,
            [this](auto *light)
            {
                constexpr float ambientFactor = 0.4f;
                light->viewPosition = viewProj->getView() * lightViewProj->getPosition();
                light->ambient = ghost_white * ambientFactor;
                light->diffuse = ghost_white;
                light->specular = ghost_white;
            });
    }

    void updateParameters()
    {
        if (!parameters)
            parameters = std::make_shared<magma::UniformBuffer<Parameters>>(device);
        magma::helpers::mapScoped<Parameters>(parameters,
            [this](auto *parameters)
            {
                parameters->screenSize = rapid::float4a(float(width), float(height), 1.f/width, 1.f/height);
                parameters->radius = radius;
                // Bias depends on filter radius to avoid shadow leakage
                parameters->zbias = rapid::mapRange(1.0f, 20.0f, radius, 0.0f, 0.025f);
            });
    }

    void updateTransforms()
    {
        const rapid::matrix rotation = rapid::rotationY(rapid::radians(-spinX / 4.f));
        std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms(MaxObjects);
        transforms[Cube] = objTransforms[Cube] * rotation,
            transforms[Teapot] = objTransforms[Teapot] * rotation,
            transforms[Sphere] = objTransforms[Sphere] * rotation,
            transforms[Ground] = objTransforms[Ground];
        updateObjectTransforms(transforms);
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

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-8.f, 12.f, 1.f);
        lightViewProj->setFocus(0.f, 0.f, 0.f);
        lightViewProj->setFieldOfView(70.f);
        lightViewProj->setNearZ(5.f);
        lightViewProj->setFarZ(30.f);
        lightViewProj->setAspectRatio(1.f);
        lightViewProj->updateView();
        lightViewProj->updateProjection();

        updateViewProjTransforms();
    }

    void setupTransforms()
    {
        createTransformBuffer(MaxObjects);
        constexpr float radius = 4.f;
        objTransforms[Cube] = rapid::translation(radius, 1.f, 0.f) * rapid::rotationY(rapid::radians(30.f));
        const rapid::matrix upset =
            rapid::rotationX(rapid::radians(-98.f)) *
            rapid::rotationZ(rapid::radians(-30.f)) *
            rapid::translation(0.f, 2.05f, 0.f);
        objTransforms[Teapot] = upset *
            rapid::rotationY(rapid::radians(-140.f)) *
            rapid::translation(radius, 0.f, 0.f) * rapid::rotationY(rapid::radians(150.f));
        objTransforms[Sphere] = rapid::translation(radius, 1.5f, 0.f) * rapid::rotationY(rapid::radians(270.f));
        constexpr float bias = -0.05f; // Shift slightly down to get rid of shadow leakage
        objTransforms[Ground] = rapid::translation(0.f, bias, 0.f);
    }

    void setupMaterials()
    {
        materials = std::make_shared<magma::DynamicUniformBuffer<PhongMaterial>>(device, MaxObjects);
        magma::helpers::mapScoped<PhongMaterial>(materials,
            [this](magma::helpers::AlignedUniformArray<PhongMaterial>& materials)
            {
                constexpr float ambientFactor = 0.4f;
                materials[Cube].ambient = medium_sea_green * ambientFactor;
                materials[Cube].diffuse = medium_sea_green;
                materials[Cube].specular = medium_sea_green;
                materials[Cube].shininess = 2.f; // High roughness for metal look like

                materials[Teapot].ambient = pale_golden_rod * ambientFactor;
                materials[Teapot].diffuse = pale_golden_rod;
                materials[Teapot].specular = pale_golden_rod;
                materials[Teapot].shininess = 128.f; // Low roughness for plastic look like

                materials[Sphere].ambient = medium_blue * ambientFactor;
                materials[Sphere].diffuse = medium_blue;
                materials[Sphere].specular = deep_sky_blue;
                materials[Sphere].shininess = 2.f;

                materials[Ground].ambient = floral_white * ambientFactor;
                materials[Ground].diffuse = floral_white;
                materials[Ground].specular = floral_white * 0.1f;
                materials[Ground].shininess = 2.f;
            });
    }

    void createShadowMap()
    {
        constexpr VkFormat depthFormat = VK_FORMAT_D16_UNORM; // 16 bits of depth is enough for a tiny scene
        constexpr VkExtent2D extent{2048, 2048};
        shadowMap = std::make_shared<magma::aux::DepthFramebuffer>(device, depthFormat, extent);
        shadowSampler = std::make_shared<magma::DepthSampler>(device, magma::samplers::magMinNearestCompareLessOrEqual);
    }

    void createMeshObjects()
    {
        objects[Cube] = std::make_unique<quadric::Cube>(cmdCopyBuf);
        objects[Teapot] = std::make_unique<quadric::Teapot>(16, cmdCopyBuf);
        objects[Sphere] = std::make_unique<quadric::Sphere>(1.5f, 64, 64, false, cmdCopyBuf);
        objects[Ground] = std::make_unique<quadric::Plane>(100.f, 100.f, false, cmdCopyBuf);
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // Shadow map shader
        smDescriptor.layout = std::make_shared<magma::DescriptorSetLayout>(device,
            VertexStageBinding(0, DynamicUniformBuffer(1)));
        smDescriptor.set = descriptorPool->allocateDescriptorSet(smDescriptor.layout);
        smDescriptor.set->writeDescriptor(0, transforms);
        // Lighting shader
        descriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexStageBinding(0, magma::descriptors::DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1)),
                FragmentStageBinding(3, DynamicUniformBuffer(1)),
                FragmentStageBinding(4, UniformBuffer(1)),
                FragmentStageBinding(5, CombinedImageSampler(1))
            }));
        descriptor.set = descriptorPool->allocateDescriptorSet(descriptor.layout);
        descriptor.set->writeDescriptor(0, transforms);
        descriptor.set->writeDescriptor(1, viewProjTransforms);
        descriptor.set->writeDescriptor(2, lightSource);
        descriptor.set->writeDescriptor(3, materials);
        descriptor.set->writeDescriptor(4, parameters);
        descriptor.set->writeDescriptor(5, shadowMap->getDepthView(), shadowSampler);
    }

    void setupGraphicsPipelines()
    {
        shadowMapPipeline = createShadowMapPipeline(
            "shadowMap.o",
            objects[0]->getVertexInput(),
            magma::renderstates::fillCullFrontCW, // Draw only back faces to get rid of shadow acne
            smDescriptor.layout,
            shadowMap);
        phongShadowPipeline = createCommonPipeline(
            "transform.o", "phong.o",
            objects[0]->getVertexInput(),
            descriptor.layout);
    }

    void renderScene(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->begin();
        {
            shadowMapPass(cmdBuffer);
            lightingPass(cmdBuffer);
        }
        cmdBuffer->end();
    }

    void shadowMapPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(shadowMap->getRenderPass(), shadowMap->getFramebuffer(),
            {
                magma::clears::depthOne
            });
        {
            cmdBuffer->setViewport(magma::Viewport(0, 0, shadowMap->getExtent()));
            cmdBuffer->setScissor(magma::Scissor(0, 0, shadowMap->getExtent()));
            cmdBuffer->bindPipeline(shadowMapPipeline);
            for (uint32_t i = Cube; i < Ground; ++i)
            {
                cmdBuffer->bindDescriptorSet(shadowMapPipeline, smDescriptor.set, transforms->getDynamicOffset(i));
                objects[i]->draw(cmdBuffer);
            }
        }
        cmdBuffer->endRenderPass();
    }

    void lightingPass(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        cmdBuffer->beginRenderPass(msaaFramebuffer->getRenderPass(), msaaFramebuffer->getFramebuffer(),
            {
                magma::ClearColor(0.35f, 0.53f, 0.7f, 1.0f),
                magma::clears::depthOne
            });
        {
            cmdBuffer->setViewport(magma::Viewport(0, 0, msaaFramebuffer->getExtent()));
            cmdBuffer->setScissor(magma::Scissor(0, 0, msaaFramebuffer->getExtent()));
            cmdBuffer->bindPipeline(phongShadowPipeline);
            for (uint32_t i = Cube; i < MaxObjects; ++i)
            {
                cmdBuffer->bindDescriptorSet(phongShadowPipeline, descriptor.set, {
                    transforms->getDynamicOffset(i),
                    materials->getDynamicOffset(i)
                });
                objects[i]->draw(cmdBuffer);
            }
            if (showDepthMap)
                drawDepthMap(cmdBuffer);
        }
        cmdBuffer->endRenderPass();
    }

    void drawDepthMap(std::shared_ptr<magma::CommandBuffer> cmdBuffer)
    {
        constexpr VkRect2D rect = VkRect2D{0, 0, 256U, 256U};
        if (!bltRect) bltRect = std::make_unique<magma::aux::BlitRectangle>(renderPass, loadShader("linearizeDepth.o"));
        bltRect->blit(std::move(cmdBuffer), shadowMap->getDepthView(), VK_FILTER_NEAREST, rect);
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::make_unique<PcfPoissonShadowMapping>(entry);
}
