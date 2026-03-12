
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

// #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES // tu align memory cua uniform
// shader
//                                            // va struct trong Code (luu y: no
//                                            // del hd voi truct long nhau =>
//                                            neu
//                                            // co long nhau phai dung alignas
//                                            // (example: alignas(16) Foo
//                                            f2))=>
//                                            // luon chi ro align. clang hien
//                                            tai dang k sp :v nen comment vao
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <chrono>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "src/platform/Window.hpp"
#include "src/render/Types.hpp"
#include "src/render/vulkan/Device.hpp"
#include "src/render/vulkan/Instance.hpp"
#include "src/render/vulkan/Renderer.hpp"
#include "src/render/vulkan/Swapchain.hpp"


class HelloTriangleApplication {
public:
  void run() {
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  Window window_{WIDTH, HEIGHT, "Vulkan"};
  VulkanInstance vulkanInstance_;
  Device device_;
  Swapchain swapchain_;
  Renderer renderer_;

  void initVulkan() {
    vulkanInstance_.init(window_);
    device_.init(vulkanInstance_.getInstance(), vulkanInstance_.getSurface());
    swapchain_.init(device_.getPhysicalDevice(), device_,
                    vulkanInstance_.getSurface(), window_);
    renderer_.init(device_, swapchain_, window_, vulkanInstance_.getSurface());
  }


  void mainLoop() {
    while (!window_.shouldClose()) {
      window_.pollEvents();
      renderer_.drawFrame();
    }

    device_.getDevice().waitIdle();
  }

  void cleanup() {
    renderer_.cleanup();
    swapchain_.cleanup();
  }
};

int main() {
  HelloTriangleApplication app;

  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}