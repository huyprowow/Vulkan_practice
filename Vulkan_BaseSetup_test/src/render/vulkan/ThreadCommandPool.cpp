#include "ThreadCommandPool.hpp"
#include <stdexcept>

/// tao pool/buffer cho moi worker thread x moi frame
void ThreadCommandPool::init(VulkanDevice &device, uint32_t queueFamilyIndex,
                             uint32_t threadCount, uint32_t buffersPerThread) {
  device_ = &device;
  threadCount_ = threadCount;
  buffersPerThread_ = buffersPerThread;

  pools_.clear();
  buffers_.clear();
  pools_.reserve(threadCount);
  buffers_.reserve(threadCount);

  for (uint32_t t = 0; t < threadCount; t++) {
    // 1 command pool RIENG cho moi thread (Vulkan spec: pool khong thread-safe)
    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndex};
    pools_.emplace_back(device_->getDevice(), poolInfo);

    // buffersPerThread buffer cho thread nay (tranh race voi MAX_FRAMES > 1)
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *pools_[t],
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = buffersPerThread};
    auto rawBuffers = vk::raii::CommandBuffers(device_->getDevice(), allocInfo);

    std::vector<vk::raii::CommandBuffer> threadBufs;
    threadBufs.reserve(buffersPerThread);
    for (auto &b : rawBuffers) {
      threadBufs.emplace_back(std::move(b));
    }
    buffers_.emplace_back(std::move(threadBufs));
  }
}

/// truy cap command buffer (KHONG lock).
/// caller phai dam bao chi thread `threadIndex` goi voi index `threadIndex`.
vk::raii::CommandBuffer &
ThreadCommandPool::getCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) {
  if (threadIndex >= threadCount_) {
    throw std::runtime_error("ThreadCommandPool: invalid threadIndex");
  }
  if (frameIndex >= buffersPerThread_) {
    throw std::runtime_error("ThreadCommandPool: invalid frameIndex");
  }
  // KHONG lock: vector da build xong o init(), tu day chi doc.
  // race-free vi caller dam bao thread isolation theo threadIndex.
  return buffers_[threadIndex][frameIndex];
}

/// huy tat ca pool/buffer (RAII tu xu nhung can control timing)
void ThreadCommandPool::cleanup() {
  buffers_.clear();
  pools_.clear();
}