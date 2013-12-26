cmake_minimum_required(VERSION 2.8)
project(cpp-opencl CXX)

set(CPACK_PACKAGE_NAME "cpp-opencl")
set(CPACK_PACKAGE_CONTACT "Dimitios Christodoulou <dimitri.christodoulou at gmail dot com>")
set(CPACK_PACKAGE_VENDOR ${CPACK_PACKAGE_CONTACT})
set(CPACK_PACKAGE_VERSION "1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++ to OpenCl kernel translation")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_GENERATOR "TGZ;RPM;DEB")
set(CPACK_RPM_PACKAGE_RELEASE "1%{?dist}")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
set(CPACK_RPM_PACKAGE_URL "http://github.com/dimitrs/cpp-opencl")
set(CPACK_RPM_PACKAGE_DESCRIPTION "C++ to OpenCl kernel translation")
include(CPack)
include(GNUInstallDirs)
install(FILES COPYING README.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR})

set(GCC_COMMON_WARNING_FLAGS "-Wall -Wextra -O -g -Wformat=2 -Wfloat-equal -Wno-unused-parameter")
set(C_WARNINGS "${GCC_COMMON_WARNING_FLAGS}")
set(CXX_WARNINGS "${GCC_COMMON_WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_WARNINGS} -g -O0 -std=c++11 ")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/)
Find_Package(LLVM REQUIRED)
Find_Package(Clang REQUIRED)

add_subdirectory(include)
add_subdirectory(sources)
add_subdirectory(tests)

