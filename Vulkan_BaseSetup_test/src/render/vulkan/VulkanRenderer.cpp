#include "VulkanRenderer.hpp"
#include "../../core/Types.hpp"
#include "VulkanSwapchain.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <chrono>
#include <fstream>
#include <iostream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

/// Khởi tạo toàn bộ renderer: pipeline, command buffers, textures, buffers, sync objects
void VulkanRenderer::init(VulkanDevice &device, VulkanSwapchain &swapchain,
                          Window &window) {
  device_ = &device;
  swapchain_ = &swapchain;
  window_ = &window;
  surface_ = &swapchain_->getSurface();

  createDescriptorSetLayout(); // layout descriptor de bind cac uniform vao
                               // pipeline
  createGraphicsPipeline();    // tao pipeline de render
  createCommandPool();         // tao command pool luu cac command buffer
  createColorResources();      // multi-sampled color buffer
  createDepthResources();      // depth buffer
  createTextureImage(); // tao texture image, tai image len gpu de toi uu (neu
                        // k no doc PCI -> cham), chuyen layout
  createTextureImageView(); // tao image view cho texture image
  createTextureSampler();   // tao sampler cho texture image (truy cap image
                            // thong qua sampler de ap dung cac phep bien doi
                            // (mipmap, filter,...))
  loadModel();
  createVertexBuffer();   // tao vertex buffer luu cac vertex data
  createIndexBuffer();    // tao index buffer luu cac index data (tranh ve trung
                          // dinh)
  createUniformBuffers(); // uniform
  createDescriptorPool(); // tao descriptor pool luu cac descriptor set (1:n -
                          // pool:set)
  createDescriptorSets(); // 1:1 - descriptor set: buffer resoure
  createCommandBuffers(); // tao command buffer luu cac command
  createSyncObjects();    // dong bo ( semaphore cho swapchain (chan gpu tranh
                          // xung dot- xac dinh thu tu thao tac), fence cho viec
                          // render chi 1 khung hinh tai 1 thoi diem giu gpu cpu
                          // dong bo)
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

/// Tạo graphics pipeline: shader stages, fixed-function state, dynamic rendering
void VulkanRenderer::createGraphicsPipeline() {
  // shader stage: shader code
  vk::raii::ShaderModule shaderModule =
      createShaderModule(readFile("shaders/slang.spv"));
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
      .frontFace =
          vk::FrontFace::eCounterClockwise, // project matrix lat y nen doi
                                            // lai huong clockwise check tranh
                                            // mat sau bi loai bo (backface
                                            // culling)
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
  vk::Format depthFormat = findDepthFormat();

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

std::vector<char> VulkanRenderer::readFile(const std::string &filename) {
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

vk::raii::CommandBuffer VulkanRenderer::beginSingleTimeCommands() {
  vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool_,
                                          .level =
                                              vk::CommandBufferLevel::ePrimary,
                                          .commandBufferCount = 1};
  vk::raii::CommandBuffer commandBuffer =
      std::move(device_->getDevice().allocateCommandBuffers(allocInfo).front());

  vk::CommandBufferBeginInfo beginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffer.begin(beginInfo);

  return commandBuffer;
}

void VulkanRenderer::endSingleTimeCommands(
    vk::raii::CommandBuffer &commandBuffer) {
  commandBuffer.end();

  vk::SubmitInfo submitInfo{.commandBufferCount = 1,
                            .pCommandBuffers = &*commandBuffer};
  device_->getGraphicsQueue().submit(submitInfo, nullptr);
  device_->getGraphicsQueue().waitIdle();
}

void VulkanRenderer::createCommandBuffers() {
  commandBuffers_.clear();
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = commandPool_,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

  commandBuffers_ = vk::raii::CommandBuffers(device_->getDevice(), allocInfo);
}

/// Ghi command buffer: transition layout, begin rendering, bindpipeline, draw, transition present
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
  // transition for the depth image
  transition_image_layout(*depthImage_, vk::ImageLayout::eUndefined,
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
      .imageView = colorImageView_,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .resolveMode = vk::ResolveModeFlagBits::eAverage,
      .resolveImageView = swapchain_->getImageViews()[imageIndex],
      .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor,
  };

  vk::RenderingAttachmentInfo depthAttachmentInfo = {
      .imageView = depthImageView_,
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
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                             *graphicsPipeline_);
  commandBuffer.setViewport(
      0, vk::Viewport(
             0.0f, 0.0f, static_cast<float>(swapchain_->getExtent().width),
             static_cast<float>(swapchain_->getExtent().height), 0.0f, 1.0f));
  commandBuffer.setScissor(
      0, vk::Rect2D(vk::Offset2D(0, 0), swapchain_->getExtent()));
  commandBuffer.bindVertexBuffers(0, *vertexBuffer_, {0});
  commandBuffer.bindIndexBuffer(*indexBuffer_, 0, vk::IndexType::eUint32);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   pipelineLayout_, 0,
                                   *descriptorSets_[frameIndex_], nullptr);
  commandBuffer.drawIndexed(
      indices_.size(), 1, 0, 0,
      0); //(index count, instance count, offset index buffer, offset vertex
          // before indexing to vertex buffer, offset for instancing)
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

/// Chuyển đổi layout image bằng pipeline barrier (dùng cho texture upload)
void VulkanRenderer::transitionImageLayout(const vk::raii::Image &image,
                                           vk::ImageLayout oldLayout,
                                           vk::ImageLayout newLayout,
                                           uint32_t mipLevels) {
  // vk cho phep chuyen doi bo cuc toi uu cho tung nhiem vu
  auto commandBuffer = beginSingleTimeCommands();

  vk::ImageMemoryBarrier barrier{
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .image = *image, // dereference RAII image -> raw VkImage
      .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0,
                           mipLevels, // levelCount
                           0, 1}};

  // 0 xd -> transfer dst: thao tac ghi k can cho
  // transfer dst -> shader read: doc shader can cho ghi dl transfer xong
  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined &&
      newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr,
                                barrier);
  endSingleTimeCommands(commandBuffer);
}

