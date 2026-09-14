#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>
#import <Metal/Metal.h>

#include <memory>
#include <stdexcept>
#include <unistd.h>

#include "src/platform/ios/IOSWindow.hpp"
#include "src/render/vulkan/VulkanDevice.hpp"
#include "src/render/vulkan/VulkanInstance.hpp"
#include "src/render/vulkan/VulkanRenderer.hpp"
#include "src/render/vulkan/VulkanSwapchain.hpp"

namespace {
class IOSVulkanApp {
public:
  explicit IOSVulkanApp(CAMetalLayer *layer) { window_.setMetalLayer((__bridge void *)layer); }

  void start() {
    if (initialized_)
      return;
    instance_.init(window_);
    device_.init(instance_.getInstance(), instance_.getSurface());
    swapchain_.init(device_.getPhysicalDevice(), device_, instance_.getSurface(), window_);
    renderer_.init(device_, swapchain_, window_);
    initialized_ = true;
  }

  bool draw() {
    if (!initialized_)
      return true;
    try {
      renderer_.drawFrame();
      return true;
    } catch (const std::exception &error) {
      NSLog(@"Vulkan frame failed: %s", error.what());
      return false;
    }
  }

  void resize() {
    if (initialized_)
      renderer_.recreateSwapChain();
  }

  void stop() {
    if (!initialized_)
      return;
    device_.getDevice().waitIdle();
    renderer_.cleanup();
    swapchain_.cleanup();
    initialized_ = false;
  }

  ~IOSVulkanApp() { stop(); }

private:
  IOSWindow window_;
  VulkanInstance instance_;
  VulkanDevice device_;
  VulkanSwapchain swapchain_;
  VulkanRenderer renderer_;
  bool initialized_ = false;
};
} // namespace

@interface VulkanView : UIView
@end

@implementation VulkanView
+ (Class)layerClass { return [CAMetalLayer class]; }

- (void)layoutSubviews {
  [super layoutSubviews];
  CAMetalLayer *layer = (CAMetalLayer *)self.layer;
  layer.contentsScale = self.contentScaleFactor;
  layer.drawableSize = CGSizeMake(self.bounds.size.width * layer.contentsScale,
                                  self.bounds.size.height * layer.contentsScale);
}
@end

@interface VulkanViewController : UIViewController
@property(nonatomic) CADisplayLink *displayLink;
@end

@interface VulkanViewController () {
  std::unique_ptr<IOSVulkanApp> _app;
}
@end

@implementation VulkanViewController
- (void)loadView {
  self.view = [[VulkanView alloc] initWithFrame:UIScreen.mainScreen.bounds];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  CAMetalLayer *layer = (CAMetalLayer *)self.view.layer;
  layer.device = MTLCreateSystemDefaultDevice();
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  layer.framebufferOnly = YES;
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];

  try {
    _app = std::make_unique<IOSVulkanApp>(layer);
    _app->start();
    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(drawFrame:)];
    [self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
  } catch (const std::exception &error) {
    NSLog(@"Vulkan initialization failed: %s", error.what());
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (_app)
    _app->resize();
}

- (void)drawFrame:(CADisplayLink *)displayLink {
  (void)displayLink;
  if (!_app->draw()) {
    [self.displayLink invalidate];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.displayLink invalidate];
  self.displayLink = nil;
  _app.reset();
}
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic) UIWindow *window;
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
  (void)application;
  (void)options;
  // Engine resource paths are relative (shaders/, models/, textures/).
  chdir(NSBundle.mainBundle.resourcePath.fileSystemRepresentation);
  self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
  self.window.rootViewController = [VulkanViewController new];
  [self.window makeKeyAndVisible];
  return YES;
}
@end

int main(int argc, char *argv[]) {
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass(AppDelegate.class));
  }
}
