# tuidu — the TUI-first `du` application.
#
# tuidu-core holds all the testable application logic (domain model, scanner,
# data-driven tables, components, app wiring). The `tuidu` executable is a thin
# composition root over it, and `test-tuidu` exercises the same library.

add_library(tuidu-core STATIC
    # Domain model
    Tree.cpp
    # Data-driven tables
    SizeFormat.cpp
    SortMode.cpp
    Keymap.cpp
    ChordRecognizer.cpp
    Search.cpp
    Columns.cpp
    ColorRules.cpp
    # Scanning
    Scanner.cpp
    ScanWorker.cpp
    # Deletion
    DeleteWorker.cpp
    # UI: model adapter, theming, help, dialogs, app
    DiskUsageModel.cpp
    ThemeController.cpp
    HelpOverlay.cpp
    DeleteProgressDialog.cpp
    App.cpp
    # Configuration: CLI parsing + YAML config file
    Cli.cpp
    Config.cpp
)

target_include_directories(tuidu-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(tuidu-core PUBLIC tui endo-platform endo-coro)
# yaml-cpp and crispy::cli back the config file and CLI parsing respectively; both are
# implementation details confined to Config.cpp / Cli.cpp, so keep them PRIVATE.
target_link_libraries(tuidu-core PRIVATE yaml-cpp::yaml-cpp crispy::core)
target_compile_definitions(tuidu-core PRIVATE TUIDU_VERSION="${TUIDU_VERSION_STRING}")
target_compile_features(tuidu-core PUBLIC cxx_std_23)

set_pedantic_compiler_warnings(tuidu-core)
enable_sanitizers(tuidu-core)
enable_clang_tidy(tuidu-core)

add_executable(tuidu main.cpp)
# On Windows, embed a manifest that sets the process active code page to UTF-8 so
# std::filesystem narrow conversions are lossless (and never throw on non-ANSI filenames).
# CMake forwards a .manifest in the source list to the linker for MSVC-like toolchains.
if(WIN32)
    target_sources(tuidu PRIVATE tuidu.manifest)
endif()
target_link_libraries(tuidu PRIVATE tuidu-core)
target_compile_features(tuidu PRIVATE cxx_std_23)
enable_sanitizers(tuidu)
enable_clang_tidy(tuidu)
# Produce a fully self-contained executable when ENABLE_STATIC_LINKING=ON (a no-op
# otherwise). Applied to the final executable — the -static link option must sit on
# the binary that pulls in the C/C++ runtimes, not on the intermediate static libs.
enable_static_linking(tuidu)

# Tests
if(ENDO_TESTING)
    add_executable(test-tuidu
        test_main.cpp
        Tree_test.cpp
        SizeFormat_test.cpp
        SortMode_test.cpp
        Keymap_test.cpp
        ChordRecognizer_test.cpp
        Search_test.cpp
        Columns_test.cpp
        ColorRules_test.cpp
        Scanner_test.cpp
        ScanWorker_test.cpp
        DeleteWorker_test.cpp
        ThemeController_test.cpp
        DiskUsageModel_test.cpp
        HelpOverlay_test.cpp
        DeleteProgressDialog_test.cpp
        App_test.cpp
        Cli_test.cpp
        Config_test.cpp
        ConfigPaths_test.cpp
    )
    target_link_libraries(test-tuidu PRIVATE tuidu-core Catch2::Catch2)
    add_test(NAME test-tuidu COMMAND test-tuidu)
    enable_sanitizers(test-tuidu)
    enable_clang_tidy(test-tuidu)
    enable_coverage(test-tuidu)
    enable_coverage(tuidu-core)
    add_coverage_target(test-tuidu)
endif()
