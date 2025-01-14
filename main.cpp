#include <iostream>
#include <vector>

#include <vulkan/vulkan.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_vulkan.h>

typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;
typedef ptrdiff_t isize;
typedef intptr_t  iptr;

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef size_t    usize;
typedef uintptr_t uptr;

typedef float  f32;
typedef double f64;

#define CHECK_RESULT(VR) if ((VR) != VK_SUCCESS) { printf("VULKAN ERROR %d (%x): %s(%d)\n", (VR), (VR), __FILE__, __LINE__); std::exit((VR)); }

// macro to help with Get_X(Args args..., u32 *count, X *array) calling pattern where array must be called with nullptr to retrive the required count value
// allocates space for appends the resulting array to the end of a vector
#define COUNT_APPEND_HELPER(VECTOR, FUNC, ARGS...) ([&]() { u32 count = 0; FUNC(ARGS, &count, nullptr); VECTOR.resize(VECTOR.size() + count); return FUNC(ARGS, &count, VECTOR.data()+(VECTOR.size()-count)); })()
#define COUNT_APPEND_HELPER0(VECTOR, FUNC)         ([&]() { u32 count = 0; FUNC(      &count, nullptr); VECTOR.resize(VECTOR.size() + count); return FUNC(      &count, VECTOR.data()+(VECTOR.size()-count)); })()

// simple macro to safely and easily compare command-line arguments
#define STREQ(STR, EXPR) (strncmp((STR), (EXPR), sizeof(STR)/sizeof(*(STR))) == 0)

// program arguments
bool main_list_supporeted_extensions = false;
bool main_list_physical_devices_info = false;
bool main_prefer_high_performance_device = false; // TODO: unimplemented