/// Chuyển đổi layout image bằng VkImageMemoryBarrier2 (Vulkan 1.3 synchronization2)
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

void VulkanRenderer::createImage(uint32_t width, uint32_t height,
                                 uint32_t mipLevels,
                                 vk::SampleCountFlagBits numSamples,
                                 vk::Format format, vk::ImageTiling tiling,
                                 vk::ImageUsageFlags usage,
                                 vk::MemoryPropertyFlags properties,
                                 vk::raii::Image &image,
                                 vk::raii::DeviceMemory &imageMemory) {
  vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
                                .format = format,
                                .extent = {width, height, 1},
                                .mipLevels = mipLevels,
                                .arrayLayers = 1,
                                .samples = numSamples,
                                .tiling = tiling,
                                .usage = usage,
                                .sharingMode = vk::SharingMode::eExclusive};

  image = vk::raii::Image(device_->getDevice(), imageInfo);

  vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryType(memRequirements.memoryTypeBits, properties)};
  imageMemory = vk::raii::DeviceMemory(device_->getDevice(), allocInfo);
  image.bindMemory(imageMemory, 0);
}

void VulkanRenderer::createColorResources() {
  vk::Format colorFormat = swapchain_->getImageFormat();
  vk::Extent2D swapChainExtent = swapchain_->getExtent();

  createImage(swapChainExtent.width, swapChainExtent.height, 1,
              device_->msaaSamples_, colorFormat, vk::ImageTiling::eOptimal,
              vk::ImageUsageFlagBits::eTransientAttachment |
                  vk::ImageUsageFlagBits::eColorAttachment,
              vk::MemoryPropertyFlagBits::eDeviceLocal, colorImage_,
              colorImageMemory_);
  colorImageView_ = createImageView(colorImage_, colorFormat,
                                    vk::ImageAspectFlagBits::eColor, 1);
}

