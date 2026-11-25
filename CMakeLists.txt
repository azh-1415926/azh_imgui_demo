cmake_minimum_required(VERSION 3.16)
project(ToolFramework LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

# ---------- GLAD ----------
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# ---------- GLFW ----------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

# ---------- ImGui ----------
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    third_party/imgui
    third_party/imgui/backends
)
target_link_libraries(imgui PUBLIC glfw glad)

# ---------- NFD Extended ----------
set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/nativefiledialog-extended)

# ---------- Application ----------
add_executable(ToolFramework
    src/main.cpp
    src/App.cpp
    src/modules/ProductExportModule.cpp
)
target_include_directories(ToolFramework PRIVATE src)
target_link_libraries(ToolFramework PRIVATE
    glfw glad imgui nfd OpenGL::GL
)