# Generic Zig cross-compilation toolchain for CMake
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/zig.toolchain.cmake \
#         -DZIG_TARGET=x86_64-linux-gnu \
#         -DCMAKE_BUILD_TYPE=Release
#
# Common targets:
#   x86_64-linux-gnu      - Linux x86_64
#   aarch64-linux-gnu     - Linux ARM64
#   aarch64-windows-gnu   - Windows ARM64

# Persist ZIG_TARGET across CMake's internal try_compile calls:
# Use CMAKE_CURRENT_LIST_DIR (the toolchain file's directory) which is stable
# across all contexts including try_compile scratch directories.
set(_ZIG_TARGET_FILE "${CMAKE_CURRENT_LIST_DIR}/_zig_target.txt")

if(DEFINED ZIG_TARGET)
    # Directly provided on the command line (-DZIG_TARGET=...)
    file(WRITE "${_ZIG_TARGET_FILE}" "${ZIG_TARGET}")
elseif(EXISTS "${_ZIG_TARGET_FILE}")
    # Already persisted by a previous top-level configure (used during try_compile)
    file(READ "${_ZIG_TARGET_FILE}" ZIG_TARGET)
    string(STRIP "${ZIG_TARGET}" ZIG_TARGET)
elseif(DEFINED ENV{ZIG_TARGET})
    # Allow passing via environment variable as a fallback
    set(ZIG_TARGET $ENV{ZIG_TARGET})
    string(STRIP "${ZIG_TARGET}" ZIG_TARGET)
    file(WRITE "${_ZIG_TARGET_FILE}" "${ZIG_TARGET}")
else()
    message(FATAL_ERROR "ZIG_TARGET is not defined. Example: -DZIG_TARGET=x86_64-linux-gnu")
endif()

# --- Detect CMAKE_SYSTEM_NAME ---
if(ZIG_TARGET MATCHES "linux")
    set(CMAKE_SYSTEM_NAME Linux)
elseif(ZIG_TARGET MATCHES "windows")
    set(CMAKE_SYSTEM_NAME Windows)
elseif(ZIG_TARGET MATCHES "macos")
    set(CMAKE_SYSTEM_NAME Darwin)
endif()

# --- Detect CMAKE_SYSTEM_PROCESSOR ---
if(ZIG_TARGET MATCHES "^x86_64")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
elseif(ZIG_TARGET MATCHES "^aarch64")
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
elseif(ZIG_TARGET MATCHES "^arm")
    set(CMAKE_SYSTEM_PROCESSOR arm)
elseif(ZIG_TARGET MATCHES "^i[3-6]86")
    set(CMAKE_SYSTEM_PROCESSOR i686)
endif()

# --- Create zig wrapper scripts in build directory ---
set(_ZIG_DIR "${CMAKE_BINARY_DIR}/_zig_wrappers")
file(MAKE_DIRECTORY "${_ZIG_DIR}")

file(WRITE "${_ZIG_DIR}/zig-cc"
    "#!/bin/sh\nexec zig cc -target ${ZIG_TARGET} \"$@\"\n")
file(WRITE "${_ZIG_DIR}/zig-cxx"
    "#!/bin/sh\nexec zig c++ -target ${ZIG_TARGET} \"$@\"\n")
file(WRITE "${_ZIG_DIR}/zig-ar"
    "#!/bin/sh\nexec zig ar \"$@\"\n")
file(WRITE "${_ZIG_DIR}/zig-ranlib"
    "#!/bin/sh\nexec zig ranlib \"$@\"\n")

execute_process(COMMAND chmod +x
    "${_ZIG_DIR}/zig-cc"
    "${_ZIG_DIR}/zig-cxx"
    "${_ZIG_DIR}/zig-ar"
    "${_ZIG_DIR}/zig-ranlib"
)

set(CMAKE_C_COMPILER   "${_ZIG_DIR}/zig-cc")
set(CMAKE_CXX_COMPILER "${_ZIG_DIR}/zig-cxx")
set(CMAKE_AR           "${_ZIG_DIR}/zig-ar"     CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       "${_ZIG_DIR}/zig-ranlib"  CACHE FILEPATH "" FORCE)

# --- Cross-compilation search settings ---
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
