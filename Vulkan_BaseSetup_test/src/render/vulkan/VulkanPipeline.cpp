#include "VulkanPipeline.hpp"

vk::raii::ShaderModule createShaderModule(const vk::raii::Device &device,
                                          const std::vector<char> &spv) {
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = spv.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t *>(spv.data())};
  return vk::raii::ShaderModule{device, createInfo};
}

vk::raii::Pipeline createComputePipeline(const vk::raii::Device &device,
                                         const vk::raii::ShaderModule &module,
                                         const char *entryName,
                                         const vk::raii::PipelineLayout &layout) {
  vk::PipelineShaderStageCreateInfo stageInfo{
      .stage = vk::ShaderStageFlagBits::eCompute,
      .module = *module,
      .pName = entryName};
  vk::ComputePipelineCreateInfo pipelineInfo{.stage = stageInfo,
                                             .layout = *layout};
  return vk::raii::Pipeline(device, nullptr, pipelineInfo);
}