#include <iostream>

#include <vulkan/vulkan.h>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_vulkan.h>

#define CHECK_RESULT(VR) if ((VR) != VK_SUCCESS) {printf("%s(%d)\n", __FILE__, __LINE__); std::exit((VR));}

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

int main()
{
    VkResult vr = VK_SUCCESS;
    /*  This also exists, but initialises a tonne of additional subsytems for things like
        controllers which we don't really need:

        SDL_Init(SDL_INIT_EVERYTHING); */

    SDL_InitSubSystem(SDL_INIT_VIDEO);

    int extension_count = 0;
    const char *extension_names[64];

    SDL_Window *window = SDL_CreateWindow(
        "Vulkan Demo",                                          //  Title
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,       //  Pos X / Pos Y
        640, 480,                                               //  Width / height
        SDL_WINDOW_VULKAN                                       //  Flags: May be chained with OR
    );

    if(window == NULL)
    {
        std::cerr << "Error: Failed to create window instance. Check availability of Vulkan drivers." << std::endl;
    }
    else
    {
        bool window_is_open = true;

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

            // get extension requirements from SDL
            std::vector<char*> extensions;

            u32 sdl_n_extensions = 0;
            SDL_Vulkan_GetInstanceExtensions(window, &sdl_n_extensions, nullptr);
            extensions.resize(sdl_n_extensions);
            SDL_Vulkan_GetInstanceExtensions(window, &sdl_n_extensions, (const char**) extensions.data());

            instance_info.enabledExtensionCount   = extensions.size();
            instance_info.ppEnabledExtensionNames = extensions.data();

            std::cout << "Requiring extensions:" << std::endl;
            for (int i = 0; i < instance_info.enabledExtensionCount; i++) {
                std::cout << instance_info.ppEnabledExtensionNames[i] << std::endl;
                extension_names[extension_count++] = instance_info.ppEnabledExtensionNames[i];
            };

            vr = vkCreateInstance(&instance_info, NULL, &vk_instance);
            CHECK_RESULT(vr);
        };

        VkSurfaceKHR vk_surface = {};
        SDL_Vulkan_CreateSurface(window, vk_instance, &vk_surface);
        SDL_Surface* win_surface = SDL_GetWindowSurface(window);

        VkDevice logical_device = {}; {
            // query available hardware
            uint32_t device_count = 0;
            vr = vkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr);
            std::cout << "Querying [" << device_count << "] devices:" << std::endl;

            if(device_count < 1)
            {
                std::cerr << "No devices found" << std::endl;
                std::exit(0);
            };

            std::vector<VkPhysicalDevice> devices;
            devices.resize(device_count);
            vr = vkEnumeratePhysicalDevices(vk_instance, &device_count, devices.data());
            CHECK_RESULT(vr);

            // check hardware capabilities
            std::vector<VkQueueFamilyProperties> queue_families;
            for (u32 i = 0; i < device_count; i++) {
                // log the device name
                VkPhysicalDeviceProperties properties = {};
                vkGetPhysicalDeviceProperties(devices[i], &properties);
                std::cout << std::endl << properties.deviceName << ":" << std::endl;

                // check device queue support
                u32 n_queue_families = {};
                vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &n_queue_families, nullptr);
                if (n_queue_families < 1)
                {
                    std::cout << "no queue families" << std::endl;
                    continue;
                } else {
                    std::cout << "[" << n_queue_families << "] supported queue families:" << std::endl;
                }
                queue_families.resize(n_queue_families);
                vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &n_queue_families, queue_families.data());

                for (u32 i = 0; i < n_queue_families; i++) {
                    VkQueueFamilyProperties family = queue_families[i];

                    // log family index and count
                    std::cout << "family " << i << ":\tcount:\t" << family.queueCount << "\tflags: | ";
                    // log queue family flags
                    std::cout << (family.queueFlags & VK_QUEUE_GRAPHICS_BIT         ? "GRAPHICS | "       : "         | ");
                    std::cout << (family.queueFlags & VK_QUEUE_COMPUTE_BIT          ? "COMPUTE | "        : "        | ");
                    std::cout << (family.queueFlags & VK_QUEUE_TRANSFER_BIT         ? "TRANSFER | "       : "         | ");
                    std::cout << (family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT   ? "SPARSE_BINDING | " : "               | ");
                    std::cout << (family.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR ? "DECODE | "         : "       | ");
                    std::cout << (family.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR ? "ENCODE | "         : "       | ");
                    std::cout << (family.queueFlags & VK_QUEUE_PROTECTED_BIT        ? "PROTECTED | "      : "          | ");
                    // log image transfer granularity
                    std::cout << " transfer granularity: (" << family.minImageTransferGranularity.width << ", " << family.minImageTransferGranularity.height << ", " << family.minImageTransferGranularity.width << ")";
                    std::cout << std::endl;
                }
            }

            //  define device queue properties
            std::vector<float> priorities = {1.0};
            std::vector<VkDeviceQueueCreateInfo> queues;

            VkDeviceQueueCreateInfo queue_info = {};
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            queue_info.queueFamilyIndex = 0;
            queue_info.pQueuePriorities = priorities.data();
            queues.push_back(queue_info);

            // define device interface
            VkDeviceCreateInfo device_info = {};
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.pQueueCreateInfos = queues.data();
            device_info.queueCreateInfoCount = queues.size();

            vr = vkCreateDevice(devices[0], &device_info, NULL, &logical_device);
            CHECK_RESULT(vr);
        };

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

                int status = SDL_UpdateWindowSurface(window);             //   Render any changes to the surface to the screen (swap front/back buffers)

                if(status != 0)
                {
                    const char *message = SDL_GetError();
                    std::cerr << "Error: " << message << std::endl;
                };
            };
        };

        vkDestroyInstance(vk_instance, NULL);
        SDL_DestroyWindow(window);
    };

    return 0;
};
