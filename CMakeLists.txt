cmake_minimum_required(VERSION 3.5.2 FATAL_ERROR)
project(InfoLogger VERSION 1.0.5 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Common CONFIG REQUIRED)
find_package(MySQL)
find_package(PythonLibs)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
set(INCLUDE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/include")
set(CMAKE_INSTALL_LIBDIR lib)

if(NOT MYSQL_FOUND)
  message(WARNING "MySQL not found, the corresponding classes won't be built.")
else()
  add_definitions(-D_WITH_MYSQL)
endif()

# Definition of modules to be built by default, build all if some specific
# modules are specified, build only these ones
set(INFOLOGGER_BUILD_ALL 1)
if(INFOLOGGER_BUILD_LIB)
  set(INFOLOGGER_BUILD_ALL 0)
  message(STATUS "Enabling just libInfologger.")
endif()
if(INFOLOGGER_BUILD_ALL)
    message(STATUS "Enabling all InfoLogger modules.")
  set(INFOLOGGER_BUILD_LIB 0)
endif()

# Set the default build type to "RelWithDebInfo"
if(NOT CMAKE_BUILD_TYPE)
  set(
    CMAKE_BUILD_TYPE "RelWithDebInfo"
    CACHE
      STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel Coverage."
    FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(INFOLOGGER_BUILD_ALL)
    include(BuildAll.cmake)
endif()

if(INFOLOGGER_BUILD_LIB)
    include(BuildLib.cmake)
endif()

include(InstallAndExport.cmake)
