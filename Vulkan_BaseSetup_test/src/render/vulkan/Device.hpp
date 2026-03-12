#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>


class VulkanInstance;

class Device {
public:
  void init(const vk::raii::Instance &instance,
            const vk::raii::SurfaceKHR &surface);

  const vk::raii::PhysicalDevice &getPhysicalDevice() const {
    return physicalDevice_;
  }
  const vk::raii::Device &getDevice() const { return device_; }
  const vk::raii::Queue &getGraphicsQueue() const { return graphicsQueue_; }
  uint32_t getQueueIndex() const { return queueIndex_; }
  const std::vector<const char *> &getRequiredDeviceExtension() const {
    return requiredDeviceExtension_;
  }

private:
  vk::raii::PhysicalDevice physicalDevice_{nullptr}; // card do hoa
  vk::raii::Device device_{
      nullptr}; //  logical device tuong tac voi physical device
  vk::raii::Queue graphicsQueue_{nullptr};
  uint32_t queueIndex_ = ~0u;
  std::vector<const char *> requiredDeviceExtension_;

  void pickPhysicalDevice(const vk::raii::Instance &instance,
                          const vk::raii::SurfaceKHR &surface);
  void createLogicalDevice(const vk::raii::SurfaceKHR &surface);
};