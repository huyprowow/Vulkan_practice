#include "VulkanDevice.hpp"
#include "../../core/Types.hpp"

#include <cstring>
#include <ranges>
#include <iostream>

/// Khởi tạo VulkanDevice: chọn physical device và tạo logical device
void VulkanDevice::init(const vk::raii::Instance &instance,
                        const vk::raii::SurfaceKHR &surface) {
  // danh sách extension
  requiredDeviceExtension_ = {
      vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
      vk::KHRSynchronization2ExtensionName,
      vk::KHRCreateRenderpass2ExtensionName
#ifdef __APPLE__
      ,
      "VK_KHR_portability_subset" // Required for MoltenVK devices
#endif
  };

  pickPhysicalDevice(instance, surface); // chon card
  createLogicalDevice(surface); // tao logical device tu physical device
}

void VulkanDevice::pickPhysicalDevice(const vk::raii::Instance &instance,
                                      const vk::raii::SurfaceKHR &surface) {
  auto devices = instance.enumeratePhysicalDevices();
  if (devices.empty()) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }
  // chấm điểm cho từng thiết bị và chọn thiết bị cao nhất
  // // Use an ordered map to automatically sort candidates by increasing
  // score std::multimap<int, vk::raii::PhysicalDevice> candidates;

  // for (const auto &device : devices)
  // {
  //     physicalDevice = std::make_unique<vk::raii::PhysicalDevice>(device);
  //     auto deviceProperties = device.getProperties(); // thong tin card, pb
  //     vk sp,... auto deviceFeatures = device.getFeatures();     // thong
  //     tin tinh nang card ho tro uint32_t score = 0;

  //     // Discrete GPUs have a significant performance advantage
  //     if (deviceProperties.deviceType ==
  //     vk::PhysicalDeviceType::eDiscreteGpu)
  //     {
  //         score += 1000;
  //     }

  //     // Maximum possible size of textures affects graphics quality
  //     score += deviceProperties.limits.maxImageDimension2D;

  //     // Application can't function without geometry shaders
  //     if (!deviceFeatures.geometryShader)
  //     {
  //         continue;
  //     }
  //     candidates.insert(std::make_pair(score, device));
  // }

  // // Check if the best candidate is suitable at all
  // if (candidates.rbegin()->first > 0)
  // {
  //     physicalDevice =
  //     std::make_unique<vk::raii::PhysicalDevice>(candidates.rbegin()->second);
  // }
  // else
  // {
  //     throw std::runtime_error("failed to find a suitable GPU!");
  // }

  // chi check ho tro vk1.3 và 1 so extension
  const auto devIter = std::ranges::find_if(devices, [&](auto const &device) {
    // Check if the device supports the Vulkan 1.3 API version
    bool supportsVulkan1_3 =
        device.getProperties().apiVersion >= VK_API_VERSION_1_3;

    // Check if any of the queue families support graphics operations
    auto queueFamilies = device.getQueueFamilyProperties();
    bool supportsGraphics =
        std::ranges::any_of(queueFamilies, [](auto const &qfp) {
          return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
        });

    // Check if all required device extensions are available
    auto availableDeviceExtensions =
        device.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions = std::ranges::all_of(
        requiredDeviceExtension_,
        [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
          return std::ranges::any_of(
              availableDeviceExtensions,
              [requiredDeviceExtension](auto const &availableDeviceExtension) {
                return std::strcmp(availableDeviceExtension.extensionName,
                                   requiredDeviceExtension) == 0;
              });
        });

    auto features = device.template getFeatures2<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsRequiredFeatures =
        features.template get<vk::PhysicalDeviceVulkan11Features>()
            .shaderDrawParameters &&
        features.template get<vk::PhysicalDeviceVulkan13Features>()
            .dynamicRendering &&
        features.template get<vk::PhysicalDeviceVulkan13Features>()
            .synchronization2 &&
        features
            .template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
            .extendedDynamicState;

    // also require that at least one queue family supports present on this
    // surface
    bool supportsPresent =
        std::ranges::any_of(queueFamilies, [&](auto const & /*qfp*/) {
          // we don't know index here, so check by index later
          return true;
        });

    (void)supportsPresent; // present support is checked in createLogicalDevice

    return supportsVulkan1_3 && supportsGraphics &&
           supportsAllRequiredExtensions && supportsRequiredFeatures;
  });

  if (devIter != devices.end()) {
    physicalDevice_ = *devIter;
    msaaSamples_ = getMaxUsableSampleCount();
    std::cout << "MSAA Samples: " << static_cast<uint32_t>(msaaSamples_)
              << std::endl;
  } else {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
}

