## need:

- package install vcpkg, ninja, cmake, vulkan sdk lunar (1.4.321.1) (apple -> molten vulkan), MSCV (VS) or LLVM(21.1.4)/clang/gcc, android: NDK (30.0.14904198 rc1), sdk (36, min sdk version is 24)

note:

- gpu driver
  - mac/ios: often built in driver Metal not vk driver (lock Metal api)=> then need use molten vk translate to Metal to run (same ios)
  - window: auto detect, built in but if wrong, go to official home page install the correct hardware providing drivers for best performance and to avoid errors
  - linux: 1 billion cases for this because few people use it + a ton of distros. often builtin opensource driver. for famous distro install/use official driver for best perf, other wise use mesa driver stable but performance reduction (not sure)
  - android/raspberry pi/nitendo switch: built in
- can't run in xbox(lock only DirectX api),playstation (have GNM, GNMX api not suport vk may be need to porting)

### 1. build

#### a. desktop

- install package:
  `vcpkg install`
- (option) remove build file if build

note: using terminal "x64 Native Tools Command Prompt for VS 2022" window if use msvc or build sln, if use llvm/clang/gcc add it to path/enviroment

- create cmake ninja build
  - normaly:  
     `cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake`

  - Visual Studio: open .sln file then build
    `cmake -G "<idle_name>" -B build-vs -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake`

  ex:  
   `cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=C:/Users/ADMIN/Data/Lean/vcpkg/scripts/buildsystems/vcpkg.cmake -DENABLE_DX12=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`  
   `cmake -G "Visual Studio 17 2022" -B build-vs -DCMAKE_TOOLCHAIN_FILE=C:/Users/ADMIN/Data/Lean/vcpkg/scripts/buildsystems/vcpkg.cmake`
  note: for apple may be need add custom triple:
    `cmake -G Ninja -B build \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_OVERLAY_TRIPLETS=Cmake\custom-triplets \
    -DCMAKE_BUILD_TYPE=Release`

- (option) compiler shader:
  change path slang compiler if need
  win: `./compile.bat`
  linux:
  `chmod +x compile.sh`
  `./compile.sh`

- build exe:  
  `cmake --build build`

- (option) build docs:

```
cd docs && doxygen Doxyfile
open html/index.html
```

or

```
cmake -B build -DBUILD_DOCS=ON
cmake --build build --target docs
```

#### b. android

- Open Vulkan_BaseSetup_test/android/ in Android Studio
- already install NDK, android SDK + CMake installed (SDK Manager)
  - NDK version: `30.0.14904198 rc1`
  - android SDK: `36` min android SDK: `24`

- Sync grandle, Build/Run from Android Studio (or gradlew :app:assembleDebug)
  note: Native build uses:
  - `android/app/src/main/cpp/CMakeLists.txt`
  - outputs shaders to `.externalNativeBuild/.../shaders`
  - Gradle packs them via `sourceSets.main.assets.srcDirs`

note:

- android not include vukan header by default in android NDK then must download, this action handle by fetch content cmake `FindVulkanHpp.cmake` (clone and include) version Vulkan header will using is `v1.4.317` Compatible with NDK `30.0.14904198 rc1` used in grandle build
- to make it work with Vulkan-Hpp + designated initializers must turn on buld option `-DVULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1` and `-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1`
- ABI note (important):
  - Vulkan-Hpp may fail to compile on 32-bit ABIs (`armeabi-v7a`, `x86`) with errors like:
    `invalid operands to binary expression ('const vk::ShaderModule' and 'const vk::ShaderModule')`
    This happens inside Vulkan-Hpp generated `operator==` code for some handle types on 32-bit. then build only `arm64-v8a` and `x86_64`.
  - Check ABI of a device/emulator:
    `adb shell getprop ro.product.cpu.abilist`

### 2.run

#### a. desktop

go to build folder
`cd build`
then
`.\Vukan_Learn.exe`

#### b. android

run directly by android studio with emulator or copy .apk file then install in android device:
go to android build folder: `android\app\build\` apk file will in: `android\app\build\outputs\apk\debug\app-debug.apk` or `android\app\build\outputs\apk\release\app-release.apk`

### 3.debug

use: Nsight graphic or render doc
Nsight:

- create new project
- set application executable (path to exe file):
  ex: `C:/Users/ADMIN/Data/Lean/Vulkan/Vulkan_BaseSetup_test/build/Vulkan_Learn.exe`
- set working directory (filee dirctory include exe file):
  ex: `C:/Users/ADMIN/Data/Lean/Vulkan/Vulkan_BaseSetup_test/build`
- choose activity and launch debug => run exe and start debug.

render doc:
set path to exe then launch or attach/inject to exe process need debug then start

android runtime crash :
`adb logcat -c`
`adb logcat *:E Vulkan:D DEBUG:D AndroidRuntime:E libc:F`
then run application crash it will show log

## maintain

got 2 entry point for android and desktop

- android: `android\app\src\main\cpp\android_main.cpp`
- desktop: `main.cpp`
  share core in `src` folder

shader code share cross platform in `shaders` folder, Compile shader when cmake build, (android: after cmake build shader, build tool link/map to apk resource), then read shader built for each platform

cmake manager, find lib and install from git if fallback
game_activity_bridge.cpp not use, use `android_native_app_glue.c`,`android_native_app_glue.h` for ndk version installed `30.0.14904198 rc1`