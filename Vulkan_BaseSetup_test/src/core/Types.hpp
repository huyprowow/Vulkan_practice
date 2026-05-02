#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
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
#define GLM_FORCE_CXX11
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr uint32_t PARTICLE_COUNT =
    8192; // INVOCATIONS_SIZE = 256 nen particle count phai chia het cho
          // INVOCATIONS_SIZE de lay so work group x
constexpr uint32_t INVOCATIONS_SIZE =
    256; // invocations trong 1 work group( dinh nghia trong shader compute )
const std::string MODEL_PATH = "models/viking_room.glb";
const std::string TEXTURE_PATH = "textures/viking_room.ktx2";
// Define the number of objects to render
constexpr int MAX_OBJECTS = 3;

// Validation layers co the quan li bang vulkanconfig thay cho hard-coded o day
// (cai vulkan configurator (GUI) roi chinh hoang chinh bang vkconfig.exe)

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT =
    2; ///< cho phep nhieu hung hinh xu li dong
       ///< thoi max la 2 thay vi doi tung khung

/// So worker thread toi da cho compute multithreading.
/// Cap = 4 vi:
/// - PARTICLE_COUNT (8192) chia het cho 1/2/4/8
/// - >4 thread → overhead sync (mutex/CV) > work, lai cham hon
/// - Cap nay tranh tao qua nhieu thread tren may CPU 16/32 core
constexpr uint32_t MAX_COMPUTE_THREADS = 4;

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
      const Vertex &other) const { ///< vi Vertex tu dinh nghia nen phai trien
                                   ///< khai de dung voi unordered_map (lam key)
    return pos == other.pos && color == other.color &&
           texCoord == other.texCoord;
  }
};

namespace std {
template <>
struct hash<Vertex> { ///< vi Vertex tu dinh nghia nen phai trien khai de dung
                      ///< voi unordered_map (lam key)
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

// Define a structure to hold per-object data
struct GameObject {
  // Transform properties
  glm::vec3 position = {0.0f, 0.0f, 0.0f};
  glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
  glm::vec3 scale = {1.0f, 1.0f, 1.0f};

  // Uniform buffer for this object (one per frame in flight)
  std::vector<vk::raii::Buffer> uniformBuffers;
  std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
  std::vector<void *> uniformBuffersMapped;

  // Descriptor sets for this object (one per frame in flight)
  std::vector<vk::raii::DescriptorSet> descriptorSets;

  // Calculate model matrix based on position, rotation, and scale
  glm::mat4 getModelMatrix() const {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
  }
};

// struc cho 1 hạt particle
struct Particle {
  alignas(8) glm::vec2 position; // std430: vec2 align 8
  alignas(8) glm::vec2 velocity;
  alignas(16) glm::vec4 color; // std430: vec4 align 16
  static vk::VertexInputBindingDescription getBindingDescription() {
    return {0, sizeof(Particle), vk::VertexInputRate::eVertex};
  }
  // Chỉ position + color, KHÔNG có velocity (chỉ compute dùng)
  static std::array<vk::VertexInputAttributeDescription, 2>
  getAttributeDescriptions() {
    return {vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat,
                                                offsetof(Particle, position)),
            vk::VertexInputAttributeDescription(1, 0,
                                                vk::Format::eR32G32B32A32Sfloat,
                                                offsetof(Particle, color))};
  }
};

// struct cho uniform buffer object cho compute shader
struct ComputeUBO {
  float deltaTime;
};