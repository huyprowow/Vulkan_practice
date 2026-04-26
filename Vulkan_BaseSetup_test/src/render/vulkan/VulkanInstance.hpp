#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

using namespace std;

class IWindow;

/// Gom instance, debug messenger, surface vào một chỗ
class VulkanInstance {
public:
  void init(const IWindow &window);

  const vk::raii::Context &getContext() const { return context_; }
  const vk::raii::Instance &getInstance() const { return instance_; }
  const vk::raii::SurfaceKHR &getSurface() const { return surface_; }

private:
  vk::raii::Context context_;
  vk::raii::Instance instance_{nullptr};
  vk::raii::DebugUtilsMessengerEXT debugMessenger_{nullptr};
  vk::raii::SurfaceKHR surface_{nullptr};

  std::vector<const char *> getRequiredExtensions(const IWindow &window) const;
  void createInstance(const IWindow &window);
  void setupDebugMessenger();
  void createSurface(const IWindow &window);

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *);
};