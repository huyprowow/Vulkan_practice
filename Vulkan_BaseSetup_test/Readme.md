## need:

- install vcpkg, ninja, cmake, vulkan sdk lunar

### 1. build
- install package:
  `vcpkg install`
- (option) remove build file if build
- create cmake build:  
   `cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake`
  ex:  
   `cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=C:/Users/ADMIN/Data/Lean/vcpkg/scripts/buildsystems/vcpkg.cmake`
- build exe: (using terminal "x64 Native Tools Command Prompt for VS 2022") window  
  `cmake --build build`

### 2.run

`.\build\Vukan_Learn.exe`
