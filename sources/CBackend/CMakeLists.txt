include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${CLANG_INCLUDE_DIRS}")

SET(HEADERS
    ./CTargetMachine.h
)

add_definitions(${CLANG_DEFINITIONS})

link_directories( 
    "/usr/local/lib/"
    "${LLVM_LIB_DIR}" 
)

add_library(CBackendCodeGen 
    CBackend.cpp
)

add_subdirectory(TargetInfo)
