#pragma once

#include "../IWindow.hpp"

struct ANativeWindow;

class AndroidWindow final : public IWindow {
public:
  AndroidWindow() = default;
  explicit AndroidWindow(ANativeWindow *w) : nativeWindow_(w) {}

  void setNativeWindow(ANativeWindow *w) { nativeWindow_ = w; }
  ANativeWindow *getNativeWindow() const { return nativeWindow_; }

  std::vector<const char *>
  getRequiredInstanceExtensions(bool enableValidation) const override;
  VkSurfaceKHR createSurface(VkInstance instance) const override;
  void getFramebufferSize(int &width, int &height) const override;

private:
  ANativeWindow *nativeWindow_ = nullptr;
};