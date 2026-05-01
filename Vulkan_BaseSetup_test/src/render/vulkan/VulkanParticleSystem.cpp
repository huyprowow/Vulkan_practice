#include "VulkanParticleSystem.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

void VulkanParticleSystem::init(VulkanDevice &device, VulkanMemory &memory,
                                vk::raii::CommandPool &commandPool,
                                vk::Format colorFormat, vk::Format depthFormat,
                                vk::SampleCountFlagBits msaaSamples,
                                const std::vector<char> &computeShaderSpv) {
  device_ = &device;
  memory_ = &memory;
  commandPool_ = &commandPool;

  createComputeDescriptorSetLayout(); // tao descriptor set layout cho compute
                                      // shader
  createComputePipeline(computeShaderSpv); // tao pipeline cho compute shader
  createParticleGraphicsPipeline(computeShaderSpv, colorFormat, depthFormat,
                                 msaaSamples); // tao pipeline ve particle
  createComputeUniformBuffers();               // tao UBO cho compute shader
  createShaderStorageBuffers();                // tao SSBO cho particle
  createComputeDescriptorPool();
  createComputeDescriptorSets(); // tao descriptor set cho compute shader
  createComputeCommandBuffers(); // tao command buffer cho compute shader
  createComputeSyncObjects();
}

vk::raii::ShaderModule
VulkanParticleSystem::createShaderModule(const std::vector<char> &code) const {
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t *>(code.data())};
  vk::raii::ShaderModule shaderModule{device_->getDevice(), createInfo};
  return shaderModule;
}

void VulkanParticleSystem::recordComputeCommandBuffer(uint32_t frameIndex) {
  /// ghi lenh compute moi fframe , dispatch (PARTICLE_COUNT /
  /// INVOCATIONS_SIZE) work groups x size 1 work group y, 1 work group z
  auto &cmd = computeCommandBuffers_[frameIndex];
  uint32_t work_group_countX = PARTICLE_COUNT / INVOCATIONS_SIZE;
  cmd.begin({});
  cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline_);
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                         computePipelineLayout_, 0,
                         *computeDescriptorSets_[frameIndex], nullptr);
  cmd.dispatch(work_group_countX, 1,
               1); // 32 work group x, 1 work
  // group y, 1 work group z
  cmd.end();
  static std::once_flag flag;
  std::call_once(flag, [&] {
    std::cout << "dispatch (X,Y,Z): (" << work_group_countX << ",1,1)"
              << std::endl;
  });
}

void VulkanParticleSystem::updateComputeUniformBuffer(uint32_t currentFrame) {
  /// cap nhap UBO cho compute shader moi khung dua tren tg delta time
  static auto startTime = std::chrono::high_resolution_clock::now();
  auto currentTime = std::chrono::high_resolution_clock::now();
  float time =
      std::chrono::duration<float, std::milli>(currentTime - startTime).count();

  ComputeUBO ubo{};
  ubo.deltaTime = (time - lastFrameTime_) * 2.0f;
  lastFrameTime_ = time;

  std::memcpy(computeUniformBuffersMapped_[currentFrame], &ubo, sizeof(ubo));
}

void VulkanParticleSystem::recordDraw(vk::raii::CommandBuffer &cmdBuf,
                                      uint32_t frameIndex) {
  // VẼ PARTICLES (2d nen tat depth test, ve sau luon de len model)
  cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,
                             *particlePipeline_);
  cmdBuf.bindVertexBuffers(0, *shaderStorageBuffers_[frameIndex], {0});
  cmdBuf.draw(PARTICLE_COUNT, 1, 0, 0);
}

// 🔴 NEW — accessors cho Renderer truy cập từ ngoài
const vk::raii::CommandBuffer &
VulkanParticleSystem::computeCommandBuffer(uint32_t frameIndex) const {
  return computeCommandBuffers_[frameIndex];
}

const vk::raii::Semaphore &
VulkanParticleSystem::computeFinishedSemaphore(uint32_t frameIndex) const {
  return computeFinishedSemaphores_[frameIndex];
}

const vk::raii::Fence &
VulkanParticleSystem::computeInFlightFence(uint32_t frameIndex) const {
  return computeInFlightFences_[frameIndex];
}

void VulkanParticleSystem::resetSwapChainResources() {
  computeCommandBuffers_.clear();
  computeFinishedSemaphores_.clear();
  computeInFlightFences_.clear();
}

void VulkanParticleSystem::cleanup() {
  // sync + pool 
  computeFinishedSemaphores_.clear();
  computeInFlightFences_.clear();
  computeCommandBuffers_.clear();
  computeDescriptorSets_.clear();
  descriptorPool_ = nullptr;

  // compute
  computeUniformBuffersMapped_.clear();
  computeUniformBuffersMemory_.clear();
  computeUniformBuffers_.clear();
  shaderStorageBuffers_.clear();
  shaderStorageBuffersMemory_.clear();
  computePipeline_ = nullptr;
  computePipelineLayout_ = nullptr;
  computeDescriptorSetLayout_ = nullptr;
  particlePipeline_ = nullptr;
  particlePipelineLayout_ = nullptr;
}

