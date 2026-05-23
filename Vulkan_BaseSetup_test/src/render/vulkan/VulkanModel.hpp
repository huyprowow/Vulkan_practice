#pragma once

#include "../../core/Types.hpp"

#include "MeshData.hpp"
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
  // NEW: API trung gian cho phép caller dùng MeshData tự build
  void uploadFromMeshData(VulkanDevice &device, VulkanMemory &memory,
                          mesh::MeshData &&data);

  /// Bind vertex + index buffer (gọi 1 lần trước khi draw nhiều objects)
  void bind(vk::raii::CommandBuffer &cmdBuf) const;

  /// Draw indexed (gọi cho mỗi object/instance — sau khi bind descriptor)
  void drawIndexed(vk::raii::CommandBuffer &cmdBuf,
                   uint32_t instanceCount = 1) const;

  const mesh::MeshData &data() const { return data_; }
  vk::Buffer vertexBuffer() const { return *vertexBuffer_; }
  vk::Buffer indexBuffer() const { return *indexBuffer_; }
  vk::Buffer uvBuffer() const { return *uvBuffer_; }
  uint32_t getIndexCount() const {
    return static_cast<uint32_t>(data_.indices.size());
  }
  uint32_t getVertexCount() const {
    return static_cast<uint32_t>(data_.vertices.size());
  }

  void cleanup();

private:
  VulkanDevice *device_ = nullptr;
  VulkanMemory *memory_ = nullptr;

  mesh::MeshData data_;
  vk::raii::Buffer vertexBuffer_{nullptr};
  vk::raii::DeviceMemory vertexBufferMemory_{nullptr};
  vk::raii::Buffer indexBuffer_{nullptr};
  vk::raii::DeviceMemory indexBufferMemory_{nullptr};
  vk::raii::Buffer uvBuffer_{nullptr};
  vk::raii::DeviceMemory uvBufferMemory_{nullptr};

  void createVertexBuffer();
  void createIndexBuffer();
  void createUVBuffer();
};