#include "VulkanRenderer.hpp"
#include "../../core/FileUtil.hpp"
#include "../../core/Types.hpp"
#include "VulkanBarrier.hpp"
#include "VulkanSwapchain.hpp"

#include <ktx.h>

#include <chrono>
#include <fstream>
#include <iostream>

#if !defined(__ANDROID__) && !defined(VULKAN_IOS)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif			

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#endif

/// Khởi tạo renderer: command pool, command buffers, sync objects
void VulkanRenderer::init(VulkanDevice &device, VulkanSwapchain &swapchain,
                          IWindow &window,
#if defined(__ANDROID__)
                          AAssetManager *assetManager
#else
                          void *assetManager
#endif
) {
  device_ = &device;
  swapchain_ = &swapchain;
  window_ = &window;
  memory_ = std::make_unique<VulkanMemory>(*device_, commandPool_);
#if defined(__ANDROID__)
  assetManager_ = assetManager;
#endif

  surface_ = &swapchain_->getSurface();

  createCommandPool();      // tao command pool luu cac command buffer
  createCommandBuffers(); // tao command buffer luu cac command

  createSyncObjects(); // dong bo ( semaphore cho swapchain (chan gpu tranh
                       // xung dot- xac dinh thu tu thao tac), fence cho viec
                       // render chi 1 khung hinh tai 1 thoi diem giu gpu cpu
                       // dong bo)
}

/// doi sample
void VulkanRenderer::setSample(ISample *sample) {
  if (sample_) {
    device_->getDevice().waitIdle(); // sample cũ có thể còn việc đang chạy trên GPU
    sample_->shutdown();
  }
  sample_ = sample;
  if (sample_) {
    sample_->init(SampleContext{.device = *device_,
                                .memory = *memory_,
                                .swapchain = *swapchain_,
                                .commandPool = commandPool_,
                                .queueSubmitMutex = queueSubmitMutex_,
#if defined(__ANDROID__)
                             .assetManager = assetManager_,
#endif
                                });
  }
}


void VulkanRenderer::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = device_->getQueueIndex()};
  commandPool_ = vk::raii::CommandPool(device_->getDevice(), poolInfo);
}

void VulkanRenderer::createCommandBuffers() {
  commandBuffers_.clear();
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = commandPool_,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

  commandBuffers_ = vk::raii::CommandBuffers(device_->getDevice(), allocInfo);
}

