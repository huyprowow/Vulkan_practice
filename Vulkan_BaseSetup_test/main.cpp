
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
#include "src/core/Time.hpp"
#include "src/core/Types.hpp"
#include "src/platform/desktop/Window.hpp"

#include "src/render/vulkan/VulkanDevice.hpp"
#include "src/render/vulkan/VulkanInstance.hpp"
#include "src/render/vulkan/VulkanRenderer.hpp"
#include "src/render/vulkan/VulkanSwapchain.hpp"
#include "src/samples/raster_basic/RasterBasicSample.hpp"
#include "src/samples/Samples.hpp"
#include "src/ui/ImGuiLayer.hpp"
#include "src/ui/SandboxUi.hpp"

enum class Backend { Vulkan, DX12, WebGPU };

class Application {
public:
  void run() {
    initGraphics();
    try {
      mainLoop();
    } catch (...) {
      cleanup(); // dừng thread particle, chờ GPU xong rồi mới cho lỗi đi tiếp
      throw;
    }
    cleanup();
  }

private:
  Window window_{WIDTH, HEIGHT, "Vulkan"};
  VulkanInstance vulkanInstance_;
  VulkanDevice device_;
  VulkanSwapchain swapchain_;
  std::unique_ptr<ISample> sample_;
  std::unique_ptr<IRenderer> renderer_;
  VulkanRenderer *vkRenderer_ = nullptr; ///< renderer_ ở dạng Vulkan, để gọi setSample
  ImGuiLayer imgui_;
  FrameTimer frameTimer_;
  int sampleIndex_ = 0;     ///< vị trí trong SAMPLES
  int nextSampleIndex_ = 0; ///< chọn trong menu, đổi sau khi dựng xong UI

  Backend chooseBackend() {
    // read config from any where, now hard code vk
    return Backend::Vulkan;
  }

  void initGraphics() {
    Backend backend = chooseBackend();

    switch (backend) {
    case Backend::Vulkan: {
      // vkInstance, device, swapchain, renderer
      vulkanInstance_.init(window_); // IWindow
      device_.init(vulkanInstance_.getInstance(), vulkanInstance_.getSurface());
      swapchain_.init(device_.getPhysicalDevice(), device_,
                      vulkanInstance_.getSurface(), window_);
      sample_ = SAMPLES[sampleIndex_].create();
      auto vkRenderer = std::make_unique<VulkanRenderer>(); // vk backend
      vkRenderer->init(device_, swapchain_, window_);
      vkRenderer->setSample(sample_.get());
      imgui_.init(window_, vulkanInstance_.getInstance(), device_, swapchain_);
      vkRenderer->setOverlay([this](const SampleFrame &frame) {
        imgui_.record(frame.cmd, frame.swapchainView, frame.extent);
      });
      vkRenderer_ = vkRenderer.get();
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

      if (window_.wasResized()) {// glfw window resized, recreate swapchain
        window_.clearResized();
        renderer_->recreateSwapChain();
      }
     
      frameTimer_.tick();
      imgui_.beginFrame();
      nextSampleIndex_ = drawSandboxUi(sampleIndex_, *sample_);
      imgui_.endFrame();
      switchSampleIfRequested();
      sample_->update(frameTimer_.deltaSeconds());
      renderer_->drawFrame();
    }

    device_.getDevice().waitIdle();
  }


  void switchSampleIfRequested() {
    if (nextSampleIndex_ == sampleIndex_) {
      return;
    }
    auto next = SAMPLES[nextSampleIndex_].create();
    vkRenderer_->setSample(next.get()); // shutdown sample cũ trước khi hủy nó
    sample_ = std::move(next);
    sampleIndex_ = nextSampleIndex_;
  }

  void cleanup() {
    device_.getDevice().waitIdle(); // GPU xong frame cuối rồi mới hủy tài nguyên ImGui
    imgui_.shutdown();
    renderer_->cleanup();
    swapchain_.cleanup();
  }
};

int main() {
  Application app;

  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}