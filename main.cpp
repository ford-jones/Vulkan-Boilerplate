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

struct Queue_Family_Details {
    u32                     index;
    VkQueueFamilyProperties props;
};

struct Physical_Device_Detials {
    VkPhysicalDevice           handle;
    VkPhysicalDeviceProperties props;
    std::vector<Queue_Family_Details> queue_families;
};

std::vector<Physical_Device_Detials> get_suitable_physical_devices_and_queue_families(VkInstance &vk_instance, VkSurfaceKHR &vk_surface, bool log_devices = false);

// program arguments
bool main_list_supporeted_extensions = false;
bool main_list_physical_devices_info = false;
bool main_prefer_high_performance_device = false; // TODO: unimplemented
bool main_disabled_validation_layer = false;

int main(i32 argc, char** argv)
{
    //
    // PARSE ARGUMENTS
    //

    for (i32 i = 1 ; i < argc; i++) {
        if      (STREQ("-x", argv[i]) || STREQ("--list-extensions", argv[i])) main_list_supporeted_extensions = true;
        else if (STREQ("-d", argv[i]) || STREQ("--list-devices",    argv[i])) main_list_physical_devices_info = true;
        else if (STREQ("-p", argv[i]) || STREQ("--high-perf",       argv[i])) main_prefer_high_performance_device = true;
        else if (STREQ("-z", argv[i]) || STREQ("--no-validate",     argv[i])) main_disabled_validation_layer = true;
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
    // VULKAN DEVICE INIT
    // After initializing SDL, we create the core set of Vulkan objects to interact with SDL's windowing system
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
        std::vector<const char *> layers = {};

        //  Enable API validation layer
        if(!main_disabled_validation_layer)
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        };

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
    // The queue family index is an integer that is required by several other functions
    // The physical device is retained to query cababilities only; it carries little API functionality
    VkPhysicalDevice vk_physical_device = {};
    VkDevice         vk_device = {};
    u32              vk_queue_family_index = 0; // TODO: implement selection logic
    VkQueue          vk_queue = {};
    {
        // select physical device and queue family to execute on
        // TOOD: implement selection logic
        u32                     target_physical_device_index = 0;
        VkQueueFamilyProperties target_queue_family_properties = {};

        std::vector<Physical_Device_Detials> suitable_physical_devices = get_suitable_physical_devices_and_queue_families(vk_instance, vk_surface, main_list_physical_devices_info);
        if(suitable_physical_devices.size() < 1)
        {
            std::cerr << "No suitable physical devices found" << std::endl;
            std::exit(-1);
        };
        // TODO: smarter selection of
        vk_physical_device    = suitable_physical_devices[0].handle;
        vk_queue_family_index = suitable_physical_devices[0].queue_families[0].index;

        // Configure queue
        // this demo will only require a single queue with graphics and transfer capabilities
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = vk_queue_family_index;
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
        vkGetDeviceQueue(vk_device, vk_queue_family_index, 0, &vk_queue);
    };

    //  Swapchain creation
    //  This application assumes that there will only ever be one swapchain because things like screen resize don't occur during runtime
    const u32 vk_swapchain_min_image_count = 3;
    VkSwapchainKHR vk_swapchain = {};
    {
        //  Query available hardware extensions and surface properties
        VkSurfaceCapabilitiesKHR surface_capabilities = {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &surface_capabilities);

        //  Define swapchain interface
        VkSwapchainCreateInfoKHR swapchain_info = {};
        swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchain_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.compositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchain_info.minImageCount = vk_swapchain_min_image_count;
        swapchain_info.imageArrayLayers = 1;
        swapchain_info.surface = vk_surface;
        swapchain_info.imageExtent = surface_capabilities.currentExtent;
        swapchain_info.clipped = VK_TRUE;

        std::cout << "Creating swapchain..." << std::endl;
        vr = vkCreateSwapchainKHR(vk_device, &swapchain_info, nullptr, &vk_swapchain);
        CHECK_RESULT(vr);
    };

    //  Retrieve images from swapchain
    std::vector<VkImage> swapchain_images = {};
    vr = COUNT_APPEND_HELPER(swapchain_images, vkGetSwapchainImagesKHR, vk_device, vk_swapchain);
    CHECK_RESULT(vr);

    //
    // VULKAN PIPELINE INIT
    // With the primary Vulkan interfaces initialized, we can start creating our app's graphics pipeline
    //

    // Command pool creation
    // This abstracts the backing allocation for command buffers; each command buffer must be created in association with a specific command pool
    // Synchronisation of command execution must be done explicitly in the pipeline; the command pool must not be reset while it is being executed on the queue
    VkCommandPool cmd_pool = {};
    {
        VkCommandPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        // Flags we may want to use:
        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : implementation hint, indicates command pool will be reset in a short timeframe
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : enables the use of vkResetCommandBuffer. currently we will use only a single command buffer, so we can just reset the command pool directly
        info.flags = 0;

        info.queueFamilyIndex = vk_queue_family_index; // the command pool needs to know which queue family it will be used for

        // Create command pool
        std::cout << "Creating command pool..." << std::endl;
        vr = vkCreateCommandPool(vk_device, &info, nullptr, &cmd_pool);
        CHECK_RESULT(vr);
    }

    // Command buffer allocation
    // This is the API endpoint for recording API instructions, after which they are submitted to the queue for execution
    // Once execution has finished, their containing command pool must be reset so that commands may be recorded for the next execution
    // This is a primary command buffer, so that commands can be recorded to it and then directly executed on a queue
    VkCommandBuffer cmd_buf = {};
    {
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandPool = cmd_pool;
        info.commandBufferCount = 1;

        // Allocate command buffer
        std::cout << "Allocating command buffer..." << std::endl;
        vr = vkAllocateCommandBuffers(vk_device, &info, &cmd_buf);
        CHECK_RESULT(vr);
    }

    // //  Command buffer begin recording config
    // VkCommandBufferBeginInfo cmd_buf_begin_info = {};
    // cmd_buf_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // cmd_buf_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    // //  Begin recording to command buffer
    // vr = vkBeginCommandBuffer(cmd_buf, &cmd_buf_begin_info);
    // CHECK_RESULT(vr);

    // //  Frame color
    // VkClearColorValue color = {{1.0, 0.0, 1.0}};

    // //  Image range definition
    // VkImageSubresourceRange image_range = {};
    // image_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // image_range.baseMipLevel = 0;
    // image_range.levelCount = 1;
    // image_range.baseArrayLayer = 0;
    // image_range.layerCount = 1;

    // vkCmdClearColorImage(cmd_buf, swapchain_images[0], VK_IMAGE_LAYOUT_GENERAL, &color, 1, &image_range);

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

    // pipeline
    vkDestroyCommandPool(vk_device, cmd_pool, nullptr);
    // vulkan
    vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);
    vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
    vkDestroyDevice(vk_device, NULL);
    vkDestroyInstance(vk_instance, NULL);
    // sdl
    SDL_DestroyWindow(window);

    return 0;
};

