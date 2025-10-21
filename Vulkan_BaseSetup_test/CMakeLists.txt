cmake_minimum_required(VERSION 3.10)

project(Vukan_Learn)
# specify the C++ standard
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED True)   

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

# Add option to enable/disable C++ 20 module
option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)

# Enable C++ module dependency scanning only if C++ 20 module is enabled
if(ENABLE_CPP20_MODULE)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

find_package (glfw3 REQUIRED)
find_package (glm REQUIRED)
find_package (tinyobjloader REQUIRED)
find_package (Vulkan REQUIRED)

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
      /std:c++latest      # Use latest C++ standard for better module support
      /permissive-        # Standards conformance mode
      /Zc:__cplusplus     # Enable correct __cplusplus macro
      /EHsc               # Enable C++ exception handling
      /Zc:preprocessor    # Use conforming preprocessor
      /translateInclude   # Automatically translate #include to import for standard library
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

add_executable(Vukan_Learn main.cpp)
target_link_libraries (${PROJECT_NAME} Vulkan::cppm glfw )