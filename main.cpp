#include <iostream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

int main()
{
    /**
     * VULKAN INSTANCE CONFIGURATION
     */

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
        /**
         * TODO:
         * Make sensible choices about layers.
         * 
         * This cut-off is a brutish way of removing weird old layers, specifically because on
         * debian one named VK_LAYER_INTEL_nullhw caused me much trouble and was the direct 
         * cause of segmentation faults during device creation. I strongly suspect that this (or worse)
         * could be the case with numerous other vendor specific layers accross different platforms.
         * There may be other layers with lower spec versions which are genuinely helpful though 
         * so this is not a suitable solution.
         * 
         * The other solution would be to run the program with VK_LOADER_LAYERS_DISABLE=layername
         */
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

    //  Determine which extensions MUST be present to create a vulkan instance with glfw
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

    //  Create the vulkan instance
    vk::InstanceCreateInfo instance_info = {};
    instance_info.pApplicationInfo = &vk_app_info;
    instance_info.enabledLayerCount = instance_layers.size();
    instance_info.ppEnabledLayerNames = instance_layers.data();
    instance_info.enabledExtensionCount = instance_extensions.size();
    instance_info.ppEnabledExtensionNames = instance_extensions.data();
    
    /**
     * NOTE:
     * The vulkan cpp api included from vulkain_raii.hpp can throw exceptions
     * as opposed to returning a VkResult like the C api does in places where
     * the return parameter is reserved for something else
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

    /**
     * WINDOW SETUP
     */
    
    //  Initialise GLFW and the vulkan loader using the process ID of the vulkan instance that was just created
    glfwInitVulkanLoader(vkGetInstanceProcAddr);

    //  Check if the current version of glfw supports vulkan at all
    if(!glfwInit() || !glfwVulkanSupported())
    {
        const char *error = "";
        glfwGetError(&error);

        printf("GLFW ERROR %s: %s(%d)\n", error, __FILE__, __LINE__);
        std::exit(0);
    }

    //  Create the application window
    //  Explicitly instruct glfw NOT to create a window that is bound to an OpenGL context, as it does by default
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *glfw_window = glfwCreateWindow(800, 600, "Vulkan Demo", NULL, NULL);
    glfwSetWindowCloseCallback(
        glfw_window, 
        [](GLFWwindow *win) {
            glfwSetWindowShouldClose(win, GLFW_TRUE);
            return;
        }
    );

    //  Surface creation
    vk::SurfaceKHR vk_surface = {};
    VkResult surface_creation = glfwCreateWindowSurface(vk_instance, glfw_window, NULL, (VkSurfaceKHR *)(&vk_surface));
    if(surface_creation != VK_SUCCESS)
    {
        printf("VULKAN ERROR %d: %s(%d)\n", surface_creation, __FILE__, __LINE__);
        std::exit(surface_creation);
    }

    /**
     * HARDWARE SELECTION
     */

    // Query available physical hardware devices
    std::cout << "\n" << "Physical Devices:" << "\n" << std::endl;
    bool vk_appropriate_device_found = false;
    uint32_t vk_queue_family_index = 0;
    uint32_t vk_physical_device_index = 0;
    auto available_physical_devices = vk_instance.enumeratePhysicalDevices();

    //  Inspect the properties of the hardware that has been discovered
    //  Find a device that is suitable for rendering graphics
    for(size_t i = 0; i < available_physical_devices.size(); i++)
    {
        vk::PhysicalDevice &physical_device_handle = available_physical_devices[i];
        std::cout << "- \n Checking device properties" << std::endl;
        vk::PhysicalDeviceProperties physical_device_properties = physical_device_handle.getProperties();
        std::cout << "Name: " << physical_device_properties.deviceName << std::endl;
        std::cout << "Type: " << &physical_device_properties.deviceType << std::endl;

        vk::PhysicalDeviceFeatures physical_device_features = physical_device_handle.getFeatures();

        //  Determine whether the device has rendering capabilities
        /**
         * NOTE:
         * Geometry shaders won't be used and are ideally avoided.
         * Despite that; they're checked here because their presence is indicitive of a device that supports graphics
         * 
         * There are others that could be used e.g. VIRTUAL_GPU or even CPU, but won't be using em
         */
        if( physical_device_features.geometryShader &&
            physical_device_properties.apiVersion >= VK_API_VERSION_1_4                     &&
            (physical_device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ||
            physical_device_properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu))
            {
            std::cout << "Rendering hardware found" << std::endl;
            //  Determine whether the device has the appropriate functionality for a command-queue
            /**
             * NOTE:
             * Should have surface support
             * Should have atleast GRAPHICS and optionally TRANSFER or COMPUTE bits
             * Should have a FIFO presentation mode
             */
            std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device_handle.getQueueFamilyProperties();
            for(size_t j = 0; j < queue_family_properties.size(); j++)
            {
                //  Determine whether the device has surface capabilities
                vk::Bool32 surface_support = physical_device_handle.getSurfaceSupportKHR(j, vk_surface);
                if(surface_support)
                {
                    std::cout << "Device has surface support:" << std::endl;
                    
                    //  Determine whether the device has queue capabilities
                    //  Note: Could also check for eCompute for compute pipelines etc
                    /**
                     * NOTE:
                     * Could also check for eCompute for compute pipelines but dont plan to currently use em
                     * Two other useful queue families that are commonly used in renderers:
                     * TRANSFER - used to move data back and fourth between the GPU and the host
                     * COMPUTE - for dispatching commands to the GPU to be executed off-screen
                     */
                    const auto &family_properties = queue_family_properties[j];
                    if(family_properties.queueFlags & vk::QueueFlagBits::eGraphics)
                    {
                        std::cout << "Device has appropriate graphics queue flags: " << &family_properties.queueFlags << std::endl;

                        //  Determine presentation capabilities
                        //  Check for the availability of a (first-in-first-out) queue (w/r/t the treatment of swapchain images, not the treatment vkCommands as they will be executed asynchronously on the GPU)
                        std::vector<vk::PresentModeKHR> present_modes = physical_device_handle.getSurfacePresentModesKHR(vk_surface);
                        for(const auto &present_mode : present_modes)
                            if(present_mode == vk::PresentModeKHR::eFifo)
                            {
                                std::cout << "Device has FIFO presentation mode" << std::endl;
                                vk_appropriate_device_found = vk::True;
                                vk_physical_device_index = i;
                                vk_queue_family_index = j;
                            }
                    }
                }
            };
        }
    }

    //  Select rendering hardware
    if(!vk_appropriate_device_found)
    {
        printf("ERROR Failed to locate appropriate rendering hardware: %s(%d)\n", __FILE__, __LINE__);
        std::exit(0);
    }
    vk::PhysicalDevice vk_physical_device = available_physical_devices[vk_physical_device_index];

    /**
     * LOGICAL DEVICE INTERFACE
     */
    
    //  Configure a queue to be created on the device that will he used for graphics rendering operations
    //  An instance can have multiple queues, they're processed in order of pQueuePriorities
    //  1.0 being the highest
    const float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo graphics_queue_info = {};
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priority;
    graphics_queue_info.queueFamilyIndex = vk_queue_family_index;

    //  Extensions and features
    std::vector<const char *> extension_names = {vk::KHRSwapchainExtensionName};

    vk::PhysicalDeviceFeatures vk_physical_device_features = {};

    //  Create a structure chain used to expose the vulkan features being used (i.e. vulkan 1_0 - 1_1 - 1_3)
    /**
     * NOTE:
     * Just because a 1_3 instance was created doesn't mean there is currently access to all the features
     * They have to be explicitly specified like so; otherwise the logical device will be created without them
     * Vulkan provides a SwapChain structure for tidying this up, but I've done it with verbose semantics here
     * to show more clearly what is going on using pNext
     */
    vk::PhysicalDeviceVulkan13Features vk_13_features = {};
    vk_13_features.dynamicRendering = vk::True;
    vk::PhysicalDeviceVulkan11Features vk_11_features = {};
    vk_11_features.shaderDrawParameters = vk::True;
    vk_11_features.pNext = &vk_13_features;

    // Configure the logical device which will be used to interface with physical device
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

    /**
     * GRAPHICS QUEUE, SWAPCHAIN AND IMAGES
     */

    /**
     * TODO: 
     * Fetching the queue handle could be moved
     * Querying additional physical device properties should be moved to the rest of physical device creation
     */
    //  Retrieve a handle for interfacing with the queue, only a single queue from the family has been created hence index 0
    vk::Queue vk_graphics_queue_handle = vk_logical_device.getQueue(vk_queue_family_index, 0);
    
    //  Check additional hardware capabilities related to surface management
    vk::SurfaceCapabilitiesKHR surface_capabilities = vk_physical_device.getSurfaceCapabilitiesKHR(vk_surface);

    //  Query the hardware's support for different presentation modes
    std::vector<vk::PresentModeKHR> surface_supported_present_modes = vk_physical_device.getSurfacePresentModesKHR(vk_surface);

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

    //  Determine the hardware's support for surface formats
    std::vector<vk::SurfaceFormatKHR> surface_supported_image_formats = vk_physical_device.getSurfaceFormatsKHR(vk_surface);

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

    //  Create the swapchain used by the device to manage writable image buffers
    //  Also retrieve the images themselves
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
    swapchain_info.clipped = vk::True;

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

    /**
     * LOADING SHADERS
     */

    /**
     * NOTE:
     * Unlike OpenGL, compiling the translation units that make up a shader program can be done ahead of time.
     * This means startup time should be greatly reduced as the compiler doesn't need to be run programatically.
     * In this demo glsl is still being used but it has been precompiled to SPIRV. This process could be 
     * done in Cmake. The Vulkan tutorials use Slang with slangc.
     */
    
    //  Load vertex shader to memory
    std::string vertex_shader_raw = "";
    std::string vertex_shader_filepath = "";
    const char *vertex_shader_filename = "shaders/vert.spv";
    uint32_t vertex_shader_bytesize = 0;
    if(std::filesystem::exists(vertex_shader_filename))
    {
        std::filesystem::path absolute_path = std::filesystem::absolute(vertex_shader_filename);
        if(!std::filesystem::exists(absolute_path))
        {
            printf("FILESYSTEM ERROR %s: %s(%d)\n", vertex_shader_filename, __FILE__, __LINE__);
            std::exit(0);
        }
        vertex_shader_filepath = absolute_path.string();
        vertex_shader_bytesize = std::filesystem::file_size(vertex_shader_filepath);

        std::cout << "Loading shader: " << vertex_shader_filepath << std::endl;
        std::ifstream vert_file_stream;
        vert_file_stream.open(vertex_shader_filepath, std::ios::in | std::ios::binary);
        if(!vert_file_stream.is_open())
        {
            printf("FILESYSTEM ERROR %s: %s(%d)\n", vertex_shader_filename, __FILE__, __LINE__);
            std::exit(0);
        }

        vertex_shader_raw.resize(vertex_shader_bytesize);
        vert_file_stream.read(vertex_shader_raw.data(), vertex_shader_bytesize);
        vert_file_stream.close();
    }

    vk::ShaderModuleCreateInfo vert_shader_info = {};
    vert_shader_info.codeSize = vertex_shader_bytesize;
    vert_shader_info.pCode = reinterpret_cast<const uint32_t *>(vertex_shader_raw.data());      //  SPIRV is read in 32bit chunks(?), so this needs to be cast from uchar(8)
    vk::ShaderModule vk_vertex_shader = vk_logical_device.createShaderModule(vert_shader_info);

    vk::PipelineShaderStageCreateInfo vertex_shader_stage_info = {};
    vertex_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vertex_shader_stage_info.module = vk_vertex_shader;
    vertex_shader_stage_info.pName = "main";

    //  Load fragment shader to memory
    std::string fragment_shader_raw = "";
    std::string fragment_shader_filepath = "";
    const char *fragment_shader_filename = "shaders/frag.spv";
    uint32_t fragment_shader_bytesize = 0;
    if(std::filesystem::exists(fragment_shader_filename))
    {
        std::filesystem::path absolute_path = std::filesystem::absolute(fragment_shader_filename);
        if(!std::filesystem::exists(absolute_path))
        {
            printf("FILESYSTEM ERROR %s: %s(%d)\n", fragment_shader_filename, __FILE__, __LINE__);
            std::exit(0);
        }
        fragment_shader_filepath = absolute_path.string();
        fragment_shader_bytesize = std::filesystem::file_size(fragment_shader_filepath);

        std::cout << "Loading shader: " << fragment_shader_filepath << std::endl;
        std::ifstream vert_file_stream;
        vert_file_stream.open(fragment_shader_filepath, std::ios::in | std::ios::binary);
        if(!vert_file_stream.is_open())
        {
            printf("FILESYSTEM ERROR %s: %s(%d)\n", fragment_shader_filename, __FILE__, __LINE__);
            std::exit(0);
        }

        fragment_shader_raw.resize(fragment_shader_bytesize);
        vert_file_stream.read(fragment_shader_raw.data(), fragment_shader_bytesize);
        vert_file_stream.close();
    }
    
    vk::ShaderModuleCreateInfo frag_shader_info = {};
    frag_shader_info.codeSize = fragment_shader_bytesize;
    frag_shader_info.pCode = reinterpret_cast<const uint32_t *>(fragment_shader_raw.data());
    vk::ShaderModule vk_fragment_shader = vk_logical_device.createShaderModule(frag_shader_info);

    vk::PipelineShaderStageCreateInfo fragment_shader_stage_info = {};
    fragment_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    fragment_shader_stage_info.module = vk_fragment_shader;
    fragment_shader_stage_info.pName = "main";

    /**
     * GRAPHICS PIPELINE INITIALISATION
     */

    std::vector<vk::PipelineShaderStageCreateInfo> vk_shader_stages = {vertex_shader_stage_info, fragment_shader_stage_info};

    //  Create a command pool that will dispatch to the graphics queue and make sure it can be rewritten to 
    vk::CommandPoolCreateInfo command_pool_info = {};
    command_pool_info.queueFamilyIndex = vk_queue_family_index;                                     //  Which queue the commands recorded in this pool should be dispatched to
    command_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;                   //  May be individually (this command pool) rerecorded
    vk::CommandPool vk_command_pool = vk_logical_device.createCommandPool(command_pool_info);

    //  Allocate the pools command buffers in VRAM
    //  there will be used for recording the render commands that are going to get pushed to the queue
    vk::CommandBufferAllocateInfo command_buffer_info = {};
    command_buffer_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffer_info.commandPool = vk_command_pool;
    command_buffer_info.commandBufferCount = 1;

    vk::CommandBuffer vk_command_buffer = vk_logical_device.allocateCommandBuffers(command_buffer_info)[0];

    //  Configure dynamic rendering state
    /**
     * NOTE:
     * Dynamic rendering constituents have their configuration ignored during static/fixed rendering state config
     * This means that their values must instead be passed in at draw time, meaning the pipeline DOESNT need to be recreated when these values change
     * I.e. viewport size (resizing)
     */
    std::vector<vk::DynamicState> dynamic_rendering_concerns = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo graphics_pipeline_dynamic_state = {};
    graphics_pipeline_dynamic_state.dynamicStateCount = dynamic_rendering_concerns.size();
    graphics_pipeline_dynamic_state.pDynamicStates = dynamic_rendering_concerns.data();
    
    //  Configure static (fixed-function) rendering state (i.e. the rasterizer itself / things that aren't going to be respecified every draw)

    //  Describe the layout of vertex input data
    /**
     * NOTE:
     * Just drawing a blue screen, so won't be including any actual vertex attributes
     * This is similar to glEnableVertexAttribArray + glVertexAttribPointer
     */
    vk::PipelineVertexInputStateCreateInfo graphics_pipeline_vertex_input_info = {};
    
    //  Describe how vertices should be grouped and assembled.
    //  Using a TriangleList; It's implicit that a face is constructed from exactly 3 vertices
    vk::PipelineInputAssemblyStateCreateInfo graphics_pipeline_vertex_assemble_info = {};
    graphics_pipeline_vertex_assemble_info.topology = vk::PrimitiveTopology::eTriangleList;

    //  Describe the viewport itself
    //  This viewport passes depth-tests which output between 0.0 - 1.0
    vk::Viewport vk_viewport = {};
    vk_viewport.minDepth = 0.0f;            //  Depth-test min
    vk_viewport.maxDepth = 1.0f;            //  Depth-test max
    vk_viewport.width = swapchain_info.imageExtent.width;
    vk_viewport.height = swapchain_info.imageExtent.height;

    //  Note that viewport and scissor state were flagged as dynamic
    //  So their values must be passed in AT DRAW TIME!! (so not here, just define how many to expect, if it were done here the pipeline would need to be recreated every time the screen resized)
    vk::PipelineViewportStateCreateInfo graphics_pipeline_viewport_info = {};
    graphics_pipeline_viewport_info.viewportCount = 1;        //  There is only 1 viewport, this isn't VR or a flight-sim lol
    graphics_pipeline_viewport_info.scissorCount = 1;         //  scissor for cropping / cutting-out part of the viewport at present-time

    //  Describe the rasterization stage
    vk::PipelineRasterizationStateCreateInfo graphics_pipeline_rasterizer_info = {};
    graphics_pipeline_rasterizer_info.frontFace = vk::FrontFace::eCounterClockwise;     //  Winding-order
    graphics_pipeline_rasterizer_info.cullMode = vk::CullModeFlagBits::eBack;           //  Backface culling
    graphics_pipeline_rasterizer_info.rasterizerDiscardEnable = vk::False;              //  Whether geometry should pass NOT through the rasterizer (i.e. should it NOT be presented to the framebuffer)
    graphics_pipeline_rasterizer_info.depthBiasEnable = vk::False;                      //  Discard fragments which fall outside the depth testing range, instead of clamping
    graphics_pipeline_rasterizer_info.lineWidth = 1.0f;
    
    //  Multisampling (should frag collisions sample the surrounding area for an aggregate result (think MSAA))
    vk::PipelineMultisampleStateCreateInfo graphics_pipeline_multisampling_info = {};
    graphics_pipeline_multisampling_info.rasterizationSamples = vk::SampleCountFlagBits::e1;    //  1 sample, no anti-aliasing
    graphics_pipeline_multisampling_info.sampleShadingEnable = vk::False;

    //  Set the framebuffers blending operation(s) similar to glBlendFunc
    //  Framebuffer-specific blend configuration
    vk::PipelineColorBlendAttachmentState graphics_pipeline_framebuffer_attachment = {};
    graphics_pipeline_framebuffer_attachment.blendEnable = vk::False;
    graphics_pipeline_framebuffer_attachment.colorWriteMask = (vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    //  Global blend configuration
    vk::PipelineColorBlendStateCreateInfo graphics_pipeline_colour_blend_info = {};
    graphics_pipeline_colour_blend_info.logicOpEnable = vk::False;
    graphics_pipeline_colour_blend_info.logicOp = vk::LogicOp::eCopy;
    graphics_pipeline_colour_blend_info.attachmentCount = 1;
    graphics_pipeline_colour_blend_info.pAttachments = &graphics_pipeline_framebuffer_attachment;

    //  Describe pipeline layout (basically for baking uniform values into the pipeline for shaders to use, there are none right now)
    vk::PipelineLayoutCreateInfo graphics_pipeline_layout_info = {};
    graphics_pipeline_layout_info.setLayoutCount = 0;
    graphics_pipeline_layout_info.pushConstantRangeCount = 0;
    vk::PipelineLayout vk_graphics_pipeline_layout = vk_logical_device.createPipelineLayout(graphics_pipeline_layout_info);

    //  Describe pipeline rendering information
    vk::PipelineRenderingCreateInfo graphics_pipeline_rendering_info = {};
    graphics_pipeline_rendering_info.colorAttachmentCount = 1;
    graphics_pipeline_rendering_info.pColorAttachmentFormats = &swapchain_info.imageFormat;
    
    //  Create the graphics pipeline asynchronously on the GPU
    vk::GraphicsPipelineCreateInfo graphics_pipeline_info = {};
    graphics_pipeline_info.layout = vk_graphics_pipeline_layout;
    graphics_pipeline_info.pColorBlendState = &graphics_pipeline_colour_blend_info;
    graphics_pipeline_info.pMultisampleState = &graphics_pipeline_multisampling_info;
    graphics_pipeline_info.pRasterizationState  = &graphics_pipeline_rasterizer_info;
    graphics_pipeline_info.pViewportState = &graphics_pipeline_viewport_info;
    graphics_pipeline_info.pInputAssemblyState = &graphics_pipeline_vertex_assemble_info;
    graphics_pipeline_info.pVertexInputState = &graphics_pipeline_vertex_input_info;
    graphics_pipeline_info.pDynamicState = &graphics_pipeline_dynamic_state;
    graphics_pipeline_info.stageCount = 2;
    graphics_pipeline_info.pStages = vk_shader_stages.data();
    graphics_pipeline_info.renderPass = nullptr;
    graphics_pipeline_info.pNext = &graphics_pipeline_rendering_info;   //  Chain to pipeline rendering structure

    vk::ResultValue<vk::Pipeline> vk_graphics_pipeline = vk_logical_device.createGraphicsPipeline(nullptr, graphics_pipeline_info);

    //  Create binary semaphores (GPU queue delimeter) used for signaling the completion of tasks
    //  Consider a situation where there are two pending queue operations, A and B: 
    //  Semaphores can be used to instruct that while A is in an 'on' state, B must implicitly be 'off'
    vk::Semaphore vk_rendering_complete_semaphore = vk_logical_device.createSemaphore(vk::SemaphoreCreateInfo());
    vk::Semaphore vk_presenting_complete_semaphore = vk_logical_device.createSemaphore(vk::SemaphoreCreateInfo());

    //  Create a fence for scheduling draw operations on the host (CPU) 
    //  similar to a semaphore except it blocks execution on the host until the
    //  attatched task is complete
    vk::FenceCreateInfo drawing_fence_info = {};
    drawing_fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
    vk::Fence vk_draw_fence = vk_logical_device.createFence(drawing_fence_info);

    /**
     * MAIN RENDER LOOP
     */
    
    //  Open window / start rendering
    glfwSetWindowShouldClose(glfw_window, GLFW_FALSE);
    while(!glfwWindowShouldClose(glfw_window))
    {
        //  Ensure previous frame has finished before starting this frame by awaiting the fence
        //  THIS BLOCKS EXECUTION ON THE HOST
        vk::Result wait_for_draw = vk_logical_device.waitForFences(vk_draw_fence, vk::True, UINT32_MAX);
        if(wait_for_draw != vk::Result::eSuccess)
        {
            printf("VULKAN ERROR %d: %s(%d)\n", surface_creation, __FILE__, __LINE__);
            std::exit(surface_creation);
        }
        vk_logical_device.resetFences(vk_draw_fence);

        //  Acquire next image off of the swapchain
        //  Asynchronously await availability and retrieval of the image, signaled by the semaphore
        vk::ResultValue<uint32_t> swapchain_index = vk_logical_device.acquireNextImageKHR(vk_swapchain, UINT32_MAX, vk_presenting_complete_semaphore, nullptr);
        uint32_t image_index = swapchain_index.value;

        //  Begin recording graphics commands
        //  The variety of commands submitted here are graphics commands because they are issued on a graphics queue
        vk::CommandBufferBeginInfo command_buffer_start_info = {};
        // command_buffer_start_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        vk_command_buffer.begin(command_buffer_start_info);
        
        //  Transition memory layout to one that is suitable for rendering
        //  In this scenario the clearcolor is set, so use a memory barrier to optimise
        //  the image for color attatchment
        vk::ImageMemoryBarrier2 bg_color_memory_barrier = {};
        bg_color_memory_barrier.image = vk_swapchain_images[image_index];
        bg_color_memory_barrier.oldLayout = vk::ImageLayout::eUndefined;
        bg_color_memory_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        bg_color_memory_barrier.srcAccessMask = vk::AccessFlagBits2::eNone;
        bg_color_memory_barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
        bg_color_memory_barrier.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        bg_color_memory_barrier.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        bg_color_memory_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        bg_color_memory_barrier.subresourceRange.baseMipLevel = 0;
        bg_color_memory_barrier.subresourceRange.levelCount = 1;
        bg_color_memory_barrier.subresourceRange.baseArrayLayer = 0;
        bg_color_memory_barrier.subresourceRange.levelCount = 1;
        
        //  Begin drawing to the next frame image, performing rendering via the graphics pipeline
        //  Note that there is one draw command here, but that nothing is actually drawn because graphics_pipeline_vertex_input_info is empty
        vk::ClearValue bg_colour = vk::ClearColorValue(0.0f, 0.0f, 1.0f, 1.0f);
        vk::RenderingAttachmentInfo rendering_attatchment_info = {};
        rendering_attatchment_info.clearValue = bg_colour;
        rendering_attatchment_info.imageView = vk_swapchain_image_views[image_index];
        rendering_attatchment_info.imageLayout = bg_color_memory_barrier.newLayout;
        rendering_attatchment_info.loadOp = vk::AttachmentLoadOp::eClear;
        rendering_attatchment_info.storeOp = vk::AttachmentStoreOp::eStore;
    
        vk::RenderingInfo bg_rendering_info = {};
        bg_rendering_info.layerCount = 1;
        bg_rendering_info.colorAttachmentCount = 1;
        bg_rendering_info.renderArea.offset.x = 0;
        bg_rendering_info.renderArea.offset.y = 0;
        bg_rendering_info.renderArea.extent.width = swapchain_info.imageExtent.width;
        bg_rendering_info.renderArea.extent.height = swapchain_info.imageExtent.height;
        bg_rendering_info.pColorAttachments = &rendering_attatchment_info;
    
        vk_command_buffer.beginRendering(bg_rendering_info);
        vk_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk_graphics_pipeline.value);
        vk_command_buffer.setViewport(0, vk_viewport);
        vk_command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_info.imageExtent));

        uint32_t vertex_count = 3;
        uint32_t instance_count = 1;
        uint32_t first_vertex = 0;
        uint32_t first_instance = 0;
        vk_command_buffer.draw(vertex_count, instance_count, first_vertex, first_instance);

        vk_command_buffer.endRendering();
        vk_command_buffer.end();

        //  Submit the command buffer
        vk::PipelineStageFlags vk_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo submit_command_info = {};
        submit_command_info.commandBufferCount = 1;
        submit_command_info.pCommandBuffers = &vk_command_buffer;
        submit_command_info.waitSemaphoreCount = 1;
        submit_command_info.pWaitSemaphores = &vk_presenting_complete_semaphore;    //  I.e. Which sempaphore to wait for before beginning command execution
        submit_command_info.signalSemaphoreCount = 1;
        submit_command_info.pSignalSemaphores = &vk_rendering_complete_semaphore;   //  I.e. Which semaphore to signal once command buffer / work on gpu is finished
        submit_command_info.pWaitDstStageMask = &vk_stage_flags;                    //  I.e. Which stage of the pipeline to wait for
            
        vk_graphics_queue_handle.submit(submit_command_info, vk_draw_fence);

        //  Present the frame
        vk::PresentInfoKHR present_info = {};
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &vk_rendering_complete_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &vk_swapchain;
        present_info.pImageIndices = &image_index;

        vk::Result present_result = vk_graphics_queue_handle.presentKHR(present_info);
        if(present_result != vk::Result::eSuccess)
        {
            printf("VULKAN ERROR %d: %s(%d)\n", surface_creation, __FILE__, __LINE__);
            std::exit(0);
        }

        glfwPollEvents();
    }

    //  Allow all operations to finsh before exiting
    vk_logical_device.waitIdle();

    /**
     * TODO:
     * Cleanup resources
     */
    glfwDestroyWindow(glfw_window);

    return 1;
}