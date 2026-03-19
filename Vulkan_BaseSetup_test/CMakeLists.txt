cmake_minimum_required(VERSION 3.10)

project(Vulkan_Learn)
# specify the C++ standard
#set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED True)   

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

# Add option to enable/disable C++ 20 module
option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)

# Enable C++ module dependency scanning only if C++ 20 module is enabled
if(ENABLE_CPP20_MODULE)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(Vulkan REQUIRED)

#slang
# add_executable (glslang::validator IMPORTED)
# find_program (GLSLANG_VALIDATOR "glslangValidator" HINTS $ENV{VULKAN_SDK}/bin REQUIRED)
# set_property (TARGET glslang::validator PROPERTY IMPORTED_LOCATION "${GLSLANG_VALIDATOR}")
find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)


# set up Vulkan C++ module only if enabled
if(ENABLE_CPP20_MODULE)
  add_library(VulkanCppModule)
  add_library(Vulkan::cppm ALIAS VulkanCppModule)

  target_compile_definitions(VulkanCppModule
    PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
  )
  target_include_directories(VulkanCppModule
    PUBLIC
    "${Vulkan_INCLUDE_DIR}"
  )
  target_link_libraries(VulkanCppModule
    PUBLIC
    Vulkan::Vulkan
  )

  set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)

  # Add MSVC-specific compiler options for proper C++ module support
  if(MSVC)
    target_compile_options(VulkanCppModule PRIVATE
      /std:c++latest # Use latest C++ standard for better module support
      /permissive- # Standards conformance mode
      /Zc:__cplusplus # Enable correct __cplusplus macro
      /EHsc # Enable C++ exception handling
      /Zc:preprocessor # Use conforming preprocessor
      /translateInclude # Automatically translate #include to import for standard library
    )
  endif()

  target_sources(VulkanCppModule
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
    BASE_DIRS
    "${Vulkan_INCLUDE_DIR}"
    FILES
    "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
  )


  # Add the vulkan.cppm file directly as a source file
  target_sources(VulkanCppModule
    PRIVATE
    "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
  )
else()
  # Create a dummy interface library when C++ 20 module is disabled
  add_library(VulkanCppModule INTERFACE)
  add_library(Vulkan::cppm ALIAS VulkanCppModule)
  target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
  target_compile_definitions(VulkanCppModule
    INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
  )
endif()

function(add_slang_shader_target TARGET)
  cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})
  set(SHADERS_DIR ${CMAKE_BINARY_DIR}/shaders)
  set(ENTRY_POINTS -entry vertMain -entry fragMain)
  add_custom_command(
    OUTPUT ${SHADERS_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command(
    OUTPUT ${SHADERS_DIR}/slang.spv
    COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
    COMMENT "Compiling Slang Shaders"
    VERBATIM
  )
  add_custom_target(${TARGET}_shader DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()

function(add_to_build CPP_FILE)
  cmake_parse_arguments(ARG
    "" "SHADER" "LIBS;TEXTURES;MODELS" ${ARGN})

  add_executable(${PROJECT_NAME} ${CPP_FILE})
  set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
  # add files to compile
  target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(${PROJECT_NAME} PRIVATE
    src/platform/Window.cpp
    src/render/vulkan/VulkanInstance.cpp 
    src/render/vulkan/VulkanDevice.cpp
    src/render/vulkan/VulkanSwapchain.cpp
    src/render/vulkan/VulkanRenderer.cpp
  )

  # Add compile definition if C++ 20 module is enabled
  if(ENABLE_CPP20_MODULE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CPP20_MODULES=1)
  endif()

  if(WIN32)
    if(${CMAKE_GENERATOR} MATCHES "Visual Studio.*")
      set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/${CHAPTER_NAME}")
    endif()
  endif()

  # process arguments passing to function
  if(DEFINED ARG_LIBS)
    target_link_libraries(${PROJECT_NAME} ${ARG_LIBS})
  endif()

  if(DEFINED ARG_SHADER)
    file(GLOB SHADER_SLANG_SOURCES ${CMAKE_CURRENT_LIST_DIR}/shaders/${ARG_SHADER}.slang)
    if(SHADER_SLANG_SOURCES)
      add_slang_shader_target(${PROJECT_NAME} SOURCES ${SHADER_SLANG_SOURCES})
      add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_shader)
    else()
      message(WARNING "Shader file not found: ${CMAKE_CURRENT_LIST_DIR}/shaders/${ARG_SHADER}.slang")
    endif()
  endif()

  if(DEFINED ARG_MODELS)
    list(TRANSFORM ARG_MODELS PREPEND "${CMAKE_SOURCE_DIR}/assets/")
    file(COPY ${ARG_MODELS} DESTINATION ${CMAKE_BINARY_DIR}/models)
  endif()
  if(DEFINED ARG_TEXTURES)
    list(TRANSFORM ARG_TEXTURES PREPEND "${CMAKE_SOURCE_DIR}/assets/")
    file(COPY ${ARG_TEXTURES} DESTINATION ${CMAKE_BINARY_DIR}/textures)
  endif()

endfunction()



add_to_build(main.cpp
  LIBS Vulkan::cppm glfw glm::glm
  SHADER shader
  TEXTURES textures/viking_room.png
  MODELS models/viking_room.obj
)

# --- Documentation (Doxygen + Graphviz) ---
option(BUILD_DOCS "Build documentation" OFF)

if(BUILD_DOCS)
  find_package(Doxygen REQUIRED)

  add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_SOURCE_DIR}/docs/Doxyfile"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/docs"
    COMMENT "Generating documentation"
    VERBATIM
  )
endif()
