# EndoThirdParties.cmake (trimmed for tuidu)
#
# Handles third-party dependencies. It first tries to find packages on the system,
# and falls back to CPM if not found. When ENABLE_STATIC_LINKING is ON, dependencies
# are built from source via CPM so static libraries are available.
#
# tuidu needs the subset required by the vendored coro/platform/tui chain:
#   Catch2 (tests), Microsoft.GSL + boxed-cpp + reflection-cpp (crispy::core deps),
#   libunicode (unicode::unicode), stb (stb_image), plus yaml-cpp for the configuration
#   file. The remaining endo-only deps (nlohmann_json, CURL, mbedTLS, llama.cpp) are dropped.

set(CPM_VERSION "0.40.8")
set(CPM_HASH_SUM "78ba32abdf798bc616bab7c73aac32a17bbd7b06ad9e26a6add69de8f3ae4791")

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM_${CPM_VERSION}.cmake")
endif()

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_VERSION}/CPM.cmake
  ${CPM_DOWNLOAD_LOCATION}
  EXPECTED_HASH SHA256=${CPM_HASH_SUM}
)
include(${CPM_DOWNLOAD_LOCATION})

# Helper macro for displaying dependency status
macro(EndoThirdPartiesSummary2)
    message(STATUS "==============================================================================")
    message(STATUS "    tuidu ThirdParties")
    message(STATUS "------------------------------------------------------------------------------")
    message(STATUS "Catch2              ${THIRDPARTY_BUILTIN_Catch2}")
    message(STATUS "GSL                 ${THIRDPARTY_BUILTIN_GSL}")
    message(STATUS "libunicode          ${THIRDPARTY_BUILTIN_libunicode}")
    message(STATUS "boxed-cpp           ${THIRDPARTY_BUILTIN_boxed_cpp}")
    message(STATUS "reflection-cpp      ${THIRDPARTY_BUILTIN_reflection_cpp}")
    message(STATUS "stb                 ${THIRDPARTY_BUILTIN_stb}")
    message(STATUS "yaml-cpp            ${THIRDPARTY_BUILTIN_yaml_cpp}")
    message(STATUS "------------------------------------------------------------------------------")
endmacro()

# ==============================================================================
# Catch2 v3 - Unit testing framework
# ==============================================================================
if(ENDO_TESTING)
    find_package(Catch2 3 QUIET)
    if(TARGET Catch2::Catch2)
        set(THIRDPARTY_BUILTIN_Catch2 "system package")
    else()
        CPMAddPackage(
            NAME Catch2
            VERSION 3.8.0
            GITHUB_REPOSITORY catchorg/Catch2
            EXCLUDE_FROM_ALL YES
            SYSTEM YES
        )
        set(THIRDPARTY_BUILTIN_Catch2 "CPM (v3.8.0)")
    endif()
    # The global -D_UNICODE definition causes Catch2WithMain to define wmain()
    # instead of main(), leading to unresolved symbol errors on MSVC.
    # Undefine _UNICODE for Catch2WithMain so it provides the standard main().
    if(WIN32 AND TARGET Catch2WithMain)
        target_compile_options(Catch2WithMain PRIVATE /U_UNICODE)
    endif()
endif()

# ==============================================================================
# Microsoft GSL - Guidelines Support Library (crispy::core dependency)
# ==============================================================================
if(WIN32)
    find_package(Microsoft.GSL CONFIG QUIET)
else()
    find_package(Microsoft.GSL QUIET)
endif()
if(TARGET Microsoft.GSL::GSL)
    set(THIRDPARTY_BUILTIN_GSL "system package")
else()
    CPMAddPackage(
        NAME GSL
        VERSION 4.1.0
        GITHUB_REPOSITORY microsoft/GSL
        EXCLUDE_FROM_ALL YES
        SYSTEM YES
    )
    set(THIRDPARTY_BUILTIN_GSL "CPM (v4.1.0)")
endif()

