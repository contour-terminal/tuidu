cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Version)
GetVersionInformation(TUIDU_VERSION TUIDU_VERSION_STRING)

project(tuidu VERSION "${TUIDU_VERSION}" LANGUAGES CXX)

include(GNUInstallDirs)


# tuidu requires at least C++23 — it is built coroutine-first and relies on C++23
# library features (std::expected, std::format, std::print, ranges). Enforce it as a
# hard minimum: set the standard, forbid fallback to an older one, and fail
# configuration early (with a clear message) on a toolchain that cannot provide it.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT "cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(FATAL_ERROR
        "tuidu requires a C++23-capable compiler. The detected compiler "
        "(${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}) does not advertise "
        "the 'cxx_std_23' feature. Use a newer compiler (e.g. Clang >= 17, GCC >= 13).")
endif()

# On macOS, ensure the SDK sysroot is set so Homebrew Clang (and clang-scan-deps)
# can find system headers like <time.h>.
if(APPLE AND NOT CMAKE_OSX_SYSROOT)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE _macos_sdk_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_macos_sdk_path)
        set(CMAKE_OSX_SYSROOT "${_macos_sdk_path}")
    endif()
endif()

# Homebrew Clang compiles against its own libc++ headers but links against the
# system libc++ by default, causing undefined symbols for newer APIs (e.g.
# __hash_memory).  Point the linker at Homebrew's libc++ instead.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    get_filename_component(_llvm_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_llvm_prefix "${_llvm_bin_dir}/.." ABSOLUTE)
    set(_llvm_libcxx "${_llvm_prefix}/lib/c++")
    if(EXISTS "${_llvm_libcxx}")
        # Use -nostdlib++ to prevent the Clang driver from implicitly linking
        # the system's libc++/libc++abi.  Then explicitly link Homebrew's
        # versions so that compile-time headers and runtime exception handling
        # use the same library (avoiding ABI mismatches in catch blocks).
        # Also link Homebrew's libunwind so that stack unwinding uses the same
        # runtime as the exception handling (required for catch blocks to work
        # when exceptions cross translation-unit boundaries).
        set(_llvm_libunwind "${_llvm_prefix}/lib/unwind")
        add_link_options(
            "-nostdlib++"
            "-L${_llvm_libcxx}"
            "-Wl,-rpath,${_llvm_libcxx}"
            "-lc++"
            "-lc++abi"
        )
        if(EXISTS "${_llvm_libunwind}")
            add_link_options(
                "-L${_llvm_libunwind}"
                "-Wl,-rpath,${_llvm_libunwind}"
                "-lunwind"
            )
            message(STATUS "Using Homebrew libunwind: ${_llvm_libunwind}")
        endif()
        message(STATUS "Using Homebrew libc++: ${_llvm_libcxx}")
    endif()

    # Prevent CMake from adding -isystem <SDK>/usr/include, which places the
    # macOS SDK's C headers (math.h, string.h, …) ahead of libc++'s own C
    # wrapper headers (cmath, cstring, …).  The SDK's math.h defines signbit,
    # isfinite, isnan as macros that break libc++'s std::signbit usage.
    # The compiler still finds SDK headers via -isysroot.
    if(CMAKE_OSX_SYSROOT)
        list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "${CMAKE_OSX_SYSROOT}/usr/include")
        list(APPEND CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES "${CMAKE_OSX_SYSROOT}/usr/include")
    endif()

    # Disable libc++ availability annotations (e.g. std::from_chars for
    # floating-point being marked macOS 26.0+).  These annotations reflect the
    # *system* libc++ version, but we link against Homebrew's libc++ which
    # already provides these symbols.
    add_compile_definitions(_LIBCPP_DISABLE_AVAILABILITY)
endif()

if(WIN32)
    # Prevent <windows.h> from defining min/max macros that conflict with std::min/std::max
    add_definitions(-DNOMINMAX)
    # Exclude rarely-used Windows headers (prevents winsock.h/winsock2.h conflicts)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    # Target Windows 10+
    add_definitions(-D_WIN32_WINNT=0x0A00)
    # Use Unicode APIs
    add_definitions(-DUNICODE -D_UNICODE)
endif()


# TUIDU_TESTING is the user-facing toggle. The vendored coro/platform/tui/crispy
# sublibraries (copied from the endo project) still gate their test targets on the
# internal ENDO_TESTING / CONTOUR_TESTING variables — we keep those names so the
# vendored CMake applies unchanged (see the project's "core invariant"). Alias the
# user toggle onto them. ENDO_TESTING must be set before include(EndoThirdParties):
# the Catch2 fetch there is guarded by it.
option(TUIDU_TESTING "Build unit tests [default: ON]" ON)
set(ENDO_TESTING ${TUIDU_TESTING})
set(CONTOUR_TESTING ${TUIDU_TESTING})

if(TUIDU_TESTING)
    enable_testing()
endif()

include(EnableCcache)
include(StaticLinking)
include(EndoThirdParties)
include(PedanticCompiler)
include(Sanitizers)
include(ClangTidy)
include(Coverage)

check_static_system_requirements()

if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")
endif()

if(NOT WIN32 AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the build mode." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)
endif()

# Enables STL container checker if not building a release.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_GLIBCXX_ASSERTIONS)
endif()

add_subdirectory(src)

EndoThirdPartiesSummary2()
message(STATUS "==============================================================================")
message(STATUS "          build flags")
message(STATUS "------------------------------------------------------------------------------")
message(STATUS "Version             ${TUIDU_VERSION_STRING}")
message(STATUS "Testing             ${TUIDU_TESTING}")
message(STATUS "Static linking      ${ENABLE_STATIC_LINKING}")
message(STATUS "Address Sanitizer   ${ENABLE_ASAN}")
message(STATUS "UB Sanitizer        ${ENABLE_UBSAN}")
message(STATUS "Thread Sanitizer    ${ENABLE_TSAN}")
message(STATUS "------------------------------------------------------------------------------")
