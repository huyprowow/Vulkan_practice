#include "AndroidWindow.hpp"
#define VK_USE_PLATFORM_ANDROID_KHR
#include <android/native_window.h>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

std::vector<const char *>
AndroidWindow::getRequiredInstanceExtensions(bool enableValidation) const {
  std::vector<const char *> extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
  };

  if (enableValidation) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

VkSurfaceKHR AndroidWindow::createSurface(VkInstance instance) const {
  if (!nativeWindow_) {
    throw std::runtime_error("AndroidWindow: native window is null");
  }

  VkAndroidSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  createInfo.window = nativeWindow_;

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkResult res =
      vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface);
  if (res != VK_SUCCESS) {
    throw std::runtime_error("AndroidWindow: vkCreateAndroidSurfaceKHR failed");
  }

  return surface;
}

void AndroidWindow::getFramebufferSize(int &width, int &height) const {
  if (!nativeWindow_) {
    width = 0;
    height = 0;
    return;
  }
  width = ANativeWindow_getWidth(nativeWindow_);
  height = ANativeWindow_getHeight(nativeWindow_);
}