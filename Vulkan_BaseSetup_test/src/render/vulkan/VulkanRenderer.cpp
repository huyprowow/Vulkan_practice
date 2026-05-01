#include "VulkanRenderer.hpp"
#include "../../core/Types.hpp"
#include "VulkanSwapchain.hpp"

#include <ktx.h>

#include <chrono>
#include <fstream>
#include <iostream>

#if !defined(__ANDROID__)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#endif

/// Khởi tạo toàn bộ renderer: pipeline, command buffers, textures, buffers,
/// (android: assetManager) sync objects
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

  createDescriptorSetLayout(); // layout descriptor de bind cac uniform vao
  // pipeline
  createGraphicsPipeline(); // tao pipeline de render
  createCommandPool();      // tao command pool luu cac command buffer
  texture_.init(*device_, *memory_, *swapchain_, TEXTURE_PATH
#if defined(__ANDROID__)
                ,
                assetManager_
#endif
  );

  model_.load(*device_, *memory_, MODEL_PATH
#if defined(__ANDROID__)
              ,
              assetManager_
#endif
  );
  
  setupGameObjects();
  createUniformBuffers(); // uniform cho mỗi gameObject
  createDescriptorPool(); // tao descriptor pool luu cac descriptor set (1:n -
  // pool:set)
  createDescriptorSets(); // 1:1 - descriptor set: buffer resoure
  createCommandBuffers(); // tao command buffer luu cac command

  createSyncObjects(); // dong bo ( semaphore cho swapchain (chan gpu tranh
                       // xung dot- xac dinh thu tu thao tac), fence cho viec
                       // render chi 1 khung hinh tai 1 thoi diem giu gpu cpu
                       // dong bo)

  //init particle system
  auto computeSpv = readFile("shaders/compute.spv"
#if defined(__ANDROID__)
                             ,
                             assetManager_
#endif
  );
  particleSystem_.init(*device_, *memory_, commandPool_,
                       swapchain_->getImageFormat(), texture_.getDepthFormat(),
                       device_->msaaSamples_, computeSpv);
}

void VulkanRenderer::createDescriptorSetLayout() {
  std::array bindings = {
      vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                     vk::ShaderStageFlagBits::eVertex, nullptr),
      vk::DescriptorSetLayoutBinding(
          1, vk::DescriptorType::eCombinedImageSampler, 1,
          vk::ShaderStageFlagBits::eFragment, nullptr)};

  vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = bindings.size(),
                                               .pBindings = bindings.data()};
  descriptorSetLayout_ =
      vk::raii::DescriptorSetLayout(device_->getDevice(), layoutInfo);
}

/// Tạo graphics pipeline: shader stages, fixed-function state, dynamic
/// rendering
void VulkanRenderer::createGraphicsPipeline() {
  // shader stage: shader code
  vk::raii::ShaderModule shaderModule =
      createShaderModule(readFile("shaders/shader.spv"
#if defined(__ANDROID__)
                                  ,
                                  assetManager_
#endif
                                  ));
  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = shaderModule,
      .pName = "vertMain"};
  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = shaderModule,
      .pName = "fragMain"};

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};
  // Fixed-function state: xac dinh giai doan co dinh (input assembly,
  // rasterizer, viewport and color blending)
  // bind dl vertex
  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescription,
      .vertexAttributeDescriptionCount = attributeDescriptions.size(),
      .pVertexAttributeDescriptions = attributeDescriptions.data()};

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList};
  vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                    .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = vk::False,
      .depthBiasSlopeFactor = 1.0f,
      .lineWidth = 1.0f};

  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = device_->msaaSamples_,
      .sampleShadingEnable = vk::True,
      .minSampleShading = 0.2f};

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = vk::False,
      .colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

  vk::PipelineDepthStencilStateCreateInfo depthStencil{
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eLess,
      .depthBoundsTestEnable = vk::False,
      .stencilTestEnable = vk::False};
  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment};

  std::vector dynamicStates = {vk::DynamicState::eViewport,
                               vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()};

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1,
                                                  .pSetLayouts =
                                                      &*descriptorSetLayout_,
                                                  .pushConstantRangeCount = 0};

  // pipeline layout: uniform, push shader nhan co the cap nhap luc ve
  pipelineLayout_ =
      vk::raii::PipelineLayout(device_->getDevice(), pipelineLayoutInfo);

  // render pass
  // dynamic render: dinh dang cac tep dinh kem sd khi render
  vk::Format depthFormat = texture_.findDepthFormat(*device_);

  vk::Format colorFormat = swapchain_->getImageFormat();
  vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &colorFormat,
      .depthAttachmentFormat = depthFormat};
  vk::GraphicsPipelineCreateInfo pipelineInfo{
      .pNext = &pipelineRenderingCreateInfo,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depthStencil,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = pipelineLayout_,
      .renderPass = nullptr // dat null de dung dynamic render thay cho render
                            // pass truyen thong
  };
  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfo>
      pipelineCreateInfoChain = {pipelineInfo, pipelineRenderingCreateInfo};

  graphicsPipeline_ = vk::raii::Pipeline(
      device_->getDevice(), nullptr,
      pipelineCreateInfoChain
          .get<vk::GraphicsPipelineCreateInfo>()); // tham so thu 2 la
                                                   // VK_NULL_HANDLE (dung de)
}

