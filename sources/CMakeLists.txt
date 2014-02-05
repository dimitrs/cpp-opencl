include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${CLANG_INCLUDE_DIRS}")

add_definitions(${CLANG_DEFINITIONS})

add_subdirectory(CBackend)


set(HEADERS
    ../include/cl.h
    compiler/MainEntry.h
    compiler/BitcodeDisassembler.h
    compiler/Compiler.h
    compiler/Rewriter.h
    compute/ParallelForEach.h
)

set(SOURCES
    compiler/MainEntry.cpp
    compiler/BitcodeDisassembler.cpp
    compiler/Compiler.cpp
    compiler/Rewriter.cpp
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

configure_file(compute/ParallelForEach.h ParallelForEach.h COPYONLY)
configure_file(../include/cl.h cl.h COPYONLY)

add_executable(cpp_opencl ${HEADERS} ${SOURCES} Main.cpp)
target_link_libraries(cpp_opencl ${OPENCL_LIB} ${LIBS} ${LLVM_LIBS_CORE} ${CLANG_LIBS} )






