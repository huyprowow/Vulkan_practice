#pragma once

#include "../IWindow.hpp"

// CAMetalLayer is kept opaque here so engine sources remain C++.
class IOSWindow final : public IWindow {
public:
  void setMetalLayer(void *layer) { metalLayer_ = layer; }

  std::vector<const char *> getRequiredInstanceExtensions(
      bool enableValidation) const override;
  VkSurfaceKHR createSurface(VkInstance instance) const override;
  void getFramebufferSize(int &width, int &height) const override;

private:
  void *metalLayer_ = nullptr;
};
