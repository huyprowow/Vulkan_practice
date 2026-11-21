#pragma once

#include <string>
#include <vector>

#if defined(__ANDROID__)
struct AAssetManager;
#endif

/// Đọc nguyên file nhị phân (vd shader .spv) vào bộ nhớ.
std::vector<char> readFile(const std::string &filename
#if defined(__ANDROID__)
                           ,
                           AAssetManager *assetManager
#endif
);