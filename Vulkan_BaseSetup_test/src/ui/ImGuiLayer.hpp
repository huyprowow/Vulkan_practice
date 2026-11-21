#pragma once

#include <vulkan/vulkan_raii.hpp>

class IWindow;
class VulkanDevice;
class VulkanSwapchain;

/// Dear ImGui trên Vulkan dynamic rendering, vẽ đè lên swapchain image sau
/// sample. Phần Vulkan dùng chung mọi nền tảng; phần cửa sổ và input nằm trong
/// initPlatform / newFramePlatform / shutdownPlatform.
class ImGuiLayer {
public:
  /// uiScale: hệ số phóng to tự chọn, nhân thêm với tỉ lệ màn hình của hệ điều hành.
  void init(IWindow &window, const vk::raii::Instance &instance,
            VulkanDevice &device, VulkanSwapchain &swapchain, float uiScale = 1.0f);
  /// Gọi đầu frame, trước mọi lệnh ImGui::...
  void beginFrame();
  /// Gọi sau khi khai báo xong UI của frame.
  void endFrame();
  /// Ghi lệnh vẽ UI; swapchain image đang ở eColorAttachmentOptimal.
  void record(const vk::raii::CommandBuffer &cmd, vk::ImageView swapchainView,
              vk::Extent2D extent);
  void shutdown();

private:
  bool initialized_ = false;
  VkFormat colorFormat_ = VK_FORMAT_UNDEFINED; ///< backend giữ con trỏ tới biến này
  
  float contentScalePlatform(IWindow &window) const;
  void initPlatform(IWindow &window);
  void newFramePlatform();
  void shutdownPlatform();
};