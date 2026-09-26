# EndoThirdParties.cmake (trimmed for tuidu)
#
# Handles third-party dependencies. It first tries to find packages on the system,
# and falls back to CPM if not found. When ENABLE_STATIC_LINKING is ON, dependencies
# are built from source via CPM so static libraries are available.
#
# tuidu needs: Catch2 (tests), core-cpp (the TUI, platform, coroutine and CLI layers), libunicode
# (core::tui's, resolved here so ENABLE_STATIC_LINKING can insist on a static copy), and yaml-cpp
# for the configuration file. core-cpp resolves its remaining dependency, stb, itself.

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
    message(STATUS "libunicode          ${THIRDPARTY_BUILTIN_libunicode}")
    message(STATUS "yaml-cpp            ${THIRDPARTY_BUILTIN_yaml_cpp}")
    message(STATUS "core-cpp            ${THIRDPARTY_BUILTIN_core_cpp}")
    message(STATUS "------------------------------------------------------------------------------")
endmacro()

# ==============================================================================
# Catch2 v3 - Unit testing framework
# ==============================================================================
if(TUIDU_TESTING)
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
# libunicode - Unicode library (unicode::unicode, used by core::tui)
# ==============================================================================
# Resolved here rather than left to core-cpp, which would take a shared system copy even when
# ENABLE_STATIC_LINKING asks for a static one. Keep this at or above the version core-cpp's
# dependency table asks for (0.9.3).
set(LIBUNICODE_REQUIRED_VERSION "0.9.3")
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
            "PEDANTIC_COMPILER OFF"
            "PEDANTIC_COMPILER_WERROR OFF"
            "BUILD_SHARED_LIBS OFF"
        EXCLUDE_FROM_ALL YES
        SYSTEM YES
    )
    set(THIRDPARTY_BUILTIN_libunicode "CPM (v${LIBUNICODE_REQUIRED_VERSION}, static)")
endif()

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

# ==============================================================================
# core-cpp - the shared foundation of the Contour Terminal projects
# ==============================================================================
# core::tui, core::platform, core::async, core::net and core::cli, plus core::testing for the
# tests. Declared last, so libunicode above is the parent's target core-cpp resolves its own
# dependency from, and one copy is built. TLS stays off: tuidu has no network transport, so it
# links no OpenSSL. CPM_core-cpp_SOURCE points it at a local checkout.
CPMAddPackage(
    NAME core-cpp
    GITHUB_REPOSITORY contour-terminal/core-cpp
    GIT_TAG v0.5.0
    VERSION 0.5.0
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
        "CORE_CPP_TESTING OFF"
        "CORE_CPP_WITH_TLS OFF"
)
set(THIRDPARTY_BUILTIN_core_cpp "CPM (v0.5.0)")
