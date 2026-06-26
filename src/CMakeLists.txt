# Auto-fetch crispy from contour-terminal if missing.
# (vtparser is intentionally NOT fetched — nothing in the coro/platform/tui chain uses it.)
if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/crispy")
    find_program(_Python3 NAMES python3 python REQUIRED)
    message(STATUS "Fetching crispy via get_contour_dirs.py ...")
    execute_process(
        COMMAND "${_Python3}" "${CMAKE_SOURCE_DIR}/scripts/get_contour_dirs.py"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _fetch_result
    )
    if(NOT _fetch_result EQUAL 0)
        message(FATAL_ERROR "Failed to fetch crispy (exit ${_fetch_result}). Run scripts/get_contour_dirs.py manually.")
    endif()
endif()

add_subdirectory(coro)
add_subdirectory(crispy)
set_target_properties(crispy-core PROPERTIES SYSTEM TRUE)
add_subdirectory(platform)
add_subdirectory(tui)
add_subdirectory(tuidu)
