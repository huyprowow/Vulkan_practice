#pragma once

#include <vector>
#include <vulkan/vulkan.h>

class IWindow {
public:
  virtual ~IWindow() = default;

  virtual std::vector<const char *>
  getRequiredInstanceExtensions(bool enableValidation) const = 0;

  virtual VkSurfaceKHR createSurface(VkInstance instance) const = 0;

  virtual void getFramebufferSize(int &width, int &height) const = 0;
};