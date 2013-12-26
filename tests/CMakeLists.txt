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
)

set(SOURCES
    ../sources/compiler/MainEntry.cpp
    ../sources/compiler/BitcodeDisassembler.cpp
    ../sources/compiler/Compiler.cpp
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

add_executable(test ${HEADERS} ${SOURCES} test.cpp)

target_link_libraries(test ${OPENCL_LIB} ${LIBS} ${LLVM_LIBS_CORE} ${CLANG_LIBS} )




