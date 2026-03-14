#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

class Window;
class VulkanDevice;

class VulkanSwapchain {
public:
  void init(const vk::raii::PhysicalDevice &physicalDevice,
            const VulkanDevice &device, const vk::raii::SurfaceKHR &surface,
            const Window &window);

  void recreate(const vk::raii::PhysicalDevice &physicalDevice,
                const VulkanDevice &device, const vk::raii::SurfaceKHR &surface,
                const Window &window);

  void cleanup();

  const vk::raii::SwapchainKHR &getSwapChain() const { return swapChain_; }
  const vk::SurfaceFormatKHR &getSurfaceFormat() const {
    return swapChainSurfaceFormat_;
  }
  const vk::raii::SurfaceKHR &getSurface() const { return *surface_; }

  const std::vector<vk::Image> &getImages() const { return swapChainImages_; }
  vk::Format getImageFormat() const { return swapChainImageFormat_; }
  const vk::Extent2D &getExtent() const { return swapChainExtent_; }
  const std::vector<vk::raii::ImageView> &getImageViews() const {
    return swapChainImageViews_;
  }

  // dùng cho depth/texture: tạo view từ raw image handle
  vk::raii::ImageView createImageView(const vk::Image &image, vk::Format format,
                                      vk::ImageAspectFlags aspectFlags,
                                      uint32_t mipLevels,
                                      const vk::raii::Device &device) const;

private:
  vk::raii::SwapchainKHR swapChain_{nullptr};
  vk::SurfaceFormatKHR swapChainSurfaceFormat_{};
  const vk::raii::SurfaceKHR *surface_ = nullptr;
  std::vector<vk::Image> swapChainImages_;
  vk::Format swapChainImageFormat_ = vk::Format::eUndefined;
  vk::Extent2D swapChainExtent_{};
  std::vector<vk::raii::ImageView> swapChainImageViews_;

  void createSwapChain(const vk::raii::PhysicalDevice &physicalDevice,
                       const VulkanDevice &device,
                       const vk::raii::SurfaceKHR &surface,
                       const Window &window);

  void createImageViews(const VulkanDevice &device);

  static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR> &availableFormats);

  static vk::PresentModeKHR chooseSwapPresentMode(
      const std::vector<vk::PresentModeKHR> &availablePresentModes);

  vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities,
                                const Window &window) const;
};