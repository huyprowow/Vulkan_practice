#include "RasterBasicSample.hpp"
#include "../../core/FileUtil.hpp"
#include "../../core/Types.hpp"
#include "../../render/vulkan/VulkanBarrier.hpp"
#include "../../render/vulkan/VulkanPipeline.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <thread>

#if defined(SANDBOX_IMGUI)
#include <imgui.h>
#endif

/// Khởi tạo sample: pipeline, texture, scene, uniform, descriptor, particle
void RasterBasicSample::init(const SampleContext &ctx) {
  device_ = &ctx.device;
  memory_ = &ctx.memory;
  swapchain_ = &ctx.swapchain;
  queueSubmitMutex_ = &ctx.queueSubmitMutex;
#if defined(__ANDROID__)
  assetManager_ = ctx.assetManager;
#endif

  createDescriptorSetLayout(); // layout descriptor de bind cac uniform vao
  // pipeline
  createGraphicsPipeline(); // tao pipeline de render
  texture_.init(*device_, *memory_, *swapchain_, TEXTURE_PATH
#if defined(__ANDROID__)
                ,
                assetManager_
#endif
  );

  scene_.init(*device_, *memory_, MODEL_PATH
#if defined(__ANDROID__)
              ,
              assetManager_
#endif
  );

  createUniformBuffers(); // uniform cho mỗi gameObject
  createDescriptorPool(); // tao descriptor pool luu cac descriptor set (1:n -
  // pool:set)
  createDescriptorSets(); // 1:1 - descriptor set: buffer resoure

  // init particle system
  auto computeSpv = readFile("shaders/compute.spv"
#if defined(__ANDROID__)
                             ,
                             assetManager_
#endif
  );

  particleSystem_.init(*device_, *memory_, ctx.commandPool,
                       swapchain_->getImageFormat(), texture_.getDepthFormat(),
                       device_->msaaSamples_, computeSpv);

  // khoi tao worker threads cho particleSystem.
  // PARTICLE_COUNT=8192 chia het cho 2/4/8 → chon 4 thread (2048
  // particle/thread). hardware_concurrency()/2 de khong chiem het core
  // (con dung graphics + main).
  uint32_t threadCount =
      std::min<uint32_t>(MAX_COMPUTE_THREADS,
                         std::max(1u, std::thread::hardware_concurrency() / 2));
  particleSystem_.initThreads(threadCount, ctx.queueSubmitMutex);
}

/// Phần CPU của frame: animation scene, lưu dt cho particle.
void RasterBasicSample::update(float dtSeconds) {
  dtSeconds_ = dtSeconds * timeScale_;
  scene_.update(dtSeconds_);
}


void RasterBasicSample::ui() {
#if defined(SANDBOX_IMGUI)
  ImGui::SliderFloat("Time scale", &timeScale_, 0.0f, 3.0f);
#endif
}

/// Particle chạy compute đa luồng ở submit riêng; graphics chờ semaphore trả về
/// trước khi đọc particle ở vertex input.
SampleWait RasterBasicSample::submitBeforeGraphics(uint32_t frameIndex) {
  auto computeFenceResult = device_->getDevice().waitForFences(
      *particleSystem_.computeInFlightFence(frameIndex), vk::True, UINT64_MAX);

  if (computeFenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for compute fence!");
  }

  particleSystem_.updateComputeUniformBuffer(frameIndex, dtSeconds_);
  device_->getDevice().resetFences(
      *particleSystem_.computeInFlightFence(frameIndex));
  // dispatch multithreaded thay cho recordComputeCommandBuffer don gian.
  // worker threads record song song theo range particle. tra ve N cmd buffers
  // (N = threadCount_) de submit cung 1 lan.
  auto computeCmdBuffers = particleSystem_.dispatchMultithreaded(frameIndex);
  vk::SubmitInfo computeSubmitInfo{
      .commandBufferCount = static_cast<uint32_t>(computeCmdBuffers.size()),
      .pCommandBuffers = computeCmdBuffers.data(),
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
          &*particleSystem_.computeFinishedSemaphore(frameIndex)};
  // mutex bao ve queue.submit() (Vulkan spec: vk::Queue khong thread-safe)
  {
    std::lock_guard<std::mutex> lock(*queueSubmitMutex_);
    device_->getGraphicsQueue().submit(
        computeSubmitInfo, *particleSystem_.computeInFlightFence(frameIndex));
  }

  return {.semaphore = *particleSystem_.computeFinishedSemaphore(frameIndex),
          .stage = vk::PipelineStageFlagBits::eVertexInput};
}

