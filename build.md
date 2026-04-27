# joltc 跨平台构建指南 (macOS 交叉编译)

所有平台均可从一台 Mac 完成编译。JoltPhysics (C++) 会被静态链接进 joltc，最终产物是**单一库文件**。

## 环境准备

```shell
brew install cmake ninja
brew install mingw-w64   # Windows x64/x86 交叉编译
brew install zig          # Linux 交叉编译 & Windows ARM64
```

## 预下载 JoltPhysics（推荐）

在项目根目录 clone 一份 JoltPhysics，所有平台构建共享同一份源码，避免每个 build 目录重复下载:



```shell
git clone --branch v5.5.0 --depth 1 https://github.com/jrouwe/JoltPhysics.git
```

或者直接以子模块形式添加到项目中:
```shell
git submodule add https://github.com/jrouwe/JoltPhysics.git 
```

> CMakeLists.txt 会自动检测 `./JoltPhysics/` 目录，如果存在则直接使用，不再从远程下载。

---

## macOS 构建

```shell
# Apple Silicon + x86_64 Universal Binary
cmake -S . -B build_osx -G Ninja \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11
cmake --build build_osx --config Distribution --parallel
```

产物: `build_osx/lib/libjoltc.dylib`

验证 Universal Binary:
```shell
lipo -info build_osx/lib/libjoltc.dylib
# Architectures in the fat file: libjoltc.dylib are: x86_64 arm64
```

---

## iOS 构建

iOS 强制构建为静态库 (`.a`)。交叉编译时需禁用 samples 和 tests。

```shell
# arm64 真机
cmake -S . -B build_ios_arm64 -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_ios_arm64 --config Release -- -sdk iphoneos
```

```shell
# arm64 模拟器 (Apple Silicon)
cmake -S . -B build_ios_sim_arm64 -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_ios_sim_arm64 --config Release -- -sdk iphonesimulator
```

```shell
# x86_64 模拟器 (Intel Mac)
cmake -S . -B build_ios_sim_x64 -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_ios_sim_x64 --config Release -- -sdk iphonesimulator
```

```shell
# 打包 XCFramework
xcodebuild -create-xcframework \
  -library build_ios_arm64/lib/Release/libjoltc.a \
  -library build_ios_sim_arm64/lib/Release/libjoltc.a \
  -output joltc.xcframework
```

产物: `build_ios_arm64/lib/Release/libjoltc.a`

---

## Android 构建

```shell
# NDK 路径（根据实际安装位置修改）
# Android Studio:  ~/Library/Android/sdk/ndk/<version>
# Unity:           /Applications/Unity/Hub/Editor/<version>/PlaybackEngines/AndroidPlayer/NDK
export NDK_PATH=/Applications/Unity/Hub/Editor/6000.3.9f1/PlaybackEngines/AndroidPlayer/NDK
```

```shell
# arm64-v8a (16KB page alignment, Android 15+ 要求)
cmake -S . -B build_android_arm64-v8a \
  -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21 \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_android_arm64-v8a --config Distribution --parallel
```

```shell
# armeabi-v7a
cmake -S . -B build_android_armeabi-v7a \
  -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=armeabi-v7a \
  -DANDROID_PLATFORM=android-21 \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_android_armeabi-v7a --config Distribution --parallel
```

```shell
# x86
cmake -S . -B build_android_x86 \
  -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=x86 \
  -DANDROID_PLATFORM=android-21 \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_android_x86 --config Distribution --parallel
```

```shell
# x86_64
cmake -S . -B build_android_x86_64 \
  -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=x86_64 \
  -DANDROID_PLATFORM=android-21 \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_android_x86_64 --config Distribution --parallel
```

产物: `build_android_<abi>/lib/libjoltc.so`

---

## Windows 构建

### win-x64 (mingw-w64)

```shell
cmake -S . -B build_win_x64 -G Ninja \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static -lpthread" \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF \
  -DJPH_USE_DX12=OFF -DJPH_USE_DXC=OFF
cmake --build build_win_x64 --config Distribution --parallel
```

### win-x86 (mingw-w64)

```shell
rm -rf build_win_x86
cmake -S . -B build_win_x86 -G Ninja \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=i686-w64-mingw32-windres \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DCMAKE_CXX_FLAGS="-msse2" \
  -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static -lpthread" \
  -DUSE_SSE4_1=OFF -DUSE_SSE4_2=OFF -DUSE_AVX=OFF -DUSE_AVX2=OFF \
  -DUSE_LZCNT=OFF -DUSE_TZCNT=OFF -DUSE_F16C=OFF -DUSE_FMADD=OFF \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF \
  -DJPH_USE_DX12=OFF -DJPH_USE_DXC=OFF
cmake --build build_win_x86 --config Distribution --parallel
```

> **注意**: i686 (32位) 默认不启用 SSE，需通过 `-DCMAKE_CXX_FLAGS="-msse2"` 全局启用 SSE2，并禁用 SSE4/AVX 等高级指令集。

### win-arm64 (zig)

