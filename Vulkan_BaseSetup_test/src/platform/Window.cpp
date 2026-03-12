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