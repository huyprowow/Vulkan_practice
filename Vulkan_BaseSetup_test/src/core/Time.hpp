#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

/// Đồng hồ của vòng lặp frame. Mọi thời gian tính bằng giây.
class FrameTimer {
public:
  /// Gọi đúng 1 lần đầu mỗi frame.
  void tick() {
    const Clock::time_point now = Clock::now();
    if (frameCount_ > 0) {
      deltaSeconds_ = std::min(
          std::chrono::duration<float>(now - last_).count(), MAX_DELTA_SECONDS);
    }
    last_ = now;
    elapsedSeconds_ += deltaSeconds_;
    ++frameCount_;
  }

  float deltaSeconds() const { return deltaSeconds_; }
  double elapsedSeconds() const { return elapsedSeconds_; }
  uint64_t frameCount() const { return frameCount_; }

private:
  // steady_clock không nhảy khi đồng hồ hệ thống bị chỉnh
  using Clock = std::chrono::steady_clock;

  // kẹp dt sau lúc app bị treo (breakpoint, kéo cửa sổ, app ra nền) để mô phỏng không nhảy vọt
  static constexpr float MAX_DELTA_SECONDS = 0.1f;

  Clock::time_point last_{};
  float deltaSeconds_ = 0.0f;   // frame đầu = 0 vì chưa có frame trước
  double elapsedSeconds_ = 0.0; // float mất độ chính xác sau vài giờ chạy
  uint64_t frameCount_ = 0;
};

/// Gom delta time thành các bước cố định cho mô phỏng (fluid, SPH). Mỗi frame:
///   fixedStep.add(timer.deltaSeconds());
///   while (fixedStep.step()) simulate(fixedStep.stepSeconds());
/// Cần substep thì chia tiếp trong simulate: stepSeconds() / số substep.
class FixedStep {
public:
  explicit FixedStep(float stepSeconds, uint32_t maxStepsPerFrame = 8)
      : stepSeconds_(stepSeconds), maxStepsPerFrame_(maxStepsPerFrame) {}

  /// Gọi 1 lần mỗi frame, trước vòng while.
  void add(float dtSeconds) {
    accumulatorSeconds_ += dtSeconds;
    stepsThisFrame_ = 0;
  }

  /// true khi còn đủ thời gian cho thêm 1 bước.
  bool step() {
    if (accumulatorSeconds_ < stepSeconds_) {
      return false;
    }
    if (stepsThisFrame_ == maxStepsPerFrame_) {
      // máy không theo kịp: bỏ phần dư để mô phỏng chậm lại, thay vì dồn bước
      // sang frame sau khiến frame sau càng chậm hơn
      accumulatorSeconds_ = 0.0f;
      return false;
    }
    accumulatorSeconds_ -= stepSeconds_;
    ++stepsThisFrame_;
    return true;
  }

  float stepSeconds() const { return stepSeconds_; }

private:
  float stepSeconds_;
  uint32_t maxStepsPerFrame_;
  float accumulatorSeconds_ = 0.0f;
  uint32_t stepsThisFrame_ = 0;
};