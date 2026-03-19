#include "VulkanSwapchain.hpp"
#include "../../platform/Window.hpp"
#include "VulkanDevice.hpp"

#include <cassert>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

/// Khởi tạo swapchain: tạo swap chain và image views
void VulkanSwapchain::init(const vk::raii::PhysicalDevice &physicalDevice,
                     const VulkanDevice &device, const vk::raii::SurfaceKHR &surface,
                     const Window &window) {
  surface_ = &surface;
  createSwapChain(physicalDevice, device, surface,
                  window); // tao swap chain, la mot chuoi cac image de hien thi
                           // len man hinh
  createImageViews(device); // tao image view, khung nhin cho moi anh cho swap
                            // chain de xem
}

/// Tạo lại swapchain khi window resize hoặc swapchain out-of-date
void VulkanSwapchain::recreate(const vk::raii::PhysicalDevice &physicalDevice,
                         const VulkanDevice &device,
                         const vk::raii::SurfaceKHR &surface,
                         const Window &window) {
  surface_ = &surface;
  cleanup();
  createSwapChain(physicalDevice, device, surface, window);
  createImageViews(device);
}

void VulkanSwapchain::cleanup() {
  swapChainImageViews_.clear();
  swapChain_ = nullptr;
  surface_ = nullptr;
}

void VulkanSwapchain::createSwapChain(const vk::raii::PhysicalDevice &physicalDevice,
                                const VulkanDevice &device,
                                const vk::raii::SurfaceKHR &surface,
                                const Window &window) {
  auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
  swapChainSurfaceFormat_ =
      chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
  swapChainExtent_ = chooseSwapExtent(surfaceCapabilities, window);
  auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
  minImageCount = (surfaceCapabilities.maxImageCount > 0 &&
                   minImageCount > surfaceCapabilities.maxImageCount)
                      ? surfaceCapabilities.maxImageCount
                      : minImageCount;

  vk::SwapchainCreateInfoKHR swapChainCreateInfo{
      .flags = vk::SwapchainCreateFlagsKHR(),
      .surface = surface,
      .minImageCount = minImageCount,
      .imageFormat = swapChainSurfaceFormat_.format,
      .imageColorSpace = swapChainSurfaceFormat_.colorSpace,
      .imageExtent = swapChainExtent_,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surfaceCapabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = chooseSwapPresentMode(
          physicalDevice.getSurfacePresentModesKHR(surface)),
      .clipped = true,
      .oldSwapchain = nullptr};

  swapChain_ = vk::raii::SwapchainKHR(device.getDevice(), swapChainCreateInfo);
  swapChainImages_ = swapChain_.getImages();

  swapChainImageFormat_ = swapChainSurfaceFormat_.format;
}

vk::SurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  assert(!availableFormats.empty());
  const auto formatIt =
      std::ranges::find_if(availableFormats, [](const auto &format) {
        return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      });
  return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::Extent2D
VulkanSwapchain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities,
                            const Window &window) const {
  if (capabilities.currentExtent.width != 0xFFFFFFFF) {
    return capabilities.currentExtent;
  }
  int width, height;
  // dùng GLFW trực tiếp theo code gốc
  glfwGetFramebufferSize(window.getHandle(), &width, &height);

  return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                               capabilities.maxImageExtent.width),
          std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height)};
}

vk::PresentModeKHR VulkanSwapchain::chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes) {
  assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) {
    return presentMode == vk::PresentModeKHR::eFifo;
  }));
  return std::ranges::any_of(availablePresentModes,
                             [](const vk::PresentModeKHR value) {
                               return vk::PresentModeKHR::eMailbox == value;
                             })
             ? vk::PresentModeKHR::eMailbox
             : vk::PresentModeKHR::eFifo;
}

void VulkanSwapchain::createImageViews(const VulkanDevice &device) {
  swapChainImageViews_.clear();
  swapChainImageViews_.reserve(swapChainImages_.size());

  for (auto image : swapChainImages_) {
    swapChainImageViews_.emplace_back(createImageView(
        image, swapChainImageFormat_, vk::ImageAspectFlagBits::eColor, 1,
        device.getDevice()));
  }
}

vk::raii::ImageView
VulkanSwapchain::createImageView(const vk::Image &image, vk::Format format,
                           vk::ImageAspectFlags aspectFlags, uint32_t mipLevels,
                           const vk::raii::Device &device) const {
  vk::ImageViewCreateInfo viewInfo{.image = image,
                                   .viewType = vk::ImageViewType::e2D,
                                   .format = format,
                                   .subresourceRange = {aspectFlags, 0,
                                                        mipLevels, // levelCount
                                                        0, 1}};
  return vk::raii::ImageView(device, viewInfo);
}