// DEVICE QUEURYING AND SUITABILITY CHECKING

bool check_queue_family_suitability(VkSurfaceKHR &vk_surface, VkPhysicalDevice &physical_device, Queue_Family_Details &queue_family)
{
    VkBool32 surface_support = true;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family.index, vk_surface, &surface_support);
    if (!surface_support) return false;

    // check support for graphics and transfer commands
    if (!(queue_family.props.queueFlags & VK_QUEUE_GRAPHICS_BIT)) return false;
    if (!(queue_family.props.queueFlags & VK_QUEUE_TRANSFER_BIT)) return false;

    return true;
}

bool check_physical_device_suitability(VkSurfaceKHR &vk_surface, Physical_Device_Detials &physical_device)
{
    VkResult vr =  VK_SUCCESS;

    if (physical_device.queue_families.size() < 1) return false;

    bool present_fifo_support = false;
    std::vector<VkPresentModeKHR> present_modes;
    vr = COUNT_APPEND_HELPER(present_modes, vkGetPhysicalDeviceSurfacePresentModesKHR, physical_device.handle, vk_surface);
    CHECK_RESULT(vr);
    for (auto &present_mode : present_modes) if (present_mode == VK_PRESENT_MODE_FIFO_KHR) { present_fifo_support = true; break; }
    if (!present_fifo_support) return false;

    return true;
}

