# Vulkan-Boilerplate
X-platform setup for vulkan graphics applications using SDL 2: By Ford Jones and Marc Gluyas

## Getting started:
1. Make sure you have clang installed:
```
clang -v
```
If you don't you will have to download and install it.

2. Install [SDL2](https://wiki.libsdl.org/SDL2/Installation)
3. Install the [Vulkan SDK](https://vulkan.lunarg.com/)

## Running the project:
Compile the source like so:
```
clang -std=c++17 main.cpp -lSDL2 -lstdc++
```

Run the project with:
```
./a.out
```
