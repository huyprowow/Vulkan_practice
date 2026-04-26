#include "VulkanInstance.hpp"
#include "../../core/Types.hpp"
#include "../../platform/IWindow.hpp"

#include <cstring>
#include <iostream>

/// Khởi tạo VulkanInstance: tạo instance, debug messenger, và surface
void VulkanInstance::init(const IWindow &window) {
  createInstance(
      window); // tao vk instance, thiet lap validation layers, kiem tra
               // cac required layer, extension co duoc ho tro khong
  setupDebugMessenger(); // thiet lap debug messenger cho validation layer
  createSurface(window);
}

void VulkanInstance::createSurface(const IWindow &window) {
  VkSurfaceKHR rawSurface =
      window.createSurface(static_cast<VkInstance>(*instance_));
  surface_ = vk::raii::SurfaceKHR(instance_, rawSurface);
}

std::vector<const char *>
VulkanInstance::getRequiredExtensions(const IWindow &window) const {
  return window.getRequiredInstanceExtensions(enableValidationLayers);
}

void VulkanInstance::createInstance(const IWindow &window) {
  // Use Vulkan 1.3 for macOS (MoltenVK), 1.4 for Windows/Linux (native
  // Vulkan)
#ifdef __APPLE__
  constexpr uint32_t vulkanApiVersion =
      vk::ApiVersion13; // MoltenVK supports up to 1.3
#else
  constexpr uint32_t vulkanApiVersion =
      vk::ApiVersion14; // Native Vulkan on Windows/Linux
#endif

  constexpr vk::ApplicationInfo appInfo{
      .pApplicationName = "Vk Application",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = vulkanApiVersion};

  bool useValidationLayers = enableValidationLayers;

  // get required layers
  std::vector<char const *> requiredLayers;
  if (useValidationLayers) {
    requiredLayers.assign(validationLayers.begin(), validationLayers.end());
  }
  // check if the required layers are supported by the Vulkan implementation.
  auto layerProperties = context_.enumerateInstanceLayerProperties();
  for (auto const &requiredLayer : requiredLayers) {
    if (std::ranges::none_of(
            layerProperties, [requiredLayer](auto const &layerProperty) {
              return std::strcmp(layerProperty.layerName, requiredLayer) == 0;
            })) {
      std::cerr << "Validation layer not supported: " << requiredLayer
                << " (disabling validationLayers)" << std::endl;
      useValidationLayers = false;
      requiredLayers.clear();
      break;
    }
  }

  // Get the required extensions.
  auto requiredExtensions =
      window.getRequiredInstanceExtensions(useValidationLayers);

  // Check if the required extensions are supported by the Vulkan
  // implementation.
  auto extensionProperties = context_.enumerateInstanceExtensionProperties();
  for (auto const &requiredExtension : requiredExtensions) {
    if (std::ranges::none_of(
            extensionProperties,
            [requiredExtension](auto const &extensionProperty) {
              return std::strcmp(extensionProperty.extensionName,
                                 requiredExtension) == 0;
            })) {
      throw std::runtime_error("Required extension not supported: " +
                               std::string(requiredExtension));
    }
  }
  vk::InstanceCreateInfo createInfo{
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
      .ppEnabledLayerNames = requiredLayers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
      .ppEnabledExtensionNames = requiredExtensions.data()};

  // Enable portability enumeration for MoltenVK on macOS
#ifdef __APPLE__
  createInfo.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

  instance_ = vk::raii::Instance(context_, createInfo);
  std::cout << "Validation layers: " << (useValidationLayers ? "ON" : "OFF")
            << std::endl;
}

void VulkanInstance::setupDebugMessenger() {
  if (!enableValidationLayers)
    return;

  vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
  vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
  vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
      .messageSeverity = severityFlags,
      .messageType = messageTypeFlags,
      .pfnUserCallback = &debugCallback};
  debugMessenger_ =
      instance_.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanInstance::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
  if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
      severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
    std::cerr << "validation layer: type " << to_string(type)
              << " msg: " << pCallbackData->pMessage << std::endl;
  }
  return vk::False;
}