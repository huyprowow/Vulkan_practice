#include "VulkanTexture.hpp"

#include <cstring>
#include <ktx.h>
#include <stdexcept>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#endif

void VulkanTexture::init(VulkanDevice &device, VulkanMemory &memory,
                         VulkanSwapchain &swapchain,
                         const std::string &texturePath
#if defined(__ANDROID__)
                         ,
                         AAssetManager *assetManager
#endif
) {
  device_ = &device;
  memory_ = &memory;
  swapchain_ = &swapchain;

  // Cache depth format (dùng cho pipeline + dynamic rendering)
  depthFormat_ = findDepthFormat(*device_);

  createColorResources(); // multi-sampled color buffer
  createDepthResources(); // depth buffer
  createTextureImage(texturePath
#if defined(__ANDROID__)
                     ,
                     assetManager
#endif
  ); // tao texture image, tai image len gpu de toi uu (neu
  // k no doc PCI -> cham), chuyen layout
  createTextureImageView(); // tao image view cho texture image
  createTextureSampler(); // tao sampler cho texture image (truy cap image thong
                          // qua sampler de ap dung cac phep bien doi (mipmap,
                          // filter,...))
}

void VulkanTexture::recreateSwapChainResources() {
  // Texture chính giữ nguyên, chỉ tạo lại color + depth
  // Tự đọc extent + format mới từ swapchain_
  createColorResources();
  createDepthResources();
}

void VulkanTexture::resetSwapChainResources() {
  // Color + depth phụ thuộc swapchain → reset trước khi recreate
  colorImageView_ = nullptr;
  colorImage_ = nullptr;
  colorImageMemory_ = nullptr;
  depthImageView_ = nullptr;
  depthImage_ = nullptr;
  depthImageMemory_ = nullptr;
}

void VulkanTexture::cleanup() {
  // Texture
  textureSampler_ = nullptr;
  textureImageView_ = nullptr;
  textureImage_ = nullptr;
  textureImageMemory_ = nullptr;

  // Color + depth
  colorImageView_ = nullptr;
  colorImage_ = nullptr;
  colorImageMemory_ = nullptr;
  depthImageView_ = nullptr;
  depthImage_ = nullptr;
  depthImageMemory_ = nullptr;
}

