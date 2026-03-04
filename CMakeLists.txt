cmake_minimum_required(VERSION 3.24)

project(GaussianClouds
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Dependencies via FetchContent ---
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

# GLM (header-only math library)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glfw glm imgui)

# --- Dear ImGui (not a CMake project, build manually) ---
add_library(imgui OBJECT
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# --- GLAD (generated OpenGL 4.5 core loader) ---
add_library(glad OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/gl.c
)
target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)

# --- Main executable ---
add_executable(GaussianClouds
    src/main.cpp
    src/Shader.cpp
    src/Renderer.cpp
    src/Camera.cpp
    src/PLYLoader.cpp
    src/PointCloudRenderer.cpp
    src/SplatRenderer.cpp
)

target_include_directories(GaussianClouds PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(GaussianClouds PRIVATE
    glfw
    glm::glm
    glad
    imgui
    opengl32
)

# Copy shaders next to the executable
add_custom_command(TARGET GaussianClouds POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:GaussianClouds>/shaders
    COMMENT "Copying shaders to output directory"
)

# --- Future CUDA support ---
# To enable CUDA:
# 1. Add CUDA to project languages: project(GaussianClouds LANGUAGES CXX CUDA)
# 2. Uncomment:
# find_package(CUDAToolkit REQUIRED)
# target_sources(GaussianClouds PRIVATE cuda/gaussian_kernel.cu)
# target_link_libraries(GaussianClouds PRIVATE CUDA::cudart)
# set_target_properties(GaussianClouds PROPERTIES
#     CUDA_SEPARABLE_COMPILATION ON
#     CUDA_ARCHITECTURES "75;86;89"
# )
