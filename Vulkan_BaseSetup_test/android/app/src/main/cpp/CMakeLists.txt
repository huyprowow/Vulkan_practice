cmake_minimum_required(VERSION 3.22.1)

project(Vulkan_Learn_Android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Repo root: Vulkan_BaseSetup_test/
set(PROJ_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
list(APPEND CMAKE_MODULE_PATH "${PROJ_ROOT}/Cmake")


# Shader compilation tools (slangc) 
add_executable(slangc::slangc IMPORTED)
find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)
set_property(TARGET slangc::slangc PROPERTY IMPORTED_LOCATION "${SLANGC_EXECUTABLE}")

# Shader build (compile .slang -> .spv)
# Output matches VulkanRenderer.cpp:
#   shaders/shader.spv
#   shaders/compute.spv
set(SHADER_SOURCE_DIR "${PROJ_ROOT}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

file(GLOB SHADER_SLANG_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/*.slang"
)

set(SHADER_OUTPUTS "")

foreach(SRC_FILE ${SHADER_SLANG_SOURCES})
    get_filename_component(NAME_WE "${SRC_FILE}" NAME_WE)
    set(OUT_FILE "${SHADER_OUTPUT_DIR}/${NAME_WE}.spv")

    # Heuristic: compute/comp shaders use compMain, others use vertMain/fragMain
    set(ENTRY_POINTS -entry vertMain -entry fragMain)
    string(TOLOWER "${NAME_WE}" NAME_WE_LOWER)
    if(NAME_WE_LOWER MATCHES "comp|compute")
        set(ENTRY_POINTS -entry compMain)
    endif()

    add_custom_command(
        OUTPUT "${OUT_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
        COMMAND slangc::slangc
        "${SRC_FILE}"
        -target spirv -profile spirv_1_4
        -emit-spirv-directly -fvk-use-entrypoint-name
        ${ENTRY_POINTS}
        -o "${OUT_FILE}"
        DEPENDS "${SRC_FILE}"
        COMMENT "Compiling ${NAME_WE}.slang -> ${NAME_WE}.spv"
        VERBATIM
    )

    list(APPEND SHADER_OUTPUTS "${OUT_FILE}")
endforeach()

add_custom_target(android_shaders DEPENDS ${SHADER_OUTPUTS})

# GameActivity (Prefab)
find_package(game-activity REQUIRED CONFIG)
find_package(glm REQUIRED)
find_package(tinygltf REQUIRED)
find_package(VulkanHpp REQUIRED)
find_package(stb REQUIRED)
find_package(Ktx REQUIRED)

# Native library
add_library(Vulkan_Learn_Android SHARED
    android_main.cpp

    "${PROJ_ROOT}/src/platform/android/AndroidWindow.cpp"

    "${PROJ_ROOT}/src/render/vulkan/VulkanInstance.cpp"
    "${PROJ_ROOT}/src/render/vulkan/VulkanDevice.cpp"
    "${PROJ_ROOT}/src/render/vulkan/VulkanSwapchain.cpp"
    "${PROJ_ROOT}/src/render/vulkan/VulkanRenderer.cpp"
    "${PROJ_ROOT}/src/render/vulkan/VulkanTexture.cpp"
    "${PROJ_ROOT}/src/render/vulkan/VulkanModel.cpp"
    "${PROJ_ROOT}/src/render/vulkan/MeshData.cpp"
    "${PROJ_ROOT}/src/render/vulkan/VulkanScene.cpp"
    "${PROJ_ROOT}/src/render/vulkan/VulkanParticleSystem.cpp"
    "${PROJ_ROOT}/src/render/vulkan/VulkanMemory.cpp"
    "${PROJ_ROOT}/src/render/vulkan/ThreadCommandPool.cpp"
)
target_link_options(Vulkan_Learn_Android PRIVATE
    "-u" "Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"
)
target_compile_definitions(Vulkan_Learn_Android PRIVATE
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

add_dependencies(Vulkan_Learn_Android android_shaders)

target_include_directories(Vulkan_Learn_Android PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${PROJ_ROOT}"
)

# Android system libs
find_library(android-lib android)
find_library(log-lib log)
find_library(vulkan-lib vulkan)

target_link_libraries(Vulkan_Learn_Android PRIVATE
    game-activity::game-activity
    glm::glm
    VulkanHpp::VulkanHpp
    stb::stb
    tinygltf::tinygltf
    KTX::ktx
    ${android-lib}
    ${log-lib}
    ${vulkan-lib}
)
