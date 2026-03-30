cmake_minimum_required(VERSION 3.16)
project(TestSpeech LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 修复您遇到的 C4819 中文字符集警告
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(test_speech test_speech.cpp)

# 链接 Windows Runtime 所需的核心库
target_link_libraries(test_speech PRIVATE windowsapp)
