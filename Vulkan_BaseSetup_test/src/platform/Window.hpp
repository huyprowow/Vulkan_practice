#pragma once

#include <cstdint>

struct GLFWwindow;

class Window {
public:
  Window(uint32_t width, uint32_t height, const char *title);
  ~Window();
  GLFWwindow *getHandle() const { return window_; }
  bool shouldClose() const;
  void pollEvents();
  void getFramebufferSize(int &width, int &height) const;
  void setUserPointer(void *ptr);
  bool wasResized() const { return framebufferResized_; }
  void clearResized() { framebufferResized_ = false; }

private:
  GLFWwindow *window_ = nullptr;
  bool framebufferResized_ = false;
  static void framebufferResizeCallback(GLFWwindow *window, int width,
                                        int height);
};