vk::raii::ShaderModule
VulkanRenderer::createShaderModule(const std::vector<char> &code) const {
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t *>(code.data())};
  vk::raii::ShaderModule shaderModule{device_->getDevice(), createInfo};
  return shaderModule;
}

/// Nếu Android và có assetManager → dùng AAssetManager_open để đọc file trong
/// APK. Nếu không → dùng filesystem path.(fallback)
/// trong desktop load tu file system, trong android load tu resource cua APK
std::vector<char> VulkanRenderer::readFile(const std::string &filename
#if defined(__ANDROID__)
                                           ,
                                           AAssetManager *assetManager
#endif

) {

#if defined(__ANDROID__)
  if (assetManager) {
    AAsset *asset =
        AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
      throw std::runtime_error("failed to open asset: " + filename);
    }
    const off_t length = AAsset_getLength(asset);
    std::vector<char> buffer(static_cast<size_t>(length));
    const int64_t readBytes = AAsset_read(asset, buffer.data(), length);
    AAsset_close(asset);
    if (readBytes != length) {
      throw std::runtime_error("failed to read full asset: " + filename);
    }
    return buffer;
  }
#endif
  // Desktop (or Android fallback) filesystem path
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }
  std::vector<char> buffer(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();
  return buffer;
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

/// Ghi command buffer: transition layout, begin rendering, bindpipeline, draw,
/// transition present
void VulkanRenderer::recordCommandBuffer(uint32_t imageIndex) {
  auto &commandBuffer = commandBuffers_[frameIndex_];
  commandBuffer.begin({});
  // Before starting rendering, transition the swapchain image to
  // COLOR_ATTACHMENT_OPTIMAL
  transition_image_layout(
      swapchain_->getImages()[imageIndex], vk::ImageLayout::eUndefined,
      vk::ImageLayout::eColorAttachmentOptimal,
      {}, // srcAccessMask (no need to wait for previous operations)
      vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
      vk::ImageAspectFlagBits::eColor, 1);
  // transition for the MSAA color image
  transition_image_layout(*texture_.getColorImage(), vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal, {},
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::ImageAspectFlagBits::eColor, 1);
  // transition for the depth image
  transition_image_layout(*texture_.getDepthImage(), vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthAttachmentOptimal,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::ImageAspectFlagBits::eDepth, 1);
  vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
  vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
  vk::RenderingAttachmentInfo attachmentInfo = {
      .imageView = texture_.getColorView(),
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .resolveMode = vk::ResolveModeFlagBits::eAverage,
      .resolveImageView = swapchain_->getImageViews()[imageIndex],
      .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor,
  };

  vk::RenderingAttachmentInfo depthAttachmentInfo = {
      .imageView = texture_.getDepthView(),
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = clearDepth};

  vk::RenderingInfo renderingInfo = {
      .renderArea = {.offset = {0, 0}, .extent = swapchain_->getExtent()},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachmentInfo,
      .pDepthAttachment = &depthAttachmentInfo,
  };

  commandBuffer.beginRendering(renderingInfo);
  // VẼ MULTIPLE OBJECTS — mới
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
    *graphicsPipeline_);
    
    commandBuffer.setViewport(
      0, vk::Viewport(
        0.0f, 0.0f, static_cast<float>(swapchain_->getExtent().width),
             static_cast<float>(swapchain_->getExtent().height), 0.0f, 1.0f));
  commandBuffer.setScissor(
    0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_->getExtent()));
    // Bind vertex/index buffer 1 lần (chia sẻ giữa các objects)
  model_.bind(commandBuffer);
  // Loop draw từng object
  for (auto &obj : gameObjects_) {
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
      pipelineLayout_, 0,
      *obj.descriptorSets[frameIndex_], nullptr);
      commandBuffer.drawIndexed(model_.getIndexCount(), 1, 0, 0, 0);
    }
    // VẼ PARTICLE SYSTEM
    particleSystem_.recordDraw(commandBuffer, frameIndex_);
  commandBuffer.endRendering();
  // After rendering, transition the swapchain image to PRESENT_SRC
  transition_image_layout(
      swapchain_->getImages()[imageIndex],
      vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
      {},                                                 // dstAccessMask
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
      vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
      vk::ImageAspectFlagBits::eColor, 1);
  commandBuffer.end();
}

