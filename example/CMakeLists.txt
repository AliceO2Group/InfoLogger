# minimal cmake example for InfoLogger library
# typical command:
# cmake3 -DInfoLogger_DIR=path/to/infologger/lib .. 

find_package(InfoLogger)
add_executable(exampleLog exampleLog.cxx)
target_link_libraries(exampleLog AliceO2::InfoLogger)
