#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>

int main()
{
    //  Application definition
    vk::ApplicationInfo vk_app_info = {};
    vk_app_info.pApplicationName = "Vulkan Demo";
    vk_app_info.pEngineName = "Lazarus Classified";
    vk_app_info.apiVersion = VK_API_VERSION_1_4;

    //  Query available instance layers
    std::cout << "\n" << "Instance Layers:" << "\n" << std::endl;
    std::vector<const char *> instance_layers = {};
    std::vector<vk::LayerProperties> available_layer_properties = vk::enumerateInstanceLayerProperties();
    for(const auto &instance_layer : available_layer_properties)
    {
        //  Cull out layer versions made for older versions of the spec
        //  This caused big problems for me on debian which caused device creation
        //  to segfault. Was the programmatic equivelent to doing VK_LOADER_LAYERS_DISABLE=layername.
        //  May cause problems later but seemed like the most universal way to solve the issue.
        if(instance_layer.specVersion >= VK_API_VERSION_1_2)
        {
            instance_layers.push_back(instance_layer.layerName);
            std::cout << instance_layer.layerName << std::endl;
        }
        else
        {
            std::cout << "Excluding: " << instance_layer.layerName << std::endl;
        }
    }
    instance_layers.push_back("VK_LAYER_KHRONOS_validation");

    std::vector<const char *> instance_extensions = {};

    //  Determine which extensions MUST be present in the instance
    uint32_t required_extension_count = 0;
    auto required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);

    //  Query availability of required extensions
    std::vector<vk::ExtensionProperties> available_instance_extensions = vk::enumerateInstanceExtensionProperties();

    for(size_t j = 0; j < required_extension_count; j++)
    {
        const auto &required_extension = required_extensions[j];

        auto required_available = std::find_if(
            available_instance_extensions.begin(), 
            available_instance_extensions.end(),
            [required_extension](const VkExtensionProperties &available_extension) {
                return available_extension.extensionName == required_extension;
            }
        );

        if(required_available == available_instance_extensions.end())
        {
            printf("GLFW ERROR Required extension [%s] unnavailable: %s(%d)\n", required_extension, __FILE__, __LINE__);
            std::exit(0);
        }
    }

    // Required extensions are present, so add all that are available to instance extension list
    for(size_t i = 0; i < available_instance_extensions.size(); ++i)
    {
        const VkExtensionProperties &available_extension = available_instance_extensions[i];
        instance_extensions.push_back(available_extension.extensionName);
        std::cout << available_extension.extensionName << std::endl;
    }

    //  Vulkan instance creation
    vk::InstanceCreateInfo instance_info = {};
    instance_info.pApplicationInfo = &vk_app_info;
    instance_info.enabledLayerCount = instance_layers.size();
    instance_info.ppEnabledLayerNames = instance_layers.data();
    instance_info.enabledExtensionCount = instance_extensions.size();
    instance_info.ppEnabledExtensionNames = instance_extensions.data();
    
    /**
     * NOTE:
     * The vulkan cpp api included from vulkain_raii.hpp throws its own exceptions
     * as opposed to returning a VkResult like the C api does
     */
    vk::Instance vk_instance = {};
    try
    {
        vk_instance = vk::createInstance(instance_info);
    }
    catch(const vk::Error& e)
    {
        printf("VULKAN ERROR %d: %s(%d)\n", e.what(), __FILE__, __LINE__);
        std::exit(0);
    }
    
    //  Initialise GLFW
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
    glfwInit();

    //  Check if current glfw version supports vulkan
    if(!glfwVulkanSupported())
    {
        const char *error = "";
        glfwGetError(&error);

        printf("GLFW ERROR %s: %s(%d)\n", error, __FILE__, __LINE__);
        std::exit(0);
    }

    //  Create + configure window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan Demo", NULL, NULL);
    glfwSetWindowCloseCallback(
        window, 
        [](GLFWwindow *win) {
            glfwSetWindowShouldClose(win, GLFW_TRUE);
            return;
        }
    );

    //  Surface creation
    // VkSurfaceKHR vk_surface = {};
    vk::SurfaceKHR vk_surface = {};
    VkResult surface_creation = glfwCreateWindowSurface(vk_instance, window, NULL, (VkSurfaceKHR *)(&vk_surface));
    if(surface_creation != VK_SUCCESS)
    {
        printf("VULKAN ERROR %d: %s(%d)\n", surface_creation, __FILE__, __LINE__);
        std::exit(surface_creation);
    }

    // //  Query physical hardware devices
    std::cout << "\n" << "Physical Devices:" << "\n" << std::endl;
    bool vk_appropriate_device_found = false;
    uint32_t vk_queue_family_index = 0;
    uint32_t vk_physical_device_index = 0;
    auto available_physical_devices = vk_instance.enumeratePhysicalDevices();

    //  Inspect the properties of hardware devices
    // for(auto &physical_device_handle : available_physical_devices)
    for(size_t i = 0; i < available_physical_devices.size(); i++)
    {
        vk::PhysicalDevice &physical_device_handle = available_physical_devices[i];
        std::cout << "- \n Checking device properties" << std::endl;
        vk::PhysicalDeviceProperties physical_device_properties = physical_device_handle.getProperties();
        std::cout << "Name: " << physical_device_properties.deviceName << std::endl;
        std::cout << "Type: " << &physical_device_properties.deviceType << std::endl;

        vk::PhysicalDeviceFeatures physical_device_features = physical_device_handle.getFeatures();

        //  Determine whether the device has rendering capabilities 
        //  (NOTE: others like VIRTUAL_GPU or even CPU are appropriate, but not here)
        if( physical_device_features.geometryShader &&
            physical_device_properties.apiVersion >= VK_API_VERSION_1_4                     &&
            (physical_device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ||
            physical_device_properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu))
            {
            std::cout << "Rendering hardware found" << std::endl;
            //  Determine whether the device has the appropriate command-queue functionality 
            //  Should have surface support
            //  Should have atleast GRAPHICS and TRANSFER bits
            //  Should have FIFO presentation mode
            std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device_handle.getQueueFamilyProperties();
            for(size_t j = 0; j < queue_family_properties.size(); j++)
            {
                /**
                 * TODO: 
                 * Store device properties
                 */
                //  Determine physical devices's surface capabilities
                vk::Bool32 surface_support = physical_device_handle.getSurfaceSupportKHR(j, vk_surface);
                if(surface_support)
                {
                    std::cout << "Device has surface support:" << std::endl;
                    
                    //  Determine queue capabilities
                    //  Note: Could also check for eCompute
                    const auto &family_properties = queue_family_properties[j];
                    if( (family_properties.queueFlags & vk::QueueFlagBits::eGraphics) &&
                        (family_properties.queueFlags & vk::QueueFlagBits::eTransfer))
                    {
                        std::cout << "Device has appropriate graphics and transfer queue flags: " << &family_properties.queueFlags << std::endl;

                        //  Determine presentation capabilities
                        std::vector<vk::PresentModeKHR> present_modes = physical_device_handle.getSurfacePresentModesKHR(vk_surface);
                        for(const auto &present_mode : present_modes)
                            if(present_mode == vk::PresentModeKHR::eFifo)
                            {
                                std::cout << "Device has FIFO presentation mode" << std::endl;
                                vk_appropriate_device_found = true;
                                vk_physical_device_index = i;
                                vk_queue_family_index = j;
                            }
                    }
                }
            };
        }
    }

    if(!vk_appropriate_device_found)
    {
        printf("ERROR Failed to locate appropriate rendering hardware: %s(%d)\n", __FILE__, __LINE__);
        std::exit(0);
    }
    else
    {
        std::cout << "Selected device: " << vk_physical_device_index << std::endl;
    }
    vk::PhysicalDevice vk_physical_device = available_physical_devices[vk_physical_device_index];
    
    //  Configure a queue to create on the device
    const float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo graphics_queue_info = {};
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priority;
    graphics_queue_info.queueFamilyIndex = vk_queue_family_index;

    //  Extensions and features
    std::vector<const char *> extension_names = {vk::KHRSwapchainExtensionName};

    vk::PhysicalDeviceFeatures vk_physical_device_features = {};

    //  Structure chain used to expose the vulkan features being used (i.e. vulkan 1_0 - 1_1 - 1_3)
    vk::PhysicalDeviceVulkan13Features vk_13_features = {};
    vk_13_features.dynamicRendering = VK_TRUE;
    vk::PhysicalDeviceVulkan11Features vk_11_features = {};
    vk_11_features.shaderDrawParameters = VK_TRUE;
    vk_11_features.pNext = &vk_13_features;

    // Configure logical device - is used to interface with physical device
    vk::Device vk_logical_device = {};
    vk::DeviceCreateInfo logical_device_info = {};
    logical_device_info.queueCreateInfoCount = 1;
    logical_device_info.pQueueCreateInfos = &graphics_queue_info;
    logical_device_info.enabledExtensionCount = extension_names.size();
    logical_device_info.ppEnabledExtensionNames = extension_names.data();
    logical_device_info.pNext = &vk_11_features;

    try
    {
        vk_logical_device = vk_physical_device.createDevice(logical_device_info);
    }
    catch(const vk::Error& e)
    {
        printf("VULKAN ERROR: %s: %s(%d)\n", e.what(), __FILE__, __LINE__);
        std::exit(0);
    }

    //  Retrieve queue handle, only a single queue from the family is created hence index 0
    vk::Queue vk_graphics_queue_handle = vk_logical_device.getQueue(vk_queue_family_index, 0);
    
    //  Query additional surface capabilities / support for swapchain creation
    vk::SurfaceCapabilitiesKHR surface_capabilities = vk_physical_device.getSurfaceCapabilitiesKHR(vk_surface);
    std::vector<vk::SurfaceFormatKHR> surface_supported_image_formats = vk_physical_device.getSurfaceFormatsKHR(vk_surface);
    std::vector<vk::PresentModeKHR> surface_supported_present_modes = vk_physical_device.getSurfacePresentModesKHR(vk_surface);
    if((surface_supported_image_formats.empty()) || (surface_supported_present_modes.empty()))
    {
        printf("VULKAN ERROR Surface failed to meet swapchain criteria: %s(%d)\n", __FILE__, __LINE__);
        std::exit(0);
    }

    auto fifo_present_mode = std::find_if(
        surface_supported_present_modes.begin(), 
        surface_supported_present_modes.end(),
        [](vk::PresentModeKHR present_mode){
            return present_mode == vk::PresentModeKHR::eFifo;
        }
    );

    if(fifo_present_mode == surface_supported_present_modes.end())
    {
        printf("VULKAN ERROR Surface presentation mode failed to meet swapchain criteria: %s(%d)\n", __FILE__, __LINE__);
        std::exit(0);
    }

    auto nonlinear_bgra_image_format = std::find_if(
        surface_supported_image_formats.begin(),
        surface_supported_image_formats.end(),
        [](vk::SurfaceFormatKHR surface_format){
            return surface_format.format == vk::Format::eB8G8R8A8Srgb && surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        }
    );

    if(nonlinear_bgra_image_format == surface_supported_image_formats.end())
    {
        printf("VULKAN ERROR Surface image format failed to meet swapchain criteria: %s(%d)\n", __FILE__, __LINE__);
        std::exit(0);
    }
    //  Swapchain creation
    vk::SwapchainCreateInfoKHR swapchain_info = {};
    swapchain_info.surface = vk_surface;
    swapchain_info.minImageCount = surface_capabilities.minImageCount + 1;
    swapchain_info.imageFormat = vk::Format::eB8G8R8A8Srgb;
    swapchain_info.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    swapchain_info.imageExtent = surface_capabilities.currentExtent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapchain_info.imageSharingMode = vk::SharingMode::eExclusive;
    swapchain_info.preTransform = surface_capabilities.currentTransform;
    swapchain_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapchain_info.presentMode = vk::PresentModeKHR::eFifo;
    swapchain_info.clipped = true;

    vk::SwapchainKHR vk_swapchain = {};
    std::vector<vk::Image> vk_swapchain_images = {};
    
    try
    {
        vk_swapchain = vk_logical_device.createSwapchainKHR(swapchain_info);
        vk_swapchain_images = vk_logical_device.getSwapchainImagesKHR(vk_swapchain);
    }
    catch(const vk::Error& e)
    {
        printf("VULKAN ERROR %d: %s(%d)\n", e.what(), __FILE__, __LINE__);
        std::exit(0);
    }

    //  Generate image views used for interfacing with each of the swapchain's vk_images
    std::vector<vk::ImageView> vk_swapchain_image_views = {};
    try
    {
        for(vk::Image &image : vk_swapchain_images)
        {
            vk::ImageView image_view = {};
            vk::ImageViewCreateInfo image_view_info = {};
            image_view_info.image = image;
            image_view_info.viewType = vk::ImageViewType::e2D;  //  I.e. the monitor surface
            image_view_info.format = vk::Format::eB8G8R8A8Srgb;
            image_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            image_view_info.subresourceRange.levelCount = 1;
            image_view_info.subresourceRange.layerCount = 1;
            image_view_info.components = {vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};
            image_view = vk_logical_device.createImageView(image_view_info);
            vk_swapchain_image_views.push_back(image_view);
        }
    }
    catch(const vk::Error& e)
    {
        printf("VULKAN ERROR %d: %s(%d)\n", e.what(), __FILE__, __LINE__);
        std::exit(0);
    }

    //  Open window / start rendering
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    while(1)
    {
        glfwSwapBuffers(window);                        //  Present backbuffer
    }
    return 1;
}