/// Chuyển đổi layout image bằng VkImageMemoryBarrier2 (Vulkan 1.3
/// synchronization2)
void VulkanRenderer::transition_image_layout(
    vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
    vk::ImageAspectFlags image_aspect_flags, uint32_t mipLevels) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {.aspectMask = image_aspect_flags,
                           .baseMipLevel = 0,
                           .levelCount = mipLevels,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  vk::DependencyInfo dependencyInfo = {.dependencyFlags = {},
                                       .imageMemoryBarrierCount = 1,
                                       .pImageMemoryBarriers = &barrier};
  commandBuffers_[frameIndex_].pipelineBarrier2(dependencyInfo);
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


/// Tạo uniform buffers với persistent mapping cho toàn bộ vòng đời ứng dụng
void VulkanRenderer::createUniformBuffers() {
  // persistent mapping use for all instance through app life time , not remap
  // effect perf
  // For each game object
  for (auto &gameObject : gameObjects_) {
    gameObject.uniformBuffers.clear();
    gameObject.uniformBuffersMemory.clear();
    gameObject.uniformBuffersMapped.clear();

    // Create uniform buffers for each frame in flight
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
      vk::raii::Buffer buffer({});
      vk::raii::DeviceMemory bufferMem({});
      memory_->createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                            vk::MemoryPropertyFlagBits::eHostVisible |
                                vk::MemoryPropertyFlagBits::eHostCoherent,
                            buffer, bufferMem);
      gameObject.uniformBuffers.emplace_back(std::move(buffer));
      gameObject.uniformBuffersMemory.emplace_back(std::move(bufferMem));
      gameObject.uniformBuffersMapped.emplace_back(
          gameObject.uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
  }
}

void VulkanRenderer::createDescriptorPool() {
  // nhan 2 uniform buffer cho compute shader va 1 uniform buffer cho graphics
  // shader
  std::array poolSize{
      vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,
                             MAX_FRAMES_IN_FLIGHT *
                                 MAX_OBJECTS ), 
      vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,
                             MAX_FRAMES_IN_FLIGHT * MAX_OBJECTS),
      vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer,
                             MAX_FRAMES_IN_FLIGHT *
                                 2)}; // compute SSBO ping-pong
  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = MAX_FRAMES_IN_FLIGHT * MAX_OBJECTS , 
      .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
      .pPoolSizes = poolSize.data()};
  descriptorPool_ = vk::raii::DescriptorPool(device_->getDevice(), poolInfo);
}

