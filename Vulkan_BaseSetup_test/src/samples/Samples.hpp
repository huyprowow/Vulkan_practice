#pragma once

#include "../core/ISample.hpp"
#include "raster_basic/RasterBasicSample.hpp"

#include <memory>

/// Danh sách sample để chọn trong menu. Thêm sample mới: thêm 1 dòng.
struct SampleEntry {
  const char *name;
  std::unique_ptr<ISample> (*create)();
};

inline constexpr SampleEntry SAMPLES[] = {
    {"raster_basic",
     []() -> std::unique_ptr<ISample> {
       return std::make_unique<RasterBasicSample>();
     }},
};