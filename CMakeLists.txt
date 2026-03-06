cmake_minimum_required(VERSION 3.20)
project(AirWatch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# -------------------------
# Dependencies
# -------------------------

# nlohmann/json
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# cpp-httplib (header-only)
FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(cpp_httplib)

# GLFW + glad + OpenSSL via vcpkg (reliable on Windows)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad  CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# OpenGL
find_package(OpenGL REQUIRED)

# Dear ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

# Build imgui as a library (GLFW + OpenGL3 backend)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

add_library(imgui_lib
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# ImGui backend needs GLFW headers
if (TARGET glfw)
  target_link_libraries(imgui_lib PUBLIC glfw)
elseif (TARGET glfw3::glfw3)
  target_link_libraries(imgui_lib PUBLIC glfw3::glfw3)
else()
  message(FATAL_ERROR "Could not find GLFW target (glfw or glfw3::glfw3).")
endif()

# -------------
# App target
# -------------
add_executable(AirWatch
    src/main.cpp
    src/coingecko_client.cpp
    src/storage.cpp
    src/app_state.cpp
)

target_include_directories(AirWatch PRIVATE include)

if (TARGET glfw)
  set(GLFW_TARGET glfw)
elseif (TARGET glfw3::glfw3)
  set(GLFW_TARGET glfw3::glfw3)
else()
  message(FATAL_ERROR "Could not find GLFW target (glfw or glfw3::glfw3).")
endif()

target_link_libraries(AirWatch PRIVATE
    imgui_lib
    ${GLFW_TARGET}
    glad::glad
    nlohmann_json::nlohmann_json
    httplib::httplib
    OpenGL::GL
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Enable HTTPS in cpp-httplib
target_compile_definitions(AirWatch PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# Platform specifics
if (MSVC)
  target_compile_options(AirWatch PRIVATE /W4)
else()
  target_compile_options(AirWatch PRIVATE -Wall -Wextra -Wpedantic)
endif()
