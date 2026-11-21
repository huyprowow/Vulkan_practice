# Dear ImGui dùng chung cho mọi nền tảng: lõi + backend Vulkan.
# Backend nền tảng: GLFW cho desktop, Android cho Android; iOS tự đưa input vào.
# Nơi include module này tự link Vulkan (và GLFW nếu là desktop) vào target imgui.
include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
add_library(imgui::imgui ALIAS imgui)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR}
                                        ${imgui_SOURCE_DIR}/backends)

if(ANDROID)
  target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_android.cpp)
elseif(NOT IOS)
  target_sources(imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
endif()