void VulkanRenderer::createDepthResources() {
  vk::Format depthFormat = findDepthFormat();
  vk::Extent2D swapChainExtent = swapchain_->getExtent();
  createImage(swapChainExtent.width, swapChainExtent.height, 1,
              device_->msaaSamples_, depthFormat, vk::ImageTiling::eOptimal,
              vk::ImageUsageFlagBits::eDepthStencilAttachment,
              vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage_,
              depthImageMemory_);
  depthImageView_ = createImageView(depthImage_, depthFormat,
                                    vk::ImageAspectFlagBits::eDepth, 1);
}

vk::Format
VulkanRenderer::findSupportedFormat(const std::vector<vk::Format> &candidates,
                                    vk::ImageTiling tiling,
                                    vk::FormatFeatureFlags features) {
  for (const auto format : candidates) {
    vk::FormatProperties props =
        device_->getPhysicalDevice().getFormatProperties(format);
    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    }
    if (tiling == vk::ImageTiling::eOptimal &&
        (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

vk::Format VulkanRenderer::findDepthFormat() {
  return findSupportedFormat(
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
       vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool VulkanRenderer::hasStencilComponent(vk::Format format) {
  return format == vk::Format::eD32SfloatS8Uint ||
         format == vk::Format::eD24UnormS8Uint;
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter,
                                        vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties =
      device_->getPhysicalDevice().getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

/// Tải texture image lên GPU qua staging buffer, tạo mipmaps
void VulkanRenderer::createTextureImage() {
  // load anh
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;
  mipLevels_ = static_cast<uint32_t>(
                   std::floor(std::log2(std::max(texWidth, texHeight)))) +
               1;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }
  // staging buffer tai anh len gpu
  vk::raii::Buffer stagingBuffer({});             // local RAII buffer
  vk::raii::DeviceMemory stagingBufferMemory({}); // local RAII memory
  createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               stagingBuffer, stagingBufferMemory);
  void *data = stagingBufferMemory.mapMemory(0, imageSize);
  std::memcpy(data, pixels, static_cast<size_t>(imageSize));
  stagingBufferMemory.unmapMemory();
  stbi_image_free(pixels);

  vk::raii::Image textureImageTemp({});              // temp RAII image
  vk::raii::DeviceMemory textureImageMemoryTemp({}); // temp RAII memory
  createImage(texWidth, texHeight, mipLevels_, vk::SampleCountFlagBits::e1,
              vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
              vk::ImageUsageFlagBits::eTransferSrc |
                  vk::ImageUsageFlagBits::eTransferDst |
                  vk::ImageUsageFlagBits::eSampled,
              vk::MemoryPropertyFlagBits::eDeviceLocal, textureImageTemp,
              textureImageMemoryTemp);
  // Assign to class members
  textureImage_ = std::move(textureImageTemp);
  textureImageMemory_ = std::move(textureImageMemoryTemp);

  transitionImageLayout(textureImage_, vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal,
                        mipLevels_); // chuyen layout toi uu nhan dl
                                     // (ghi)
  copyBufferToImage(stagingBuffer, textureImage_,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  // transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating
  // mipmaps
  // note: thuc te nen tao mipmap truoc roi load vao chu k tao khi chay
  generateMipmaps(textureImage_, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight,
                  mipLevels_);
}

/// Tạo mipmap bằng vkCmdBlitImage, chia đôi kích thước mỗi level
void VulkanRenderer::generateMipmaps(vk::raii::Image &image,
                                     vk::Format imageFormat, int32_t texWidth,
                                     int32_t texHeight, uint32_t mipLevels) {
  // goi vkCmdBlitImage nhieu lan de sao chep du lieu vao tung cap do
  // cua texture mipmap (dung nhu LOD) nhung k dam bao tat ca nen tang (yeu
  // cau phai ho tro loc tuyen tinh)

  // Check if image format supports linear blit-ing
  vk::FormatProperties formatProperties =
      device_->getPhysicalDevice().getFormatProperties(imageFormat);

  if (!(formatProperties.optimalTilingFeatures &
        vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
    throw std::runtime_error(
        "texture image format does not support linear blitting!");
  }
  vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();

  vk::ImageMemoryBarrier barrier = {
      .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
      .dstAccessMask = vk::AccessFlagBits::eTransferRead,
      .oldLayout = vk::ImageLayout::eTransferDstOptimal,
      .newLayout = vk::ImageLayout::eTransferSrcOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = image};
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  // tao mip map, chuyen doi layout moi level (chuyen lai layout toi uu cho
  // shader doc hoac layout de sao chep), chia doi kich thuoc mip map
  int32_t mipWidth = texWidth;
  int32_t mipHeight = texHeight;

  for (uint32_t i = 1; i < mipLevels; i++) {
    // chuyen doi i - 1 sang VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                  vk::PipelineStageFlagBits::eTransfer, {}, {},
                                  {}, barrier);

    // chi dinh vung duoc sd trong thao tac sao chep (blit) (xac dinh kich
    // thuoc region blit)
    vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
    offsets[0] = vk::Offset3D(0, 0, 0);
    offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
    dstOffsets[0] = vk::Offset3D(0, 0, 0);
    dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1,
                                 mipHeight > 1 ? mipHeight / 2 : 1, 1);
    vk::ImageBlit blit = {.srcSubresource = {},
                          .srcOffsets = offsets,
                          .dstSubresource = {},
                          .dstOffsets = dstOffsets};
    blit.srcSubresource = vk::ImageSubresourceLayers(
        vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
    blit.dstSubresource =
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);
    // ghi lenh len command buffer
    commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image,
                            vk::ImageLayout::eTransferDstOptimal, {blit},
                            vk::Filter::eLinear);

    // chuyen doi muc mip i - 1 sang VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                  vk::PipelineStageFlagBits::eFragmentShader,
                                  {}, {}, {}, barrier);

    // chia doi kich thuoc mip map hien tai, kiem tra kich thuoc khac 0 (k
    // phai hinh vuong)
    if (mipWidth > 1)
      mipWidth /= 2;
    if (mipHeight > 1)
      mipHeight /= 2;
  }

  // chuyen mip cuoi cung tu VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ->
  // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
  barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

  commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                vk::PipelineStageFlagBits::eFragmentShader, {},
                                {}, {}, barrier);

  endSingleTimeCommands(commandBuffer);
}

/// Overload for RAII images
/// de su dung cho texture image no dung vk::raii::Image nen phai nap chong
vk::raii::ImageView
VulkanRenderer::createImageView(const vk::raii::Image &image, vk::Format format,
                                vk::ImageAspectFlags aspectFlags,
                                uint32_t mipLevels) {
  vk::ImageViewCreateInfo viewInfo{.image = *image,
                                   .viewType = vk::ImageViewType::e2D,
                                   .format = format,
                                   .subresourceRange = {aspectFlags, 0,
                                                        mipLevels, // levelCount
                                                        0, 1}};
  return vk::raii::ImageView(device_->getDevice(), viewInfo);
}

void VulkanRenderer::createTextureImageView() {
  textureImageView_ =
      createImageView(textureImage_, vk::Format::eR8G8B8A8Srgb,
                      vk::ImageAspectFlagBits::eColor, mipLevels_);
}

/// Tạo sampler cho texture: filter, mipmap mode, anisotropy
void VulkanRenderer::createTextureSampler() {
  // sampler kiem soat dl doc, muc mipmap, filer,...
  vk::PhysicalDeviceProperties properties =
      device_->getPhysicalDevice().getProperties();
  vk::SamplerCreateInfo samplerInfo{
      .magFilter = vk::Filter::eLinear, // gan camera dung cai nay
      .minFilter = vk::Filter::eLinear, // xa camera dung cai nay
      .mipmapMode =
          vk::SamplerMipmapMode::eLinear, // linear select 2mip interpolation
                                          // between mipmaps, nearest lod
                                          // selection mip
      .addressModeU = vk::SamplerAddressMode::eRepeat,
      .addressModeV = vk::SamplerAddressMode::eRepeat,
      .addressModeW = vk::SamplerAddressMode::eRepeat,
      .mipLodBias =
          0.0f, // lod khong am, va chi bang 0 khi o gan camera.
                // mipLodBias cho phep buoc Vulkan su dung gia tri
                // thap hon lod va level so voi gia tri ma no thuong su dung
      .anisotropyEnable = vk::True,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways,
      .minLod = 0.0f,            // dat muc mipmap thap nhat
      .maxLod = vk::LodClampNone // khong gioi han muc mipmap dam bao toan bo
                                 // pv mip dc dung
  };
  samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  samplerInfo.unnormalizedCoordinates = vk::False;
  // samplerInfo.minLod = static_cast<float>(mipLevels_ / 2); //test mip

  textureSampler_ = vk::raii::Sampler(device_->getDevice(), samplerInfo);
}

void VulkanRenderer::createBuffer(vk::DeviceSize size,
                                  vk::BufferUsageFlags usage,
                                  vk::MemoryPropertyFlags properties,
                                  vk::raii::Buffer &buffer,
                                  vk::raii::DeviceMemory &bufferMemory) {
  vk::BufferCreateInfo bufferInfo{
      .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
  buffer = vk::raii::Buffer(device_->getDevice(), bufferInfo);
  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryType(memRequirements.memoryTypeBits, properties)};
  bufferMemory = vk::raii::DeviceMemory(device_->getDevice(), allocInfo);
  buffer.bindMemory(*bufferMemory, 0);
}

void VulkanRenderer::copyBuffer(vk::raii::Buffer &srcBuffer,
                                vk::raii::Buffer &dstBuffer,
                                vk::DeviceSize size) {
  vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
  commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer,
                               vk::BufferCopy(0, 0, size));
  endSingleTimeCommands(commandCopyBuffer);
}

