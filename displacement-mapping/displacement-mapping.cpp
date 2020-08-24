#include "graphicsApp.h"
#include "textureLoader.h"
#include "colorTable.h"
#include "quadric/include/torus.h"
#include "quadric/include/sphere.h"

class DisplacementMapping : public GraphicsApp
{
    struct Constants
    {
        VkBool32 showNormals = false;
    };

    struct alignas(16) Parameters
    {
        rapid::float4a screenSize; // x, y, 1/x, 1/y
        float displacement;
    };

    std::unique_ptr<quadric::Torus> torus;
    std::unique_ptr<quadric::Sphere> dot;
    std::shared_ptr<magma::ImageView> displacementMap;
    std::shared_ptr<magma::UniformBuffer<Parameters>> parameters;
    std::shared_ptr<magma::DynamicUniformBuffer<PhongMaterial>> material;
    std::shared_ptr<magma::GraphicsPipeline> displacementPipeline;
    std::shared_ptr<magma::GraphicsPipeline> fillPipeline;
    DescriptorSet displacementDescriptor;
    DescriptorSet fillDescriptor;

    Constants constants;
    float displacement = 0.35f;

public:
    explicit DisplacementMapping(const AppEntry& entry):
        GraphicsApp(entry, TEXT("Displacement mapping"), 1280, 720, true)
    {
        setupViewProjection();
        setupMaterials();
        updateParameters();
        createTransformBuffer(2);
        createMeshObjects();
        loadDisplacementMap();
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
        constexpr float step = 0.1f;
        switch (key)
        {
        case AppKey::Left:
            lightViewProj->translate(-step, 0.f, 0.f);
            break;
        case AppKey::Right:
            lightViewProj->translate(step, 0.f, 0.f);
            break;
        case AppKey::Up:
            lightViewProj->translate(0.f, step, 0.f);
            break;
        case AppKey::Down:
            lightViewProj->translate(0.f, -step, 0.f);
            break;
        case AppKey::PgUp:
            lightViewProj->translate(0.f, 0.f, step);
            break;
        case AppKey::PgDn:
            lightViewProj->translate(0.f, 0.f, -step);
            break;
        case AppKey::Home:
            if (displacement < 0.5f)
            {
                displacement += 0.01f;
                updateParameters();
            }
            break;
        case AppKey::End:
            if (displacement >= 0.21f)
            {
                displacement -= 0.01f;
                updateParameters();
            }
            break;
        case AppKey::Space:
            constants.showNormals = !constants.showNormals;
            setupGraphicsPipelines();
            renderScene(drawCmdBuffer);
            break;
        }
        updateTransforms();
        updateLightSource();
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

    void updateTransforms()
    {
        rapid::matrix lightTransform = rapid::translation(rapid::vector3(lightViewProj->getPosition()));
        const std::vector<rapid::matrix, core::aligned_allocator<rapid::matrix>> transforms = {
            arcball->transform(),
            lightTransform
        };
        updateObjectTransforms(transforms);
    }

    void updateParameters()
    {
        if (!parameters)
            parameters = std::make_shared<magma::UniformBuffer<Parameters>>(device);
        magma::helpers::mapScoped<Parameters>(parameters,
            [this](auto *parameters)
            {
                parameters->screenSize = rapid::float4a(float(width), float(height), 1.f/width, 1.f/height);
                parameters->displacement = displacement;
            });
    }

    void setupViewProjection()
    {
        viewProj = std::make_unique<LeftHandedViewProjection>();
        viewProj->setPosition(0.f, 0.f, -4.f);
        viewProj->setFocus(0.f, 0.f, 0.f);
        viewProj->setFieldOfView(60.f);
        viewProj->setNearZ(1.f);
        viewProj->setFarZ(100.f);
        viewProj->setAspectRatio(width / (float)height);
        viewProj->updateView();
        viewProj->updateProjection();

        lightViewProj = std::make_unique<LeftHandedViewProjection>();
        lightViewProj->setPosition(-1., 2.f, -1.f);

        updateViewProjTransforms();
    }

    void setupMaterials()
    {
        material = std::make_shared<magma::DynamicUniformBuffer<PhongMaterial>>(device, 1);
        magma::helpers::mapScoped<PhongMaterial>(material,
            [this](magma::helpers::AlignedUniformArray<PhongMaterial>& materials)
            {
                constexpr float ambientFactor = 0.4f;
                materials[0].ambient = azure * ambientFactor;
                materials[0].diffuse = azure;
                materials[0].specular = alice_blue;
                materials[0].shininess = 64.f;
            });
    }

    void createMeshObjects()
    {
        torus = std::make_unique<quadric::Torus>(0.2f, 1.0f, 128, 128, true, cmdCopyBuf);
        dot = std::make_unique<quadric::Sphere>(0.02f, 8, 8, true, cmdCopyBuf);
    }

    void loadDisplacementMap()
    {   // https://freepbr.com/materials/stucco-1/
        displacementMap = loadDxtTexture(cmdCopyImg, "stucco1_normal_height.dds");
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // Displacement shader
        displacementDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                VertexFragmentStageBinding(0, DynamicUniformBuffer(1)),
                FragmentStageBinding(1, UniformBuffer(1)),
                FragmentStageBinding(2, UniformBuffer(1)),
                FragmentStageBinding(3, DynamicUniformBuffer(1)),
                VertexFragmentStageBinding(4, UniformBuffer(1)),
                VertexFragmentStageBinding(5, CombinedImageSampler(1))
            }));
        displacementDescriptor.set = descriptorPool->allocateDescriptorSet(displacementDescriptor.layout);
        displacementDescriptor.set->update(0, transforms);
        displacementDescriptor.set->update(1, viewProjTransforms);
        displacementDescriptor.set->update(2, lightSource);
        displacementDescriptor.set->update(3, material);
        displacementDescriptor.set->update(4, parameters);
        displacementDescriptor.set->update(5, displacementMap, anisotropicClampToEdge);
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
            magma::SpecializationEntry(0, &Constants::showNormals)));
        displacementPipeline = createCommonSpecializedPipeline(
            "displace.o", "phong.o",
            std::move(specialization),
            torus->getVertexInput(),
            displacementDescriptor.layout);
        if (!fillPipeline)
        {
            fillPipeline = createCommonPipeline(
                "transform.o", "fill.o",
                dot->getVertexInput(),
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
                cmdBuffer->bindPipeline(displacementPipeline);
                cmdBuffer->bindDescriptorSet(displacementPipeline, displacementDescriptor.set,
                    {
                        transforms->getDynamicOffset(0),
                        material->getDynamicOffset(0)
                    });
                torus->draw(cmdBuffer);
                cmdBuffer->bindPipeline(fillPipeline);
                cmdBuffer->bindDescriptorSet(fillPipeline, fillDescriptor.set, transforms->getDynamicOffset(1));
                dot->draw(cmdBuffer);
            }
            cmdBuffer->endRenderPass();
        }
        cmdBuffer->end();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::unique_ptr<DisplacementMapping>(new DisplacementMapping(entry));
}