// compute shader

/// tao descriptor set layout cho compute shader
void VulkanParticleSystem::createComputeDescriptorSetLayout() {
  // co 2 layout binding cho SSBO do vi tri hat cap nhap tung khung dua tren tg
  // chenh lech  nen can biet vi tri cua khung truoc de cap nhap
  std::array bindings = {
      vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute},
      vk::DescriptorSetLayoutBinding{1, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute},
      vk::DescriptorSetLayoutBinding{2, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute}};
  vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = bindings.size(),
                                               .pBindings = bindings.data()};
  computeDescriptorSetLayout_ =
      vk::raii::DescriptorSetLayout(device_->getDevice(), layoutInfo);
}

/// tao pipeline cho compute shader
void VulkanParticleSystem::createComputePipeline(const std::vector<char> &spv) {
  vk::raii::ShaderModule shaderModule = createShaderModule(spv);
  vk::PipelineShaderStageCreateInfo stageInfo{
      .stage = vk::ShaderStageFlagBits::eCompute,
      .module = shaderModule,
      .pName = "compMain"};
  vk::PipelineLayoutCreateInfo layoutInfo{
      .setLayoutCount = 1, .pSetLayouts = &*computeDescriptorSetLayout_};
  computePipelineLayout_ =
      vk::raii::PipelineLayout(device_->getDevice(), layoutInfo);
  vk::ComputePipelineCreateInfo pipelineInfo{.stage = stageInfo,
                                             .layout = computePipelineLayout_};
  computePipeline_ =
      vk::raii::Pipeline(device_->getDevice(), nullptr, pipelineInfo);
}

/// pipeline ve particle
void VulkanParticleSystem::createParticleGraphicsPipeline(
    const std::vector<char> &spv, vk::Format colorFormat,
    vk::Format depthFormat, vk::SampleCountFlagBits msaaSamples) {
  vk::raii::ShaderModule shaderModule = createShaderModule(spv);
  vk::PipelineShaderStageCreateInfo vertStage{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = shaderModule,
      .pName = "vertMain"};
  vk::PipelineShaderStageCreateInfo fragStage{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = shaderModule,
      .pName = "fragMain"};
  vk::PipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

  auto bindingDesc = Particle::getBindingDescription();
  auto attrDescs = Particle::getAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertexInput{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDesc,
      .vertexAttributeDescriptionCount = attrDescs.size(),
      .pVertexAttributeDescriptions = attrDescs.data()};
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::ePointList};
  vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                    .scissorCount = 1};
  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eNone,
      .lineWidth = 1.0f};
  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = device_->msaaSamples_,
      .sampleShadingEnable = vk::True,
      .minSampleShading = 0.2f};
  vk::PipelineDepthStencilStateCreateInfo depthStencil{.depthTestEnable =
                                                           vk::False};
  vk::PipelineColorBlendAttachmentState colorBlendAttach{
      .blendEnable = vk::True,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .attachmentCount = 1, .pAttachments = &colorBlendAttach};
  std::vector dynamicStates = {vk::DynamicState::eViewport,
                               vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()};

  vk::PipelineLayoutCreateInfo layoutInfo{};
  particlePipelineLayout_ =
      vk::raii::PipelineLayout(device_->getDevice(), layoutInfo);

  vk::PipelineRenderingCreateInfo renderingInfo{
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &colorFormat,
      .depthAttachmentFormat = depthFormat};
  vk::GraphicsPipelineCreateInfo pipelineInfo{
      .pNext = &renderingInfo,
      .stageCount = 2,
      .pStages = stages,
      .pVertexInputState = &vertexInput,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depthStencil,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = particlePipelineLayout_,
      .renderPass = nullptr};
  vk::StructureChain chain = {pipelineInfo, renderingInfo};
  particlePipeline_ =
      vk::raii::Pipeline(device_->getDevice(), nullptr,
                         chain.get<vk::GraphicsPipelineCreateInfo>());
}

