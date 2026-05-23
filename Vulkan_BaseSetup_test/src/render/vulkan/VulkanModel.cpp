#include "VulkanModel.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

void VulkanModel::load(VulkanDevice &device, VulkanMemory &memory,
                       const std::string &modelPath
#if defined(__ANDROID__)
                       ,
                       AAssetManager *assetManager
#endif
) {
  uploadFromMeshData(device, memory,
                     mesh::MeshData::loadFromGltf(modelPath
#if defined(__ANDROID__)
                                                  ,
                                                  assetManager
#endif
                                                  ));
}

// API trung gian — caller có thể tự tạo MeshData
void VulkanModel::uploadFromMeshData(VulkanDevice &device, VulkanMemory &memory,
                                     mesh::MeshData &&data) {
  device_ = &device;
  memory_ = &memory;
  data_ = std::move(data);
  createVertexBuffer(); // tao vertex buffer luu cac vertex data
  createIndexBuffer();  // tao index buffer luu cac index data (tranh ve trung
                        // dinh)
  createUVBuffer(); //  UV buffer riêng cho RT shader
}

void VulkanModel::bind(vk::raii::CommandBuffer &cmdBuf) const {
  cmdBuf.bindVertexBuffers(0, *vertexBuffer_, {0});
  cmdBuf.bindIndexBuffer(*indexBuffer_, 0, vk::IndexType::eUint32);
}

void VulkanModel::drawIndexed(vk::raii::CommandBuffer &cmdBuf,
                              uint32_t instanceCount) const {
  cmdBuf.drawIndexed(static_cast<uint32_t>(data_.indices.size()), instanceCount, 0,
                     0, 0);
}

void VulkanModel::cleanup() {
  uvBuffer_ = nullptr;
  uvBufferMemory_ = nullptr;
  indexBuffer_ = nullptr;
  indexBufferMemory_ = nullptr;
  vertexBuffer_ = nullptr; 
  vertexBufferMemory_ = nullptr;
  data_ = {};
}

/// Tạo vertex buffer: staging buffer trên host, copy sang device-local memory
void VulkanModel::createVertexBuffer() {
  // vi cpu k the truy cap truc tiep vung nho toi uu nhat trong gpu
  // (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) => dung bo dem tam thoi tren host
  // (cpu). sau do khi hoat dong copy dl tu host sang bo nho local cua device
  // (gpu)

  // staging buffer: bo dem tam thoi tren host
  vk::DeviceSize bufferSize = sizeof(data_.vertices[0]) * data_.vertices.size();
  vk::raii::Buffer stagingBuffer({}); // local staging
  vk::raii::DeviceMemory stagingBufferMemory({});

  memory_->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent,
                        stagingBuffer, stagingBufferMemory);

  void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  std::memcpy(dataStaging, data_.vertices.data(), static_cast<size_t>(bufferSize));
  stagingBufferMemory.unmapMemory();

  // vertex buffer: bo dem local cua device (gpu)
  memory_->createBuffer(
      bufferSize,
      vk::BufferUsageFlagBits::eVertexBuffer |
          vk::BufferUsageFlagBits::eTransferDst|
      // Cần eShaderDeviceAddress để getBufferAddressKHR sau này
      // Cần eAccelerationStructureBuildInputReadOnlyKHR vì BLAS đọc
      //   vertex/index buffer làm input. Bật flag ở không gây side
      //   effect vì feature chỉ activate khi gọi build AS.
      vk::BufferUsageFlagBits::eShaderDeviceAddress |
          vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
      vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer_,
      vertexBufferMemory_);

  memory_->copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);
}

void VulkanModel::createIndexBuffer() {
  vk::DeviceSize bufferSize = sizeof(data_.indices[0]) * data_.indices.size();

  vk::raii::Buffer stagingBuffer({});             // local staging
  vk::raii::DeviceMemory stagingBufferMemory({}); // local memory
  memory_->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent,
                        stagingBuffer, stagingBufferMemory);

  void *data = stagingBufferMemory.mapMemory(0, bufferSize);
  std::memcpy(data, data_.indices.data(), static_cast<size_t>(bufferSize));
  stagingBufferMemory.unmapMemory();

  memory_->createBuffer(
      bufferSize,
      vk::BufferUsageFlagBits::eTransferDst |
          vk::BufferUsageFlagBits::eIndexBuffer |
          // eShaderDeviceAddress, eAccelerationStructureBuildInputReadOnlyKHR:
          //   giống lý do ở vertex buffer.
          // eStorageBuffer: RT shader bind index buffer làm SSBO để
          //   lookup primitive triangle ở hit point.
          vk::BufferUsageFlagBits::eShaderDeviceAddress |
          vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
          vk::BufferUsageFlagBits::eStorageBuffer,
      vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer_,
      indexBufferMemory_);

  memory_->copyBuffer(stagingBuffer, indexBuffer_, bufferSize);
}

//  UV buffer riêng —RT shader sample texture qua đây
void VulkanModel::createUVBuffer() {
  // Trích texCoord từ data_.vertices ra một buffer std::vector<vec2> riêng,
  // rồi upload thành StorageBuffer. Shader RT sẽ:
  //   StructuredBuffer<float2> uvBuffer;
  // và lookup uv tại vertex index lấy từ index buffer.
  std::vector<glm::vec2> uvs;
  uvs.reserve(data_.vertices.size());
  for (auto &v : data_.vertices) {
    uvs.push_back(v.texCoord);
  }
  vk::DeviceSize bufferSize = sizeof(glm::vec2) * uvs.size();
  if (bufferSize == 0) {
    return; // model rỗng — không sao
  }
  vk::raii::Buffer stagingBuffer({});
  vk::raii::DeviceMemory stagingBufferMemory({});
  memory_->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent,
                        stagingBuffer, stagingBufferMemory);
  void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  std::memcpy(dataStaging, uvs.data(), static_cast<size_t>(bufferSize));
  stagingBufferMemory.unmapMemory();
  memory_->createBuffer(bufferSize,
                        vk::BufferUsageFlagBits::eStorageBuffer |
                            vk::BufferUsageFlagBits::eTransferDst,
                        vk::MemoryPropertyFlagBits::eDeviceLocal, uvBuffer_,
                        uvBufferMemory_);
  memory_->copyBuffer(stagingBuffer, uvBuffer_, bufferSize);
}