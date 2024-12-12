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
        SDL_Surface* winSurface = SDL_GetWindowSurface(window);
        int status = SDL_UpdateWindowSurface(window);             //   Render any changes to the surface to the screen (swap front/back buffers) 

        if(status != 0)
        {
            const char *message = SDL_GetError();

            std::cerr << "Error: " << message << std::endl;
        };
    };

    return 0;
};