void VulkanRenderer::copyBufferToImage(const vk::raii::Buffer &buffer,
                                       vk::raii::Image &image, uint32_t width,
                                       uint32_t height) {
  vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
  vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1}};

  commandBuffer.copyBufferToImage(
      buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
  // Submit the buffer copy to the graphics queue
  endSingleTimeCommands(commandBuffer);
}

/// Tạo vertex buffer: staging buffer trên host, copy sang device-local memory
void VulkanRenderer::createVertexBuffer() {
  // vi cpu k the truy cap truc tiep vung nho toi uu nhat trong gpu
  // (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) => dung bo dem tam thoi tren host
  // (cpu). sau do khi hoat dong copy dl tu host sang bo nho local cua device
  // (gpu)

  // staging buffer: bo dem tam thoi tren host
  vk::DeviceSize bufferSize = sizeof(vertices_[0]) * vertices_.size();
  vk::raii::Buffer stagingBuffer({}); // local staging
  vk::raii::DeviceMemory stagingBufferMemory({});

  createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               stagingBuffer, stagingBufferMemory);

  void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  std::memcpy(dataStaging, vertices_.data(), static_cast<size_t>(bufferSize));
  stagingBufferMemory.unmapMemory();

  // vertex buffer: bo dem local cua device (gpu)
  createBuffer(bufferSize,
               vk::BufferUsageFlagBits::eVertexBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer_,
               vertexBufferMemory_);

  copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);
}