void VulkanDevice::createLogicalDevice(const vk::raii::SurfaceKHR &surface) {
  // note: Các trình điều khiển hiện có chỉ cho phép bạn tạo một số lượng nhỏ
  // hàng đợi cho mỗi họ hàng đợi, không cần nhiều hơn vì có thể tạo tất cả
  // các bộ đệm lệnh trên nhiều luồng, sau đó gửi tất cả chúng cùng một lúc
  // trên luồng chính chỉ với một lệnh gọi

  // hang doi ho tro do hoa, gan muc do uu tien
  // find the index of the first queue family that supports graphics
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      physicalDevice_.getQueueFamilyProperties();

  // lay index dau tien vao queueFamilyProperties ho tro do hoa va present
  for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size();
       qfpIndex++) {
    if ((queueFamilyProperties[qfpIndex].queueFlags &
         vk::QueueFlagBits::eGraphics) &&
        physicalDevice_.getSurfaceSupportKHR(qfpIndex, *surface)) {
      // found a queue family that supports both graphics and present
      queueIndex_ = qfpIndex;
      break;
    }
  }
  if (queueIndex_ == ~0u) {
    throw std::runtime_error(
        "Could not find a queue for graphics and present -> terminating");
  }

  // truy van tat ca chuc nang toi vk1.3 vi mac dinh chi vk1.0
  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan11Features,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain = {
          {.features = {.sampleRateShading = true,
                        .samplerAnisotropy =
                            true}},       // vk::PhysicalDeviceFeatures2
          {.shaderDrawParameters = true}, // vk::PhysicalDeviceVulkan11Features
          {.synchronization2 = true,
           .dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
          {.extendedDynamicState =
               true} // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
      };

  // create a VulkanDevice
  float queuePriority = 0.0f;
  vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
      .queueFamilyIndex = queueIndex_,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority};
  vk::DeviceCreateInfo deviceCreateInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &deviceQueueCreateInfo,
      .enabledExtensionCount =
          static_cast<uint32_t>(requiredDeviceExtension_.size()),
      .ppEnabledExtensionNames = requiredDeviceExtension_.data()};

  device_ = vk::raii::Device(physicalDevice_, deviceCreateInfo);
  graphicsQueue_ =
      vk::raii::Queue(device_, queueIndex_, 0); // hang doi lenh do hoa
}

// tim queue family ho tro cac lenh do hoa
// uint32_t VulkanDevice::findQueueFamilies() const
// {
//     // find the index of the first queue family that supports graphics
//     std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
//         physicalDevice_.getQueueFamilyProperties();
//
//     // get the first index into queueFamilyProperties which supports graphics
//     auto graphicsQueueFamilyProperty =
//         std::find_if(queueFamilyProperties.begin(),
//                      queueFamilyProperties.end(),
//                      [](vk::QueueFamilyProperties const &qfp)
//                      { return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
//                      });
//
//     return static_cast<uint32_t>(
//         std::distance(queueFamilyProperties.begin(),
//         graphicsQueueFamilyProperty));
// }

/// tim maximum sample ho tro
vk::SampleCountFlagBits VulkanDevice::getMaxUsableSampleCount() {
  vk::PhysicalDeviceProperties physicalDeviceProperties =
      physicalDevice_.getProperties();

  vk::SampleCountFlags counts =
      physicalDeviceProperties.limits.framebufferColorSampleCounts &
      physicalDeviceProperties.limits.framebufferDepthSampleCounts;
  if (counts & vk::SampleCountFlagBits::e64) {
    return vk::SampleCountFlagBits::e64;
  }
  if (counts & vk::SampleCountFlagBits::e32) {
    return vk::SampleCountFlagBits::e32;
  }
  if (counts & vk::SampleCountFlagBits::e16) {
    return vk::SampleCountFlagBits::e16;
  }
  if (counts & vk::SampleCountFlagBits::e8) {
    return vk::SampleCountFlagBits::e8;
  }
  if (counts & vk::SampleCountFlagBits::e4) {
    return vk::SampleCountFlagBits::e4;
  }
  if (counts & vk::SampleCountFlagBits::e2) {
    return vk::SampleCountFlagBits::e2;
  }

  return vk::SampleCountFlagBits::e1;
}