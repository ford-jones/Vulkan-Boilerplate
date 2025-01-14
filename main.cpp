#include <iostream>

#include <vulkan/vulkan.h>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_vulkan.h>

#define CHECK_RESULT(VR) if ((VR) != VK_SUCCESS) { printf("VULKAN ERROR %d (%x): %s(%d)\n", (VR), (VR), __FILE__, __LINE__); std::exit((VR));}

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

#define GPU_TYPE_PREFERENCE VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU

int main()
{
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

        // instance configuratuion
        VkInstanceCreateInfo instance_info = {};

        // basic config
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pApplicationInfo = &app_info;

        // create list of extension requirements
        std::vector<char*> extensions;

        // query SDL extension requirements
        u32 sdl_n_extensions = 0;
        SDL_Vulkan_GetInstanceExtensions(window, &sdl_n_extensions, nullptr);
        extensions.resize(sdl_n_extensions);
        SDL_Vulkan_GetInstanceExtensions(window, &sdl_n_extensions, (const char**) extensions.data());

        instance_info.enabledExtensionCount   = extensions.size();
        instance_info.ppEnabledExtensionNames = extensions.data();

        // log extension requirements
        std::cout << "Requiring extensions:" << std::endl;
        for (int i = 0; i < extensions.size(); i++) {
            std::cout << extensions[i] << std::endl;
        };

        // create vulkan instance
        vr = vkCreateInstance(&instance_info, NULL, &vk_instance);
        CHECK_RESULT(vr);
    };

    // Create Vulkan surface
    // This functions as a platform-independent abstraction of a graphical window render-target
    VkSurfaceKHR vk_surface = {};
    SDL_Vulkan_CreateSurface(window, vk_instance, &vk_surface);

    // Create device interface
    // This is the main API interface for creating and managing GPU resources
    VkPhysicalDevice           vk_physical_device = {};
    VkDevice                   vk_device = {};
    {
        // select physical device and queue family to execute on
        // TOOD: implement selection logic
        u32                     target_physical_device_index = 0;
        VkQueueFamilyProperties target_queue_family_properties = {};
        u32                     target_queue_family_index = 0;

        std::vector<VkPhysicalDevice> physical_devices;
        {
            // query available hardware
            uint32_t physical_device_count = 0;
            vr = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, nullptr);
            std::cout << "Querying [" << physical_device_count << "] physical devices:" << std::endl;
            if(physical_device_count < 1)
            {
                std::cerr << "No physical devices found" << std::endl;
                std::exit(-1);
            };

            physical_devices.resize(physical_device_count);
            vr = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count, physical_devices.data());
            CHECK_RESULT(vr);

            // check hardware capabilities
            std::vector<VkQueueFamilyProperties> queue_families;
            for (u32 physical_device_index = 0; physical_device_index < physical_device_count; physical_device_index++)
            {
                // log the device name
                VkPhysicalDeviceProperties properties = {};
                vkGetPhysicalDeviceProperties(physical_devices[physical_device_index], &properties);
                std::cout << std::endl << properties.deviceName << std::endl;

                // log device type
                std::cout << "device type: ";
                if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_OTHER)          std::cout << "OTHER";
                if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   std::cout << "DISCRETE_GPU";
                if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) std::cout << "INTEGRATED_GPU";
                if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)            std::cout << "CPU";
                if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)    std::cout << "VIRTUAL_GPU";
                std::cout << std::endl;

                // check device queue families
                u32 n_queue_families = {};
                vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[physical_device_index], &n_queue_families, nullptr);
                queue_families.resize(n_queue_families);
                vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[physical_device_index], &n_queue_families, queue_families.data());

                // log physical device queue families
                std::cout << "queue families: [" << n_queue_families << "]" << std::endl;
                for (u32 queue_family_index = 0; queue_family_index < n_queue_families; queue_family_index++)
                {
                    VkQueueFamilyProperties family = queue_families[queue_family_index];

                    // HACK: extracts device 0's queue family 0 for final usage
                    if (physical_device_index == target_physical_device_index && queue_family_index == target_queue_family_index) {
                        target_queue_family_properties = family;
                    }

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
            }
        }
        // select physical device
        vk_physical_device = physical_devices[target_physical_device_index];
        // sanity check queue family properties to ensure capabilities
        if (
            !(target_queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) ||
            !(target_queue_family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT)
        ) {
            std::cerr << "Target queue family does not support graphics and transfer" << std::endl;
            return -1;
        }

        // configure logical device
        VkDeviceCreateInfo device_info = {};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        // configure queue selection
        // this demo will only require a single queue with graphics and transfer capabilities
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = target_queue_family_index;
        f32 queue_priority = 1.0;
        queue_info.pQueuePriorities = &queue_priority;
        queue_info.queueCount       = 1;

        device_info.pQueueCreateInfos    = &queue_info;
        device_info.queueCreateInfoCount = 1;

        // create logical device
        vr = vkCreateDevice(vk_physical_device, &device_info, NULL, &vk_device);
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

    SDL_DestroyWindowSurface(window);
    vkDestroyDevice(vk_device, NULL);
    vkDestroyInstance(vk_instance, NULL);
    SDL_DestroyWindow(window);

    return 0;
};
