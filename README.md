# Vulkan-Boilerplate
X-platform setup for vulkan graphics applications using GLFW: By Ford Jones and Marc Gluyas

## Getting started:
1. Make sure you have g++ installed:
```
g++ -v
```
If you don't you will have to download and install it.

2. Install [glfw](https://www.glfw.org/download)
3. Install the [Vulkan SDK](https://vulkan.lunarg.com/)
4. Install [glslc](https://github.com/google/shaderc/tree/main/glslc) (*You could also use `slangc` or `glslang`*)
5. The project also has an active validation layer for checking correct API usage at runtime. I.e. `VK_LAYER_KHRONOS_validation`. This may or may not ship with vulkan, download it if it isn't present.

## Precompile the shaders:
The shaders must first be compiled to SPIR-V
```
glslc -c -std=410core -fshader-stage=vertex --target-env=vulkan1.4 shaders/glsl.vert -o vert.spv

glslc -c -std=410core -fshader-stage=fragment --target-env=vulkan1.4 shaders/glsl.frag -o vert.frag
```

## Compile the project:
Using `g++`:
```
g++ main.cpp -o run -lglfw -lvulkan
```

## Run the project
```
./run

# With vulkan logs
VK_LOADER_DEBUG=warn ./run
```
