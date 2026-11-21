#pragma once

#include "../../core/ISample.hpp"
#include "VulkanParticleSystem.hpp"
#include "../../render/vulkan/VulkanScene.hpp"
#include "../../render/vulkan/VulkanTexture.hpp"

#include <vector>
#include <vulkan/vulkan_raii.hpp>

/// Sample đầu tiên: 3 viking room (MSAA, texture KTX2) + particle compute đa luồng.
class RasterBasicSample : public ISample {
public:
  void init(const SampleContext &ctx) override;
  void update(float dtSeconds) override;
  SampleWait submitBeforeGraphics(uint32_t frameIndex) override;
  void record(const SampleFrame &frame) override;
  void onSwapchainRecreated() override;
  void ui() override;
  void shutdown() override;

private:
  VulkanDevice *device_ = nullptr;
  VulkanMemory *memory_ = nullptr;
  VulkanSwapchain *swapchain_ = nullptr;
  std::mutex *queueSubmitMutex_ = nullptr;
#if defined(__ANDROID__)
  AAssetManager *assetManager_ = nullptr;
#endif

  VulkanTexture texture_;
  VulkanScene scene_;
  VulkanParticleSystem particleSystem_;

  vk::raii::DescriptorSetLayout descriptorSetLayout_{nullptr};
  vk::raii::PipelineLayout pipelineLayout_{nullptr};
  vk::raii::Pipeline graphicsPipeline_{nullptr};
  vk::raii::DescriptorPool descriptorPool_{nullptr};

  float dtSeconds_ = 0.0f; ///< dt của frame; particle ghi vào UBO sau khi chờ fence
  float timeScale_ = 1.0f; ///< chỉnh bằng slider trong ui()

  void createDescriptorSetLayout();
  void createGraphicsPipeline();
  void createUniformBuffers();
  void createDescriptorPool();
  void createDescriptorSets();
  void updateUniformBuffer(uint32_t currentImage);
};