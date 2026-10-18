#include "VulkanDevice.hpp"
#include "../../core/Types.hpp"

// #include <vulkan/vulkan_profiles.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <ranges>

// namespace {
// const VpProfileProperties kAppProfile{
//     VP_KHR_ROADMAP_2022_NAME,
//     VP_KHR_ROADMAP_2022_SPEC_VERSION,
// };
// } // namespace

namespace {
// Chỉ bật khi GPU hỗ trợ. Buffer device address và descriptor indexing đã là
// core từ Vulkan 1.2 nên không nằm trong danh sách này (feature của chúng
// được kiểm tra trong supportsRayTracing).
const std::vector<const char *> kRayTracingExtensions = {
    vk::KHRAccelerationStructureExtensionName, vk::KHRRayQueryExtensionName,
    vk::KHRDeferredHostOperationsExtensionName};

bool supportsExtensions(const vk::raii::PhysicalDevice &device,
                        const std::vector<const char *> &extensions) {
  auto available = device.enumerateDeviceExtensionProperties();
  return std::ranges::all_of(extensions, [&](const char *name) {
    return std::ranges::any_of(available, [name](auto const &ext) {
      return std::strcmp(ext.extensionName, name) == 0;
    });
  });
}

bool supportsRayTracing(const vk::raii::PhysicalDevice &device) {
  // Chỉ query feature struct của RT khi device có extension tương ứng
  if (!supportsExtensions(device, kRayTracingExtensions)) {
    return false;
  }
  auto features = device.getFeatures2<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features,
      vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
      vk::PhysicalDeviceRayQueryFeaturesKHR>();
  // bufferDeviceAddress (BLAS input) + descriptor indexing (bindless) chi can
  // cho RT, raster khong dung → chi yeu cau khi bat RT
  auto const &vk12Features = features.get<vk::PhysicalDeviceVulkan12Features>();
  return vk12Features.bufferDeviceAddress &&
         vk12Features.runtimeDescriptorArray &&
         vk12Features.descriptorBindingPartiallyBound &&
         vk12Features.descriptorBindingVariableDescriptorCount &&
         vk12Features.descriptorBindingSampledImageUpdateAfterBind &&
         vk12Features.shaderSampledImageArrayNonUniformIndexing &&
         features.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>()
             .accelerationStructure &&
         features.get<vk::PhysicalDeviceRayQueryFeaturesKHR>().rayQuery;
}
} // namespace

