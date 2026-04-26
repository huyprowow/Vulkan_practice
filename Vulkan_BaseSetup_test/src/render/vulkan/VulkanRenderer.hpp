#pragma once

#include "../../core/IRender.hpp"
#include "../../core/Types.hpp"
#include "../../platform/IWindow.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapchain.hpp"

#include <cstdint>
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

  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;
  vk::raii::Buffer vertexBuffer_{nullptr};
  vk::raii::DeviceMemory vertexBufferMemory_{nullptr};
  vk::raii::Buffer indexBuffer_{nullptr};
  vk::raii::DeviceMemory indexBufferMemory_{nullptr};

  std::vector<vk::raii::Buffer> uniformBuffers_;
  std::vector<vk::raii::DeviceMemory> uniformBuffersMemory_;
  std::vector<void *> uniformBuffersMapped_;

  vk::raii::DescriptorPool descriptorPool_{nullptr};
  std::vector<vk::raii::DescriptorSet> descriptorSets_;

  uint32_t frameIndex_ = 0;

  uint32_t mipLevels_ = 0;
  vk::raii::Image textureImage_{nullptr};
  vk::raii::DeviceMemory textureImageMemory_{nullptr};
  vk::raii::ImageView textureImageView_{nullptr};
  vk::raii::Sampler textureSampler_{nullptr};

  vk::raii::Image depthImage_{nullptr};
  vk::raii::DeviceMemory depthImageMemory_{nullptr};
  vk::raii::ImageView depthImageView_{nullptr};

  vk::raii::Image colorImage_{nullptr};
  vk::raii::DeviceMemory colorImageMemory_{nullptr};
  vk::raii::ImageView colorImageView_{nullptr};

  std::vector<vk::raii::Buffer> shaderStorageBuffers_;
  std::vector<vk::raii::DeviceMemory> shaderStorageBuffersMemory_;

  // Compute pipeline
  vk::raii::DescriptorSetLayout computeDescriptorSetLayout_{nullptr};
  vk::raii::PipelineLayout computePipelineLayout_{nullptr};
  vk::raii::Pipeline computePipeline_{nullptr};
  std::vector<vk::raii::DescriptorSet> computeDescriptorSets_;
  std::vector<vk::raii::CommandBuffer> computeCommandBuffers_;

  // Particle graphics pipeline
  vk::raii::PipelineLayout particlePipelineLayout_{nullptr};
  vk::raii::Pipeline particlePipeline_{nullptr};

  // Compute UBO (deltaTime riêng, không sửa UBO model)
  std::vector<vk::raii::Buffer> computeUniformBuffers_;
  std::vector<vk::raii::DeviceMemory> computeUniformBuffersMemory_;
  std::vector<void *> computeUniformBuffersMapped_;

  // Compute sync
  std::vector<vk::raii::Fence> computeInFlightFences_;
  std::vector<vk::raii::Semaphore> computeFinishedSemaphores_;

  float lastFrameTime_ = 0.0f;

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

  void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   vk::SampleCountFlagBits numSamples, vk::Format format,
                   vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                   vk::MemoryPropertyFlags properties, vk::raii::Image &image,
                   vk::raii::DeviceMemory &imageMemory);

  void createColorResources();

  void createDepthResources();

  vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates,
                                 vk::ImageTiling tiling,
                                 vk::FormatFeatureFlags features);
  vk::Format findDepthFormat();
  bool hasStencilComponent(vk::Format format);

  void createTextureImage();
  void generateMipmaps(vk::raii::Image &image, vk::Format imageFormat,
                       int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
  vk::raii::ImageView createImageView(const vk::raii::Image &image,
                                      vk::Format format,
                                      vk::ImageAspectFlags aspectFlags,
                                      uint32_t mipLevels);
  void createTextureImageView();
  void createTextureSampler();

  vk::raii::CommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer);

  void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags properties,
                    vk::raii::Buffer &buffer,
                    vk::raii::DeviceMemory &bufferMemory);
  void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer,
                  vk::DeviceSize size);
  void copyBufferToImage(const vk::raii::Buffer &buffer, vk::raii::Image &image,
                         uint32_t width, uint32_t height);

  void transitionImageLayout(const vk::raii::Image &image,
                             vk::ImageLayout oldLayout,
                             vk::ImageLayout newLayout, uint32_t mipLevels);
  void transition_image_layout(vk::Image image, vk::ImageLayout oldLayout,
                               vk::ImageLayout newLayout,
                               vk::AccessFlags2 srcAccessMask,
                               vk::AccessFlags2 dstAccessMask,
                               vk::PipelineStageFlags2 srcStageMask,
                               vk::PipelineStageFlags2 dstStageMask,
                               vk::ImageAspectFlags image_aspect_flags,
                               uint32_t mipLevels);

  void loadModel();
  void createVertexBuffer();
  void createIndexBuffer();
  void createUniformBuffers();
  void createDescriptorPool();
  void createDescriptorSets();
  uint32_t findMemoryType(uint32_t typeFilter,
                          vk::MemoryPropertyFlags properties);

  void createSyncObjects();
  void updateUniformBuffer(uint32_t currentImage);

  void createShaderStorageBuffers();
  void createComputeUniformBuffers();

  void createComputeDescriptorSetLayout();
  void createComputePipeline();
  void createParticleGraphicsPipeline();
  void createComputeDescriptorSets();
  void createComputeCommandBuffers();

  void recordComputeCommandBuffer();
  void updateComputeUniformBuffer(uint32_t currentFrame);
};