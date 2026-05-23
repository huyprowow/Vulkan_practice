#include "MeshData.hpp"
#include <iostream>
#include <stdexcept>
// #define TINYGLTF_IMPLEMENTATION cmake defined
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif
namespace mesh {
/// Tải GLTF model, co the convert obj -> gltf bang obj2gltf
MeshData MeshData::loadFromGltf(const std::string &path
#if defined(__ANDROID__)
                                ,
                                AAssetManager *assetManager
#endif
) {
  MeshData out;

  // Use tinygltf to load the model instead of tinyobjloader
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = false;

#if defined(__ANDROID__)
  // Android — đọc .glb từ APK qua AAssetManager
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

  out.vertices.clear();
  out.indices.clear();

  // Process all meshes in the model
  for (const auto &mesh : model.meshes) {
    for (const auto &primitive : mesh.primitives) {
      // track submesh để (BLAS per submesh) dùng
      uint32_t startIndex = static_cast<uint32_t>(out.indices.size());
      uint32_t localMaxV = 0;
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

      bool hasNormals =
          primitive.attributes.find("NORMAL") != primitive.attributes.end();
      const tinygltf::Accessor *normalAccessor = nullptr;
      const tinygltf::BufferView *normalBufferView = nullptr;
      const tinygltf::Buffer *normalBuffer = nullptr;

      if (hasNormals) {
        normalAccessor = &model.accessors[primitive.attributes.at("NORMAL")];
        normalBufferView = &model.bufferViews[normalAccessor->bufferView];
        normalBuffer = &model.buffers[normalBufferView->buffer];
      }

      uint32_t baseVertex = static_cast<uint32_t>(out.vertices.size());

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

        if (hasNormals) {
          const float *normal = reinterpret_cast<const float *>(
              &normalBuffer->data[normalBufferView->byteOffset +
                                  normalAccessor->byteOffset + i * 12]);
          vertex.normal = {normal[0], normal[1], normal[2]};
        } else {
          vertex.normal = {0.0f, 0.0f, 1.0f};
        }

        vertex.color = {1.0f, 1.0f, 1.0f};

        out.vertices.push_back(vertex);
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

      out.indices.reserve(out.indices.size() + indexCount);

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

        // track max vertex index trong submesh để tính BLAS
        if (index > localMaxV) {
          localMaxV = index;
        }
        out.indices.push_back(baseVertex + index);
      }

      // push 1 SubMesh per primitiv
      //(BLAS) sẽ dùng : indexOffset / indexCount để chia geometry,
      //  maxVertex để giới hạn vertex range, materialID để bindless lookup

      SubMesh sm{};
      sm.indexOffset = startIndex;
      sm.indexCount = static_cast<uint32_t>(indexCount);
      sm.maxVertex = baseVertex + localMaxV + 1;
      sm.materialID = primitive.material; // -1 nếu không có material
      out.submeshes.push_back(sm);
    }
  }

  // parse materials dùng để build global material table khi merge nhiều model.
  // Có thể trống nếu glTF không có material (vd viking_room minimal).
  for (const auto &m : model.materials) {
    MaterialDesc md{};
    md.name = m.name;
    md.baseColorTextureIndex = m.pbrMetallicRoughness.baseColorTexture.index;
    md.alphaMode = (m.alphaMode == "MASK")    ? AlphaMode::Mask
                   : (m.alphaMode == "BLEND") ? AlphaMode::Blend
                                              : AlphaMode::Opaque;
    md.alphaCutoff = static_cast<float>(m.alphaCutoff);
    md.reflective = (m.pbrMetallicRoughness.metallicFactor > 0.5);
    out.materials.push_back(md);
  }

  std::cout << "Vertices: " << out.vertices.size() << std::endl;
  std::cout << "Indices: " << out.indices.size() << std::endl;
  std::cout << "Submeshes: " << out.submeshes.size() << std::endl;
  return out;
}
} // namespace mesh