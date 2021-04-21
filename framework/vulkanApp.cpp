#include "vulkanApp.h"
#include "utilities.h"

VulkanApp::VulkanApp(const AppEntry& entry, const core::tstring& caption, uint32_t width, uint32_t height, bool sRGB, bool clearOp):
    NativeApp(entry, caption, width, height),
    sRGB(sRGB),
	clearOp(clearOp)
{
}

VulkanApp::~VulkanApp()
{
    //commandPools[0]->freeCommandBuffers(commandBuffers);
}

void VulkanApp::onIdle()
{
    onPaint();
}

void VulkanApp::onPaint()
{
    const uint32_t bufferIndex = swapchain->acquireNextImage(presentFinished, nullptr);
    waitFences[bufferIndex]->wait();
    waitFences[bufferIndex]->reset();
    {
        render(bufferIndex);
    }
    queue->present(swapchain, bufferIndex, renderFinished);
    device->waitIdle(); // Flush
}

void VulkanApp::onKeyDown(char key, int repeat, uint32_t flags)
{
    NativeApp::onKeyDown(key, repeat, flags);
}

void VulkanApp::initialize()
{
    createInstance();
    createLogicalDevice();
    createSwapchain(false);
    createRenderPass();
    createFramebuffer();
    createCommandBuffers();
    createSyncPrimitives();
    pipelineCache = std::make_shared<magma::PipelineCache>(device);
}

void VulkanApp::createInstance()
{
    std::vector<const char *> layerNames;
#ifdef _DEBUG
    instanceLayers = std::make_unique<magma::InstanceLayers>();
    if (instanceLayers->KHRONOS_validation)
        layerNames.push_back("VK_LAYER_KHRONOS_validation");
    else if (instanceLayers->LUNARG_standard_validation)
        layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

    std::vector<const char *> extensionNames = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_XCB_KHR)
        VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif // VK_USE_PLATFORM_XCB_KHR
    };
    instanceExtensions = std::make_unique<magma::InstanceExtensions>();
    if (instanceExtensions->EXT_debug_report)
        extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    if (instanceExtensions->KHR_get_physical_device_properties2)
        extensionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    MAGMA_STACK_ARRAY(char, appName, caption.length() + 1);
#ifdef VK_USE_PLATFORM_WIN32_KHR
    size_t count = 0;
    wcstombs_s(&count, appName, appName.size(), caption.c_str(), appName.size());
#else
    strcpy(appName, caption.c_str());
#endif

    instance = std::make_shared<magma::Instance>(
        appName,
        "Magma",
        VK_API_VERSION_1_0,
        layerNames, extensionNames);

    debugReportCallback = std::make_shared<magma::DebugReportCallback>(
        instance,
        utilities::reportCallback);

    physicalDevice = instance->getPhysicalDevice(0);
    extensions = std::make_unique<magma::PhysicalDeviceExtensions>(physicalDevice);
}

void VulkanApp::enableDeviceFeatures(VkPhysicalDeviceFeatures& features) const noexcept
{
    features.fillModeNonSolid = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    features.textureCompressionBC = VK_TRUE;
    features.occlusionQueryPrecise = VK_TRUE;
}

void VulkanApp::enableDeviceFeaturesExt(std::vector<void *>& features) const
{
#ifdef VK_KHR_separate_depth_stencil_layouts
    if (extensions->KHR_separate_depth_stencil_layouts)
    {
        static VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures separateDepthStencilLayouts;
        separateDepthStencilLayouts.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES;
        separateDepthStencilLayouts.pNext = nullptr;
        separateDepthStencilLayouts.separateDepthStencilLayouts = true;
        features.push_back(&separateDepthStencilLayouts);
    }
#endif // VK_KHR_separate_depth_stencil_layouts
}

void VulkanApp::enableExtensions(std::vector<const char*>& extensionNames) const
{
#ifdef VK_KHR_swapchain
    if (extensions->KHR_swapchain)
        extensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#endif
#ifdef VK_NV_fill_rectangle
    if (extensions->NV_fill_rectangle)
        extensionNames.push_back(VK_NV_FILL_RECTANGLE_EXTENSION_NAME);
#endif
}

void VulkanApp::createLogicalDevice()
{
    const std::vector<float> defaultQueuePriorities = {1.0f};
    const magma::DeviceQueueDescriptor graphicsQueue(physicalDevice, VK_QUEUE_GRAPHICS_BIT, defaultQueuePriorities);
    const magma::DeviceQueueDescriptor transferQueue(physicalDevice, VK_QUEUE_TRANSFER_BIT, defaultQueuePriorities);
    std::vector<magma::DeviceQueueDescriptor> queueDescriptors;
    queueDescriptors.push_back(graphicsQueue);
    if (transferQueue.queueFamilyIndex != graphicsQueue.queueFamilyIndex)
        queueDescriptors.push_back(transferQueue);

    // Enable some widely used features
    VkPhysicalDeviceFeatures features = {0};
    enableDeviceFeatures(features);

    std::vector<void *> extendedFeatures;
    enableDeviceFeaturesExt(extendedFeatures);

    std::vector<const char*> extensionNames;
    enableExtensions(extensionNames);

    const std::vector<const char*> noLayers;
    device = physicalDevice->createDevice(queueDescriptors, noLayers, extensionNames, features, extendedFeatures);
}

