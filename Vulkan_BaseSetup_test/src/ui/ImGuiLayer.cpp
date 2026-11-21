#include "ImGuiLayer.hpp"
#include "../platform/IWindow.hpp"
#include "../render/vulkan/VulkanDevice.hpp"
#include "../render/vulkan/VulkanSwapchain.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if !defined(__ANDROID__) && !defined(VULKAN_IOS)
#include "../platform/desktop/Window.hpp"
#include <imgui_impl_glfw.h>
#endif

#include <stdexcept>

void ImGuiLayer::init(IWindow &window, const vk::raii::Instance &instance,
                      VulkanDevice &device, VulkanSwapchain &swapchain,
                      float uiScale) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  initPlatform(window);

  // nạp font đúng cỡ đã nhân tỉ lệ cho chữ nét; phóng to ảnh font bằng
  // FontGlobalScale thì chữ bị mờ
  const float scale = uiScale * contentScalePlatform(window);
  ImFontConfig fontConfig;
  fontConfig.SizePixels = 13.0f * scale;
  ImGui::GetIO().Fonts->AddFontDefault(&fontConfig);
  ImGui::GetStyle().ScaleAllSizes(scale);

  colorFormat_ = static_cast<VkFormat>(swapchain.getImageFormat());
  auto imageCount = static_cast<uint32_t>(swapchain.getImages().size());

  ImGui_ImplVulkan_InitInfo info{};
  // instance của app >= 1.3 nên backend dùng vkCmdBeginRendering của core
  info.ApiVersion = VK_API_VERSION_1_3;
  info.Instance = static_cast<VkInstance>(*instance);
  info.PhysicalDevice =
      static_cast<VkPhysicalDevice>(*device.getPhysicalDevice());
  info.Device = static_cast<VkDevice>(*device.getDevice());
  info.QueueFamily = device.getQueueIndex();
  info.Queue = static_cast<VkQueue>(*device.getGraphicsQueue());
  // backend tự tạo descriptor pool riêng cho font atlas
  // + chỗ cho ảnh hiện trong UI bằng ImGui_ImplVulkan_AddTexture (vd lưới fluid)
  info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 8;
  info.MinImageCount = imageCount;
  info.ImageCount = imageCount;
  info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  info.UseDynamicRendering = true;
  info.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &colorFormat_};
  if (!ImGui_ImplVulkan_Init(&info)) {
    throw std::runtime_error("failed to init ImGui Vulkan backend!");
  }
  initialized_ = true;
}

void ImGuiLayer::beginFrame() {
  ImGui_ImplVulkan_NewFrame();
  newFramePlatform();
  ImGui::NewFrame();
}

void ImGuiLayer::endFrame() { ImGui::Render(); }

void ImGuiLayer::record(const vk::raii::CommandBuffer &cmd,
                        vk::ImageView swapchainView, vk::Extent2D extent) {
  vk::RenderingAttachmentInfo colorAttachment{
      .imageView = swapchainView,
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eLoad, // giữ hình sample đã vẽ
      .storeOp = vk::AttachmentStoreOp::eStore};
  vk::RenderingInfo renderingInfo{
      .renderArea = {.offset = {0, 0}, .extent = extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment};
  cmd.beginRendering(renderingInfo);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                  static_cast<VkCommandBuffer>(*cmd));
  cmd.endRendering();
}

void ImGuiLayer::shutdown() {
  if (!initialized_) {
    return;
  }
  ImGui_ImplVulkan_Shutdown();
  shutdownPlatform();
  ImGui::DestroyContext();
  initialized_ = false;
}

#if !defined(__ANDROID__) && !defined(VULKAN_IOS)
void ImGuiLayer::initPlatform(IWindow &window) {
  ImGui_ImplGlfw_InitForVulkan(static_cast<Window &>(window).getHandle(), true);
}
float ImGuiLayer::contentScalePlatform(IWindow &window) const {
  GLFWwindow *handle = static_cast<Window &>(window).getHandle();
  float xScale = 1.0f;
  float yScale = 1.0f;
  glfwGetWindowContentScale(handle, &xScale, &yScale);
  // ImGui đo theo toạ độ cửa sổ. macOS/Wayland: toạ độ cửa sổ là điểm logic,
  // framebuffer đã lớn gấp xScale lần nên không nhân thêm. X11/Windows: toạ độ
  // cửa sổ là pixel thật nên phải nhân.
  int windowWidth = 0;
  int windowHeight = 0;
  int framebufferWidth = 0;
  int framebufferHeight = 0;
  glfwGetWindowSize(handle, &windowWidth, &windowHeight);
  glfwGetFramebufferSize(handle, &framebufferWidth, &framebufferHeight);
  const float pixelsPerUnit =
      windowWidth > 0 ? static_cast<float>(framebufferWidth) / windowWidth : 1.0f;
  return xScale / pixelsPerUnit;
}
void ImGuiLayer::newFramePlatform() { ImGui_ImplGlfw_NewFrame(); }
void ImGuiLayer::shutdownPlatform() { ImGui_ImplGlfw_Shutdown(); }
#else
#error "ImGuiLayer: chưa có phần nền tảng cho Android/iOS"
#endif