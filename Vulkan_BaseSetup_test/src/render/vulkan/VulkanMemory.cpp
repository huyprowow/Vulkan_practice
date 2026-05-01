#include "VulkanMemory.hpp"
#include <stdexcept>

VulkanMemory::VulkanMemory(VulkanDevice &device,
                           vk::raii::CommandPool &commandPool)
    : device_(&device), commandPool_(&commandPool) {}

void VulkanMemory::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                vk::MemoryPropertyFlags properties,
                                vk::raii::Buffer &buffer,
                                vk::raii::DeviceMemory &bufferMemory) {
  vk::BufferCreateInfo bufferInfo{
      .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
  buffer = vk::raii::Buffer(device_->getDevice(), bufferInfo);
  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryType(memRequirements.memoryTypeBits, properties)};
  bufferMemory = vk::raii::DeviceMemory(device_->getDevice(), allocInfo);
  buffer.bindMemory(*bufferMemory, 0);
}

void VulkanMemory::copyBuffer(vk::raii::Buffer &srcBuffer,
                              vk::raii::Buffer &dstBuffer,
                              vk::DeviceSize size) {
  vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
  commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer,
                               vk::BufferCopy(0, 0, size));
  endSingleTimeCommands(commandCopyBuffer);
}

void VulkanMemory::createImage(uint32_t width, uint32_t height,
                               uint32_t mipLevels,
                               vk::SampleCountFlagBits numSamples,
                               vk::Format format, vk::ImageTiling tiling,
                               vk::ImageUsageFlags usage,
                               vk::MemoryPropertyFlags properties,
                               vk::raii::Image &image,
                               vk::raii::DeviceMemory &imageMemory) {
  vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
                                .format = format,
                                .extent = {width, height, 1},
                                .mipLevels = mipLevels,
                                .arrayLayers = 1,
                                .samples = numSamples,
                                .tiling = tiling,
                                .usage = usage,
                                .sharingMode = vk::SharingMode::eExclusive};

  image = vk::raii::Image(device_->getDevice(), imageInfo);

  vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryType(memRequirements.memoryTypeBits, properties)};
  imageMemory = vk::raii::DeviceMemory(device_->getDevice(), allocInfo);
  image.bindMemory(imageMemory, 0);
}

void VulkanMemory::copyBufferToImage(const vk::raii::Buffer &buffer,
                                     vk::raii::Image &image, uint32_t width,
                                     uint32_t height) {
  vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
  vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1}};

  commandBuffer.copyBufferToImage(
      buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
  // Submit the buffer copy to the graphics queue
  endSingleTimeCommands(commandBuffer);
}

/// Chuyển đổi layout image bằng pipeline barrier (dùng cho texture upload)
void VulkanMemory::transitionImageLayout(const vk::raii::Image &image,
                                         vk::ImageLayout oldLayout,
                                         vk::ImageLayout newLayout,
                                         uint32_t mipLevels) {
  // vk cho phep chuyen doi bo cuc toi uu cho tung nhiem vu
  auto commandBuffer = beginSingleTimeCommands();

  vk::ImageMemoryBarrier barrier{
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .image = *image, // dereference RAII image -> raw VkImage
      .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0,
                           mipLevels, // levelCount
                           0, 1}};

  // 0 xd -> transfer dst: thao tac ghi k can cho
  // transfer dst -> shader read: doc shader can cho ghi dl transfer xong
  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined &&
      newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr,
                                barrier);
  endSingleTimeCommands(commandBuffer);
}

/// Overload for RAII images
/// de su dung cho texture image no dung vk::raii::Image nen phai nap chong
vk::raii::ImageView
VulkanMemory::createImageView(const vk::raii::Image &image, vk::Format format,
                              vk::ImageAspectFlags aspectFlags,
                              uint32_t mipLevels) {
  vk::ImageViewCreateInfo viewInfo{.image = *image,
                                   .viewType = vk::ImageViewType::e2D,
                                   .format = format,
                                   .subresourceRange = {aspectFlags, 0,
                                                        mipLevels, // levelCount
                                                        0, 1}};
  return vk::raii::ImageView(device_->getDevice(), viewInfo);
}

uint32_t VulkanMemory::findMemoryType(uint32_t typeFilter,
                                      vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties =
      device_->getPhysicalDevice().getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

vk::raii::CommandBuffer VulkanMemory::beginSingleTimeCommands() {
  vk::CommandBufferAllocateInfo allocInfo{.commandPool = *commandPool_,
                                          .level =
                                              vk::CommandBufferLevel::ePrimary,
                                          .commandBufferCount = 1};
  vk::raii::CommandBuffer commandBuffer =
      std::move(device_->getDevice().allocateCommandBuffers(allocInfo).front());

  vk::CommandBufferBeginInfo beginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffer.begin(beginInfo);

  return commandBuffer;
}

void VulkanMemory::endSingleTimeCommands(
    vk::raii::CommandBuffer &commandBuffer) {
  commandBuffer.end();

  vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                            .pCommandBuffers = &*commandBuffer};
  device_->getGraphicsQueue().submit(submitInfo, nullptr);
  device_->getGraphicsQueue().waitIdle();
}