```shell
rm -rf build_win_arm64
# Multi-line (if you prefer, ensure the -D assignments are not split across lines):
cmake -S . -B build_win_arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig.toolchain.cmake \
  -DZIG_TARGET=aarch64-windows-gnu \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF \
  -DJPH_USE_DX12=OFF -DJPH_USE_DXC=OFF
cmake --build build_win_arm64 --config Distribution --parallel
```

产物: `build_win_<arch>/lib/libjoltc.dll`

注意:
- 如果 CMake 在内部执行 try_compile 导致看不到顶层的 `-DZIG_TARGET`，可使用环境变量回退：
  `export ZIG_TARGET=aarch64-windows-gnu`（在 macOS/zsh 下）或直接传递到 cmake 命令行。
- 本项目自带的 `cmake/zig.toolchain.cmake` 会尝试从 `CMakeCache.txt` 或环境变量读取 `ZIG_TARGET`，以提高在复杂 configure 流程中的鲁棒性。

---

## Linux 构建 (zig 交叉编译)

使用项目内的 zig toolchain 文件，zig 会自动处理 C++ 运行时的静态链接。

### linux-x86_64

```shell
cmake -S . -B build_linux_x86_64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig-linux-x86_64.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_linux_x86_64 --config Distribution --parallel
```

### linux-arm64

```shell
cmake -S . -B build_linux_arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig-linux-aarch64.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_linux_arm64 --config Distribution --parallel
```

也可以使用通用 toolchain 文件:
```shell
cmake -S . -B build_linux_arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig.toolchain.cmake \
  -DZIG_TARGET=aarch64-linux-gnu \
  -DCMAKE_BUILD_TYPE=Distribution \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_linux_arm64 --config Distribution --parallel
```

### linux-arm64 Debug（保留调试信息，用于内存泄漏排查）

使用 `Debug` 构建类型，保留完整调试符号，**不进行 strip**。适用于 AddressSanitizer / Valgrind 等工具定位内存问题。

```shell
cmake -S . -B build_linux_arm64_debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig-linux-aarch64.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_linux_arm64_debug --config Debug --parallel
```

产物: `build_linux_arm64_debug/lib/libjoltc.so`（含完整调试符号，不要 strip）

产物: `build_linux_<arch>/lib/libjoltc.so`

---

## 一键构建脚本

```shell
#!/bin/bash
set -e

BUILD_TYPE=${1:-Distribution}
NDK_PATH=${NDK_PATH:-/Applications/Unity/Hub/Editor/6000.3.9f1/PlaybackEngines/AndroidPlayer/NDK}

echo "=== joltc cross-platform build (${BUILD_TYPE}) ==="

# ─── macOS Universal ───
echo ">>> macOS Universal (x86_64 + arm64)"
cmake -S . -B build_osx -G Ninja \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11
cmake --build build_osx --parallel

# ─── iOS ───
echo ">>> iOS arm64 (Device)"
cmake -S . -B build_ios_arm64 -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_ios_arm64 --config Release -- -sdk iphoneos

echo ">>> iOS Simulator arm64"
cmake -S . -B build_ios_sim_arm64 -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_ios_sim_arm64 --config Release -- -sdk iphonesimulator

# ─── Android ───
if [ -d "$NDK_PATH" ]; then
  TOOLCHAIN=$NDK_PATH/build/cmake/android.toolchain.cmake
  for ABI in arm64-v8a armeabi-v7a x86_64 x86; do
    echo ">>> Android ${ABI}"
    EXTRA_FLAGS=""
    if [ "$ABI" = "arm64-v8a" ]; then
      EXTRA_FLAGS="-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384"
    fi
    cmake -S . -B "build_android_${ABI}" \
      -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN \
      -DANDROID_ABI=${ABI} \
      -DANDROID_PLATFORM=android-21 \
      -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
      -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF \
      ${EXTRA_FLAGS}
    cmake --build "build_android_${ABI}" --parallel
  done
else
  echo "!!! NDK_PATH not found: $NDK_PATH (skipping Android)"
fi

# ─── Windows (mingw-w64) ───
echo ">>> Windows x64"
cmake -S . -B build_win_x64 -G Ninja \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static -lpthread" \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF \
  -DJPH_USE_DX12=OFF -DJPH_USE_DXC=OFF
cmake --build build_win_x64 --parallel

echo ">>> Windows x86"
cmake -S . -B build_win_x86 -G Ninja \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=i686-w64-mingw32-windres \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCMAKE_CXX_FLAGS="-msse2" \
  -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static -lpthread" \
  -DUSE_SSE4_1=OFF -DUSE_SSE4_2=OFF -DUSE_AVX=OFF -DUSE_AVX2=OFF \
  -DUSE_LZCNT=OFF -DUSE_TZCNT=OFF -DUSE_F16C=OFF -DUSE_FMADD=OFF \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF \
  -DJPH_USE_DX12=OFF -DJPH_USE_DXC=OFF
cmake --build build_win_x86 --parallel

echo ">>> Windows ARM64 (zig)"
cmake -S . -B build_win_arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig.toolchain.cmake \
  -DZIG_TARGET=aarch64-windows-gnu \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF \
  -DJPH_USE_DX12=OFF -DJPH_USE_DXC=OFF
cmake --build build_win_arm64 --parallel

# ─── Linux (zig) ───
echo ">>> Linux x86_64 (zig)"
cmake -S . -B build_linux_x86_64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig-linux-x86_64.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_linux_x86_64 --parallel

echo ">>> Linux ARM64 (zig)"
cmake -S . -B build_linux_arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/zig-linux-aarch64.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DJPH_SAMPLES=OFF -DJPH_TESTS=OFF
cmake --build build_linux_arm64 --parallel

echo "=== Done ==="
```

