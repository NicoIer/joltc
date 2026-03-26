cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
project(JoltPhysicsModule CXX)

# C++17 required by JoltPhysics
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11" CACHE STRING "Minimum OS X deployment version")

# ============================================================================
# SWIG
# ============================================================================
find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# ============================================================================
# JoltPhysics (via parent project's FetchContent)
# ============================================================================
include(FetchContent)

# JoltPhysics options
option(DOUBLE_PRECISION "Use double precision math" OFF)
set(OBJECT_LAYER_BITS 32)
option(ENABLE_OBJECT_STREAM "Compile the ObjectStream class and RTTI attribute information" ON)
option(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE "Enable debug renderer" OFF)
option(DEBUG_RENDERER_IN_DISTRIBUTION "Enable debug renderer in all builds" OFF)
option(PROFILER_IN_DEBUG_AND_RELEASE "Enable the profiler" OFF)
option(CPP_RTTI_ENABLED "Enable C++ RTTI" OFF)
option(CPP_EXCEPTIONS_ENABLED "Enable C++ exceptions" OFF)

# Locate JoltPhysics
set(JOLT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JoltPhysics")
if(EXISTS "${JOLT_SOURCE_DIR}/Build/CMakeLists.txt")
    message(STATUS "Using local JoltPhysics from ${JOLT_SOURCE_DIR}")
    FetchContent_Declare(
        JoltPhysics
        SOURCE_DIR "${JOLT_SOURCE_DIR}"
        SOURCE_SUBDIR "Build"
    )
else()
    message(STATUS "Fetching JoltPhysics from GitHub...")
    FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY "https://github.com/jrouwe/JoltPhysics"
        GIT_TAG v5.5.0
        SOURCE_SUBDIR "Build"
    )
endif()
FetchContent_MakeAvailable(JoltPhysics)

# Compiler flags for non-MSVC (match parent project)
if(NOT MSVC)
    if(NOT CPP_RTTI_ENABLED)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
    endif()
    if(NOT CPP_EXCEPTIONS_ENABLED)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
    endif()
endif()

# ============================================================================
# SWIG C# generation
# ============================================================================

set_source_files_properties(joltphysics.i PROPERTIES
    CPLUSPLUS ON
    SWIG_FLAGS "-namespace;JoltPhysics"
)

# Create the SWIG library
swig_add_library(JoltPhysicsModule
    TYPE SHARED
    LANGUAGE csharp
    SOURCES joltphysics.i joltphysics_helpers.cpp
)

# Include paths
target_include_directories(JoltPhysicsModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${joltphysics_SOURCE_DIR}
)

# Link JoltPhysics
target_link_libraries(JoltPhysicsModule PRIVATE Jolt)

# SWIG director code requires C++ exceptions even though JoltPhysics disables them.
# Enable exceptions only for the wrapper compilation unit.
if(NOT MSVC)
    target_compile_options(JoltPhysicsModule PRIVATE -fexceptions)
endif()

# On macOS, set the rpath
if(APPLE)
    set_target_properties(JoltPhysicsModule PROPERTIES
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# ============================================================================
# Copy generated C# files to test directory
# ============================================================================
add_custom_command(TARGET JoltPhysicsModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/test/Generated"
    COMMENT "Created test/Generated/ directory"
)

# ============================================================================
# Print build info
# ============================================================================
message(STATUS "JoltPhysics source: ${joltphysics_SOURCE_DIR}")