void VulkanTexture::createColorResources() {
  vk::Format colorFormat = swapchain_->getImageFormat();
  vk::Extent2D extent = swapchain_->getExtent();

  memory_->createImage(extent.width, extent.height, 1, device_->msaaSamples_,
                       colorFormat, vk::ImageTiling::eOptimal,
                       vk::ImageUsageFlagBits::eTransientAttachment |
                           vk::ImageUsageFlagBits::eColorAttachment,
                       vk::MemoryPropertyFlagBits::eDeviceLocal, colorImage_,
                       colorImageMemory_);
  colorImageView_ = memory_->createImageView(
      colorImage_, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}

void VulkanTexture::createDepthResources() {
  vk::Extent2D extent = swapchain_->getExtent();

  memory_->createImage(extent.width, extent.height, 1, device_->msaaSamples_,
                       depthFormat_, vk::ImageTiling::eOptimal,
                       vk::ImageUsageFlagBits::eDepthStencilAttachment,
                       vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage_,
                       depthImageMemory_);
  depthImageView_ = memory_->createImageView(
      depthImage_, depthFormat_, vk::ImageAspectFlagBits::eDepth, 1);
}

vk::Format VulkanTexture::findSupportedFormat(
    VulkanDevice &device, const std::vector<vk::Format> &candidates,
    vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
  for (const auto format : candidates) {
    vk::FormatProperties props =
        device.getPhysicalDevice().getFormatProperties(format);
    if (tiling == vk::ImageTiling::eLinear &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    }
    if (tiling == vk::ImageTiling::eOptimal &&
        (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

vk::Format VulkanTexture::findDepthFormat(VulkanDevice &device) {
  return findSupportedFormat(
      device,
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
       vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool VulkanTexture::hasStencilComponent(vk::Format format) {
  return format == vk::Format::eD32SfloatS8Uint ||
         format == vk::Format::eD24UnormS8Uint;
}

/// Tải texture image lên GPU qua staging buffer, tạo mipmaps
/// tao ktx file:
/// - PNG/JPEG/TIFF, HDR/EXR, PSD: dung toktx hoac dung dung thu vien ktx tao =
/// code
/// - DDS (DirectX Texture Format): texconv DDS to PNG + toktx PNG to KTX2 hoac
/// ktx2ktx2
//  ho tro nhieu compression type: Basis Universal, ASTC, BC7, ETC2.
/// hotro tao mipmap, co the them dl vao anh
void VulkanTexture::createTextureImage(const std::string &path
#if defined(__ANDROID__)
                                       ,
                                       AAssetManager *assetManager
#endif
) {
  // 1. Load KTX2 (đã chứa mipmaps pre-computed)
  ktxTexture *kTexture;

#if defined(__ANDROID__)
  // Android — load từ AAssetManager (KTX2 từ APK)
  AAsset *asset =
      AAssetManager_open(assetManager, path.c_str(), AASSET_MODE_BUFFER);
  if (!asset) {
    throw std::runtime_error("failed to open ktx asset: " + path);
  }
  size_t assetSize = AAsset_getLength(asset);
  const void *assetData = AAsset_getBuffer(asset);
  KTX_error_code result = ktxTexture_CreateFromMemory(
      reinterpret_cast<const ktx_uint8_t *>(assetData), assetSize,
      KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);
  AAsset_close(asset);
#else
  KTX_error_code result = ktxTexture_CreateFromNamedFile(
      path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);
#endif

  if (result != KTX_SUCCESS) {
    throw std::runtime_error("failed to load ktx texture image!");
  }

  uint32_t texWidth = kTexture->baseWidth;
  uint32_t texHeight = kTexture->baseHeight;
  // Get mipmap levels
  mipLevels_ = kTexture->numLevels;
  ktx_size_t totalSize = ktxTexture_GetDataSize(kTexture);
  ktx_uint8_t *ktxData = ktxTexture_GetData(kTexture);

  // 2. Staging buffer chứa toàn bộ data (tất cả mip levels)
  vk::raii::Buffer stagingBuffer({});
  vk::raii::DeviceMemory stagingBufferMemory({});
  memory_->createBuffer(totalSize, vk::BufferUsageFlagBits::eTransferSrc,
                        vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent,
                        stagingBuffer, stagingBufferMemory);

  void *mapped = stagingBufferMemory.mapMemory(0, totalSize);
  std::memcpy(mapped, ktxData, totalSize);
  stagingBufferMemory.unmapMemory();

  // 3. Tạo image với đầy đủ mipLevels (KHÔNG cần eTransferSrc vì không blit)
  vk::Format textureFormat = vk::Format::eR8G8B8A8Srgb;
  memory_->createImage(
      texWidth, texHeight, mipLevels_, vk::SampleCountFlagBits::e1,
      textureFormat, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage_,
      textureImageMemory_);

  // 4. Transition: undefined → transferDst (tất cả levels)
  memory_->transitionImageLayout(textureImage_, vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eTransferDstOptimal,
                                 mipLevels_);

  // 5. Copy từng level từ staging buffer trong 1 command buffer
  auto cmdBuf = memory_->beginSingleTimeCommands();
  std::vector<vk::BufferImageCopy> regions;
  for (uint32_t level = 0; level < mipLevels_; level++) {
    ktx_size_t offset;
    ktxTexture_GetImageOffset(kTexture, level, 0, 0, &offset);
    regions.push_back(vk::BufferImageCopy{
        .bufferOffset = offset,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, level, 0, 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {std::max(1u, texWidth >> level),
                        std::max(1u, texHeight >> level), 1}});
  }
  cmdBuf.copyBufferToImage(*stagingBuffer, *textureImage_,
                           vk::ImageLayout::eTransferDstOptimal, regions);
  memory_->endSingleTimeCommands(cmdBuf);

  // 6. Transition: transferDst → shaderReadOnly (tất cả levels)
  memory_->transitionImageLayout(
      textureImage_, vk::ImageLayout::eTransferDstOptimal,
      vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels_);

  // KHÔNG còn generateMipmaps(...) - KTX2 đã pre-compute rồi
  ktxTexture_Destroy(kTexture);
}

void VulkanTexture::createTextureImageView() {
  textureImageView_ =
      memory_->createImageView(textureImage_, vk::Format::eR8G8B8A8Srgb,
                               vk::ImageAspectFlagBits::eColor, mipLevels_);
}

/// Tạo sampler cho texture: filter, mipmap mode, anisotropy
void VulkanTexture::createTextureSampler() {
  // sampler kiem soat dl doc, muc mipmap, filer,...
  vk::PhysicalDeviceProperties properties =
      device_->getPhysicalDevice().getProperties();
  vk::SamplerCreateInfo samplerInfo{
      .magFilter = vk::Filter::eLinear, // gan camera dung cai nay
      .minFilter = vk::Filter::eLinear, // xa camera dung cai nay
      .mipmapMode =
          vk::SamplerMipmapMode::eLinear, // linear select 2mip interpolation
                                          // between mipmaps, nearest lod
                                          // selection mip
      .addressModeU = vk::SamplerAddressMode::eRepeat,
      .addressModeV = vk::SamplerAddressMode::eRepeat,
      .addressModeW = vk::SamplerAddressMode::eRepeat,
      .mipLodBias =
          0.0f, // lod khong am, va chi bang 0 khi o gan camera.
                // mipLodBias cho phep buoc Vulkan su dung gia tri
                // thap hon lod va level so voi gia tri ma no thuong su dung
      .anisotropyEnable = vk::True,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways,
      .minLod = 0.0f,            // dat muc mipmap thap nhat
      .maxLod = vk::LodClampNone // khong gioi han muc mipmap dam bao toan bo
                                 // pv mip dc dung
  };
  samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  samplerInfo.unnormalizedCoordinates = vk::False;
  // samplerInfo.minLod = static_cast<float>(mipLevels_ / 2); //test mip

  textureSampler_ = vk::raii::Sampler(device_->getDevice(), samplerInfo);
}