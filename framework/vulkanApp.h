#pragma once
#ifdef VK_USE_PLATFORM_WIN32_KHR
#include "winApp.h"
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#include "xlibApp.h"
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#include "xcbApp.h"
#endif // VK_USE_PLATFORM_XCB_KHR
#include "magma/magma.h"
#include "rapid/rapid.h"

#ifdef VK_USE_PLATFORM_WIN32_KHR
typedef Win32App NativeApp;
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
typedef XlibApp NativeApp;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
typedef XcbApp NativeApp;
#endif // VK_USE_PLATFORM_XCB_KHR

class VulkanApp : public NativeApp
{
public:
    VulkanApp(const AppEntry& entry, const core::tstring& caption,
        uint32_t width, uint32_t height, bool sRGB, bool clearOp);
    ~VulkanApp();
    virtual void render(uint32_t bufferIndex) = 0;
    virtual void onIdle() override;
    virtual void onPaint() override;
    virtual void onKeyDown(char key, int repeat, uint32_t flags) override;

protected:
    virtual void initialize();
    virtual void createInstance();
    virtual void enableDeviceFeatures(VkPhysicalDeviceFeatures& features) const noexcept;
    virtual void enableDeviceFeaturesExt(std::vector<void *>& features) const;
    virtual void enableExtensions(std::vector<const char*>& extensionNames) const;
    virtual void createLogicalDevice();
    virtual void createSwapchain(bool vSync);
    virtual void createRenderPass();
    virtual void createFramebuffer();
    virtual void createCommandBuffers();
    virtual void createSyncPrimitives();
    VkSurfaceFormatKHR chooseSurfaceFormat() const noexcept;

protected:
    enum { FrontBuffer = 0, BackBuffer };

protected:
    std::shared_ptr<magma::Instance> instance;
    std::unique_ptr<magma::InstanceLayers> instanceLayers;
    std::unique_ptr<magma::InstanceExtensions> instanceExtensions;
    std::shared_ptr<magma::DebugReportCallback> debugReportCallback;
    std::shared_ptr<magma::Surface> surface;
    std::shared_ptr<magma::PhysicalDevice> physicalDevice;
    std::unique_ptr<magma::PhysicalDeviceExtensions> extensions;
    std::shared_ptr<magma::Device> device;
    std::shared_ptr<magma::Swapchain> swapchain;

    std::shared_ptr<magma::CommandPool> commandPools[2];
    std::vector<std::shared_ptr<magma::CommandBuffer>> commandBuffers;
    std::shared_ptr<magma::CommandBuffer> cmdCopyImg;
    std::shared_ptr<magma::CommandBuffer> cmdCopyBuf;

    std::shared_ptr<magma::RenderPass> renderPass;
    std::vector<std::shared_ptr<magma::Framebuffer>> framebuffers;
    std::shared_ptr<magma::Queue> queue;
    std::shared_ptr<magma::Queue> transferQueue;
    std::shared_ptr<magma::Semaphore> presentFinished;
    std::shared_ptr<magma::Semaphore> renderFinished;
    std::vector<std::shared_ptr<magma::Fence>> waitFences;

    std::shared_ptr<magma::PipelineCache> pipelineCache;

    
    bool sRGB;
	bool clearOp;
};
