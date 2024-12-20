#include <iostream>
#include <SDL2/SDL.h>


int main()
{   
    /*  This also exists, but initialises a tonne of additional subsytems for things like
        controllers which we don't really need: 

        SDL_Init(SDL_INIT_EVERYTHING); */

    SDL_InitSubSystem(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "Vulkan Demo",                                          //  Title
        0, 0,                                                   //  Pos X / Pos Y
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
        }
    };

    return 0;
};