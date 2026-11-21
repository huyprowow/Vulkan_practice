#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

/// Tạo shader module từ bytes SPIR-V (đọc bằng readFile).
vk::raii::ShaderModule createShaderModule(const vk::raii::Device &device,
                                          const std::vector<char> &spv);

/// Tạo compute pipeline từ 1 entry trong file .spv. Một file .spv chứa được
/// nhiều entry compute; mỗi entry là một pipeline riêng.
vk::raii::Pipeline createComputePipeline(const vk::raii::Device &device,
                                         const vk::raii::ShaderModule &module,
                                         const char *entryName,
                                         const vk::raii::PipelineLayout &layout);

/// Số work group cần để phủ `size` phần tử khi mỗi group có `localSize` thread.
inline uint32_t groupCount(uint32_t size, uint32_t localSize) {
  return (size + localSize - 1) / localSize;
}