/// Khởi tạo VulkanDevice: chọn physical device và tạo logical device
void VulkanDevice::init(const vk::raii::Instance &instance,
                        const vk::raii::SurfaceKHR &surface) {
  // danh sách extension
  requiredDeviceExtension_ = {
      vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
      vk::KHRSynchronization2ExtensionName,
      vk::KHRCreateRenderpass2ExtensionName,
#ifdef __APPLE__
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
  auto isDeviceSuitable = [&](auto const &device) {
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
    bool supportsAllRequiredExtensions =
        supportsExtensions(device, requiredDeviceExtension_);

    // phai khop voi featureChain trong createLogicalDevice, neu khong
    // createDevice se fail voi VK_ERROR_FEATURE_NOT_PRESENT
    auto features = device.template getFeatures2<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    auto const &coreFeatures =
        features.template get<vk::PhysicalDeviceFeatures2>().features;
    bool supportsRequiredFeatures =
        coreFeatures.sampleRateShading && coreFeatures.samplerAnisotropy &&
        features.template get<vk::PhysicalDeviceVulkan11Features>()
            .shaderDrawParameters &&
        features.template get<vk::PhysicalDeviceVulkan13Features>()
            .dynamicRendering &&
        features.template get<vk::PhysicalDeviceVulkan13Features>()
            .synchronization2 &&
        features
            .template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
            .extendedDynamicState;
    // bufferDeviceAddress + descriptor indexing: chi bat buoc khi co RT, xem
    // supportsRayTracing

    // chua check present o day (lambda duoi luon tra ve true), present duoc
    // check theo tung queue family trong createLogicalDevice
    bool supportsPresent =
        std::ranges::any_of(queueFamilies, [&](auto const & /*qfp*/) {
          // we don't know index here, so check by index later
          return true;
        });

    (void)supportsPresent; // present support is checked in createLogicalDevice

    // VkBool32 profileSupported = VK_FALSE;
    // VkResult pr =
    //     vpGetPhysicalDeviceProfileSupport(*instance, // VkInstance
    //                                       *device,   // VkPhysicalDevice
    //                                       &kAppProfile, &profileSupported);
    // if (pr != VK_SUCCESS || profileSupported != VK_TRUE) {
    //   return false;
    // }

    return supportsVulkan1_3 && supportsGraphics &&
           supportsAllRequiredExtensions && supportsRequiredFeatures;
  };

  // uu tien GPU co ray tracing, roi toi GPU roi (may co ca iGPU lan dGPU)
  const vk::raii::PhysicalDevice *bestDevice = nullptr;
  int bestScore = -1;
  for (auto const &device : devices) {
    if (!isDeviceSuitable(device)) {
      continue;
    }
    int score = (supportsRayTracing(device) ? 2 : 0) +
                (device.getProperties().deviceType ==
                         vk::PhysicalDeviceType::eDiscreteGpu
                     ? 1
                     : 0);
    if (score > bestScore) {
      bestDevice = &device;
      bestScore = score;
    }
  }

  if (!bestDevice) {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
  physicalDevice_ = *bestDevice;
  rayTracingSupported_ = supportsRayTracing(physicalDevice_);
  msaaSamples_ = getMaxUsableSampleCount();
  std::cout << "MSAA Samples: " << static_cast<uint32_t>(msaaSamples_)
            << std::endl;
}

void VulkanDevice::createLogicalDevice(const vk::raii::SurfaceKHR &surface) {
  // note: Các trình điều khiển hiện có chỉ cho phép bạn tạo một số lượng nhỏ
  // hàng đợi cho mỗi họ hàng đợi, không cần nhiều hơn vì có thể tạo tất cả
  // các bộ đệm lệnh trên nhiều luồng, sau đó gửi tất cả chúng cùng một lúc
  // trên luồng chính chỉ với một lệnh gọi

  // hang doi ho tro do hoa, gan muc do uu tien
  // find the index of the first queue family that supports graphics, present
  // and compute
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      physicalDevice_.getQueueFamilyProperties();

  // lay index dau tien vao queueFamilyProperties ho tro do hoa, present va
  // compute
  for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size();
       qfpIndex++) {
    if ((queueFamilyProperties[qfpIndex].queueFlags &
         vk::QueueFlagBits::eGraphics) &&
        physicalDevice_.getSurfaceSupportKHR(qfpIndex, *surface) &&
        (queueFamilyProperties[qfpIndex].queueFlags &
         vk::QueueFlagBits::eCompute)) {
      // found a queue family that supports both graphics, present and compute
      queueIndex_ = qfpIndex;
      break;
    }
  }
  if (queueIndex_ == ~0u) {
    throw std::runtime_error(
        "Could not find a queue for graphics, present and compute -> "
        "terminating");
  }

  // bat cac chuc nang toi vk1.3 vi mac dinh chi vk1.0
  vk::StructureChain<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
      vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
      vk::PhysicalDeviceRayQueryFeaturesKHR>
      featureChain = {
          // vk::PhysicalDeviceFeatures2
          {.features = {.sampleRateShading = true, .samplerAnisotropy = true}},
          // vk::PhysicalDeviceVulkan11Features
          {.shaderDrawParameters = true},
          // Vulkan12Features - bufferDeviceAddress (BLAS input)
          // + descriptor indexing (bindless), chỉ bật khi GPU hỗ trợ RT
          // Field phải đúng thứ tự khai báo trong struct (C++20 designated
          // initializer), sai thứ tự thì GCC/MSVC báo lỗi
          {.shaderSampledImageArrayNonUniformIndexing = rayTracingSupported_,
           .descriptorBindingSampledImageUpdateAfterBind = rayTracingSupported_,
           .descriptorBindingPartiallyBound = rayTracingSupported_,
           .descriptorBindingVariableDescriptorCount = rayTracingSupported_,
           .runtimeDescriptorArray = rayTracingSupported_,
           .bufferDeviceAddress = rayTracingSupported_},
          // vk::PhysicalDeviceVulkan13Features
          {.synchronization2 = true, .dynamicRendering = true},
          // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
          {.extendedDynamicState = true},
          // vk::PhysicalDeviceAccelerationStructureFeaturesKHR
          {.accelerationStructure = true},
          // vk::PhysicalDeviceRayQueryFeaturesKHR
          {.rayQuery = true}};

  std::vector<const char *> enabledExtensions = requiredDeviceExtension_;
  if (rayTracingSupported_) {
    enabledExtensions.insert(enabledExtensions.end(),
                             kRayTracingExtensions.begin(),
                             kRayTracingExtensions.end());
  } else {
    // feature struct cua extension khong duoc bat khong duoc nam trong pNext
    featureChain.unlink<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
    featureChain.unlink<vk::PhysicalDeviceRayQueryFeaturesKHR>();
  }

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
      .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
      .ppEnabledExtensionNames = enabledExtensions.data()};

  device_ = vk::raii::Device(physicalDevice_, deviceCreateInfo);
  graphicsQueue_ =
      vk::raii::Queue(device_, queueIndex_, 0); // hang doi lenh do hoa

  std::cout << "Ray tracing: "
            << (rayTracingSupported_ ? "ENABLED" : "disabled (GPU not supported)")
            << std::endl;
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