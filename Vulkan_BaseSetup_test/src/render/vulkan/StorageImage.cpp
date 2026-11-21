#include "StorageImage.hpp"
#include "VulkanBarrier.hpp"

void StorageImage::create(VulkanMemory &memory, uint32_t width,
                          uint32_t height, vk::Format imageFormat) {
  format = imageFormat;
  extent = vk::Extent2D{width, height};
  memory.createImage(width, height, 1, vk::SampleCountFlagBits::e1, format,
                     vk::ImageTiling::eOptimal,
                     vk::ImageUsageFlagBits::eStorage |
                         vk::ImageUsageFlagBits::eSampled,
                     vk::MemoryPropertyFlagBits::eDeviceLocal, image,
                     imageMemory);
  view = memory.createImageView(image, format, vk::ImageAspectFlagBits::eColor,
                                1);

  // chuyển 1 lần sang eGeneral; nội dung ban đầu không xác định
  auto cmd = memory.beginSingleTimeCommands();
  cmdTransitionImageLayout(
      cmd, *image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, {},
      vk::AccessFlagBits2::eShaderStorageWrite |
          vk::AccessFlagBits2::eShaderStorageRead |
          vk::AccessFlagBits2::eShaderSampledRead,
      vk::PipelineStageFlagBits2::eNone,
      vk::PipelineStageFlagBits2::eComputeShader |
          vk::PipelineStageFlagBits2::eFragmentShader,
      vk::ImageAspectFlagBits::eColor, 1);
  memory.endSingleTimeCommands(cmd);
}

void StorageImage::destroy() {
  view = nullptr;
  image = nullptr;
  imageMemory = nullptr;
}