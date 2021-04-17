#include "rayTracingApp.h"
#include "cornellBox.h"
#include "colorTable.h"

#ifdef VK_NV_ray_tracing
class RayTracing : public RayTracingApp
{
    enum Object : uint32_t
    {
        Box = 0, ShortBlock, TallBlock, Light,
        Max
    };

    std::shared_ptr<CornellBox> cornellBox;
    DescriptorSet raygenDescriptor;
    DescriptorSet rayhitDescriptor;

    const float lightExtension = 0.15f;
    rapid::matrix lightTransform;
    bool rotateLight = true;

public:
    explicit RayTracing(const AppEntry& entry):
        RayTracingApp(entry, TEXT("Cornell box"), 720, 720, true)
    {
        cornellBox = std::make_shared<CornellBox>(cmdCopyBuf);
        createInstanceBuffer();
        buildAccelerationStructures();
        setupDescriptorSets();
        setupRayTracingPipeline();

        raytraceScene();
        blit(outputImageView, FrontBuffer);
        blit(outputImageView, BackBuffer);

        timer->run();
    }

    virtual void render(uint32_t bufferIndex) override
    {
        if (rotateLight)
            updateLightSource();
        updateTopLevelAccelerationStructure();
        submitCommandBuffers(bufferIndex);
    }

    virtual void onKeyDown(char key, int repeat, uint32_t flags) override
    {
        switch (key)
        {
        case AppKey::Space:
            rotateLight = !rotateLight;
            timer->run();
            break;
        }
        VulkanApp::onKeyDown(key, repeat, flags);
    }

    rapid::matrix calculateLightTransform()
    {
        const float radius = 1.f - lightExtension;
        static float theta = 0.0f;
        theta += rapid::radians(timer->millisecondsElapsed() / 25.f);
        return rapid::translation(radius, 1.f - 1e-5f, 0.f) * rapid::rotationY(theta);
    }

    void updateLightSource()
    {
        rapid::matrix transform = calculateLightTransform();
        magma::helpers::mapScoped<LightSource>(lightSource,
            [&transform](LightSource *light)
            {   // Shift a bit down actual light position
                light->viewPosition = transform * rapid::vector3(0.f, -0.1f, 0.0f);
                light->ambient = old_lace * 0.2f;
                light->diffuse = old_lace;
                light->specular = old_lace;
            });

        rapid::vector3 position = transform * rapid::vector3(0.f);
        lightTransform = rapid::scaling(lightExtension, 1.f, lightExtension) * rapid::translation(position);
        magma::helpers::mapScoped<RtTransforms>(transforms,
            [this](RtTransforms *data)
            {
                data[Light].world = lightTransform;
                data[Light].normal = rapid::transpose(rapid::inverse(lightTransform));
            });
    }

    void updateTopLevelAccelerationStructure()
    {
        static std::once_flag scratch;
        std::call_once(scratch, [this]() {
            resizeScratchBuffer(tlas->getUpdateScratchMemoryRequirements().size);
        });
        buildCmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        {   // Update transform of light source instance
            magma::TransformMatrix transform;
            lightTransform.store3x4(transform.matrix);
            instanceBuffer->getInstance(Light).setTransform(transform);
            instanceBuffer->update(buildCmdBuffer, Light, 1);
            // Rebuild top-level acceleration structure
            buildCmdBuffer->buildAccelerationStructure(instanceBuffer, 0, true, tlas, tlas, scratchBuffer);
        }
        buildCmdBuffer->end();
        queue->submit(buildCmdBuffer);
    }

