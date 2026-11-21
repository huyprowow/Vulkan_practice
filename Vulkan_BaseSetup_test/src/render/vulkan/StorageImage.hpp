#pragma once

#include "VulkanMemory.hpp"

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

/// Ảnh để compute shader ghi (storage) và shader khác đọc (sampled).
/// Luôn ở layout eGeneral từ lúc tạo: giữa các pass chỉ cần barrier bộ nhớ,
/// không đổi layout, nên nội dung còn nguyên qua các frame.
struct StorageImage {
  // hủy theo thứ tự ngược khai báo: view → image → memory
  vk::raii::DeviceMemory imageMemory{nullptr};
  vk::raii::Image image{nullptr};
  vk::raii::ImageView view{nullptr};
  vk::Format format = vk::Format::eUndefined;
  vk::Extent2D extent{};

  void create(VulkanMemory &memory, uint32_t width, uint32_t height,
              vk::Format imageFormat);
  void destroy();
};