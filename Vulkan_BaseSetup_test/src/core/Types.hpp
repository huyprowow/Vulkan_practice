#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES // tu align memory cua uniform
// shader
//                                            // va struct trong Code (luu y: no
//                                            // del hd voi truct long nhau =>
//                                            neu
//                                            // co long nhau phai dung alignas
//                                            // (example: alignas(16) Foo
//                                            f2))=>
//                                            // luon chi ro align. clang hien
//                                            tai dang k sp :v nen comment vao
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.hpp>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2; // cho phep nhieu hung hinh xu li dong
                                        // thoi max la 2 thay vi doi tung khung

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
  }

  static std::array<vk::VertexInputAttributeDescription, 3>
  getAttributeDescriptions() {
    return {vk::VertexInputAttributeDescription(
                0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(
                1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat,
                                                offsetof(Vertex, texCoord))};
  }

  bool operator==(
      const Vertex &other) const { // vi Vertex tu dinh nghia nen phai trien
                                   // khai de dung voi unordered_map (lam key)
    return pos == other.pos && color == other.color &&
           texCoord == other.texCoord;
  }
};

namespace std {
template <>
struct hash<Vertex> { // vi Vertex tu dinh nghia nen phai trien khai de dung voi
                      // unordered_map (lam key)
  size_t operator()(Vertex const &vertex) const {
    return ((hash<glm::vec3>()(vertex.pos) ^
             (hash<glm::vec3>()(vertex.color) << 1)) >>
            1) ^
           (hash<glm::vec2>()(vertex.texCoord) << 1);
  }
};
} // namespace std

struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};