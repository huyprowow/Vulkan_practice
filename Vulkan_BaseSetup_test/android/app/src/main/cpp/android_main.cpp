#include <android/log.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <memory>
#include <stdexcept>
// Your shared engine code
#include "src/platform/android/AndroidWindow.hpp"
#include "src/render/vulkan/VulkanDevice.hpp"
#include "src/render/vulkan/VulkanInstance.hpp"
#include "src/render/vulkan/VulkanRenderer.hpp"
#include "src/render/vulkan/VulkanSwapchain.hpp"

// Logging
#define LOGI(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_INFO, "VulkanPractice", __VA_ARGS__))
#define LOGE(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_ERROR, "VulkanPractice", __VA_ARGS__))

/// native game loop state and Vulkan objects to android lifecycle manager
namespace {
struct AndroidVulkanApp {
  android_app *app = nullptr;
  bool hasWindow = false;
  bool initialized = false;
  AndroidWindow window{};
  VulkanInstance instance{};
  VulkanDevice device{};
  VulkanSwapchain swapchain{};
  VulkanRenderer renderer{};

  /// initialize Vulkan objects if window is available
  void initIfPossible() {
    if (!hasWindow || initialized)
      return;
    if (!app || !app->window)
      return;
    window.setNativeWindow(app->window);
    instance.init(window);
    device.init(instance.getInstance(), instance.getSurface());
    swapchain.init(device.getPhysicalDevice(), device, instance.getSurface(),
                   window);
    renderer.init(device, swapchain, window, app->activity->assetManager);
    initialized = true;
    LOGI("Vulkan initialized");
  }
  /// clean up window lifecycle
  void shutdown() {
    if (!initialized)
      return;
    try {
      renderer.cleanup();
      swapchain.cleanup();
      // instance/device cleanup is RAII inside classes (if any), otherwise
      // add explicit cleanup here
    } catch (...) {
      // best effort
    }
    initialized = false;
    LOGI("Vulkan shutdown");
  }
  /// draw frame if Vulkan objects are initialized
  void draw() {
    if (!initialized)
      return;
    renderer.drawFrame();
  }
};
/// Receives lifecycle commands from android_native_app_glue.
/// - APP_CMD_INIT_WINDOW: a native window is available -> init Vulkan.
/// - APP_CMD_TERM_WINDOW: native window destroyed -> cleanup Vulkan.
static void handleAppCmd(android_app *app, int32_t cmd) {
  auto *vkApp = reinterpret_cast<AndroidVulkanApp *>(app->userData);
  if (!vkApp)
    return;
  switch (cmd) {
  case APP_CMD_INIT_WINDOW:
    vkApp->hasWindow = (app->window != nullptr);
    vkApp->initIfPossible();
    break;
  case APP_CMD_TERM_WINDOW:
    vkApp->hasWindow = false;
    vkApp->shutdown();
    break;
  default:
    break;
  }
}
} // namespace

/// main loop va life cycle cua app
extern "C" void android_main(android_app *app) {
  AndroidVulkanApp vkApp{};
  vkApp.app = app;
  app->userData = &vkApp;
  app->onAppCmd = handleAppCmd;
  // Main loop
  while (true) {
    int events = 0;
    android_poll_source *source = nullptr;
    // If we don't have a window yet, block until an event arrives.
    // If we have a window, poll non-blocking so we can render continuously.
    const int timeoutMs = vkApp.hasWindow ? 0 : -1;
    int ident = 0;
    while ((ident = ALooper_pollOnce(timeoutMs, nullptr, &events,
                                     (void **)&source)) >= 0) {
      if (source) {
        source->process(app, source);
      }
      if (app->destroyRequested) {
        vkApp.shutdown();
        return;
      }
      // timeoutMs==0: poll non-blocking, xử lý xong event queue thì render
      if (timeoutMs == 0) {
        break;
      }
    }
    if (vkApp.hasWindow && vkApp.initialized) {
      try {
        vkApp.draw();
      } catch (const std::exception &e) {
        LOGE("Exception in draw: %s", e.what());
        // If something goes wrong, shut down and wait for window again
        vkApp.shutdown();
      }
    }
  }
}