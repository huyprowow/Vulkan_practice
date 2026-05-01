#pragma once
#include "../../core/Types.hpp"
#include "VulkanDevice.hpp"
#include "VulkanMemory.hpp"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

///  quản lý compute shader + particle rendering (Ch.11 Khronos tutorial).
class VulkanParticleSystem {
public:
  /// Khởi tạo toàn bộ particle system
  /// @param device VulkanDevice đã init
  /// @param memory VulkanMemory để cấp/copy buffer
  /// @param commandPool Command pool dùng chung (lấy từ Renderer)
  /// @param colorFormat Format swapchain image (cho dynamic rendering)
  /// @param depthFormat Format depth attachment
  /// @param msaaSamples Số sample MSAA (cùng với main graphics pipeline)
  /// @param computeShaderSpv Bytes SPIR-V của compute.spv (Renderer load)
  void init(VulkanDevice &device, VulkanMemory &memory,
            vk::raii::CommandPool &commandPool, vk::Format colorFormat,
            vk::Format depthFormat, vk::SampleCountFlagBits msaaSamples,
            const std::vector<char> &computeShaderSpv);
  void updateComputeUniformBuffer(uint32_t currentFrame);
  void recordComputeCommandBuffer(uint32_t frameIndex);
  /// Record particle draw vào primary command buffer của frame
  /// (Renderer gọi sau khi vẽ model, trong cùng beginRendering scope)
  void recordDraw(vk::raii::CommandBuffer &cmdBuf, uint32_t frameIndex);
  
  /// Truy cập sync objects và compute command buffer (Renderer dùng để submit)
  const vk::raii::CommandBuffer &
  computeCommandBuffer(uint32_t frameIndex) const;
  const vk::raii::Semaphore &
  computeFinishedSemaphore(uint32_t frameIndex) const;
  const vk::raii::Fence &computeInFlightFence(uint32_t frameIndex) const;
  
  /// Reset compute sync objects (gọi khi recreateSwapChain)
  void resetSwapChainResources();

  /// Cleanup tất cả resource khi shutdown
  void cleanup();

private:
  VulkanDevice *device_ = nullptr;
  VulkanMemory *memory_ = nullptr;
  vk::raii::CommandPool *commandPool_ = nullptr;

  // Compute pipeline
  vk::raii::DescriptorSetLayout computeDescriptorSetLayout_{nullptr};
  vk::raii::PipelineLayout computePipelineLayout_{nullptr};
  vk::raii::Pipeline computePipeline_{nullptr};
  
  // Particle graphics pipeline
  vk::raii::PipelineLayout particlePipelineLayout_{nullptr};
  vk::raii::Pipeline particlePipeline_{nullptr};
  
  // Descriptor pool RIÊNG (không share với Renderer): UBO + 2 SSBO ping-pong
  vk::raii::DescriptorPool descriptorPool_{nullptr};
  std::vector<vk::raii::DescriptorSet> computeDescriptorSets_;


  // SSBO ping-pong + compute UBO
  std::vector<vk::raii::Buffer> shaderStorageBuffers_;
  std::vector<vk::raii::DeviceMemory> shaderStorageBuffersMemory_;
  // Compute UBO (deltaTime riêng, không sửa UBO model)
  std::vector<vk::raii::Buffer> computeUniformBuffers_;
  std::vector<vk::raii::DeviceMemory> computeUniformBuffersMemory_;
  std::vector<void *> computeUniformBuffersMapped_;
  
  // Compute command buffers 
  std::vector<vk::raii::CommandBuffer> computeCommandBuffers_;

  // Compute sync
  std::vector<vk::raii::Fence> computeInFlightFences_;
  std::vector<vk::raii::Semaphore> computeFinishedSemaphores_;

  float lastFrameTime_ = 0.0f;

  [[nodiscard]] vk::raii::ShaderModule
  createShaderModule(const std::vector<char> &code) const;

  void createComputeDescriptorSetLayout();
  void createShaderStorageBuffers();
  void createComputePipeline(const std::vector<char> &spv);
  void createParticleGraphicsPipeline(const std::vector<char> &spv,
                                      vk::Format colorFormat,
                                      vk::Format depthFormat,
                                      vk::SampleCountFlagBits msaaSamples);
  void createComputeUniformBuffers();
  void createComputeDescriptorPool();
  void createComputeDescriptorSets();
  void createComputeCommandBuffers();
  void createComputeSyncObjects();
};