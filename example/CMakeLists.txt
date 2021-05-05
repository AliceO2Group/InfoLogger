# minimal cmake example for InfoLogger library
# typical command:
# cmake -DInfoLogger_DIR=path/to/infologger/lib/cmake/InfoLogger  .. 

project (InfoLoggerExample)
cmake_minimum_required(VERSION 3.17)

find_package(InfoLogger REQUIRED)
set(CMAKE_CXX_STANDARD 14)
add_executable(exampleLog exampleLog.cxx)
target_link_libraries(exampleLog AliceO2::InfoLogger)
