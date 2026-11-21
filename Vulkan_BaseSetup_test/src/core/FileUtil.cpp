#include "FileUtil.hpp"
#include <cstdint>
#include <fstream>
#include <stdexcept>
#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif


/// Nếu Android và có assetManager → dùng AAssetManager_open để đọc file trong
/// APK. Nếu không → dùng filesystem path.(fallback)
/// trong desktop load tu file system, trong android load tu resource cua APK
std::vector<char> readFile(const std::string &filename
#if defined(__ANDROID__)
                                           ,
                                           AAssetManager *assetManager
#endif

) {

#if defined(__ANDROID__)
  if (assetManager) {
    AAsset *asset =
        AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
      throw std::runtime_error("failed to open asset: " + filename);
    }
    const off_t length = AAsset_getLength(asset);
    std::vector<char> buffer(static_cast<size_t>(length));
    const int64_t readBytes = AAsset_read(asset, buffer.data(), length);
    AAsset_close(asset);
    if (readBytes != length) {
      throw std::runtime_error("failed to read full asset: " + filename);
    }
    return buffer;
  }
#endif
  // Desktop (or Android fallback) filesystem path
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }
  std::vector<char> buffer(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();
  return buffer;
}