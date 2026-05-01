#include "VulkanModel.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

// #define TINYGLTF_IMPLEMENTATION cmake defined
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

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
  device_ = &device;
  memory_ = &memory;

  loadModel(modelPath
#if defined(__ANDROID__)
            ,
            assetManager
#endif
  );
  createVertexBuffer(); // tao vertex buffer luu cac vertex data
  createIndexBuffer();  // tao index buffer luu cac index data (tranh ve trung
  // dinh)
}

void VulkanModel::bind(vk::raii::CommandBuffer &cmdBuf) const {
  cmdBuf.bindVertexBuffers(0, *vertexBuffer_, {0});
  cmdBuf.bindIndexBuffer(*indexBuffer_, 0, vk::IndexType::eUint32);
}

void VulkanModel::drawIndexed(vk::raii::CommandBuffer &cmdBuf,
                              uint32_t instanceCount) const {
  cmdBuf.drawIndexed(static_cast<uint32_t>(indices_.size()), instanceCount, 0,
                     0, 0);
}

void VulkanModel::cleanup() {
  indexBuffer_ = nullptr;
  indexBufferMemory_ = nullptr;
  vertexBuffer_ = nullptr;
  vertexBufferMemory_ = nullptr;
  vertices_.clear();
  indices_.clear();
}

/// Tạo vertex buffer: staging buffer trên host, copy sang device-local memory
void VulkanModel::createVertexBuffer() {
  // vi cpu k the truy cap truc tiep vung nho toi uu nhat trong gpu
  // (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) => dung bo dem tam thoi tren host
  // (cpu). sau do khi hoat dong copy dl tu host sang bo nho local cua device
  // (gpu)

  // staging buffer: bo dem tam thoi tren host
  vk::DeviceSize bufferSize = sizeof(vertices_[0]) * vertices_.size();
  vk::raii::Buffer stagingBuffer({}); // local staging
  vk::raii::DeviceMemory stagingBufferMemory({});

  memory_->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent,
                        stagingBuffer, stagingBufferMemory);

  void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  std::memcpy(dataStaging, vertices_.data(), static_cast<size_t>(bufferSize));
  stagingBufferMemory.unmapMemory();

  // vertex buffer: bo dem local cua device (gpu)
  memory_->createBuffer(bufferSize,
                        vk::BufferUsageFlagBits::eVertexBuffer |
                            vk::BufferUsageFlagBits::eTransferDst,
                        vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer_,
                        vertexBufferMemory_);

  memory_->copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);
}

void VulkanModel::createIndexBuffer() {
  vk::DeviceSize bufferSize = sizeof(indices_[0]) * indices_.size();

  vk::raii::Buffer stagingBuffer({});             // local staging
  vk::raii::DeviceMemory stagingBufferMemory({}); // local memory
  memory_->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent,
                        stagingBuffer, stagingBufferMemory);

  void *data = stagingBufferMemory.mapMemory(0, bufferSize);
  std::memcpy(data, indices_.data(), static_cast<size_t>(bufferSize));
  stagingBufferMemory.unmapMemory();

  memory_->createBuffer(bufferSize,
                        vk::BufferUsageFlagBits::eTransferDst |
                            vk::BufferUsageFlagBits::eIndexBuffer,
                        vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer_,
                        indexBufferMemory_);

  memory_->copyBuffer(stagingBuffer, indexBuffer_, bufferSize);
}

