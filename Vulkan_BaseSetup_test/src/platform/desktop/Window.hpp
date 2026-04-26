#pragma once

#include <cstdint>

#include "../IWindow.hpp"
struct GLFWwindow;

class Window : public IWindow {
public:
  Window(uint32_t width, uint32_t height, const char *title);
  ~Window();

  // Desktop-only helpers
  GLFWwindow *getHandle() const { return window_; }
  bool shouldClose() const;
  void pollEvents();
  void setUserPointer(void *ptr);
  bool wasResized() const { return framebufferResized_; }
  void clearResized() { framebufferResized_ = false; }

  // IWindow implementation
  std::vector<const char *>
  getRequiredInstanceExtensions(bool enableValidation) const override;
  VkSurfaceKHR createSurface(VkInstance instance) const override;
  void getFramebufferSize(int &width, int &height) const override;

private:
  GLFWwindow *window_ = nullptr;
  bool framebufferResized_ = false;
  static void framebufferResizeCallback(GLFWwindow *window, int width,
                                        int height);
};