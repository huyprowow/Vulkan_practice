#pragma once
#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

/// Chuyển đổi layout image bằng VkImageMemoryBarrier2 (Vulkan 1.3
/// synchronization2). Chỉ ghi barrier vào command buffer đang mở, không tự
/// submit (khác VulkanMemory::transitionImageLayout).
inline void cmdTransitionImageLayout(
    const vk::raii::CommandBuffer &commandBuffer, vk::Image image,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
    vk::ImageAspectFlags image_aspect_flags, uint32_t mipLevels) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {.aspectMask = image_aspect_flags,
                           .baseMipLevel = 0,
                           .levelCount = mipLevels,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  vk::DependencyInfo dependencyInfo = {.dependencyFlags = {},
                                       .imageMemoryBarrierCount = 1,
                                       .pImageMemoryBarriers = &barrier};
  commandBuffer.pipelineBarrier2(dependencyInfo);
}


/// Barrier bộ nhớ toàn cục: pass sau chờ pass trước ghi xong. Dùng cho buffer
/// (SSBO), hoặc cho ảnh khi không đổi layout.
inline void cmdMemoryBarrier(const vk::raii::CommandBuffer &commandBuffer,
                             vk::AccessFlags2 srcAccessMask,
                             vk::AccessFlags2 dstAccessMask,
                             vk::PipelineStageFlags2 srcStageMask,
                             vk::PipelineStageFlags2 dstStageMask) {
  vk::MemoryBarrier2 barrier{.srcStageMask = srcStageMask,
                             .srcAccessMask = srcAccessMask,
                             .dstStageMask = dstStageMask,
                             .dstAccessMask = dstAccessMask};
  vk::DependencyInfo dependencyInfo{.memoryBarrierCount = 1,
                                    .pMemoryBarriers = &barrier};
  commandBuffer.pipelineBarrier2(dependencyInfo);
}