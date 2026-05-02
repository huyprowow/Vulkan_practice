#pragma once
#include "VulkanDevice.hpp"
#include <vector>
#include <vulkan/vulkan_raii.hpp>

    /// Quan li command pool/buffer per-thread x per-frame cho multithreading.
    /// Layout: pools_[threadIdx], buffers_[threadIdx][frameIdx]
    ///
    /// THREAD SAFETY:
    /// - init() / cleanup(): chi goi tu main thread
    /// - getCommandBuffer(): KHONG co mutex.
    ///   Caller phai dam bao: thread N chi truy cap command buffer cua index N.
    ///   (Vulkan spec: command pool khong thread-safe nhung moi thread co pool
    ///   rieng nen an toan.)
    class ThreadCommandPool {
public:
  /// tao pool/buffer cho moi workerthread x moi frame
  /// @param device VulkanDevice da init
  /// @param queueFamilyIndex queue family (lay tu device.getQueueIndex())
  /// @param threadCount so worker thread
  /// @param buffersPerThread thuong = MAX_FRAMES_IN_FLIGHT
  void init(VulkanDevice &device, uint32_t queueFamilyIndex,
            uint32_t threadCount, uint32_t buffersPerThread);

  /// truy cap command buffer (KHONG lock).
  /// caller phai dam bao chi thread `threadIndex` goi voi index `threadIndex`.
  vk::raii::CommandBuffer &getCommandBuffer(uint32_t threadIndex,
                                            uint32_t frameIndex);

  /// huy tat ca pool/buffer (RAII tu xu nhung can control timing)
  void cleanup();

  uint32_t getThreadCount() const { return threadCount_; }
  uint32_t getBuffersPerThread() const { return buffersPerThread_; }

private:
  VulkanDevice *device_ = nullptr;
  uint32_t threadCount_ = 0;
  uint32_t buffersPerThread_ = 0;

  // moi thread 1 command pool RIENG (Vulkan spec: pool khong thread-safe)
  std::vector<vk::raii::CommandPool> pools_;
  // moi thread N buffer (N = MAX_FRAMES_IN_FLIGHT) tranh race voi frame in
  // flight
  std::vector<std::vector<vk::raii::CommandBuffer>> buffers_;
};