#ifndef VK_USE_PLATFORM_METAL_EXT
#define VK_USE_PLATFORM_METAL_EXT 1
#endif

#include "IOSWindow.hpp"
#include <QuartzCore/CAMetalLayer.h>
#include <stdexcept>
#include <vulkan/vulkan_metal.h>

std::vector<const char *> IOSWindow::getRequiredInstanceExtensions(
    bool enableValidation) const {
  std::vector<const char *> extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_EXT_METAL_SURFACE_EXTENSION_NAME,
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };
  if (enableValidation)
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  return extensions;
}

VkSurfaceKHR IOSWindow::createSurface(VkInstance instance) const {
  if (!metalLayer_)
    throw std::runtime_error("IOSWindow: CAMetalLayer is null");

  VkMetalSurfaceCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
  createInfo.pLayer = (__bridge const CAMetalLayer *)metalLayer_;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
    throw std::runtime_error("IOSWindow: vkCreateMetalSurfaceEXT failed");
  return surface;
}

void IOSWindow::getFramebufferSize(int &width, int &height) const {
  auto *layer = (__bridge CAMetalLayer *)metalLayer_;
  if (!layer) {
    width = height = 0;
    return;
  }
  width = static_cast<int>(layer.drawableSize.width);
  height = static_cast<int>(layer.drawableSize.height);
}
