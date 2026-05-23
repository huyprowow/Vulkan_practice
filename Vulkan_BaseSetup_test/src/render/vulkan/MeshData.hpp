#pragma once

#include "../../core/Types.hpp"
#include <cstdint>
#include <string>
#include <vector>

#if defined(__ANDROID__)
struct AAssetManager;
#endif

namespace mesh {

enum class AlphaMode { Opaque, Mask, Blend };

struct SubMesh {
  uint32_t indexOffset; // offset trong MeshData::indices
  uint32_t indexCount;
  uint32_t maxVertex; // cho BLAS 
  int materialID;     // -1 nếu không có
};

struct MaterialDesc {
  std::string name;
  int baseColorTextureIndex = -1; // index vào textures[]
  AlphaMode alphaMode = AlphaMode::Opaque;
  float alphaCutoff = 0.5f;
  bool reflective = false;
};

struct TextureSource {
  std::string path;          // file ngoài
  std::vector<uint8_t> blob; // RGBA8 đã decode (cho glTF embedded)
  uint32_t width = 0;
  uint32_t height = 0;
};

struct MeshData {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<SubMesh> submeshes;
  std::vector<MaterialDesc> materials;
  std::vector<TextureSource> textures;

  // load gltf model
  static MeshData loadFromGltf(const std::string &path
#if defined(__ANDROID__)
                               ,
                               AAssetManager *assetManager
#endif
  );

  // static MeshData loadFromObj(const std::string &path);
};

} // namespace mesh