cmake_minimum_required(VERSION 3.16)
project(JpTypingApp VERSION 1.0.0 LANGUAGES CXX)

# 强制使用 C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 开启严格警告 (大厂基操)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /utf-8")
# 引入 FetchContent 模块
include(FetchContent)

# 声明 GLFW 的仓库和版本 (使用稳定的 3.3.8 版本)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)

# 关闭 GLFW 不必要的测试和例子，加快编译速度
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# 使 GLFW 在当前工程中可用
FetchContent_MakeAvailable(glfw)

# 配置 ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

# 查找操作系统自带的 OpenGL 库
find_package(OpenGL REQUIRED)
# 主程序
add_executable(${PROJECT_NAME} src/main.cpp src/AppUI.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE include)
# 将 imgui 和系统的 OpenGL::GL 一并链接给主程序
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE imgui OpenGL::GL ws2_32 imm32)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE imgui OpenGL::GL)
endif()