    void createInstanceBuffer()
    {
        rapid::matrix worldTransforms[Object::Max];
        setupObjectTransforms(worldTransforms);
        std::shared_ptr<magma::AccelerationStructure> blas[Object::Max] = {
            cornellBox->box->getBlas(),
            cornellBox->block->getBlas(),
            cornellBox->block->getBlas(),
            cornellBox->quad->getBlas()
        };
        instanceBuffer = std::make_shared<magma::AccelerationStructureInstanceBuffer>(device, Object::Max);
        for (uint32_t i = 0, n = instanceBuffer->getInstanceCount(); i < n; ++i)
        {
            magma::TransformMatrix transform;
            worldTransforms[i].store3x4(transform.matrix);
            magma::AccelerationStructureInstance& instance = instanceBuffer->getInstance(i);
            instance.setTransform(transform);
            instance.setAccelerationStructure(blas[i]);
         }
         instanceBuffer->getInstance(Box).setTriangleFrontCCW();
    }

    void setupObjectTransforms(rapid::matrix worldTransforms[Object::Max])
    {   // http://www.graphics.cornell.edu/online/box/data.html
        constexpr float width = 555.f; // mm
        constexpr float scale = 165.f/width; // mm
        constexpr float scaleTall = 330.f/width; // mm
        worldTransforms[ShortBlock] =
            rapid::scaling(scale, scale, scale) *
            rapid::rotationY(0.275f) *
            rapid::translation(0.35f, scale - 1.f, -0.35f);
        worldTransforms[TallBlock] =
            rapid::scaling(scale, scaleTall, scale) *
            rapid::rotationY(-0.31f) *
            rapid::translation(-0.35f, scaleTall - 1.f, 0.35f);
        worldTransforms[Box] = rapid::identity();
        worldTransforms[Light] = rapid::identity();
        // Create uniform buffer
        transforms = std::make_shared<magma::UniformBuffer<RtTransforms>>(device, Object::Max);
        magma::helpers::mapScoped<RtTransforms>(transforms,
            [worldTransforms](RtTransforms *it)
            {
                for (uint32_t i = Box; i < Object::Max; ++i)
                {
                    it->world = worldTransforms[i];
                    it->normal = rapid::transpose(rapid::inverse(it->world));
                    ++it;
                }
            });
    }

    void buildBottomLevelAccelerationStructures()
    {
        for (auto& blas :{
            cornellBox->box->getBlas(),
            cornellBox->block->getBlas(),
            cornellBox->block->getBlas(),
            cornellBox->quad->getBlas()})
        {
            resizeScratchBuffer(blas->getBuildScratchMemoryRequirements().size);
            buildCmdBuffer->buildAccelerationStructure(nullptr, 0, false, blas, nullptr, scratchBuffer);
            buildCmdBuffer->pipelineBarrier(
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, // Since the scratch buffer is reused across builds,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, // we need a barrier to ensure one build is finished 
                magma::barriers::accelerationStructureReadWrite);      // before starting the next one
        }
    }

    void buildTopLevelAccelerationStructure()
    {
        tlas = std::make_shared<magma::TopLevelAccelerationStructure>(device, Object::Max,
            VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV);
        resizeScratchBuffer(tlas->getBuildScratchMemoryRequirements().size);
        buildCmdBuffer->buildAccelerationStructure(instanceBuffer, 0, false, tlas, nullptr, scratchBuffer);
    }

    void buildAccelerationStructures()
    {
        buildCmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        {
            instanceBuffer->update(buildCmdBuffer, 0, Object::Max);
            buildBottomLevelAccelerationStructures();
            buildTopLevelAccelerationStructure();
        }
        buildCmdBuffer->end();
        queue->submit(buildCmdBuffer);
        if (!queue->waitIdle())
            throw std::runtime_error("failed to build acceleration structures");
    }

