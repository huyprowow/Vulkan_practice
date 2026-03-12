#pragma once

#include "../../platform/Window.hpp"
#include "../Types.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"


#include <cstdint>
#include <vector>
#include <vulkan/vulkan_raii.hpp>


class Renderer {
public:
  void init(Device &device, Swapchain &swapchain, Window &window,
            const vk::raii::SurfaceKHR &surface);
  void drawFrame();
  void cleanup();
  void recreateSwapChain();         // gọi khi swapchain out-of-date
  void cleanupSwapChainResources(); // chỉ phần phụ thuộc swapchain
  void createSwapChainDependentResources();

private:
  Device *device_ = nullptr;
  Swapchain *swapchain_ = nullptr;
  Window *window_ = nullptr;
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

  void createDescriptorSetLayout();
  void createGraphicsPipeline();
  [[nodiscard]] vk::raii::ShaderModule
  createShaderModule(const std::vector<char> &code) const;
  static std::vector<char> readFile(const std::string &filename);

  void createCommandPool();
  void createCommandBuffers();
  void recordCommandBuffer(uint32_t imageIndex);

  void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   vk::Format format, vk::ImageTiling tiling,
                   vk::ImageUsageFlags usage,
                   vk::MemoryPropertyFlags properties, vk::raii::Image &image,
                   vk::raii::DeviceMemory &imageMemory);
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
};