void VulkanRenderer::createDescriptorSets() {
  // For each game object
  for (auto &gameObject : gameObjects_) {
    // Create descriptor sets for each frame in flight
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                                 *descriptorSetLayout_);
    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = *descriptorPool_,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()};

    gameObject.descriptorSets.clear();
    gameObject.descriptorSets =
        device_->getDevice().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo bufferInfo{.buffer =
                                              *gameObject.uniformBuffers[i],
                                          .offset = 0,
                                          .range = sizeof(UniformBufferObject)};
      vk::DescriptorImageInfo imageInfo{
          .sampler = *texture_.getSampler(),
          .imageView = *texture_.getTextureView(),
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
      std::array descriptorWrites{
          vk::WriteDescriptorSet{.dstSet = *gameObject.descriptorSets[i],
                                 .dstBinding = 0,
                                 .dstArrayElement = 0,
                                 .descriptorCount = 1,
                                 .descriptorType =
                                     vk::DescriptorType::eUniformBuffer,
                                 .pBufferInfo = &bufferInfo},
          vk::WriteDescriptorSet{.dstSet = *gameObject.descriptorSets[i],
                                 .dstBinding = 1,
                                 .dstArrayElement = 0,
                                 .descriptorCount = 1,
                                 .descriptorType =
                                     vk::DescriptorType::eCombinedImageSampler,
                                 .pImageInfo = &imageInfo}};
      device_->getDevice().updateDescriptorSets(descriptorWrites, {});
    }
  }
}

/// Initialize the game objects with different positions, rotations, and scales
void VulkanRenderer::setupGameObjects() {
  gameObjects_.resize(MAX_OBJECTS);
  // Object 1 - Center
  gameObjects_[0].position = {0.0f, 0.0f, 0.0f};
  gameObjects_[0].rotation = {0.0f, 0.0f, 0.0f};
  gameObjects_[0].scale = {1.0f, 1.0f, 1.0f};

  // Object 2 - Left
  gameObjects_[1].position = {-2.0f, 0.0f, -1.0f};
  gameObjects_[1].rotation = {0.0f, glm::radians(45.0f), 0.0f};
  gameObjects_[1].scale = {0.75f, 0.75f, 0.75f};

  // Object 3 - Right
  gameObjects_[2].position = {2.0f, 0.0f, -1.0f};
  gameObjects_[2].rotation = {0.0f, glm::radians(-45.0f), 0.0f};
  gameObjects_[2].scale = {0.75f, 0.75f, 0.75f};
}

