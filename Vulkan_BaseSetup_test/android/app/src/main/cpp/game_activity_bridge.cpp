#include <android/log.h>
#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <cstring>

/// Define logging macros
#define LOGI(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_INFO, "VulkanTutorial", __VA_ARGS__))
#define LOGW(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_WARN, "VulkanTutorial", __VA_ARGS__))
#define LOGE(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_ERROR, "VulkanTutorial", __VA_ARGS__))

/// Forward declaration of the main entry point
extern "C" void android_main(android_app *app);

/// GameActivity entry point bien game activity thanh entry native goi android
/// main
extern "C" {
void GameActivity_onCreate(GameActivity *activity, void *savedState,
                           size_t savedStateSize) {
  (void)savedState;
  (void)savedStateSize;

  LOGI("GameActivity_onCreate");

  // Create an android_app structure
  android_app *app = new android_app();
  memset(app, 0, sizeof(android_app));

  // Set up the android_app structure
  app->activity = activity;

  // Call the original android_main function
  android_main(app);

  // Clean up
  delete app;
}
}

// /// Register our callback by assigning the function-pointer exported by
// /// GameActivity.
// __attribute__((constructor)) static void RegisterGameActivityCallbacks() {
//   GameActivity_onCreate = GameActivity_onCreate;
// }