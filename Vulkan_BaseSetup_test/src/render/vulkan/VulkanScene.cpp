#include "VulkanScene.hpp"

#include <glm/gtc/matrix_transform.hpp>

void VulkanScene::init(VulkanDevice &device, VulkanMemory &memory,
                       const std::string &defaultModelPath
#if defined(__ANDROID__)
                       ,
                       AAssetManager *assetManager
#endif
) {
  device_ = &device;
  memory_ = &memory;

  model_.load(device, memory, defaultModelPath
#if defined(__ANDROID__)
              ,
              assetManager
#endif
  );

  /// (setupGameObjects) Initialize the game objects with different positions,
  /// rotations, and scales
  gameObjects_.resize(MAX_OBJECTS);
  // Object 1 - Center
  gameObjects_[0].position = {0.0f, 0.0f, 0.0f};
  gameObjects_[0].rotation = {0.0f, 0.0f, 0.0f};
  gameObjects_[0].scale = {1.0f, 1.0f, 1.0f};

  // Object 2 - Left
  gameObjects_[1].position = {-2.0f, 0.0f, -1.0f};
  gameObjects_[1].rotation = {0.0f, glm::radians(45.0f), 0.0f};
  gameObjects_[1].scale = {0.75f, 0.75f, 0.75f};

  // Object 3 - Right
  gameObjects_[2].position = {2.0f, 0.0f, -1.0f};
  gameObjects_[2].rotation = {0.0f, glm::radians(-45.0f), 0.0f};
  gameObjects_[2].scale = {0.75f, 0.75f, 0.75f};
}

void VulkanScene::update(float /*dtSeconds*/) {
  // Apply continuous rotation to the object
  for (auto &obj : gameObjects_) {
    obj.rotation.y += 0.001f;
  }
}

void VulkanScene::cleanup() {
  // graphics
  for (auto &obj : gameObjects_) {
    obj.descriptorSets.clear();
    obj.uniformBuffersMapped.clear();
    obj.uniformBuffersMemory.clear();
    obj.uniformBuffers.clear();
  }
  gameObjects_.clear();
  model_.cleanup();
}