/// Ghi lệnh vẽ: transition MSAA color + depth, begin rendering, vẽ các object
/// rồi particle, resolve MSAA vào swapchain image
void RasterBasicSample::record(const SampleFrame &frame) {
  updateUniformBuffer(frame.frameIndex);

  auto &commandBuffer = frame.cmd;
  // transition for the MSAA color image
  cmdTransitionImageLayout(commandBuffer,*texture_.getColorImage(),
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal, {},
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::ImageAspectFlagBits::eColor, 1);
  // transition for the depth image
  cmdTransitionImageLayout(commandBuffer,*texture_.getDepthImage(),
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthAttachmentOptimal,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::ImageAspectFlagBits::eDepth, 1);
  vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
  vk::ClearValue clearDepth = vk::ClearDepthStencilValue{1.0f, 0};
  vk::RenderingAttachmentInfo attachmentInfo = {
      .imageView = texture_.getColorView(),
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .resolveMode = vk::ResolveModeFlagBits::eAverage,
      .resolveImageView = frame.swapchainView,
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
      .renderArea = {.offset = {0, 0}, .extent = frame.extent},
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
      0, vk::Viewport{0.0f, 0.0f,
              static_cast<float>(frame.extent.width),
              static_cast<float>(frame.extent.height), 0.0f, 1.0f});
  commandBuffer.setScissor(
      0, vk::Rect2D{vk::Offset2D{0, 0}, frame.extent});
  // Bind vertex/index buffer 1 lần (chia sẻ giữa các objects)
  scene_.model().bind(commandBuffer);
  // Loop draw từng object
  for (auto &obj : scene_.gameObjects()) {
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     pipelineLayout_, 0,
                                     *obj.descriptorSets[frame.frameIndex], nullptr);
    commandBuffer.drawIndexed(scene_.model().getIndexCount(), 1, 0, 0, 0);
  }
  // VẼ PARTICLE SYSTEM
  particleSystem_.recordDraw(commandBuffer, frame.frameIndex);
  commandBuffer.endRendering();
}

/// Ảnh MSAA color + depth theo kích thước swapchain nên phải tạo lại
void RasterBasicSample::onSwapchainRecreated() {
  texture_.resetSwapChainResources();
  texture_.recreateSwapChainResources();
}

void RasterBasicSample::shutdown() {
  particleSystem_.cleanup();
  texture_.cleanup();
  scene_.cleanup();

  descriptorPool_ = nullptr;

  graphicsPipeline_ = nullptr;
  pipelineLayout_ = nullptr;
  descriptorSetLayout_ = nullptr;
}

/// Cập nhật uniform buffer: model/view/projection matrix mỗi frame
void RasterBasicSample::updateUniformBuffer(
    uint32_t currentImage) { // (option)co the dung push constant truyen dl
                             // thuong xuyen thay doi
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
  for (auto &gameObject : scene_.gameObjects()) {
    // Get the model matrix for this object
    glm::mat4 model = gameObject.getModelMatrix();

    // Create and update the UBO
    UniformBufferObject ubo{.model = model, .view = view, .proj = proj};

    // Copy the UBO data to the mapped memory
    memcpy(gameObject.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }
}


/// Tạo uniform buffers với persistent mapping cho toàn bộ vòng đời ứng dụng
void RasterBasicSample::createUniformBuffers() {
  // persistent mapping use for all instance through app life time , not remap
  // effect perf
  // For each game object
  for (auto &gameObject : scene_.gameObjects()) {
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

void RasterBasicSample::createDescriptorPool() {
  // nhan 2 uniform buffer cho compute shader va 1 uniform buffer cho graphics
  // shader
std::array poolSize{
      vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer,
                              MAX_FRAMES_IN_FLIGHT * MAX_OBJECTS},
      vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler,
                              MAX_FRAMES_IN_FLIGHT * MAX_OBJECTS},
      vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer,
                              MAX_FRAMES_IN_FLIGHT *
                                  2}}; // compute SSBO ping-pong
  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = MAX_FRAMES_IN_FLIGHT * MAX_OBJECTS,
      .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
      .pPoolSizes = poolSize.data()};
  descriptorPool_ = vk::raii::DescriptorPool(device_->getDevice(), poolInfo);
}

void RasterBasicSample::createDescriptorSets() {
  // For each game object
  for (auto &gameObject : scene_.gameObjects()) {
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

void RasterBasicSample::createDescriptorSetLayout() {
std::array bindings = {
      vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1,
                                      vk::ShaderStageFlagBits::eVertex, nullptr},
      vk::DescriptorSetLayoutBinding{
          1, vk::DescriptorType::eCombinedImageSampler, 1,
          vk::ShaderStageFlagBits::eFragment, nullptr}};

  vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = bindings.size(),
                                               .pBindings = bindings.data()};
  descriptorSetLayout_ =
      vk::raii::DescriptorSetLayout(device_->getDevice(), layoutInfo);
}

/// Tạo graphics pipeline: shader stages, fixed-function state, dynamic
/// rendering
void RasterBasicSample::createGraphicsPipeline() {
  // shader stage: shader code
  vk::raii::ShaderModule shaderModule =
      createShaderModule(device_->getDevice(), readFile("shaders/shader.spv"
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