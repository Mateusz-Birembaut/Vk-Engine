#include "VkEngine/VulkanCtx.h"

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unistd.h>
#include <VkBootstrap.h>

#include "VkEngine/VkEngineInfo.h"
#include "VkEngine/Window.h"

#ifdef VKENGINE_VALIDATION
constexpr bool useValidation = true;
#else
constexpr bool useValidation = false;
#endif

VulkanCtx::~VulkanCtx()
{
    // never initialized, nothing was created (everything derives from the instance)
    if (!m_instance)
        return;

    cleanupSwapchain();
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkb::destroy_debug_utils_messenger(m_instance, m_debugMessenger);
    vkDestroyInstance(m_instance, nullptr);
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*userData*/
)
{
    // colors disabled when stderr is redirected to a file
    static const bool useColor = isatty(fileno(stderr));

    const char* color = "";
    const char* label = "INFO";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        color = "\033[1;31m"; // bold red
        label = "ERROR";
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        color = "\033[1;33m"; // bold yellow
        label = "WARNING";
    } else {
        color = "\033[90m"; // grey
    }
    const char* dim = "\033[90m";
    const char* reset = "\033[0m";
    if (!useColor)
        color = dim = reset = "";

    const char* tag = "GENERAL";
    if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        tag = "VALIDATION";
    else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        tag = "PERF"; // best-practices warnings land here

    // pMessageIdName holds the short VUID, no need to dig it out of the message
    std::string_view vuid = data->pMessageIdName ? data->pMessageIdName : "-";

    // drop the spec quote + URL, the VUID is enough to look it up
    std::string_view msg = data->pMessage ? data->pMessage : "";
    if (size_t specPos = msg.find("The Vulkan spec states"); specPos != std::string_view::npos)
        msg = msg.substr(0, specPos);
    while (msg.ends_with(' ') || msg.ends_with('\n'))
        msg.remove_suffix(1);

    std::cerr << color << "[" << label << "|" << tag << "] " << vuid << reset << '\n' << msg << '\n';

    // objects named via vkSetDebugUtilsObjectNameEXT show up here
    for (uint32_t i = 0; i < data->objectCount; ++i) {
        const auto& obj = data->pObjects[i];
        std::cerr << dim << "    object " << i << ": " << (obj.pObjectName ? obj.pObjectName : "?") << " (0x"
                  << std::hex << obj.objectHandle << std::dec << ")" << reset << '\n';
    }
    std::cerr << '\n';

    return VK_FALSE; // VK_TRUE would abort the offending Vulkan call
}

void VulkanCtx::init(Window* pWindow)
{
    m_window = pWindow;

    // VkInstance
    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name(app::NAME.data())
                        .request_validation_layers(useValidation)
                        .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
                        //.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT)
                        .set_debug_callback(debugCallback)
                        .require_api_version(app::VK_VERSION_MAJOR, app::VK_VERSION_MINOR, app::VK_VERSION_PATCH)
                        .build();
    if (!inst_ret) {
        throw std::runtime_error("[VulkanCtx] Failed to create Vulkan instance. Error: " + inst_ret.error().message());
    }
    m_instance = inst_ret.value().instance;
    m_debugMessenger = inst_ret.value().debug_messenger;

    m_surface = m_window->createSurface(m_instance);

    // PhysicalDevice, dynamic rendering feature, sync 2, scalar block layout
    constexpr VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .scalarBlockLayout = VK_TRUE,
    };

    constexpr VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    constexpr VkPhysicalDeviceVulkan14Features features14{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .maintenance5 = VK_TRUE,
    };

    vkb::PhysicalDeviceSelector selector{inst_ret.value()};
    auto phys_ret = selector.set_surface(m_surface)
                        .set_minimum_version(app::VK_VERSION_MAJOR, app::VK_VERSION_MINOR)
                        .set_required_features_12(features12)
                        .set_required_features_13(features13)
                        .set_required_features_14(features14)
                        .select();
    if (!phys_ret) {
        throw std::runtime_error(
            "[VulkanCtx] Failed to select Vulkan Physical Device. Error: " + phys_ret.error().message()
        );
    }
    m_gpu = phys_ret.value().physical_device;

    // Device
    vkb::DeviceBuilder device_builder{phys_ret.value()};
    auto dev_ret = device_builder.build();
    if (!dev_ret) {
        throw std::runtime_error("[VulkanCtx] Failed to create Vulkan device. Error: " + dev_ret.error().message());
    }
    vkb::Device& vkbDevice = dev_ret.value();
    m_device = vkbDevice.device;

    // GraphicsQueue
    auto graphics_queue_ret = vkbDevice.get_queue(vkb::QueueType::graphics);
    if (!graphics_queue_ret) {
        throw std::runtime_error(
            "[VulkanCtx] Failed to get graphics queue. Error: " + graphics_queue_ret.error().message()
        );
    }
    m_graphicsQueue = graphics_queue_ret.value();
    m_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // Swapchain
    createSwapchain();
}

void VulkanCtx::createSwapchain()
{
    VkExtent2D extent;
    if (m_window)
        extent = m_window->getSizeInPixels();
    else
        throw std::runtime_error("[VulkanCtx] Failed to get window extent");

    vkb::SwapchainBuilder swapchainBuilder{m_gpu, m_device, m_surface};

    auto swap_ret =
        swapchainBuilder.set_old_swapchain(m_swapchain)
            .set_desired_format(
                VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
            )
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(extent.width, extent.height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build();

    if (!swap_ret) {
        throw std::runtime_error("[VulkanCtx] Failed to create swapchain : " + swap_ret.error().message());
    } else {
        cleanupSwapchain();

        vkb::Swapchain& vkbSwap = swap_ret.value();

        m_swapchain = vkbSwap.swapchain;
        m_swapchainExtent = vkbSwap.extent;
        m_swapchainImgFormat = vkbSwap.image_format;

        auto imgs_ret = vkbSwap.get_images();
        if (!imgs_ret)
            throw std::runtime_error("[VulkanCtx] Failed to create images : " + imgs_ret.error().message());
        m_swapchainImgs = imgs_ret.value();

        auto views_ret = vkbSwap.get_image_views();
        if (!views_ret)
            throw std::runtime_error("[VulkanCtx] Failed to create image views: " + views_ret.error().message());
        m_swapchainImgViews = views_ret.value();
    }
}

void VulkanCtx::cleanupSwapchain()
{
    if (m_swapchain) {
        for (VkImageView view : m_swapchainImgViews)
            vkDestroyImageView(m_device, view, nullptr);

        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchainImgViews.clear();
        m_swapchainImgs.clear();
        m_swapchain = VK_NULL_HANDLE;
    }
}