/// Cập nhật uniform buffer: model/view/projection matrix mỗi frame
void VulkanRenderer::updateUniformBuffer(
    uint32_t currentImage) { // (option)co the dung push constant truyen dl
                             // thuong xuyen thay doi
  static auto startTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float>(currentTime - startTime).count();

  // Camera and projection matrices (shared by all objects)
  glm::mat4 view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 6.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 1.0f, 0.0f));
  glm::mat4 proj =
      glm::perspective(glm::radians(45.0f),
                       static_cast<float>(swapchain_->getExtent().width) /
                           static_cast<float>(swapchain_->getExtent().height),
                       0.1f, 20.0f);
  proj[1][1] *= -1; // Flip Y for Vulkan

  // Update uniform buffers for each object
  for (auto &gameObject : gameObjects_) {
    // Apply continuous rotation to the object
    gameObject.rotation.y += 0.001f; // Slow rotation around Y axis

    // Get the model matrix for this object
    glm::mat4 model = gameObject.getModelMatrix();

    // Create and update the UBO
    UniformBufferObject ubo{.model = model, .view = view, .proj = proj};

    // Copy the UBO data to the mapped memory
    memcpy(gameObject.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }
}

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
  // COMPUTE
  auto computeFenceResult = device_->getDevice().waitForFences(
      *particleSystem_.computeInFlightFence(frameIndex_), vk::True, UINT64_MAX);

  if (computeFenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for compute fence!");
  }

  particleSystem_.updateComputeUniformBuffer(frameIndex_);
  device_->getDevice().resetFences(*particleSystem_.computeInFlightFence(frameIndex_));
  particleSystem_.computeCommandBuffer(frameIndex_).reset();
  particleSystem_.recordComputeCommandBuffer(frameIndex_);
  vk::SubmitInfo computeSubmitInfo{
      .commandBufferCount = 1,
      .pCommandBuffers = &*particleSystem_.computeCommandBuffer(frameIndex_),
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*particleSystem_.computeFinishedSemaphore(frameIndex_)};
  device_->getGraphicsQueue().submit(computeSubmitInfo,
                                     *particleSystem_.computeInFlightFence(frameIndex_));
  // GRAPHICS
  auto fenceResult = device_->getDevice().waitForFences(
      *inFlightFences_[frameIndex_], vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for fence!");
  }
  device_->getDevice().resetFences(*inFlightFences_[frameIndex_]);

  auto [result, imageIndex] = swapchain_->getSwapChain().acquireNextImage(
      UINT64_MAX, *presentCompleteSemaphores_[frameIndex_], nullptr);

  if (result == vk::Result::eErrorOutOfDateKHR) {
    recreateSwapChain();
    return;
  }
  if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  commandBuffers_[frameIndex_].reset();
  recordCommandBuffer(imageIndex);

  updateUniformBuffer(frameIndex_);

  // gui bo dem lenh
  vk::Semaphore waitSemaphores[] = {// mảng 2 phần tử
                                    *particleSystem_.computeFinishedSemaphore(frameIndex_),
                                    *presentCompleteSemaphores_[frameIndex_]};
  vk::PipelineStageFlags waitStages[] = {
      // mảng 2 phần tử
      vk::PipelineStageFlagBits::eVertexInput,
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  const vk::SubmitInfo graphicsSubmitInfo{
      .waitSemaphoreCount = 2,
      .pWaitSemaphores = waitSemaphores,
      .pWaitDstStageMask = waitStages,
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffers_[frameIndex_],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*renderFinishedSemaphores_[imageIndex]};
  device_->getGraphicsQueue().submit(graphicsSubmitInfo,
                                     *inFlightFences_[frameIndex_]);

  // PRESENT
  try {
    const vk::PresentInfoKHR presentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*renderFinishedSemaphores_[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &*swapchain_->getSwapChain(),
        .pImageIndices = &imageIndex};
    result = device_->getGraphicsQueue().presentKHR(presentInfoKHR);
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
  texture_.resetSwapChainResources();
  
  commandBuffers_.clear();
  renderFinishedSemaphores_.clear();
  presentCompleteSemaphores_.clear();
  inFlightFences_.clear();
  
  particleSystem_.resetSwapChainResources();
  
}

void VulkanRenderer::createSwapChainDependentResources() {
  texture_.recreateSwapChainResources();
  createCommandBuffers();
  createSyncObjects();
}

/// Tạo lại swapchain khi window resize hoặc swapchain out-of-date
void VulkanRenderer::recreateSwapChain() {
  int width = 0, height = 0;
  window_->getFramebufferSize(width, height);
  while (width == 0 || height == 0) {
    window_->getFramebufferSize(width, height);
#if !defined(__ANDROID__)
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
  cleanupSwapChainResources();
  particleSystem_.cleanup();
  texture_.cleanup();
  // graphics
  for (auto &obj : gameObjects_) {
    obj.descriptorSets.clear();
    obj.uniformBuffersMapped.clear();
    obj.uniformBuffersMemory.clear();
    obj.uniformBuffers.clear();
  }
  gameObjects_.clear();

  descriptorPool_ = nullptr;

  model_.cleanup();

  graphicsPipeline_ = nullptr;
  pipelineLayout_ = nullptr;
  descriptorSetLayout_ = nullptr;
  memory_.reset();
  commandPool_ = nullptr;
}