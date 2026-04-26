#include "Window.hpp"
#include <stdexcept>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

Window::Window(uint32_t width, uint32_t height, const char *title) {
  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW");
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // not using OpenGL with GLFW
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);    // resize window? -> GLFW_TRUE
  window_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height),
                             title, nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}

Window::~Window() {
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(window_); }

void Window::pollEvents() { glfwPollEvents(); }

void Window::getFramebufferSize(int &width, int &height) const {
  glfwGetFramebufferSize(window_, &width, &height);
}

void Window::setUserPointer(void *ptr) {
  glfwSetWindowUserPointer(window_, ptr);
}

void Window::framebufferResizeCallback(GLFWwindow *window, int width,
                                       int height) {
  (void)width;
  (void)height;
  auto *w = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
  if (w) {
    w->framebufferResized_ = true;
  }
}

std::vector<const char *>
Window::getRequiredInstanceExtensions(bool enableValidation) const {
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char *> extensions(glfwExtensions,
                                       glfwExtensions + glfwExtensionCount);

  // Add portability extensions for MoltenVK on macOS
#ifdef __APPLE__
  extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  extensions.push_back("VK_KHR_get_physical_device_properties2");
#endif

  if (enableValidation) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  return extensions;
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (glfwCreateWindowSurface(instance, window_, nullptr, &surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface (GLFW)");
  }
  return surface;
}