void log_physical_device_props(VkPhysicalDeviceProperties &props) {
    // log name
    std::cout << props.deviceName << std::endl;

    // log device type
    std::cout << "device type: ";
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_OTHER)          std::cout << "OTHER";
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   std::cout << "DISCRETE_GPU";
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) std::cout << "INTEGRATED_GPU";
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)            std::cout << "CPU";
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)    std::cout << "VIRTUAL_GPU";
    std::cout << std::endl;
}

void log_queue_family_props(VkQueueFamilyProperties &props, u32 index) {
    // log family index and count
    std::cout << "family " << index << ":\tcount:\t" << props.queueCount << "\tflags: | ";

    // log queue family flags
    std::cout << (props.queueFlags & VK_QUEUE_GRAPHICS_BIT         ? "GRAPHICS | "       : "         | ");
    std::cout << (props.queueFlags & VK_QUEUE_COMPUTE_BIT          ? "COMPUTE | "        : "        | ");
    std::cout << (props.queueFlags & VK_QUEUE_TRANSFER_BIT         ? "TRANSFER | "       : "         | ");
    std::cout << (props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT   ? "SPARSE_BINDING | " : "               | ");
    std::cout << (props.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR ? "DECODE | "         : "       | ");
    std::cout << (props.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR ? "ENCODE | "         : "       | ");
    std::cout << (props.queueFlags & VK_QUEUE_PROTECTED_BIT        ? "PROTECTED | "      : "          | ");

    // log image transfer granularity
    std::cout << " transfer granularity: (" << props.minImageTransferGranularity.width << ", " << props.minImageTransferGranularity.height << ", " << props.minImageTransferGranularity.depth << ")";
    std::cout << std::endl;
}

std::vector<Physical_Device_Detials> get_suitable_physical_devices_and_queue_families(VkInstance &vk_instance, VkSurfaceKHR &vk_surface, bool log)
{
    VkResult vr = VK_SUCCESS;

    std::vector<Physical_Device_Detials> physical_devices = {};

    // get device handles from the instance
    std::vector<VkPhysicalDevice> physical_device_handles;
    vr = COUNT_APPEND_HELPER(physical_device_handles, vkEnumeratePhysicalDevices, vk_instance);
    CHECK_RESULT(vr);

    if (log)
    {
        std::cout << std::endl << "Querying [" << physical_device_handles.size() << "] physical devices:" << std::endl;
    }

    // populate physical devices
    for (auto& handle : physical_device_handles)
    {
        Physical_Device_Detials physical_device = {};
        physical_device.handle = handle;

        // get properties
        vkGetPhysicalDeviceProperties(handle, &physical_device.props);

        // get queue families
        std::vector<VkQueueFamilyProperties> queue_family_props;
        COUNT_APPEND_HELPER(queue_family_props, vkGetPhysicalDeviceQueueFamilyProperties, handle);

        if (log)
        {
            log_physical_device_props(physical_device.props);
            std::cout << "queue families: [" << queue_family_props.size() << "]" << std::endl;
        }

        for (u32 index = 0; index < queue_family_props.size(); index++)
        {
            Queue_Family_Details queue_family = {};
            queue_family.index = index;
            queue_family.props = queue_family_props[index];

            if (log)
            {
                log_queue_family_props(queue_family_props[index], index);
            }

            if (check_queue_family_suitability(vk_surface, handle, queue_family))
            {
                physical_device.queue_families.push_back(queue_family);
            }
        }

        if (log) std::cout << std::endl;

        if (check_physical_device_suitability(vk_surface, physical_device))
        {
            physical_devices.push_back(physical_device);
        }
    }

    // log list of suitable devices and queues
    if (log)
    {
        std::cout << "Suitable devices and queue families:" << std::endl;
        for (auto& device : physical_devices)
        {
            std::cout << device.props.deviceName << ": ";
            for (auto& queue_family : device.queue_families)
            {
                std::cout << queue_family.index << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    return physical_devices;
}
