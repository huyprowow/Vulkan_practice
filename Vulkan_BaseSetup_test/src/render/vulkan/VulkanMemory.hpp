#pragma once
#include "VulkanDevice.hpp"
#include <vulkan/vulkan_raii.hpp>
/// quản lý cấp phát/copy buffer & image trên GPU.
class VulkanMemory {
public:
  VulkanMemory(VulkanDevice &device, vk::raii::CommandPool &commandPool);
  void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags properties,
                    vk::raii::Buffer &buffer,
                    vk::raii::DeviceMemory &bufferMemory);
  void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer,
                  vk::DeviceSize size);
  void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   vk::SampleCountFlagBits numSamples, vk::Format format,
                   vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                   vk::MemoryPropertyFlags properties, vk::raii::Image &image,
                   vk::raii::DeviceMemory &imageMemory);
  void copyBufferToImage(const vk::raii::Buffer &buffer, vk::raii::Image &image,
                         uint32_t width, uint32_t height);

  void transitionImageLayout(const vk::raii::Image &image,
                             vk::ImageLayout oldLayout,
                             vk::ImageLayout newLayout, uint32_t mipLevels);


  // void generateMipmaps(vk::raii::Image &image, vk::Format imageFormat,
  //                      int32_t texWidth, int32_t texHeight, uint32_t
  //                      mipLevels);
  vk::raii::ImageView createImageView(const vk::raii::Image &image,
                                      vk::Format format,
                                      vk::ImageAspectFlags aspectFlags,
                                      uint32_t mipLevels);

  uint32_t findMemoryType(uint32_t typeFilter,
                          vk::MemoryPropertyFlags properties);
  vk::raii::CommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer);

private:
  VulkanDevice *device_ = nullptr;
  vk::raii::CommandPool *commandPool_ = nullptr;
};