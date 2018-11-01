set_source_files_properties(src/simplelog.c PROPERTIES LANGUAGE CXX)

add_library(InfoLogger SHARED)

target_sources(InfoLogger
               PRIVATE
               src/InfoLogger.cxx
               src/InfoLoggerClient.cxx
               src/InfoLoggerMessageHelper.cxx
               src/utility.c
               src/transport_server.c
               src/transport_client.c
               src/transport_files.c
               src/simplelog.c
               src/transport_proxy.c
               src/permanentFIFO.c
               src/infoLoggerMessageDecode.c
               src/InfoLoggerDispatch.cxx
               src/InfoLoggerDispatchBrowser.cxx
               src/InfoLoggerMessageList.cxx
               src/ConfigInfoLoggerServer.cxx
               src/InfoLoggerContext.cxx)
target_include_directories(InfoLogger PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
if(MYSQL_FOUND)
  target_sources(InfoLogger PRIVATE src/InfoLoggerDispatchSQL.cxx)
  target_link_libraries(InfoLogger PUBLIC AliceO2::Common mysql::client)
else()
  target_link_libraries(InfoLogger PUBLIC AliceO2::Common)
endif()

configure_file("include/InfoLogger/Version.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/include/InfoLogger/Version.h" @ONLY)

add_subdirectory(doc)

add_executable(infoLoggerServer src/infoLoggerServer.cxx)
target_link_libraries(infoLoggerServer PUBLIC InfoLogger)
add_executable(infoLoggerD src/infoLoggerD.cxx)
target_link_libraries(infoLoggerD PUBLIC InfoLogger)
add_executable(log src/log.cxx)
target_link_libraries(log PUBLIC InfoLogger)

if(MYSQL_FOUND)
  add_executable(infoLoggerAdminDB src/infoLoggerAdminDB.cxx)
  target_link_libraries(infoLoggerAdminDB PUBLIC InfoLogger)
endif()

include(BuildTest.cmake)

# Some extra targets to build standalone executables not relying on other o2
# shared libs in bin-standalone This is added optionally with cmake
# -DBUILD_STANDALONE=1 ...
if(BUILD_STANDALONE)
    include(BuildStandalone.cmake)
endif(BUILD_STANDALONE)

# Install some extra files
install(FILES src/infoBrowser.tcl
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        RENAME infoBrowser
               PERMISSIONS
               OWNER_READ
               OWNER_WRITE
               OWNER_EXECUTE
               GROUP_READ
               GROUP_EXECUTE
               WORLD_READ
               WORLD_EXECUTE)

install(FILES newMysql.sh
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
        GROUP_READ
        GROUP_EXECUTE
        WORLD_READ
        WORLD_EXECUTE)