void VulkanApp::createSwapchain(bool vSync)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    surface = std::make_shared<magma::Win32Surface>(instance, hInstance, hWnd);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    surface = std::make_shared<magma::XlibSurface>(instance, dpy, window);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    surface = std::make_shared<magma::XcbSurface>(instance, connection, window);
#endif // VK_USE_PLATFORM_XCB_KHR
    if (!physicalDevice->getSurfaceSupport(surface))
        throw std::runtime_error("surface not supported");
    // Get surface caps
    VkSurfaceCapabilitiesKHR surfaceCaps;
    surfaceCaps = physicalDevice->getSurfaceCapabilities(surface);
    assert(surfaceCaps.currentExtent.width == width);
    assert(surfaceCaps.currentExtent.height == height);
    // Find supported transform flags
    VkSurfaceTransformFlagBitsKHR preTransform;
    if (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
        preTransform = surfaceCaps.currentTransform;
    // Find supported alpha composite
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    for (auto alphaFlag : {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
    {
        if (surfaceCaps.supportedCompositeAlpha & alphaFlag)
        {
            compositeAlpha = alphaFlag;
            break;
        }
    }
    VkPresentModeKHR presentMode;
    if (vSync)
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
    else
    {   // Get available present modes
        const std::vector<VkPresentModeKHR> presentModes = physicalDevice->getSurfacePresentModes(surface);
        // Search for the first appropriate present mode
        bool found = false;
        for (auto mode : {
            VK_PRESENT_MODE_IMMEDIATE_KHR,  // AMD
            VK_PRESENT_MODE_MAILBOX_KHR,    // NVidia, Intel
            VK_PRESENT_MODE_FIFO_RELAXED_KHR})
        {
            if (std::find(presentModes.begin(), presentModes.end(), mode)
                != presentModes.end())
            {
                presentMode = mode;
                found = true;
                break;
            }
        }
        if (!found)
        {   // Must always be present
            presentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
    }
    // Get available surface formats
    const std::vector<VkSurfaceFormatKHR> surfaceFormats = physicalDevice->getSurfaceFormats(surface);
    VkSurfaceFormatKHR format = surfaceFormats[0];
    if (sRGB)
    {   // Try to find sRGB format
        for (const auto& sRGBFormat : surfaceFormats)
        {
            if (VK_FORMAT_B8G8R8A8_SRGB == sRGBFormat.format &&
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == sRGBFormat.colorSpace)
            {
                format = sRGBFormat;
                break;
            }
        }
    }
    swapchain = std::make_shared<magma::Swapchain>(device, surface,
        std::min(2U, surfaceCaps.maxImageCount),
        format, surfaceCaps.currentExtent,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // Allow screenshots
        preTransform, compositeAlpha, presentMode, 0,
        nullptr, debugReportCallback);
}

void VulkanApp::createRenderPass()
{
    const std::vector<VkSurfaceFormatKHR> surfaceFormats = physicalDevice->getSurfaceFormats(surface);
    const magma::AttachmentDescription colorAttachment(surfaceFormats[0].format, 1,
        clearOp ? magma::op::clearStore : magma::op::store,
        magma::op::dontCare,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    renderPass = std::make_shared<magma::RenderPass>(device, colorAttachment);
}

void VulkanApp::createFramebuffer()
{
    for (const auto& image : swapchain->getImages())
    {
        std::vector<std::shared_ptr<magma::ImageView>> attachments;
        std::shared_ptr<magma::ImageView> colorView(std::make_shared<magma::ImageView>(image));
        attachments.push_back(colorView);
        std::shared_ptr<magma::Framebuffer> framebuffer(std::make_shared<magma::Framebuffer>(renderPass, attachments));
        framebuffers.push_back(framebuffer);
    }
}

void VulkanApp::createCommandBuffers()
{
    queue = device->getQueue(VK_QUEUE_GRAPHICS_BIT, 0);
    transferQueue = device->getQueue(VK_QUEUE_TRANSFER_BIT, 0);
    commandPools[0] = std::make_shared<magma::CommandPool>(device, queue->getFamilyIndex());
    commandPools[1] = std::make_shared<magma::CommandPool>(device, transferQueue->getFamilyIndex());
    // Create draw command buffers
    commandBuffers = commandPools[0]->allocateCommandBuffers(static_cast<uint32_t>(framebuffers.size()), true);
    // Create image copy command buffer
    cmdCopyImg = std::make_shared<magma::PrimaryCommandBuffer>(commandPools[0]);
    // Create copy command buffer
    cmdCopyBuf = std::make_shared<magma::PrimaryCommandBuffer>(commandPools[1]);
}

void VulkanApp::createSyncPrimitives()
{
    presentFinished = std::make_shared<magma::Semaphore>(device);
    renderFinished = std::make_shared<magma::Semaphore>(device);
    for (std::size_t i = 0; i < commandBuffers.size(); ++i)
    {
        constexpr bool signaled = true; // Don't wait on first render of each command buffer
        waitFences.push_back(std::make_shared<magma::Fence>(device, signaled));
    }
}
