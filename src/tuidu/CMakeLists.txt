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
    Search.cpp
    Columns.cpp
    ColorRules.cpp
    # Scanning
    Scanner.cpp
    ScanWorker.cpp
    # UI: model adapter, theming, app
    DiskUsageModel.cpp
    ThemeController.cpp
    App.cpp
)

target_include_directories(tuidu-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(tuidu-core PUBLIC tui endo-platform endo-coro)
target_compile_features(tuidu-core PUBLIC cxx_std_23)

set_pedantic_compiler_warnings(tuidu-core)
enable_sanitizers(tuidu-core)
enable_clang_tidy(tuidu-core)

add_executable(tuidu main.cpp)
target_link_libraries(tuidu PRIVATE tuidu-core)
target_compile_features(tuidu PRIVATE cxx_std_23)
enable_sanitizers(tuidu)
enable_clang_tidy(tuidu)

# Tests
if(ENDO_TESTING)
    add_executable(test-tuidu
        test_main.cpp
        Tree_test.cpp
        SizeFormat_test.cpp
        SortMode_test.cpp
        Keymap_test.cpp
        Search_test.cpp
        Columns_test.cpp
        ColorRules_test.cpp
        Scanner_test.cpp
        ScanWorker_test.cpp
        ThemeController_test.cpp
        DiskUsageModel_test.cpp
        App_test.cpp
    )
    target_link_libraries(test-tuidu PRIVATE tuidu-core Catch2::Catch2)
    add_test(NAME test-tuidu COMMAND test-tuidu)
    enable_sanitizers(test-tuidu)
    enable_clang_tidy(test-tuidu)
    enable_coverage(test-tuidu)
    enable_coverage(tuidu-core)
    add_coverage_target(test-tuidu)
endif()
