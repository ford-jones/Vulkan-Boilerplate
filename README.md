# Vulkan-Boilerplate
X-platform setup for vulkan graphics applications using GLFW: By Ford Jones and Marc Gluyas

## Getting started:
1. Make sure you have clang installed:
```
clang -v
```
If you don't you will have to download and install it.

2. Install [glfw](https://www.glfw.org/download)
3. Install the [Vulkan SDK](https://vulkan.lunarg.com/)
4. Install [glslc](https://github.com/google/shaderc/tree/main/glslc) (*You could also use `slangc` or `glslang`*)

## Precompile the shaders:
The shaders must first be compiled to SPIR-V
```
glslc -c -std=410core -fshader-stage=vertex --target-env=vulkan1.4 shaders/glsl.vert -o vert.spv

glslc -c -std=410core -fshader-stage=fragment --target-env=vulkan1.4 shaders/glsl.frag -o vert.frag
```

## Running the project:
Compile the source like so:
```
clang -std=c++17 main.cpp -lglfw -lvulkan
```

Run the project with:
```
./a.out
```
