#pragma once

#include <cstdint>
#include <mutex>
#include <vulkan/vulkan_raii.hpp>

class VulkanDevice;
class VulkanMemory;
class VulkanSwapchain;
#if defined(__ANDROID__)
struct AAssetManager;
#endif

/// Những gì renderer đưa cho sample lúc init. Sample chỉ dùng những thứ này,
/// không lục vào bên trong VulkanRenderer.
struct SampleContext {
  VulkanDevice &device;
  VulkanMemory &memory;
  VulkanSwapchain &swapchain;
  vk::raii::CommandPool &commandPool;
  std::mutex &queueSubmitMutex; ///< khóa mọi lần submit/present lên queue
#if defined(__ANDROID__)
  AAssetManager *assetManager = nullptr;
#endif
};

/// Thông tin 1 frame khi sample ghi lệnh vẽ.
struct SampleFrame {
  vk::raii::CommandBuffer &cmd; ///< đã begin; swapchain image đang ở eColorAttachmentOptimal
  uint32_t frameIndex;          ///< 0..MAX_FRAMES_IN_FLIGHT-1
  vk::ImageView swapchainView;
  vk::Extent2D extent;
};

/// Semaphore mà graphics submit của frame phải chờ (vd compute submit riêng).
struct SampleWait {
  vk::Semaphore semaphore = nullptr;
  vk::PipelineStageFlags stage = {};
};

/// Một chương trình chạy trong sandbox. Mỗi lúc chỉ chạy một sample.
class ISample {
public:
  virtual ~ISample() = default;

  virtual void init(const SampleContext &ctx) = 0;
  /// Phần CPU của frame, gọi trước drawFrame. dtSeconds tính bằng giây.
  virtual void update(float dtSeconds) = 0;
  /// Submit việc GPU riêng trước graphics, gọi sau khi renderer đã acquire ảnh.
  virtual SampleWait submitBeforeGraphics(uint32_t /*frameIndex*/) { return {}; }
  /// Ghi lệnh vẽ vào frame.cmd. Trả về khi swapchain image vẫn ở eColorAttachmentOptimal.
  virtual void record(const SampleFrame &frame) = 0;
  /// Swapchain vừa được tạo lại: tạo lại tài nguyên theo kích thước mới.
  virtual void onSwapchainRecreated() {}
  virtual void ui() {}
  virtual void shutdown() = 0;
};