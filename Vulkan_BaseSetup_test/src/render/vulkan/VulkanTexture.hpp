#pragma once

#include "VulkanDevice.hpp"
#include "VulkanMemory.hpp"
#include "VulkanSwapchain.hpp"

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#if defined(__ANDROID__)
struct AAssetManager;
#endif

/// Quản lý texture (KTX2) + framebuffer attachments (color MSAA + depth)
class VulkanTexture {
public:
  /// Khởi tạo: load texture KTX2 + tạo color/depth attachments
  /// @param device VulkanDevice đã init (msaaSamples_ lấy từ device)
  /// @param memory VulkanMemory để cấp/copy buffer + image
  /// @param swapchain VulkanSwapchain để lấy extent + colorFormat
  /// @param texturePath đường dẫn KTX2 (Types.hpp::TEXTURE_PATH)
  void init(VulkanDevice &device, VulkanMemory &memory,
            VulkanSwapchain &swapchain, const std::string &texturePath
#if defined(__ANDROID__)
            ,
            AAssetManager *assetManager
#endif
  );

  /// Tạo lại color + depth khi swapchain resize.
  /// Tự đọc extent + format mới từ swapchain_ đã cache.
  void recreateSwapChainResources();

  /// Reset color + depth (gọi trong cleanupSwapChainResources)
  void resetSwapChainResources();

  /// Cleanup tất cả resource khi shutdown
  void cleanup();

  // Accessors — dùng khi descriptor binding + dynamic rendering
  const vk::raii::ImageView &getTextureView() const {
    return textureImageView_;
  }
  const vk::raii::Sampler &getSampler() const { return textureSampler_; }
  const vk::raii::ImageView &getColorView() const { return colorImageView_; }
  const vk::raii::ImageView &getDepthView() const { return depthImageView_; }
  // Accessors raw vk::raii::Image — dùng cho image layout transition
  const vk::raii::Image &getColorImage() const { return colorImage_; }
  const vk::raii::Image &getDepthImage() const { return depthImage_; }
  
  vk::Format getDepthFormat() const { return depthFormat_; }
  uint32_t getMipLevels() const { return mipLevels_; }

  // Static helpers — Renderer cũng có thể gọi mà không cần init() trước
  static vk::Format
  findSupportedFormat(VulkanDevice &device,
                      const std::vector<vk::Format> &candidates,
                      vk::ImageTiling tiling, vk::FormatFeatureFlags features);
  static vk::Format findDepthFormat(VulkanDevice &device);
  static bool hasStencilComponent(vk::Format format);

private:
  VulkanDevice *device_ = nullptr;
  VulkanMemory *memory_ = nullptr;
  VulkanSwapchain *swapchain_ = nullptr;

  // Texture (loaded từ KTX2)
  uint32_t mipLevels_ = 0;
  vk::raii::Image textureImage_{nullptr};
  vk::raii::DeviceMemory textureImageMemory_{nullptr};
  vk::raii::ImageView textureImageView_{nullptr};
  vk::raii::Sampler textureSampler_{nullptr};

  // Depth attachment
  vk::Format depthFormat_ = vk::Format::eUndefined;
  vk::raii::Image depthImage_{nullptr};
  vk::raii::DeviceMemory depthImageMemory_{nullptr};
  vk::raii::ImageView depthImageView_{nullptr};

  // Color attachment (MSAA resolve target)
  vk::raii::Image colorImage_{nullptr};
  vk::raii::DeviceMemory colorImageMemory_{nullptr};
  vk::raii::ImageView colorImageView_{nullptr};

  // Helpers private — không nhận param, tự đọc từ swapchain_ + device_
  void createTextureImage(const std::string &path
#if defined(__ANDROID__)
                          ,
                          AAssetManager *assetManager
#endif
  );
  void createTextureImageView();
  void createTextureSampler();
  void createColorResources();
  void createDepthResources();
};