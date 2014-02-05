include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${CLANG_INCLUDE_DIRS}")

add_definitions(${CLANG_DEFINITIONS})

set(KERNEL     ./kernel.cpp)

set(HEADERS
    ../include/cl.h
    ../sources/compiler/MainEntry.h
    ../sources/compiler/BitcodeDisassembler.h
    ../sources/compiler/Compiler.h
    ../sources/compiler/Rewriter.h
    ../sources/compute/ParallelForEach.h
)

set(SOURCES
    ../sources/compiler/MainEntry.cpp
    ../sources/compiler/BitcodeDisassembler.cpp
    ../sources/compiler/Compiler.cpp
    ../sources/compiler/Rewriter.cpp
)


set(CLANG_LIBS
    clangFrontend clangDriver clangCodeGen
    clangSema clangAnalysis clangAST
    clangParse clangLex clangBasic
    clangARCMigrate clangEdit clangFrontendTool
    clangSerialization
    clangTooling clangSema clangRewriteCore)

set(LIBS
    pthread
    dl
    CBackendCodeGen
    LLVMCBackendInfo)

set(OPENCL_LIB OpenCL)

configure_file(kernel.cpp kernel.cpp COPYONLY)
configure_file(../sources/compute/ParallelForEach.h ParallelForEach.h COPYONLY)
configure_file(../include/cl.h cl.h COPYONLY)

add_executable(test_kernel ${HEADERS} ${SOURCES} test_kernel.cpp)
target_link_libraries(test_kernel ${OPENCL_LIB} ${LIBS} ${LLVM_LIBS_CORE} ${CLANG_LIBS} )

add_executable(test_rewriter ${HEADERS} ${SOURCES} test_rewriter.cpp)
target_link_libraries(test_rewriter ${OPENCL_LIB} ${LIBS} ${LLVM_LIBS_CORE} ${CLANG_LIBS} )






