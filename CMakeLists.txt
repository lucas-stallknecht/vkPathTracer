cmake_minimum_required(VERSION 3.28)
project(vkPathTracer VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBS_DIR "${PROJECT_ROOT_DIR}/third_party")
set(SOURCE_DIR "${PROJECT_ROOT_DIR}/src")

file(GLOB_RECURSE SOURCES
        "${SOURCE_DIR}/*.cpp"
        "${SOURCE_DIR}/*.c"
)

add_executable(${PROJECT_NAME} main.cpp ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_WARNING_AS_ERROR ON
)

find_package(Vulkan REQUIRED)

target_include_directories(${PROJECT_NAME}
        PUBLIC ${PROJECT_ROOT_DIR}/include
        PRIVATE ${SOURCE_DIR}
        ${LIBS_DIR}/glfw-3.4.bin.WIN64/include
        ${LIBS_DIR}/tinyobjloader
        ${LIBS_DIR}/stb
        ${Vulkan_INCLUDE_DIRS}
)

add_subdirectory(${LIBS_DIR}/glfw-3.4)
add_subdirectory(${LIBS_DIR}/vma)
add_subdirectory( ${LIBS_DIR}/imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw ${Vulkan_LIBRARIES} GPUOpen::VulkanMemoryAllocator imgui)