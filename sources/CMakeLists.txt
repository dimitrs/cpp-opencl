include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${CLANG_INCLUDE_DIRS}")

add_definitions(${CLANG_DEFINITIONS})

add_subdirectory(CBackend)






