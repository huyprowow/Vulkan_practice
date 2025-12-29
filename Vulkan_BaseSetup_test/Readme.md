## need:

- install vcpkg, ninja, cmake, vulkan sdk lunar (1.4.321.1), MSCV (VS) or LLVM(21.1.4)/clang/gcc

### 1. build

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
   `cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=C:/Users/ADMIN/Data/Lean/vcpkg/scripts/buildsystems/vcpkg.cmake`  
   `cmake -G "Visual Studio 17 2022" -B build-vs -DCMAKE_TOOLCHAIN_FILE=C:/Users/ADMIN/Data/Lean/vcpkg/scripts/buildsystems/vcpkg.cmake`

- compiler shader:
  change path slang compiler if need
  win: `./compile.bat`
  linux:
  `chmod +x compile.sh`
  `./compile.sh`

- build exe:  
  `cmake --build build`

### 2.run

`.\build\Vukan_Learn.exe`