void VulkanRenderer::createIndexBuffer() {
  vk::DeviceSize bufferSize = sizeof(indices_[0]) * indices_.size();

  vk::raii::Buffer stagingBuffer({});             // local staging
  vk::raii::DeviceMemory stagingBufferMemory({}); // local memory
  createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               stagingBuffer, stagingBufferMemory);

  void *data = stagingBufferMemory.mapMemory(0, bufferSize);
  std::memcpy(data, indices_.data(), static_cast<size_t>(bufferSize));
  stagingBufferMemory.unmapMemory();

  createBuffer(bufferSize,
               vk::BufferUsageFlagBits::eTransferDst |
                   vk::BufferUsageFlagBits::eIndexBuffer,
               vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer_,
               indexBufferMemory_);

  copyBuffer(stagingBuffer, indexBuffer_, bufferSize);
}

/// Tạo uniform buffers với persistent mapping cho toàn bộ vòng đời ứng dụng
void VulkanRenderer::createUniformBuffers() {
  // persistent mapping use for all instance through app life time , not remap
  // effect perf
  uniformBuffers_.clear();
  uniformBuffersMemory_.clear();
  uniformBuffersMapped_.clear();

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
    vk::raii::Buffer buffer({});          // local
    vk::raii::DeviceMemory bufferMem({}); // local
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                 vk::MemoryPropertyFlagBits::eHostVisible |
                     vk::MemoryPropertyFlagBits::eHostCoherent,
                 buffer, bufferMem);
    uniformBuffers_.emplace_back(std::move(buffer));
    uniformBuffersMemory_.emplace_back(std::move(bufferMem));
    uniformBuffersMapped_.emplace_back(
        uniformBuffersMemory_[i].mapMemory(0, bufferSize));
  }
}