/// SSBO
void VulkanParticleSystem::createShaderStorageBuffers() {
  /// tao particle hinh tron ban kinh r,velocity ra ngoai copy len gpu qua SSBO
  shaderStorageBuffers_.clear();
  shaderStorageBuffersMemory_.clear();

  std::default_random_engine rndEngine(static_cast<unsigned>(time(nullptr)));
  std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

  std::vector<Particle> particles(PARTICLE_COUNT);
  for (auto &particle : particles) {
    float r = 0.25f * sqrtf(rndDist(rndEngine));
    float theta = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;
    float x = r * cosf(theta) * HEIGHT / WIDTH;
    float y = r * sinf(theta);
    particle.position = glm::vec2(x, y);
    particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
    particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine),
                               rndDist(rndEngine), 1.0f);
  }

  vk::DeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

  vk::raii::Buffer stagingBuffer({});
  vk::raii::DeviceMemory stagingBufferMemory({});
  memory_->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent,
                        stagingBuffer, stagingBufferMemory);
  void *data = stagingBufferMemory.mapMemory(0, bufferSize);
  std::memcpy(data, particles.data(), bufferSize);
  stagingBufferMemory.unmapMemory();

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::raii::Buffer buffer({});
    vk::raii::DeviceMemory bufferMem({});
    memory_->createBuffer(bufferSize,
                          vk::BufferUsageFlagBits::eStorageBuffer |
                              vk::BufferUsageFlagBits::eVertexBuffer |
                              vk::BufferUsageFlagBits::eTransferDst,
                          vk::MemoryPropertyFlagBits::eDeviceLocal, buffer,
                          bufferMem);
    memory_->copyBuffer(stagingBuffer, buffer, bufferSize);
    shaderStorageBuffers_.emplace_back(std::move(buffer));
    shaderStorageBuffersMemory_.emplace_back(std::move(bufferMem));
  }
}

/// UBO cho compute shader
void VulkanParticleSystem::createComputeUniformBuffers() {
  computeUniformBuffers_.clear();
  computeUniformBuffersMemory_.clear();
  computeUniformBuffersMapped_.clear();

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DeviceSize bufferSize = sizeof(ComputeUBO);
    vk::raii::Buffer buffer({});
    vk::raii::DeviceMemory bufferMem({});
    memory_->createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                          vk::MemoryPropertyFlagBits::eHostVisible |
                              vk::MemoryPropertyFlagBits::eHostCoherent,
                          buffer, bufferMem);
    computeUniformBuffers_.emplace_back(std::move(buffer));
    computeUniformBuffersMemory_.emplace_back(std::move(bufferMem));
    computeUniformBuffersMapped_.emplace_back(
        computeUniformBuffersMemory_[i].mapMemory(0, bufferSize));
  }
}

// Pool RIÊNG cho compute (không share với Renderer)
void VulkanParticleSystem::createComputeDescriptorPool() {
  std::array poolSize{vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,
                                             MAX_FRAMES_IN_FLIGHT),
                      vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer,
                                             MAX_FRAMES_IN_FLIGHT * 2)};
  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
      .pPoolSizes = poolSize.data()};
  descriptorPool_ = vk::raii::DescriptorPool(device_->getDevice(), poolInfo);
}

void VulkanParticleSystem::createComputeDescriptorSets() {
  /// 2 descriptor set cho compute shader moi set bind 1 UBO va 2 SSBO, 2 ssbo
  /// luan chuyen doc ghi tung khung
  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                               *computeDescriptorSetLayout_);
  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = *descriptorPool_,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data()};
  computeDescriptorSets_ =
      device_->getDevice().allocateDescriptorSets(allocInfo);

  vk::DeviceSize ssboSize = sizeof(Particle) * PARTICLE_COUNT;
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo uboInfo{.buffer = *computeUniformBuffers_[i],
                                     .offset = 0,
                                     .range = sizeof(ComputeUBO)};
    vk::DescriptorBufferInfo ssboInInfo{
        .buffer = *shaderStorageBuffers_[(i - 1 + MAX_FRAMES_IN_FLIGHT) %
                                         MAX_FRAMES_IN_FLIGHT],
        .offset = 0,
        .range = ssboSize};
    vk::DescriptorBufferInfo ssboOutInfo{
        .buffer = *shaderStorageBuffers_[i], .offset = 0, .range = ssboSize};
    std::array writes = {
        vk::WriteDescriptorSet{.dstSet = *computeDescriptorSets_[i],
                               .dstBinding = 0,
                               .descriptorCount = 1,
                               .descriptorType =
                                   vk::DescriptorType::eUniformBuffer,
                               .pBufferInfo = &uboInfo},
        vk::WriteDescriptorSet{.dstSet = *computeDescriptorSets_[i],
                               .dstBinding = 1,
                               .descriptorCount = 1,
                               .descriptorType =
                                   vk::DescriptorType::eStorageBuffer,
                               .pBufferInfo = &ssboInInfo},
        vk::WriteDescriptorSet{.dstSet = *computeDescriptorSets_[i],
                               .dstBinding = 2,
                               .descriptorCount = 1,
                               .descriptorType =
                                   vk::DescriptorType::eStorageBuffer,
                               .pBufferInfo = &ssboOutInfo}};
    device_->getDevice().updateDescriptorSets(writes, {});
  }
}

void VulkanParticleSystem::createComputeCommandBuffers() {
  computeCommandBuffers_.clear();
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = *commandPool_,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
  computeCommandBuffers_ =
      vk::raii::CommandBuffers(device_->getDevice(), allocInfo);
}
void VulkanParticleSystem::createComputeSyncObjects() {
  // sync objects cho compute shader
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    computeFinishedSemaphores_.emplace_back(device_->getDevice(),
                                            vk::SemaphoreCreateInfo());
    computeInFlightFences_.emplace_back(
        device_->getDevice(),
        vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
  }
}
