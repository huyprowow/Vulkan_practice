#pragma once

#include "../../core/IRender.hpp"
#include "../../core/Types.hpp"
#include "../../platform/IWindow.hpp"
#include "VulkanDevice.hpp"
#include "VulkanMemory.hpp"
#include "VulkanParticleSystem.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanTexture.hpp"
#include "VulkanModel.hpp"

#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#if defined(__ANDROID__)
struct AAssetManager;
#endif

class VulkanRenderer : public IRenderer {
public:
  void init(VulkanDevice &device, VulkanSwapchain &swapchain, IWindow &window,
#if defined(__ANDROID__)
            AAssetManager *assetManager
#else
            void *assetManager = nullptr
#endif
  );
  void drawFrame() override;
  void cleanup() override;
  void recreateSwapChain() override; ///< gọi khi swapchain out-of-date
  void cleanupSwapChainResources();  ///< chỉ phần phụ thuộc swapchain
  void createSwapChainDependentResources();

private:
  VulkanDevice *device_ = nullptr;
  VulkanSwapchain *swapchain_ = nullptr;
  IWindow *window_ = nullptr;
  std::unique_ptr<VulkanMemory> memory_=nullptr;
  VulkanTexture texture_;
  VulkanModel model_;
  VulkanParticleSystem particleSystem_;

#if defined(__ANDROID__)
  AAssetManager *assetManager_ = nullptr;
#endif

  const vk::raii::SurfaceKHR *surface_ = nullptr;

  vk::raii::DescriptorSetLayout descriptorSetLayout_{nullptr};
  vk::raii::PipelineLayout pipelineLayout_{nullptr};
  vk::raii::Pipeline graphicsPipeline_{nullptr};
  vk::raii::CommandPool commandPool_{nullptr};
  std::vector<vk::raii::CommandBuffer> commandBuffers_;

  std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
  std::vector<vk::raii::Fence> inFlightFences_;

  std::vector<vk::raii::Buffer> uniformBuffers_;
  std::vector<vk::raii::DeviceMemory> uniformBuffersMemory_;
  std::vector<void *> uniformBuffersMapped_;

  vk::raii::DescriptorPool descriptorPool_{nullptr};
  std::vector<vk::raii::DescriptorSet> descriptorSets_;

  std::vector<GameObject> gameObjects_;

  uint32_t frameIndex_ = 0;

  void createDescriptorSetLayout();
  void createGraphicsPipeline();
  [[nodiscard]] vk::raii::ShaderModule
  createShaderModule(const std::vector<char> &code) const;

  static std::vector<char> readFile(const std::string &filename
#if defined(__ANDROID__)
                                    ,
                                    AAssetManager *assetManager
#endif
  );

  void createCommandPool();
  void createCommandBuffers();
  void recordCommandBuffer(uint32_t imageIndex);

  void transition_image_layout(vk::Image image, vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout,
                               vk::AccessFlags2 srcAccessMask,
                               vk::AccessFlags2 dstAccessMask,
                               vk::PipelineStageFlags2 srcStageMask,
                               vk::PipelineStageFlags2 dstStageMask,
                               vk::ImageAspectFlags image_aspect_flags,
                               uint32_t mipLevels);
  void setupGameObjects();
  void createUniformBuffers();
  void createDescriptorPool();
  void createDescriptorSets();

  void createSyncObjects();
  void updateUniformBuffer(uint32_t currentImage);

};