
#pragma once

#include "../../core/Types.hpp"
#include "VulkanDevice.hpp"
#include "VulkanMemory.hpp"
#include "VulkanModel.hpp"

#include <string>
#include <vector>

#if defined(__ANDROID__)
struct AAssetManager;
#endif

/// Quản lý scene: list model + list GameObject + animation. scene chỉ giữ
/// transform + model reference.
class VulkanScene {
public:
  /// Init scene mặc định: 1 model dùng chung cho MAX_OBJECTS GameObject
  /// (giữ nguyên hành vi Ch.16 — sẽ mở rộng ở Phase 3 thành multi-model).
  void init(VulkanDevice &device, VulkanMemory &memory,
            const std::string &defaultModelPath
#if defined(__ANDROID__)
            ,
            AAssetManager *assetManager
#endif
  );

  /// Animation per-frame. Hiện tại: xoay quanh Y (giữ logic Ch.16).
  /// dtSeconds dùng cho E2 sau này khi có Camera + delta time thật.
  void update(float dtSeconds);

  /// Truy cập cho VulkanRenderer.
  std::vector<GameObject> &gameObjects() { return gameObjects_; }
  const std::vector<GameObject> &gameObjects() const { return gameObjects_; }

  VulkanModel &model() { return model_; }
  const VulkanModel &model() const { return model_; }

  void cleanup();

private:
  VulkanDevice *device_ = nullptr;
  VulkanMemory *memory_ = nullptr;

  VulkanModel model_;
  std::vector<GameObject> gameObjects_;
};