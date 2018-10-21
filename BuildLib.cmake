set_source_files_properties(src/simplelog.c PROPERTIES LANGUAGE CXX)

add_library(InfoLogger SHARED
            src/InfoLogger.cxx
            src/InfoLoggerContext.cxx
            src/InfoLoggerClient.cxx
            src/infoLoggerMessageDecode.c
            src/InfoLoggerMessageHelper.cxx
            src/utility.c
            src/simplelog.c)
target_link_libraries(InfoLogger Common::Common)
target_include_directories(InfoLogger PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# Produce the final Version.h using template Version.h.in and substituting
# variables. We don't want to polute our source tree with it, thus putting it in
# binary tree.
configure_file("include/InfoLogger/Version.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/include/InfoLogger/Version.h" @ONLY)

add_subdirectory(doc)

include(BuildTest.cmake)
