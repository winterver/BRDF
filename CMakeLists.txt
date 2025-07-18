cmake_minimum_required(VERSION 4.0)
project(brdf)

if (NOT WIN32)
    message(FATAL_ERROR "CMakeLists.txt is for Windows, use meson.build on *nix instead")
endif()

set(GLFW_HEADER C:/Users/I/vimfiles/.library)
set(GLFW_LIBRARY C:/Users/I/vimfiles/.library)
set(GLM_HEADER D:/Application/VulkanSDK/1.3.283.0/Include)
if(NOT EXISTS ${GLFW_HEADER})
    message(FATAL_ERROR "Please set library paths")
elseif(NOT EXISTS ${GLFW_LIBRARY})
    message(FATAL_ERROR "Please set library paths")
elseif(NOT EXISTS ${GLM_HEADER})
    message(FATAL_ERROR "Please set library paths")
endif()
include_directories(${GLFW_HEADER} ${GLM_HEADER})
link_directories(${GLFW_LIBRARY})

add_executable(brdf
               src/main.cpp
               src/glad.c
               src/header_only_impl.cpp)

target_link_libraries(brdf glfw3)
target_compile_options(brdf PRIVATE /MD)
