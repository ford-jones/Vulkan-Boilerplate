#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <algorithm>

int main()
{
    //  Application definition
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Vulkan Demo";
    app_info.pEngineName = "Lazarus Classified";
    app_info.apiVersion = VK_API_VERSION_1_3;

    //  Query available instance layers
    std::cout << "\n" << "Instance Layers:" << "\n" << std::endl;
    std::vector<const char *> instance_layers = {};
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_instance_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_instance_layers.data());
    // instance_layers.push_back("KHR_LAYER_KHRONOS_validation");      //  Enable API validation layer
    for(const auto &instance_layer : available_instance_layers)
    {
        instance_layers.push_back(instance_layer.layerName);
        std::cout << instance_layer.layerName << std::endl;
    }

    std::vector<const char *> instance_extensions = {};

    //  Determine which extensions MUST be present in the instance
    uint32_t required_extension_count = 0;
    auto required_extensions = glfwGetRequiredInstanceExtensions(&required_extension_count);

    //  Query availability of required extensions
    std::cout << "\n" << "Instance Extensions:" << "\n" << std::endl;
    uint32_t available_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count, nullptr);
    std::vector<VkExtensionProperties> available_instance_extensions(available_extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &available_extension_count, available_instance_extensions.data());

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
    for(size_t i = 0; i < available_extension_count; ++i)
    {
        const VkExtensionProperties &available_extension = available_instance_extensions[i];
        instance_extensions.push_back(available_extension.extensionName);
        std::cout << available_extension.extensionName << std::endl;
    }

    //  Vulkan instance creation
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = instance_layers.size();
    instance_info.ppEnabledLayerNames = instance_layers.data();
    instance_info.enabledExtensionCount = instance_extensions.size();
    instance_info.ppEnabledExtensionNames = instance_extensions.data();

    VkInstance vk_instance = {};
    VkResult instance_creation = vkCreateInstance(&instance_info, NULL, &vk_instance);
    if(instance_creation != VK_SUCCESS)
    {
        printf("VULKAN ERROR %d: %s(%d)\n", instance_creation, __FILE__, __LINE__);
        std::exit(instance_creation);
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
    VkSurfaceKHR vk_surface = {};
    VkResult surface_creation = glfwCreateWindowSurface(vk_instance, window, NULL, &vk_surface);
    if(surface_creation != VK_SUCCESS)
    {
        printf("VULKAN ERROR %d: %s(%d)\n", surface_creation, __FILE__, __LINE__);
        std::exit(surface_creation);
    }

    //  Query physical hardware devices
    std::cout << "\n" << "Physical Devices:" << "\n" << std::endl;
    bool vk_appropriate_device_found = false;
    uint32_t vk_queue_family_index = 0;
    VkPhysicalDevice vk_physical_device = {};
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, nullptr);
    std::vector<VkPhysicalDevice> available_physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, available_physical_devices.data());
    //  Inspect the properties of hardware devices
    for(auto &physical_device_handle : available_physical_devices)
    {
        std::cout << "- \n Checking device properties" << std::endl;
        VkPhysicalDeviceProperties physical_device_properties = {};
        vkGetPhysicalDeviceProperties(physical_device_handle, &physical_device_properties);
        std::cout << "Name: " << physical_device_properties.deviceName << std::endl;
        std::cout << "Type: " << physical_device_properties.deviceType << std::endl;

        VkPhysicalDeviceFeatures physical_device_features = {};
        vkGetPhysicalDeviceFeatures(vk_physical_device, &physical_device_features);

        //  Determine whether the device has rendering capabilities 
        //  (NOTE: others like VIRTUAL_GPU or even CPU are appropriate, but not here)
        if( physical_device_features.geometryShader &&
            physical_device_properties.apiVersion >= VK_API_VERSION_1_3                     &&
            physical_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   || 
            physical_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU )
            {
            std::cout << "Rendering hardware found" << std::endl;
            //  Determine whether the device has the appropriate command-queue functionality 
            //  Should have surface support
            //  Should have atleast GRAPHICS and TRANSFER bits
            //  Should have FIFO presentation mode
            uint32_t queue_family_property_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device_handle, &queue_family_property_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_property_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device_handle, &queue_family_property_count, queue_family_properties.data());
            for(size_t i = 0; i < queue_family_property_count; i++)
            {
                /**
                 * TODO: 
                 * Store device properties
                 */
                //  Determine physical devices's surface capabilities
                VkBool32 surface_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_handle, i, vk_surface, &surface_support);
                if(surface_support)
                {
                    std::cout << "Device has surface support:" << std::endl;
                    
                    //  Determine queue capabilities
                    //  Note: Could also check for COMPUTE
                    const auto &family_properties = queue_family_properties[i];
                    if( (family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                        (family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT))
                    {
                        std::cout << "Device has appropriate graphics and transfer queue flags: " << family_properties.queueFlags << std::endl;

                        //  Determine presentation capabilities
                        uint32_t present_mode_count = 0;
                        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_handle, vk_surface, &present_mode_count, nullptr);
                        std::vector<VkPresentModeKHR> present_modes(present_mode_count);
                        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_handle, vk_surface, &present_mode_count, present_modes.data());
                        for(const auto &present_mode : present_modes)
                            if(present_mode == VK_PRESENT_MODE_FIFO_KHR)
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
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo graphics_queue_info = {};
    graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priority;
    graphics_queue_info.queueFamilyIndex = vk_queue_family_index;

    //  Extensions and features
    std::vector<const char *> extension_names = {};
    extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    VkPhysicalDeviceFeatures vk_physical_device_features = {};

    //  Structure chain used to expose the vulkan features being used (i.e. vulkan 1_3)

    /**
     * TODO: 
     * Set this up properly; use those that are allowed by the physical device
     * Also see: https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/04_Logical_device_and_queues.html#_enabling_additional_device_features
     */
    // Configure logical device - is used to interface with physical device
    VkDevice vk_logical_device = {};
    VkDeviceCreateInfo logical_device_info = {};
    logical_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logical_device_info.pQueueCreateInfos = &graphics_queue_info;
    logical_device_info.queueCreateInfoCount = 1;
    logical_device_info.enabledExtensionCount = extension_names.size();
    logical_device_info.ppEnabledExtensionNames = extension_names.data();

    //  Open window / start rendering
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    while(1)
    {
        glfwSwapBuffers(window);                        //  Present backbuffer
    }
    return 1;
}