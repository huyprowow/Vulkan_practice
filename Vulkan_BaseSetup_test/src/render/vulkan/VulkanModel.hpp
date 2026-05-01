#pragma once

#include "../../core/Types.hpp"
#include "VulkanDevice.hpp"
#include "VulkanMemory.hpp"

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#if defined(__ANDROID__)
struct AAssetManager;
#endif

/// Quản lý mesh data: load glTF + vertex/index buffer trên GPU
/// Tách từ VulkanRenderer "God Class" theo plan REFACTOR #1
class VulkanModel {
public:
  /// Khởi tạo: load glTF + tạo vertex/index buffer trên device-local memory
  /// @param device VulkanDevice đã init
  /// @param memory VulkanMemory để cấp/copy buffer
  /// @param modelPath đường dẫn .glb (Types.hpp::MODEL_PATH)
  void load(VulkanDevice &device, VulkanMemory &memory,
            const std::string &modelPath
#if defined(__ANDROID__)
            ,
            AAssetManager *assetManager
#endif
  );

  /// Bind vertex + index buffer (gọi 1 lần trước khi draw nhiều objects)
  void bind(vk::raii::CommandBuffer &cmdBuf) const;

  /// Draw indexed (gọi cho mỗi object/instance — sau khi bind descriptor)
  void drawIndexed(vk::raii::CommandBuffer &cmdBuf,
                   uint32_t instanceCount = 1) const;

  uint32_t getIndexCount() const {
    return static_cast<uint32_t>(indices_.size());
  }
  uint32_t getVertexCount() const {
    return static_cast<uint32_t>(vertices_.size());
  }

  void cleanup();

private:
  VulkanDevice *device_ = nullptr;
  VulkanMemory *memory_ = nullptr;

  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;
  vk::raii::Buffer vertexBuffer_{nullptr};
  vk::raii::DeviceMemory vertexBufferMemory_{nullptr};
  vk::raii::Buffer indexBuffer_{nullptr};
  vk::raii::DeviceMemory indexBufferMemory_{nullptr};

  void loadModel(const std::string &path
#if defined(__ANDROID__)
                 ,
                 AAssetManager *assetManager
#endif
  );
  void createVertexBuffer();
  void createIndexBuffer();
};