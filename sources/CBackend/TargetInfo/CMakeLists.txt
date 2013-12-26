include(GNUInstallDirs)

include_directories(
     ${CMAKE_CURRENT_SOURCE_DIR}/..
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${CLANG_INCLUDE_DIRS}")

add_definitions(${CLANG_DEFINITIONS})

link_directories(
    "/usr/local/lib/"
    "${LLVM_LIB_DIR}"
)

add_library(LLVMCBackendInfo
    CBackendTargetInfo.cpp
)