# ==============================================================================
# boxed-cpp - Type-safe wrapper library (crispy::core dependency)
# ==============================================================================
find_package(boxed-cpp QUIET)
if(TARGET boxed-cpp::boxed-cpp)
    set(THIRDPARTY_BUILTIN_boxed_cpp "system package")
else()
    CPMAddPackage(
        NAME boxed-cpp
        GITHUB_REPOSITORY contour-terminal/boxed-cpp
        GIT_TAG v1.4.3
        EXCLUDE_FROM_ALL YES
        SYSTEM YES
    )
    set(THIRDPARTY_BUILTIN_boxed_cpp "CPM (v1.4.3)")
endif()

# ==============================================================================
# libunicode - Unicode library (unicode::unicode, used by tui + crispy)
# ==============================================================================
set(LIBUNICODE_REQUIRED_VERSION "0.9.0")
if(NOT ENABLE_STATIC_LINKING)
    find_package(libunicode ${LIBUNICODE_REQUIRED_VERSION} QUIET)
endif()
if(TARGET unicode::unicode OR TARGET unicode::core)
    set(THIRDPARTY_BUILTIN_libunicode "system package")
else()
    CPMAddPackage(
        NAME libunicode
        GITHUB_REPOSITORY contour-terminal/libunicode
        GIT_TAG v${LIBUNICODE_REQUIRED_VERSION}
        OPTIONS
            "LIBUNICODE_TESTING OFF"
            "LIBUNICODE_BENCHMARK OFF"
            "LIBUNICODE_TOOLS OFF"
            "LIBUNICODE_EXAMPLES OFF"
            "BUILD_SHARED_LIBS OFF"
        EXCLUDE_FROM_ALL YES
        SYSTEM YES
    )
    set(THIRDPARTY_BUILTIN_libunicode "CPM (v${LIBUNICODE_REQUIRED_VERSION}, static)")
endif()

# ==============================================================================
# stb - Single-header image libraries (stb_image, stb_image_resize2) for tui
# ==============================================================================
CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG master
    DOWNLOAD_ONLY YES
)
if(stb_ADDED)
    add_library(stb_image INTERFACE)
    target_include_directories(stb_image SYSTEM INTERFACE "${stb_SOURCE_DIR}")
endif()
set(THIRDPARTY_BUILTIN_stb "CPM (master)")

# ==============================================================================
# reflection-cpp - Required by crispy::core
# ==============================================================================
CPMAddPackage(
    NAME reflection-cpp
    GITHUB_REPOSITORY contour-terminal/reflection-cpp
    GIT_TAG v0.4.0
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
)
set(THIRDPARTY_BUILTIN_reflection_cpp "CPM (v0.4.0)")

# ==============================================================================
# yaml-cpp - YAML parser for the tuidu configuration file
# ==============================================================================
find_package(yaml-cpp QUIET)
if(TARGET yaml-cpp::yaml-cpp)
    set(THIRDPARTY_BUILTIN_yaml_cpp "system package")
else()
    CPMAddPackage(
        NAME yaml-cpp
        GITHUB_REPOSITORY jbeder/yaml-cpp
        GIT_TAG 0.8.0
        OPTIONS
            "YAML_CPP_BUILD_TESTS OFF"
            "YAML_CPP_BUILD_TOOLS OFF"
            "YAML_CPP_BUILD_CONTRIB OFF"
            "YAML_CPP_INSTALL OFF"
            "YAML_CPP_FORMAT_SOURCE OFF"
            "BUILD_SHARED_LIBS OFF"
            # yaml-cpp 0.8.0 still declares cmake_minimum_required(VERSION 2.6..3.5),
            # which CMake >= 4.0 rejects; allow it to configure under the old policy.
            "CMAKE_POLICY_VERSION_MINIMUM 3.5"
        EXCLUDE_FROM_ALL YES
        SYSTEM YES
    )
    set(THIRDPARTY_BUILTIN_yaml_cpp "CPM (v0.8.0)")
endif()
