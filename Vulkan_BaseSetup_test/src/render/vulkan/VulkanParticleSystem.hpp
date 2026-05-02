#pragma once
#include "../../core/Types.hpp"
#include "ThreadCommandPool.hpp"
#include "VulkanDevice.hpp"
#include "VulkanMemory.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

///  quản lý compute shader + particle rendering (Ch.11 Khronos tutorial).
class VulkanParticleSystem {
public:
  /// Khởi tạo toàn bộ particle system
  /// @param device VulkanDevice đã init
  /// @param memory VulkanMemory để cấp/copy buffer
  /// @param commandPool Command pool dùng chung (lấy từ Renderer)
  /// @param colorFormat Format swapchain image (cho dynamic rendering)
  /// @param depthFormat Format depth attachment
  /// @param msaaSamples Số sample MSAA (cùng với main graphics pipeline)
  /// @param computeShaderSpv Bytes SPIR-V của compute.spv (Renderer load)
  void init(VulkanDevice &device, VulkanMemory &memory,
            vk::raii::CommandPool &commandPool, vk::Format colorFormat,
            vk::Format depthFormat, vk::SampleCountFlagBits msaaSamples,
            const std::vector<char> &computeShaderSpv);
  void updateComputeUniformBuffer(uint32_t currentFrame);
  void recordComputeCommandBuffer(uint32_t frameIndex);//single thread
  /// Record particle draw vào primary command buffer của frame
  /// (Renderer gọi sau khi vẽ model, trong cùng beginRendering scope)
  void recordDraw(vk::raii::CommandBuffer &cmdBuf, uint32_t frameIndex);

  /// Truy cập sync objects và compute command buffer (Renderer dùng để submit)
  const vk::raii::CommandBuffer &
  computeCommandBuffer(uint32_t frameIndex) const;
  const vk::raii::Semaphore &
  computeFinishedSemaphore(uint32_t frameIndex) const;
  const vk::raii::Fence &computeInFlightFence(uint32_t frameIndex) const;

  /// Reset compute sync objects (gọi khi recreateSwapChain)
  void resetSwapChainResources();

  /// Cleanup tất cả resource khi shutdown
  void cleanup();

  /// khoi tao worker threads cho multithreaded compute
  /// @param threadCount so worker (recommend hardware_concurrency()/2)
  /// @param queueSubmitMutex mutex bao ve vk::Queue.submit() (Renderer own)
  void initThreads(uint32_t threadCount, std::mutex &queueSubmitMutex);

  /// dispatch compute multithreaded — replace recordComputeCommandBuffer().
  /// kick toan bo worker thread cung luc, doi tat ca xong, tra ve cmd buffers.
  /// @return vector vk::CommandBuffer da record de Renderer submit
  std::vector<vk::CommandBuffer> dispatchMultithreaded(uint32_t frameIndex);

private:
  VulkanDevice *device_ = nullptr;
  VulkanMemory *memory_ = nullptr;
  vk::raii::CommandPool *commandPool_ = nullptr;

  // Compute pipeline
  vk::raii::DescriptorSetLayout computeDescriptorSetLayout_{nullptr};
  vk::raii::PipelineLayout computePipelineLayout_{nullptr};
  vk::raii::Pipeline computePipeline_{nullptr};

  // Particle graphics pipeline
  vk::raii::PipelineLayout particlePipelineLayout_{nullptr};
  vk::raii::Pipeline particlePipeline_{nullptr};

  // Descriptor pool RIÊNG (không share với Renderer): UBO + 2 SSBO ping-pong
  vk::raii::DescriptorPool descriptorPool_{nullptr};
  std::vector<vk::raii::DescriptorSet> computeDescriptorSets_;

  // SSBO ping-pong + compute UBO
  std::vector<vk::raii::Buffer> shaderStorageBuffers_;
  std::vector<vk::raii::DeviceMemory> shaderStorageBuffersMemory_;
  // Compute UBO (deltaTime riêng, không sửa UBO model)
  std::vector<vk::raii::Buffer> computeUniformBuffers_;
  std::vector<vk::raii::DeviceMemory> computeUniformBuffersMemory_;
  std::vector<void *> computeUniformBuffersMapped_;

  // Compute command buffers
  std::vector<vk::raii::CommandBuffer> computeCommandBuffers_;

  // Compute sync
  std::vector<vk::raii::Fence> computeInFlightFences_;
  std::vector<vk::raii::Semaphore> computeFinishedSemaphores_;

  float lastFrameTime_ = 0.0f;

  /// range particle thread X xu li
  struct ParticleGroup {
    uint32_t startIndex;
    uint32_t count;
  };
  /// push constants gui xuong shader: (startIndex, count)
  struct PushConstants {
    uint32_t startIndex;
    uint32_t count;
  };

  uint32_t threadCount_ = 0;
  std::vector<std::thread> workerThreads_;
  std::atomic<bool> shouldExit_{false};

  // sync flags atomic vi cross-thread access
  std::vector<std::atomic<bool>> threadWorkReady_; // main → worker
  std::vector<std::atomic<bool>> threadWorkDone_;  // worker → main
  /// std::condition_variable + workMutex_: worker block trong wait() khi chua co
  /// viec (OS tam dung thread, khong ton CPU). Khac std::this_thread::yield() —
  /// yield van chay vong lap (neu chua co viec nhay sang thread khac) ton CPU. Luat: doi trang thai (flag) duoi lock,
  /// sau do notify_* de thread dang wait() duoc danh thuc; wait() luon kem
  /// predicate de tranh spurious wakeup.
  /// mutex + condition_variable: main goi notify_all, worker wait()
  std::mutex workMutex_;
  std::condition_variable workCv_;

  // mutex bao ve vk::Queue (Renderer own, PS chi giu pointer)
  std::mutex *queueSubmitMutex_ = nullptr;

  // per-thread x per-frame command resources (no lock can thiet)
  ThreadCommandPool threadCmdPool_;

  // chia particle ra cho moi thread
  std::vector<ParticleGroup> particleGroups_;

  // frame hien tai (atomic vi worker doc, main set)
  std::atomic<uint32_t> currentFrameIndex_{0};

  [[nodiscard]] vk::raii::ShaderModule
  createShaderModule(const std::vector<char> &code) const;

  void createComputeDescriptorSetLayout();
  void createShaderStorageBuffers();
  void createComputePipeline(const std::vector<char> &spv);
  void createParticleGraphicsPipeline(const std::vector<char> &spv,
                                      vk::Format colorFormat,
                                      vk::Format depthFormat,
                                      vk::SampleCountFlagBits msaaSamples);
  void createComputeUniformBuffers();
  void createComputeDescriptorPool();
  void createComputeDescriptorSets();
  void createComputeCommandBuffers();
  void createComputeSyncObjects();

  // === Multithreading helpers ===
  /// vong lap chinh cua worker thread (condition_variable wait → record →
  /// notify)
  void workerThreadFunc(uint32_t threadIndex);
  /// record cmd buffer cho range particle cua 1 thread
  void recordComputeCommandBufferRange(uint32_t threadIndex,
                                       uint32_t frameIndex);
  /// kich tat ca worker cung luc (parallel, KHONG sequential nhu tutorial)
  void signalThreadsToWork();
  /// doi tat ca worker xong (condition_variable wait_for co timeout phong
  /// deadlock)
  void waitForThreadsToComplete();
  /// shutdown worker threads (goi trong cleanup truoc khi destroy resources)
  void stopThreads();
};