/// Tải GLTF model, co the convert obj -> gltf bang obj2gltf
void VulkanModel::loadModel(const std::string &path
#if defined(__ANDROID__)
                            ,
                            AAssetManager *assetManager
#endif
) {
  // Use tinygltf to load the model instead of tinyobjloader
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = false;

#if defined(__ANDROID__)
  // MỚI: Android — đọc .glb từ APK qua AAssetManager
  AAsset *asset =
      AAssetManager_open(assetManager, path.c_str(), AASSET_MODE_BUFFER);
  if (!asset) {
    throw std::runtime_error("failed to open glb asset: " + path);
  }
  size_t assetSize = AAsset_getLength(asset);
  const void *assetData = AAsset_getBuffer(asset);
  ret = loader.LoadBinaryFromMemory(
      &model, &err, &warn, reinterpret_cast<const unsigned char *>(assetData),
      static_cast<unsigned int>(assetSize));
  AAsset_close(asset);
#else
  ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
#endif

  if (!warn.empty()) {
    std::cout << "glTF warning: " << warn << std::endl;
  }

  if (!err.empty()) {
    std::cout << "glTF error: " << err << std::endl;
  }

  if (!ret) {
    throw std::runtime_error("Failed to load glTF model");
  }

  vertices_.clear();
  indices_.clear();

  // Process all meshes in the model
  for (const auto &mesh : model.meshes) {
    for (const auto &primitive : mesh.primitives) {
      // Get indices
      const tinygltf::Accessor &indexAccessor =
          model.accessors[primitive.indices];
      const tinygltf::BufferView &indexBufferView =
          model.bufferViews[indexAccessor.bufferView];
      const tinygltf::Buffer &indexBuffer =
          model.buffers[indexBufferView.buffer];

      // Get vertex positions
      const tinygltf::Accessor &posAccessor =
          model.accessors[primitive.attributes.at("POSITION")];
      const tinygltf::BufferView &posBufferView =
          model.bufferViews[posAccessor.bufferView];
      const tinygltf::Buffer &posBuffer = model.buffers[posBufferView.buffer];

      // Get texture coordinates if available
      bool hasTexCoords =
          primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
      const tinygltf::Accessor *texCoordAccessor = nullptr;
      const tinygltf::BufferView *texCoordBufferView = nullptr;
      const tinygltf::Buffer *texCoordBuffer = nullptr;

      if (hasTexCoords) {
        texCoordAccessor =
            &model.accessors[primitive.attributes.at("TEXCOORD_0")];
        texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
        texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
      }

      uint32_t baseVertex = static_cast<uint32_t>(vertices_.size());

      for (size_t i = 0; i < posAccessor.count; i++) {
        Vertex vertex{};

        const float *pos = reinterpret_cast<const float *>(
            &posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset +
                            i * 12]);
        // glTF uses a right-handed coordinate system with Y-up
        vertex.pos = {pos[0], pos[1], pos[2]};

        if (hasTexCoords) {
          const float *texCoord = reinterpret_cast<const float *>(
              &texCoordBuffer->data[texCoordBufferView->byteOffset +
                                    texCoordAccessor->byteOffset + i * 8]);
          vertex.texCoord = {texCoord[0], texCoord[1]};
        } else {
          vertex.texCoord = {0.0f, 0.0f};
        }

        vertex.color = {1.0f, 1.0f, 1.0f};

        vertices_.push_back(vertex);
      }

      const unsigned char *indexData =
          &indexBuffer
               .data[indexBufferView.byteOffset + indexAccessor.byteOffset];
      size_t indexCount = indexAccessor.count;
      size_t indexStride = 0;

      // Determine index stride based on component type
      if (indexAccessor.componentType ==
          TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        indexStride = sizeof(uint16_t);
      } else if (indexAccessor.componentType ==
                 TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        indexStride = sizeof(uint32_t);
      } else if (indexAccessor.componentType ==
                 TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        indexStride = sizeof(uint8_t);
      } else {
        throw std::runtime_error("Unsupported index component type");
      }

      indices_.reserve(indices_.size() + indexCount);

      for (size_t i = 0; i < indexCount; i++) {
        uint32_t index = 0;

        if (indexAccessor.componentType ==
            TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
          index =
              *reinterpret_cast<const uint16_t *>(indexData + i * indexStride);
        } else if (indexAccessor.componentType ==
                   TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
          index =
              *reinterpret_cast<const uint32_t *>(indexData + i * indexStride);
        } else if (indexAccessor.componentType ==
                   TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
          index =
              *reinterpret_cast<const uint8_t *>(indexData + i * indexStride);
        }

        indices_.push_back(baseVertex + index);
      }
    }
  }

  std::cout << "Vertices: " << vertices_.size() << std::endl;
  std::cout << "Indices: " << indices_.size() << std::endl;
}