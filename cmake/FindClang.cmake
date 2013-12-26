# Copyright (c) 2012 Laszlo Nagy <rizsotto at gmail dot com>

# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:

# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# output:
#   CLANG_FOUND
#   CLANG_INCLUDE_DIRS
#   CLANG_DEFINITIONS
#   CLANG_EXECUTABLE

function(set_clang_definitions config_cmd)
  execute_process(
    COMMAND ${config_cmd} --cppflags
    OUTPUT_VARIABLE llvm_cppflags
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REGEX MATCHALL "(-D[^ ]*)" dflags ${llvm_cppflags})
  string(REGEX MATCHALL "(-U[^ ]*)" uflags ${llvm_cppflags})
  list(APPEND cxxflags ${dflags})
  list(APPEND cxxflags ${uflags})
  list(APPEND cxxflags -fno-rtti)
#  list(APPEND cxxflags -fno-exceptions)

  set(CLANG_DEFINITIONS ${cxxflags} PARENT_SCOPE)
endfunction()

function(is_clang_installed config_cmd)
  execute_process(
    COMMAND ${config_cmd} --includedir
    OUTPUT_VARIABLE include_dirs
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(
    COMMAND ${config_cmd} --src-root
    OUTPUT_VARIABLE llvm_src_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(FIND ${include_dirs} ${llvm_src_dir} result)

  set(CLANG_INSTALLED ${result} PARENT_SCOPE)
endfunction()

function(set_clang_include_dirs config_cmd)
  is_clang_installed(${config_cmd})
  if(CLANG_INSTALLED)
    execute_process(
      COMMAND ${config_cmd} --includedir
      OUTPUT_VARIABLE include_dirs
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  else()
    execute_process(
      COMMAND ${config_cmd} --src-root
      OUTPUT_VARIABLE llvm_src_dir
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
      COMMAND ${config_cmd} --obj-root
      OUTPUT_VARIABLE llvm_obj_dir
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    list(APPEND include_dirs "${llvm_src_dir}/include")
    list(APPEND include_dirs "${llvm_obj_dir}/include")
    list(APPEND include_dirs "${llvm_src_dir}/tools/clang/include")
    list(APPEND include_dirs "${llvm_obj_dir}/tools/clang/include")
  endif()

  set(CLANG_INCLUDE_DIRS ${include_dirs} PARENT_SCOPE)
endfunction()


find_program(LLVM_CONFIG
  NAMES llvm-config-3.2 llvm-config
  PATHS ENV LLVM_PATH)
if(LLVM_CONFIG)
  message(STATUS "llvm-config found : ${LLVM_CONFIG}")
else()
  message(FATAL_ERROR "Can't found program: llvm-config")
endif()

find_program(CLANG_EXECUTABLE
  NAMES clang-3.2 clang
  PATHS ENV LLVM_PATH)
if(CLANG_EXECUTABLE)
  message(STATUS "clang found : ${CLANG_EXECUTABLE}")
else()
  message(FATAL_ERROR "Can't found program: clang")
endif()

set_clang_definitions(${LLVM_CONFIG})
set_clang_include_dirs(${LLVM_CONFIG})

message(STATUS "llvm-config filtered cpp flags : ${CLANG_DEFINITIONS}")
message(STATUS "llvm-config filtered include dirs : ${CLANG_INCLUDE_DIRS}")

set(CLANG_FOUND 1)


MACRO(FIND_AND_ADD_CLANG_LIB _libname_)
find_library(CLANG_${_libname_}_LIB ${_libname_} ${LLVM_LIB_DIR} ${CLANG_LIB_DIR})
if (CLANG_${_libname_}_LIB)
   set(CLANG_LIBS ${CLANG_LIBS} ${CLANG_${_libname_}_LIB})
endif(CLANG_${_libname_}_LIB)
ENDMACRO(FIND_AND_ADD_CLANG_LIB)

set(CLANG_INCLUDE_DIRS ${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIR})
set(CLANG_INCLUDE_DIRS ${CLANG_INCLUDE_DIRS} ${CLANG_INCLUDE_DIR})

FIND_AND_ADD_CLANG_LIB(clangFrontend)
FIND_AND_ADD_CLANG_LIB(clangDriver)
FIND_AND_ADD_CLANG_LIB(clangCodeGen)
FIND_AND_ADD_CLANG_LIB(clangSema)
FIND_AND_ADD_CLANG_LIB(clangChecker)
FIND_AND_ADD_CLANG_LIB(clangAnalysis)
FIND_AND_ADD_CLANG_LIB(clangRewrite)
FIND_AND_ADD_CLANG_LIB(clangAST)
FIND_AND_ADD_CLANG_LIB(clangParse)
FIND_AND_ADD_CLANG_LIB(clangLex)
FIND_AND_ADD_CLANG_LIB(clangBasic)
FIND_AND_ADD_CLANG_LIB(clangARCMigrate)
FIND_AND_ADD_CLANG_LIB(clangEdit)
FIND_AND_ADD_CLANG_LIB(clangFrontendTool)
FIND_AND_ADD_CLANG_LIB(clangRewrite)
FIND_AND_ADD_CLANG_LIB(clangSerialization)
FIND_AND_ADD_CLANG_LIB(clangTooling)
FIND_AND_ADD_CLANG_LIB(clangStaticAnalyzerCheckers)
FIND_AND_ADD_CLANG_LIB(clangStaticAnalyzerCore)
FIND_AND_ADD_CLANG_LIB(clangStaticAnalyzerFrontend)
FIND_AND_ADD_CLANG_LIB(clangSema)
FIND_AND_ADD_CLANG_LIB(clangRewriteCore)