void VulkanRenderer::createDescriptorPool() {

  std::array poolSize{
      vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,
                             MAX_FRAMES_IN_FLIGHT),
      vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,
                             MAX_FRAMES_IN_FLIGHT)};
  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
      .pPoolSizes = poolSize.data()};
  descriptorPool_ = vk::raii::DescriptorPool(device_->getDevice(), poolInfo);
}

void VulkanRenderer::createDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                               *descriptorSetLayout_);
  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = *descriptorPool_,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data()};
  descriptorSets_.clear();
  descriptorSets_ = device_->getDevice().allocateDescriptorSets(allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo bufferInfo{.buffer = *uniformBuffers_[i],
                                        .offset = 0,
                                        .range = sizeof(UniformBufferObject)};
    vk::DescriptorImageInfo imageInfo{
        .sampler = *textureSampler_,
        .imageView = *textureImageView_,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
    std::array descriptorWrites{
        vk::WriteDescriptorSet{.dstSet = *descriptorSets_[i],
                               .dstBinding = 0,
                               .dstArrayElement = 0,
                               .descriptorCount = 1,
                               .descriptorType =
                                   vk::DescriptorType::eUniformBuffer,
                               .pBufferInfo = &bufferInfo},
        vk::WriteDescriptorSet{.dstSet = *descriptorSets_[i],
                               .dstBinding = 1,
                               .dstArrayElement = 0,
                               .descriptorCount = 1,
                               .descriptorType =
                                   vk::DescriptorType::eCombinedImageSampler,
                               .pImageInfo = &imageInfo}};
    device_->getDevice().updateDescriptorSets(descriptorWrites, {});
  }
}

