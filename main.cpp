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
    vk_app_info.apiVersion = VK_API_VERSION_1_3;

    //  Query available instance layers
    std::cout << "\n" << "Instance Layers:" << "\n" << std::endl;
    std::vector<const char *> instance_layers = {};
    std::vector<vk::LayerProperties> available_layer_properties = vk::enumerateInstanceLayerProperties();
    for(const auto &instance_layer : available_layer_properties)
    {
        instance_layers.push_back(instance_layer.layerName);
            std::cout << instance_layer.layerName << std::endl;
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
    vk::Instance vk_instance;
    try
    {
        vk_instance = vk::createInstance(instance_info);
    }
    catch(vk::Error& e)
    {
        printf("VULKAN ERROR %d: %s(%d)\n", e.what(), __FILE__, __LINE__);
        std::exit(0);
    }
    

    //  Check if current glfw version supports vulkan
    uint32_t window_vk_support = glfwVulkanSupported();
    if(window_vk_support != GLFW_NO_ERROR)
    {
        const char *error = "";
        glfwGetError(&error);

        printf("GLFW ERROR %d %s: %s(%d)\n", window_vk_support, error, __FILE__, __LINE__);
        std::exit(window_vk_support);
    }

    //  Initialise GLFW
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
    glfwInit();

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
    vk::PhysicalDevice vk_physical_device = {};
    auto available_physical_devices = vk_instance.enumeratePhysicalDevices();

    //  Inspect the properties of hardware devices
    for(auto &physical_device_handle : available_physical_devices)
    {
        std::cout << "- \n Checking device properties" << std::endl;
        vk::PhysicalDeviceProperties physical_device_properties = physical_device_handle.getProperties();
        std::cout << "Name: " << physical_device_properties.deviceName << std::endl;
        std::cout << "Type: " << &physical_device_properties.deviceType << std::endl;

        vk::PhysicalDeviceFeatures physical_device_features = physical_device_handle.getFeatures();

        //  Determine whether the device has rendering capabilities 
        //  (NOTE: others like VIRTUAL_GPU or even CPU are appropriate, but not here)
        if( physical_device_features.geometryShader &&
            physical_device_properties.apiVersion >= VK_API_VERSION_1_3                     &&
            physical_device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ||
            physical_device_properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
            {
            std::cout << "Rendering hardware found" << std::endl;
            //  Determine whether the device has the appropriate command-queue functionality 
            //  Should have surface support
            //  Should have atleast GRAPHICS and TRANSFER bits
            //  Should have FIFO presentation mode
            std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device_handle.getQueueFamilyProperties();
            for(size_t i = 0; i < queue_family_properties.size(); i++)
            {
                /**
                 * TODO: 
                 * Store device properties
                 */
                //  Determine physical devices's surface capabilities
                vk::Bool32 surface_support = physical_device_handle.getSurfaceSupportKHR(i, vk_surface);
                if(surface_support)
                {
                    std::cout << "Device has surface support:" << std::endl;
                    
                    //  Determine queue capabilities
                    //  Note: Could also check for eCompute
                    const auto &family_properties = queue_family_properties[i];
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
                                vk_physical_device = physical_device_handle;
                                vk_appropriate_device_found = true;
                                vk_queue_family_index = i;
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
        std::cout << "Selected device: " << vk_physical_device << std::endl;
    }
    
    //  Configure a queue to create on the device
    const float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo graphics_queue_info = {};
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priority;
    graphics_queue_info.queueFamilyIndex = vk_queue_family_index;

    //  Extensions and features
    std::vector<const char *> extension_names = {};
    extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

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
    // logical_device_info.enabledLayerCount = 0;
    // logical_device_info.ppEnabledLayerNames = nullptr;
    // logical_device_info.pEnabledFeatures = nullptr;
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
    
    
    //  Open window / start rendering
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    while(1)
    {
        glfwSwapBuffers(window);                        //  Present backbuffer
    }
    return 1;
}