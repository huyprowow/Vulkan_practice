
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

#include "src/core/IRender.hpp"
#include "src/core/Types.hpp"
#include "src/platform/Window.hpp"

#include "src/render/vulkan/VulkanDevice.hpp"
#include "src/render/vulkan/VulkanInstance.hpp"
#include "src/render/vulkan/VulkanRenderer.hpp"
#include "src/render/vulkan/VulkanSwapchain.hpp"

enum class Backend { Vulkan, DX12, WebGPU };

class HelloTriangleApplication {
public:
  void run() {
    initGraphics();
    mainLoop();
    cleanup();
  }

private:
  Window window_{WIDTH, HEIGHT, "Vulkan"};
  VulkanInstance vulkanInstance_;
  VulkanDevice device_;
  VulkanSwapchain swapchain_;
  std::unique_ptr<IRenderer> renderer_;

  Backend chooseBackend() {
    // read config from any where, now hard code vk
    return Backend::Vulkan;
  }

  void initGraphics() {
    Backend backend = chooseBackend();

    switch (backend) {
    case Backend::Vulkan: {
      // vkInstance, device, swapchain, renderer
      vulkanInstance_.init(window_);
      device_.init(vulkanInstance_.getInstance(), vulkanInstance_.getSurface());
      swapchain_.init(device_.getPhysicalDevice(), device_,
                      vulkanInstance_.getSurface(), window_);
      auto vkRenderer = std::make_unique<VulkanRenderer>(); // vk backend
      vkRenderer->init(device_, swapchain_, window_);
      renderer_ = std::move(vkRenderer);
      break;
    }
    case Backend::DX12:
      break;
    case Backend::WebGPU:
      break;
    }
  }

  void mainLoop() {
    while (!window_.shouldClose()) {
      window_.pollEvents();
      renderer_->drawFrame();
    }

    device_.getDevice().waitIdle();
  }

  void cleanup() {
    renderer_->cleanup();
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