/// Tải OBJ model, loại đỉnh trùng lặp bằng unordered_map
void VulkanRenderer::loadModel() {
  tinyobj::attrib_t attrib; // contain position, normal, texture coordinate
  std::vector<tinyobj::shape_t> shapes; // contain faces
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  std::unordered_map<Vertex, uint32_t>
      uniqueVertices{}; // dung unordered_map de loai dinh trung lap -> truyen
                        // zo index

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        MODEL_PATH.c_str())) {
    throw std::runtime_error(warn + err);
  }

  for (const auto &shape : shapes) {
    for (const auto &index : shape.mesh.indices) {
      Vertex vertex{};

      // arr 1d, (3i + 0, 3i + 1, 3i + 2) -> (x, y, z)
      vertex.pos = {attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};

      // arr 1d, (2i + 0, 2i + 1) -> (u, v)
      vertex.texCoord = {
          attrib.texcoords[2 * index.texcoord_index + 0],
          1.0f - attrib.texcoords[2 * index.texcoord_index +
                                  1] // OBJ: toa do y 0 la day hinh anh nhung
                                     // hien tai: tai anh len vk thi y 0 la
                                     // dinh nen phai dao nguoc
      };

      vertex.color = {1.0f, 1.0f, 1.0f};

      if (uniqueVertices.count(vertex) == 0) {
        uniqueVertices[vertex] = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back(vertex);
      }

      indices_.push_back(uniqueVertices[vertex]);
    }
  }

  std::cout << "Vertices: " << vertices_.size() << std::endl;
  std::cout << "Indices: " << indices_.size() << std::endl;
}

/// Cập nhật uniform buffer: model/view/projection matrix mỗi frame
void VulkanRenderer::updateUniformBuffer(
    uint32_t currentImage) { // (option)co the dung push constant truyen dl
                             // thuong xuyen thay doi
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();
  UniformBufferObject ubo{};
  // quay 90deg moi giay quanh truc z
  ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                     glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
  // view 45deg tu tren xuong
  ubo.proj =
      glm::perspective(glm::radians(45.0f),
                       static_cast<float>(swapchain_->getExtent().width) /
                           static_cast<float>(swapchain_->getExtent().height),
                       0.1f, 10.0f);
  ubo.proj[1][1] *= -1; // dao chieu y vk truc y duoi len tranh nguoc
  std::memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
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
  vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  const vk::SubmitInfo submitInfo{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*presentCompleteSemaphores_[frameIndex_],
      .pWaitDstStageMask = &waitDestinationStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffers_[frameIndex_],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*renderFinishedSemaphores_[imageIndex]};
  device_->getGraphicsQueue().submit(submitInfo, *inFlightFences_[frameIndex_]);

  try {
    const vk::PresentInfoKHR presentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*renderFinishedSemaphores_[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &*swapchain_->getSwapChain(),
        .pImageIndices = &imageIndex};
    result = device_->getGraphicsQueue().presentKHR(presentInfoKHR);
    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR || window_->wasResized()) {
      window_->clearResized();
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
  depthImageView_ = nullptr;
  depthImage_ = nullptr;
  depthImageMemory_ = nullptr;
  colorImageView_ = nullptr;
  colorImage_ = nullptr;
  colorImageMemory_ = nullptr;

  commandBuffers_.clear();
  renderFinishedSemaphores_.clear();
  presentCompleteSemaphores_.clear();
  inFlightFences_.clear();
}

void VulkanRenderer::createSwapChainDependentResources() {
  createDepthResources();
  createColorResources();
  createCommandBuffers();
  createSyncObjects();
}

/// Tạo lại swapchain khi window resize hoặc swapchain out-of-date
void VulkanRenderer::recreateSwapChain() {
  int width = 0, height = 0;
  window_->getFramebufferSize(width, height);
  while (width == 0 || height == 0) {
    window_->getFramebufferSize(width, height);
    glfwWaitEvents();
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

  descriptorSets_.clear();
  uniformBuffersMapped_.clear();
  uniformBuffersMemory_.clear();
  uniformBuffers_.clear();

  descriptorPool_ = nullptr;

  indexBuffer_ = nullptr;
  indexBufferMemory_ = nullptr;
  vertexBuffer_ = nullptr;
  vertexBufferMemory_ = nullptr;

  textureSampler_ = nullptr;
  textureImageView_ = nullptr;
  textureImage_ = nullptr;
  textureImageMemory_ = nullptr;

  graphicsPipeline_ = nullptr;
  pipelineLayout_ = nullptr;
  descriptorSetLayout_ = nullptr;
  commandPool_ = nullptr;
}