    void setupDescriptorSets()
    {
        using namespace magma::bindings;
        using namespace magma::descriptors;
        // Ray-gen shader
        raygenDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                RaygenClosestHitStageBinding(0, AccelerationStructure(1)),
                RaygenStageBinding(1, StorageImage(1))
            }));
        raygenDescriptor.set = descriptorPool->allocateDescriptorSet(raygenDescriptor.layout);
        raygenDescriptor.set->writeDescriptor(0, tlas);
        raygenDescriptor.set->writeDescriptor(1, outputImageView, nullptr);
        // Ray-hit shader
        rayhitDescriptor.layout = std::shared_ptr<magma::DescriptorSetLayout>(new magma::DescriptorSetLayout(device,
            {
                ClosestHitStageBinding(0, StorageBuffer(4)),
                ClosestHitStageBinding(1, StorageBuffer(4)),
                ClosestHitStageBinding(2, UniformBuffer(1)),
                ClosestHitStageBinding(3, UniformBuffer(1)),
            }));
        rayhitDescriptor.set = descriptorPool->allocateDescriptorSet(rayhitDescriptor.layout);
        rayhitDescriptor.set->writeDescriptorArray(0,
            {
                cornellBox->box->getVertexBuffer(),
                cornellBox->block->getVertexBuffer(),
                cornellBox->block->getVertexBuffer(),
                cornellBox->quad->getVertexBuffer()
            });
        rayhitDescriptor.set->writeDescriptorArray(1,
            {
                cornellBox->box->getIndexBuffer(),
                cornellBox->block->getIndexBuffer(),
                cornellBox->block->getIndexBuffer(),
                cornellBox->quad->getIndexBuffer()
            });
        rayhitDescriptor.set->writeDescriptor(2, lightSource);
        rayhitDescriptor.set->writeDescriptor(3, transforms);
    }

    void setupRayTracingPipeline()
    {
        const std::vector<magma::PipelineShaderStage> stages{
            loadShaderStage("raygen.o", VK_SHADER_STAGE_RAYGEN_BIT_NV),
            loadShaderStage("miss.o", VK_SHADER_STAGE_MISS_BIT_NV),
            loadShaderStage("missShadow.o", VK_SHADER_STAGE_MISS_BIT_NV),
            loadShaderStage("phong.o", VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
        };
        const std::vector<magma::RayTracingShaderGroup> groups{
            magma::GeneralRayTracingShaderGroup(0),
            magma::GeneralRayTracingShaderGroup(1),
            magma::GeneralRayTracingShaderGroup(2),
            magma::TrianglesHitRayTracingShaderGroup(3)
        };
        std::shared_ptr<magma::PipelineLayout> pipelineLayout = std::make_shared<magma::PipelineLayout>(
            std::vector<std::shared_ptr<magma::DescriptorSetLayout>>{
                raygenDescriptor.layout,
                rayhitDescriptor.layout
            });
        constexpr uint32_t maxRecursionDepth = 2; // Primary and shadow ray
        rtPipeline = std::make_shared<magma::RayTracingPipeline>(device,
            stages,
            groups,
            maxRecursionDepth,
            std::move(pipelineLayout));
        shaderBindingTable = std::make_shared<magma::ShaderBindingTable>(rtPipeline);
    }

    void raytraceScene()
    {
        rtCmdBuffer->begin();
        {
            const uint32_t baseAlignment = physicalDevice->getRayTracingProperties().shaderGroupBaseAlignment;
            const VkDeviceSize raygenShaderOffset = 0;
            const VkDeviceSize missShaderOffset = baseAlignment;
            const VkDeviceSize missShadowShaderOffset = missShaderOffset + baseAlignment;
            const VkDeviceSize hitShaderOffset = missShadowShaderOffset + baseAlignment;
            rtCmdBuffer->bindPipeline(rtPipeline);
            rtCmdBuffer->bindDescriptorSets(rtPipeline,
                {
                    raygenDescriptor.set,
                    rayhitDescriptor.set
                });
            rtCmdBuffer->traceRays(
                shaderBindingTable, raygenShaderOffset,
                shaderBindingTable, missShaderOffset, baseAlignment,
                shaderBindingTable, hitShaderOffset, baseAlignment,
                nullptr, 0, 0,
                width, height, 1);
        }
        rtCmdBuffer->end();
    }
};

std::unique_ptr<IApplication> appFactory(const AppEntry& entry)
{
    return std::make_unique<RayTracing>(entry);
}
#endif // VK_NV_ray_tracing
