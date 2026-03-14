#pragma once

struct IDevice;
struct ISwapChain;
class Window;

struct IRenderer {
  virtual ~IRenderer() = default;

  virtual void drawFrame() = 0;
  virtual void recreateSwapChain() = 0;
  virtual void cleanup() = 0;
};