---

## Strip（移除调试符号，减小体积）

编译产物默认包含符号表和调试信息，strip 后体积可减少 70%~92%。strip 不影响功能和性能，仅移除调试用的元数据。

> macOS (AppleClang) 和 win-arm64 (zig) 的 Distribution 构建默认已 strip，无需额外处理。

### Linux

```shell
llvm-strip --strip-unneeded build_linux_x86_64/lib/libjoltc.so
llvm-strip --strip-unneeded build_linux_arm64/lib/libjoltc.so
```

### Windows (mingw-w64)

```shell
x86_64-w64-mingw32-strip --strip-unneeded build_win_x64/lib/libjoltc.dll
i686-w64-mingw32-strip --strip-unneeded build_win_x86/lib/libjoltc.dll
```

### Android

```shell
llvm-strip --strip-unneeded build_android_arm64-v8a/lib/libjoltc.so
llvm-strip --strip-unneeded build_android_armeabi-v7a/lib/libjoltc.so
llvm-strip --strip-unneeded build_android_x86_64/lib/libjoltc.so
llvm-strip --strip-unneeded build_android_x86/lib/libjoltc.so
```

### Strip 前后体积参考

| 平台 | Strip 前 | Strip 后 |
|------|---------|---------|
| Linux x86_64 | 29 MB | 2.7 MB |
| Linux arm64 | 31 MB | 2.4 MB |
| Android arm64-v8a | 25 MB | 2.5 MB |
| Android armeabi-v7a | 22 MB | 2.0 MB |
| Android x86_64 | 22 MB | 2.6 MB |
| Android x86 | 21 MB | 2.8 MB |
| Windows x64 | 15 MB | 4.1 MB |
| Windows x86 | 14 MB | 2.9 MB |

---

## 产物汇总

| 平台 | 架构 | 工具链 | 产物 |
|------|------|--------|------|
| macOS | Universal (x86_64+arm64) | AppleClang | `libjoltc.dylib` |
| iOS | arm64 | Xcode | `libjoltc.a` |
| iOS Simulator | arm64 / x86_64 | Xcode | `libjoltc.a` |
| Android | arm64-v8a | NDK | `libjoltc.so` |
| Android | armeabi-v7a | NDK | `libjoltc.so` |
| Android | x86_64 | NDK | `libjoltc.so` |
| Android | x86 | NDK | `libjoltc.so` |
| Windows | x64 | mingw-w64 | `libjoltc.dll` |
| Windows | x86 | mingw-w64 | `libjoltc.dll` |
| Windows | ARM64 | zig | `libjoltc.dll` |
| Linux | x86_64 | zig | `libjoltc.so` |
| Linux | arm64 | zig | `libjoltc.so` |

## 关于单一库输出

CMakeLists.txt 确保所有平台最终只产出**一个库文件**，包含完整的 JoltPhysics 代码:

- **共享库** (.dll/.so/.dylib): Jolt 以 `PRIVATE` 方式链接 — 链接器自动将 Jolt 代码嵌入共享库
- **静态库** (.a，iOS 等): 通过 `$<TARGET_OBJECTS:Jolt>` 将 Jolt 的 object 文件直接编入 joltc.a（需 CMake 3.21+）

C# 桥接方式:
- 共享库平台: `[DllImport("joltc")]`
- iOS: `[DllImport("__Internal")]`（静态链接到 app）

## 常见问题

**Q: zig 编译 C++ (JoltPhysics) 会有问题吗?**
zig 内置了完整的 clang C++17 编译器和 libc++，JoltPhysics 的 C++17 特性和 SIMD 内联函数均可正常编译。如遇到 LTO 相关错误，可添加 `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF`。

**Q: mingw-w64 编译的 DLL 和 MSVC 编译的兼容吗?**
joltc 导出的是纯 C API，C ABI 在 MSVC 和 MinGW 之间完全兼容。`-static-libgcc -static-libstdc++` 确保无外部运行时依赖。

**Q: 如何构建 Double Precision 版本?**
在任意 cmake configure 命令中添加 `-DDOUBLE_PRECISION=ON`，产物名称变为 `joltc_double`。

**Q: 如何构建静态库?**
添加 `-DJPH_BUILD_SHARED=OFF`，产物为 `.a` / `.lib`。
