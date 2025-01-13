#include <iostream>

#include <vulkan/vulkan.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_vulkan.h>

template<typename T>
inline T* array_alloc(size_t n) {
    void* ptr = malloc(n * sizeof(T));
    memset(ptr, 0x00, n * sizeof(T));
    return (T*) ptr;
}

int main()
{
    VkResult vr = VK_SUCCESS;
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

            SDL_Vulkan_GetInstanceExtensions(window, &instance_info.enabledExtensionCount, nullptr);
            instance_info.ppEnabledExtensionNames = array_alloc<char*>(instance_info.enabledExtensionCount);
            SDL_Vulkan_GetInstanceExtensions(window, &instance_info.enabledExtensionCount, (const char**) instance_info.ppEnabledExtensionNames);

            std::cout << "Requiring extensions:" << std::endl;
            for (int i = 0; i < instance_info.enabledExtensionCount; i++) {
                std::cout << instance_info.ppEnabledExtensionNames[i] << std::endl;
            }

            vr = vkCreateInstance(&instance_info, NULL, &vk_instance);

            if(vr != VK_SUCCESS)
            {
                // TODO: better error handling
                std::cerr << "Failed to initialise Vulkan" << std::endl;
            };
        };

        VkSurfaceKHR vk_surface = {};
        SDL_Vulkan_CreateSurface(window, vk_instance, &vk_surface);
        SDL_Surface* win_surface = SDL_GetWindowSurface(window);

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