/// Ghi command buffer: transition swapchain image, sample ghi lệnh vẽ,
/// overlay vẽ đè (nếu có), transition present
void VulkanRenderer::recordCommandBuffer(uint32_t imageIndex) {
  auto &commandBuffer = commandBuffers_[frameIndex_];
  commandBuffer.begin({});
  // Before starting rendering, transition the swapchain image to
  // COLOR_ATTACHMENT_OPTIMAL
  cmdTransitionImageLayout(commandBuffer,
      swapchain_->getImages()[imageIndex], vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      {}, // srcAccessMask (no need to wait for previous operations)
      vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
      vk::ImageAspectFlagBits::eColor, 1);
  const SampleFrame frame{.cmd = commandBuffer,
                          .frameIndex = frameIndex_,
                          .swapchainView = swapchain_->getImageViews()[imageIndex],
                          .extent = swapchain_->getExtent()};
  sample_->record(frame);
  if (overlay_) {
    // sample ghi xong vào swapchain image rồi overlay mới vẽ đè lên
    cmdTransitionImageLayout(
        commandBuffer, swapchain_->getImages()[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::AccessFlagBits2::eColorAttachmentRead |
            vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor, 1);
    overlay_(frame);
  }
  // After rendering, transition the swapchain image to PRESENT_SRC
  cmdTransitionImageLayout(commandBuffer,
      swapchain_->getImages()[imageIndex],
      vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
      {},                                                 // dstAccessMask
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
      vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
      vk::ImageAspectFlagBits::eColor, 1);
  commandBuffer.end();
}

void VulkanRenderer::createSyncObjects() {
  assert(presentCompleteSemaphores_.empty() &&
         renderFinishedSemaphores_.empty() && inFlightFences_.empty());

  for (size_t i = 0; i < swapchain_->getImages().size(); i++) {
    renderFinishedSemaphores_.emplace_back(device_->getDevice(),
                                           vk::SemaphoreCreateInfo());
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    presentCompleteSemaphores_.emplace_back(device_->getDevice(),
                                            vk::SemaphoreCreateInfo());
    inFlightFences_.emplace_back(
        device_->getDevice(),
        vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
  }
}

/// Tạo mipmap bằng vkCmdBlitImage, chia đôi kích thước mỗi level
// void VulkanRenderer::generateMipmaps(vk::raii::Image &image,
//                                      vk::Format imageFormat, int32_t
//                                      texWidth, int32_t texHeight, uint32_t
//                                      mipLevels) {
//   // goi vkCmdBlitImage nhieu lan de sao chep du lieu vao tung cap do
//   // cua texture mipmap (dung nhu LOD) nhung k dam bao tat ca nen tang (yeu
//   // cau phai ho tro loc tuyen tinh)

//   // Check if image format supports linear blit-ing
//   vk::FormatProperties formatProperties =
//       device_->getPhysicalDevice().getFormatProperties(imageFormat);

//   if (!(formatProperties.optimalTilingFeatures &
//         vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
//     throw std::runtime_error(
//         "texture image format does not support linear blitting!");
//   }
//   vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();

//   vk::ImageMemoryBarrier barrier = {
//       .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
//       .dstAccessMask = vk::AccessFlagBits::eTransferRead,
//       .oldLayout = vk::ImageLayout::eTransferDstOptimal,
//       .newLayout = vk::ImageLayout::eTransferSrcOptimal,
//       .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
//       .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
//       .image = image};
//   barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
//   barrier.subresourceRange.baseArrayLayer = 0;
//   barrier.subresourceRange.layerCount = 1;
//   barrier.subresourceRange.levelCount = 1;

//   // tao mip map, chuyen doi layout moi level (chuyen lai layout toi uu cho
//   // shader doc hoac layout de sao chep), chia doi kich thuoc mip map
//   int32_t mipWidth = texWidth;
//   int32_t mipHeight = texHeight;

//   for (uint32_t i = 1; i < mipLevels; i++) {
//     // chuyen doi i - 1 sang VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
//     barrier.subresourceRange.baseMipLevel = i - 1;
//     barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
//     barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
//     barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
//     barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

//     commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
//                                   vk::PipelineStageFlagBits::eTransfer, {},
//                                   {},
//                                   {}, barrier);

//     // chi dinh vung duoc sd trong thao tac sao chep (blit) (xac dinh kich
//     // thuoc region blit)
//     vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
//     offsets[0] = vk::Offset3D(0, 0, 0);
//     offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
//     dstOffsets[0] = vk::Offset3D(0, 0, 0);
//     dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1,
//                                  mipHeight > 1 ? mipHeight / 2 : 1, 1);
//     vk::ImageBlit blit = {.srcSubresource = {},
//                           .srcOffsets = offsets,
//                           .dstSubresource = {},
//                           .dstOffsets = dstOffsets};
//     blit.srcSubresource = vk::ImageSubresourceLayers(
//         vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
//     blit.dstSubresource =
//         vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);
//     // ghi lenh len command buffer
//     commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
//     image,
//                             vk::ImageLayout::eTransferDstOptimal, {blit},
//                             vk::Filter::eLinear);

//     // chuyen doi muc mip i - 1 sang VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
//     barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
//     barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
//     barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
//     barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

//     commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
//                                   vk::PipelineStageFlagBits::eFragmentShader,
//                                   {}, {}, {}, barrier);

//     // chia doi kich thuoc mip map hien tai, kiem tra kich thuoc khac 0 (k
//     // phai hinh vuong)
//     if (mipWidth > 1)
//       mipWidth /= 2;
//     if (mipHeight > 1)
//       mipHeight /= 2;
//   }

//   // chuyen mip cuoi cung tu VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ->
//   // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
//   barrier.subresourceRange.baseMipLevel = mipLevels - 1;
//   barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
//   barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
//   barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
//   barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

//   commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
//                                 vk::PipelineStageFlagBits::eFragmentShader,
//                                 {},
//                                 {}, {}, barrier);

//   endSingleTimeCommands(commandBuffer);
// }


/**
 *  step rendering common
 * - doi khung truoc xong
 * - lay anh tu swap chain
 * - ghi lai 1 bo dem lenh de ve canh len
 * - gui bo dem lenh da ghi
 * - trinh chieu anh chuoi swapchain
 */
void VulkanRenderer::drawFrame() {
  /// compute -> semaphore -> graphics -> semaphore -> present
  // ACQUIRE 
  auto fenceResult = device_->getDevice().waitForFences(
      *inFlightFences_[frameIndex_], vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for fence!");
  }

  vk::Result result;
  uint32_t imageIndex;
  try {
    std::tie(result, imageIndex) = swapchain_->getSwapChain().acquireNextImage(
        UINT64_MAX, *presentCompleteSemaphores_[frameIndex_], nullptr);
  } catch (const vk::OutOfDateKHRError &) {
    // Vulkan-Hpp ném lỗi chứ không trả eErrorOutOfDateKHR. Frame này chưa
    // submit gì nên tạo lại swapchain rồi bỏ frame là an toàn
    recreateSwapChain();
    return;
  }

  // reset sau acquire: nếu return sớm ở trên thì fence vẫn còn signaled
  device_->getDevice().resetFences(*inFlightFences_[frameIndex_]);

  // COMPUTE: việc GPU riêng của sample (vd particle), graphics chờ semaphore trả về
  SampleWait sampleWait = sample_->submitBeforeGraphics(frameIndex_);
  
  // GRAPHICS
  commandBuffers_[frameIndex_].reset();
  recordCommandBuffer(imageIndex);

  // gui bo dem lenh
  vk::Semaphore waitSemaphores[] = {
      // mảng 2 phần tử
      *presentCompleteSemaphores_[frameIndex_],sampleWait.semaphore};
  vk::PipelineStageFlags waitStages[] = {
      // mảng 2 phần tử
      vk::PipelineStageFlagBits::eColorAttachmentOutput,sampleWait.stage};
  const vk::SubmitInfo graphicsSubmitInfo{
      .waitSemaphoreCount = sampleWait.semaphore ? 2u : 1u,
      .pWaitSemaphores = waitSemaphores,
      .pWaitDstStageMask = waitStages,
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffers_[frameIndex_],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*renderFinishedSemaphores_[imageIndex]};

  // mutex bao ve queue.submit() (Vulkan spec: vk::Queue khong thread-safe)
  {
    std::lock_guard<std::mutex> lock(queueSubmitMutex_);
    device_->getGraphicsQueue().submit(graphicsSubmitInfo,
                                       *inFlightFences_[frameIndex_]);
  }

  // PRESENT
  try {
    const vk::PresentInfoKHR presentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*renderFinishedSemaphores_[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &*swapchain_->getSwapChain(),
        .pImageIndices = &imageIndex};
    // mutex bao ve presentKHR() (cung Queue API, can lock)
    {
      std::lock_guard<std::mutex> lock(queueSubmitMutex_);
      result = device_->getGraphicsQueue().presentKHR(presentInfoKHR);
    }

    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR) {
      recreateSwapChain();
    } else if (result != vk::Result::eSuccess) {
      throw std::runtime_error("failed to present swap chain image!");
    }
  } catch (const vk::SystemError &e) {
    if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR)) {
      recreateSwapChain();
      return;
    } else {
      throw;
    }
  }

  frameIndex_ = (frameIndex_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::cleanupSwapChainResources() {

  commandBuffers_.clear();
  renderFinishedSemaphores_.clear();
  presentCompleteSemaphores_.clear();
  inFlightFences_.clear();

}

void VulkanRenderer::createSwapChainDependentResources() {
  createCommandBuffers();
  createSyncObjects();
  sample_->onSwapchainRecreated();
}

/// Tạo lại swapchain khi window resize hoặc swapchain out-of-date
void VulkanRenderer::recreateSwapChain() {
  int width = 0, height = 0;
  window_->getFramebufferSize(width, height);
  while (width == 0 || height == 0) {
    window_->getFramebufferSize(width, height);
#if !defined(__ANDROID__) && !defined(VULKAN_IOS)
    glfwWaitEvents();
#endif
  }
  device_->getDevice().waitIdle();

  // tương đương cleanupSwapChain() cũ: dọn phần liên quan swapchain
  cleanupSwapChainResources();
  swapchain_->cleanup(); // nếu bạn có hàm cleanup() trong Swapchain (đang có)

  // tương đương createSwapChain + createImageViews + createDepthResources cũ
  swapchain_->recreate(device_->getPhysicalDevice(), *device_, *surface_,
                       *window_);

  createSwapChainDependentResources(); // depth + command buffers + sync
}

void VulkanRenderer::cleanup() {
  device_->getDevice().waitIdle(); // GPU phải chạy xong trước khi hủy buffer, pipeline
  cleanupSwapChainResources();
  setSample(nullptr);
  memory_.reset();
  commandPool_ = nullptr;
}