int main(i32 argc, char** argv)
{
    //
    // PARSE ARGUMENTS
    //

    for (i32 i = 1 ; i < argc; i++) {
        if      (STREQ("-x", argv[i]) || STREQ("--list-extensions", argv[i])) main_list_supporeted_extensions = true;
        else if (STREQ("-d", argv[i]) || STREQ("--list-devices",    argv[i])) main_list_physical_devices_info = true;
        else if (STREQ("-p", argv[i]) || STREQ("--high-perf",       argv[i])) main_prefer_high_performance_device = true;
        else
        {
            std::cout << "Unkown argument: " << argv[i] << std::endl;
        }
    }
    std::cout << std::endl;

    //
    // SDL INIT
    //

    /*  This also exists, but initialises a tonne of additional subsytems for things like
        controllers which we don't really need:

        SDL_Init(SDL_INIT_EVERYTHING); */

    SDL_InitSubSystem(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "Vulkan Demo",                                          //  Title
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,       //  Pos X / Pos Y
        640, 480,                                               //  Width / height
        SDL_WINDOW_VULKAN                                       //  Flags: May be chained with OR
    );
    if(window == NULL)
    {
        std::cerr << "Error: Failed to create window instance. Check availability of Vulkan drivers." << std::endl;
        return -1;
    }
    bool window_is_open = true;

    SDL_Surface* sdl_surface = SDL_GetWindowSurface(window);

    //
    // VULKAN INIT
    //

    VkResult vr = VK_SUCCESS;

    // Create Vulkan instance
    // This is the highest-level interface of the API that is used to create device interfaces and select required extensions
    VkInstance vk_instance = {};
    {
        // our application info (for the driver)
        VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Vulkan Demo";
        app_info.pEngineName      = "Demo Engine";
        app_info.apiVersion       = VK_API_VERSION_1_0;

        // INSTANCE EXTENSIONS

        if (main_list_supporeted_extensions)
        {
            //  List available instance layers
            std::vector<VkLayerProperties> layers = {};
            vr = COUNT_APPEND_HELPER0(layers, vkEnumerateInstanceLayerProperties);
            CHECK_RESULT(vr);
            std::cout << "Available instance layers:" << std::endl;
            for(const auto &layer : layers)
            {
                std::cout << layer.layerName << std::endl;
            }
            std::cout << std::endl;

            //  List available instance extensions
            std::vector<VkExtensionProperties> extensions = {};
            vr = COUNT_APPEND_HELPER(extensions, vkEnumerateInstanceExtensionProperties, nullptr);
            CHECK_RESULT(vr);
            std::cout << "Available instance extensions:" << std::endl;
            for(const auto &extension : extensions)
            {
                std::cout << extension.extensionName << std::endl;
            }
            std::cout << std::endl;
        }

        // create list of extension requirements
        std::vector<const char*> extensions = {};

        // query SDL extension requirements
        if (!COUNT_APPEND_HELPER(extensions, SDL_Vulkan_GetInstanceExtensions, window))
        {
            std::cerr << SDL_GetError() << std::endl;
            return -1;
        }

        // log extension requirements
        std::cout << "Requiring extensions:" << std::endl;
        for (int i = 0; i < extensions.size(); i++)
        {
            std::cout << extensions[i] << std::endl;
        };
        std::cout << std::endl;

        // INSTANCE LAYERS

        //  Enable API validation layer
        std::vector<const char *> layers = { "VK_LAYER_KHRONOS_validation" };

        // log layer requirements
        std::cout << "Requiring layers:" << std::endl;
        for (int i = 0; i < layers.size(); i++)
        {
            std::cout << layers[i] << std::endl;
        };

        // INSTANCE INFO

        VkInstanceCreateInfo instance_info = {};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo        = &app_info;
        instance_info.ppEnabledLayerNames     = layers.data();
        instance_info.enabledLayerCount       = layers.size();
        instance_info.enabledExtensionCount   = extensions.size();
        instance_info.ppEnabledExtensionNames = extensions.data();

        // create vulkan instance
        std::cout << "Creating instance..." << std::endl;
        vr = vkCreateInstance(&instance_info, NULL, &vk_instance);
        CHECK_RESULT(vr);
    };

    // Create Vulkan surface
    // This functions as a platform-independent abstraction of a graphical window render-target
    VkSurfaceKHR vk_surface = {};
    SDL_Vulkan_CreateSurface(window, vk_instance, &vk_surface);

    // Create device interface and the queue
    // The device is the main API interface for creating and managing GPU resources
    // The queue is responsible for executing workloads on the device
    // Queue creation is tightly coupled with device creation, so both are created here
    // The physical device is retained to query cababilities only; it carries little API functionality
    VkPhysicalDevice vk_physical_device = {};
    VkDevice         vk_device = {};
    VkQueue          vk_queue = {};
    {
        // select physical device and queue family to execute on
        // TOOD: implement selection logic
        u32                     target_physical_device_index = 0;
        VkQueueFamilyProperties target_queue_family_properties = {};
        u32                     target_queue_family_index = 0;

        std::vector<VkPhysicalDevice> physical_devices;
        {
            // query available hardware
            vr = COUNT_APPEND_HELPER(physical_devices, vkEnumeratePhysicalDevices, vk_instance);
            CHECK_RESULT(vr);

            std::cout << std::endl << "Querying [" << physical_devices.size() << "] physical devices:" << std::endl;
            if(physical_devices.size() < 1)
            {
                std::cerr << "No physical devices found" << std::endl;
                std::exit(-1);
            };

            // check hardware capabilities
            std::vector<VkQueueFamilyProperties> queue_families;
            for (u32 physical_device_index = 0; physical_device_index < physical_devices.size(); physical_device_index++)
            {
                // get device
                VkPhysicalDeviceProperties properties = {};
                vkGetPhysicalDeviceProperties(physical_devices[physical_device_index], &properties);

                // check device queue families
                queue_families.clear();
                COUNT_APPEND_HELPER(queue_families, vkGetPhysicalDeviceQueueFamilyProperties, physical_devices[physical_device_index]);

                // save target queue family properties
                // TODO: implement sanity checking and target selection
                if (physical_device_index == target_physical_device_index)
                {
                    target_queue_family_properties = queue_families[target_queue_family_index];
                }

                // log the device name
                std::cout << properties.deviceName << std::endl;
                if (main_list_physical_devices_info)
                {
                    // log device type
                    std::cout << "device type: ";
                    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_OTHER)          std::cout << "OTHER";
                    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   std::cout << "DISCRETE_GPU";
                    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) std::cout << "INTEGRATED_GPU";
                    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)            std::cout << "CPU";
                    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)    std::cout << "VIRTUAL_GPU";
                    std::cout << std::endl;

                    // log physical device queue families
                    std::cout << "queue families: [" << queue_families.size() << "]" << std::endl;
                    for (u32 queue_family_index = 0; queue_family_index < queue_families.size(); queue_family_index++)
                    {
                        VkQueueFamilyProperties family = queue_families[queue_family_index];

                        // log family index and count
                        std::cout << "family " << queue_family_index << ":\tcount:\t" << family.queueCount << "\tflags: | ";
                        // log queue family flags
                        std::cout << (family.queueFlags & VK_QUEUE_GRAPHICS_BIT         ? "GRAPHICS | "       : "         | ");
                        std::cout << (family.queueFlags & VK_QUEUE_COMPUTE_BIT          ? "COMPUTE | "        : "        | ");
                        std::cout << (family.queueFlags & VK_QUEUE_TRANSFER_BIT         ? "TRANSFER | "       : "         | ");
                        std::cout << (family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT   ? "SPARSE_BINDING | " : "               | ");
                        std::cout << (family.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR ? "DECODE | "         : "       | ");
                        std::cout << (family.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR ? "ENCODE | "         : "       | ");
                        std::cout << (family.queueFlags & VK_QUEUE_PROTECTED_BIT        ? "PROTECTED | "      : "          | ");
                        // log image transfer granularity
                        std::cout << " transfer granularity: (" << family.minImageTransferGranularity.width << ", " << family.minImageTransferGranularity.height << ", " << family.minImageTransferGranularity.depth << ")";
                        std::cout << std::endl;
                    }
                    std::cout << std::endl;
                }
            }
        }
        // Select physical device / GPU hardware
        vk_physical_device = physical_devices[target_physical_device_index];
        // sanity check queue family properties to ensure capabilities
        if (
            !(target_queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) ||
            !(target_queue_family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT)
        ) {
            std::cerr << "Target queue family does not support graphics and transfer" << std::endl;
            return -1;
        }

        // Configure queue
        // this demo will only require a single queue with graphics and transfer capabilities
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = target_queue_family_index;
        f32 queue_priority = 1.0;
        queue_info.pQueuePriorities = &queue_priority;
        queue_info.queueCount       = 1;

        // Configure logical device - is used to interface with physical device
        std::vector<const char *> extension_names = {};
        extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        VkDeviceCreateInfo device_info = {};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.ppEnabledExtensionNames = extension_names.data();
        device_info.enabledExtensionCount = extension_names.size();
        device_info.pQueueCreateInfos    = &queue_info;
        device_info.queueCreateInfoCount = 1;

        // Create logical device
        std::cout << "Creating device..." << std::endl;
        vr = vkCreateDevice(vk_physical_device, &device_info, NULL, &vk_device);
        CHECK_RESULT(vr);

        if (main_list_supporeted_extensions)
        {
            std::vector<VkExtensionProperties> extensions = {};
            vr = COUNT_APPEND_HELPER(extensions, vkEnumerateDeviceExtensionProperties, vk_physical_device, nullptr);
            CHECK_RESULT(vr);

            //  Log extension names
            std::cout << std::endl << "Available device extensions:" << std::endl;
            for(const auto&extension_property : extensions)
            {
                std::cout << extension_property.extensionName << std::endl;
            };
        }

        // Retrieve the queue
        std::cout << "Retrieving device queue..." << std::endl;
        // All queue families have at least 1 queue, so queue index 0 can be safely retrieved
        vkGetDeviceQueue(vk_device, target_queue_family_index, 0, &vk_queue);
    };

    //  Swapchain creation
    //  This application assumes that there will only ever be one swapchain because things like screen resize don't occur during runtime
    VkSwapchainKHR vk_swapchain = {};
    {
        //  Query available hardware extensions and surface properties
        VkSurfaceCapabilitiesKHR surface_capabilities = {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &surface_capabilities);

        //  Define swapchain interface
        VkSwapchainCreateInfoKHR swapchain_info = {};
        swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchain_info.imageUsage = VK_IMAGE_USAGE_SAMPLED_BIT;
        swapchain_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.compositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchain_info.minImageCount = 3;
        swapchain_info.imageArrayLayers = 1;
        swapchain_info.surface = vk_surface;
        swapchain_info.imageExtent = surface_capabilities.currentExtent;
        swapchain_info.clipped = VK_TRUE;

        std::cout << "Creating swapchain..." << std::endl;
        vr = vkCreateSwapchainKHR(vk_device, &swapchain_info, nullptr, &vk_swapchain);
        CHECK_RESULT(vr);
    };

    //
    // MAIN LOOP
    //

    while(window_is_open)
    {
        SDL_Event event;
        int queue_state = SDL_PollEvent(&event);

        if(queue_state > 0)
        {
            if(event.type == SDL_QUIT)
            {
                window_is_open = false;
                break;
            }

            int status = SDL_UpdateWindowSurface(window); //   Render any changes to the surface to the screen (swap front/back buffers)

            if(status != 0)
            {
                const char *message = SDL_GetError();
                std::cerr << "Error: " << message << std::endl;
            };
        };
    };

    //
    // CLEANUP
    //

    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
    vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
    vkDestroyDevice(vk_device, NULL);
    vkDestroyInstance(vk_instance, NULL);
    SDL_DestroyWindow(window);

    return 0;
};
