  include(ExternalProject)
  externalproject_add(Common
                      GIT_REPOSITORY
                      "https://github.com/AliceO2Group/Common.git"
                      GIT_TAG
                      "master"
                      LOG_DOWNLOAD
                      1
                      UPDATE_COMMAND
                      ""
                      PATCH_COMMAND
                      ""
                      BUILD_COMMAND
                      ""
                      CONFIGURE_COMMAND
                      ""
                      TEST_COMMAND
                      ""
                      INSTALL_COMMAND
                      "")
  add_dependencies(InfoLogger Common)

  externalproject_get_property(Common source_dir)
  include_directories(${source_dir}/include)

  foreach(modul Configuration Daemon LineBuffer SimpleLog Thread Timer)
    add_library(objCommon${modul} OBJECT ${source_dir}/src/${modul}.cxx)
    set_source_files_properties(${source_dir}/src/${modul}.cxx
                                PROPERTIES
                                GENERATED
                                TRUE)
    set_target_properties(objCommon${modul}
                          PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_dependencies(objCommon${modul} Common)
  endforeach(modul)

  add_library(objInfoLoggerClient
              OBJECT
              src/InfoLogger.cxx
              src/InfoLoggerContext.cxx
              src/InfoLoggerClient.cxx
              src/infoLoggerMessageDecode.c
              src/InfoLoggerMessageHelper.cxx
              src/utility.c
              src/simplelog.c)
  set_target_properties(objInfoLoggerClient
                        PROPERTIES POSITION_INDEPENDENT_CODE ON)

  set(INFOLOGGER_STANDALONE_OBJ
      $<TARGET_OBJECTS:objCommonConfiguration>
      $<TARGET_OBJECTS:objCommonSimpleLog>
      $<TARGET_OBJECTS:objInfoLoggerClient>)
  add_library(InfoLogger-standalone SHARED ${INFOLOGGER_STANDALONE_OBJ})
  set_target_properties(InfoLogger-standalone
                        PROPERTIES POSITION_INDEPENDENT_CODE ON)

  add_library(InfoLogger-standalone-static STATIC ${INFOLOGGER_STANDALONE_OBJ})
  set_target_properties(InfoLogger-standalone-static
                        PROPERTIES OUTPUT_NAME
                                   InfoLogger-standalone
                                   POSITION_INDEPENDENT_CODE
                                   ON)

  add_library(objInfoLoggerTransport
              OBJECT
              src/permanentFIFO.c
              src/simplelog.c
              src/transport_client.c
              src/transport_files.c
              src/transport_proxy.c
              src/transport_server.c
              src/utility.c)
  set_target_properties(objInfoLoggerTransport
                        PROPERTIES POSITION_INDEPENDENT_CODE ON)

  add_executable(infoLoggerD-s
                 src/infoLoggerD.cxx
                 $<TARGET_OBJECTS:objInfoLoggerTransport>
                 $<TARGET_OBJECTS:objCommonConfiguration>
                 $<TARGET_OBJECTS:objCommonSimpleLog>
                 $<TARGET_OBJECTS:objCommonDaemon>)
  target_link_libraries(infoLoggerD-s pthread)

  # Library for Python
  if(PYTHONLIBS_FOUND)
    add_library(infoLoggerForPython
                SHARED
                src/infoLoggerForPython_wrap.cxx
                ${INFOLOGGER_STANDALONE_OBJ})
    target_include_directories(infoLoggerForPython
                               PUBLIC ${PYTHON_INCLUDE_PATH})
    set_target_properties(infoLoggerForPython PROPERTIES PREFIX "_")
  endif()

  # Library for Tcl
  if(TCL_FOUND)
    add_library(infoLoggerForTcl
                SHARED
                src/infoLoggerForTcl_wrap.cxx
                ${INFOLOGGER_STANDALONE_OBJ})
    target_include_directories(infoLoggerForTcl PUBLIC ${TCL_INCLUDE_PATH})
  endif()

  add_executable(infoLoggerServer-s
                 src/infoLoggerServer.cxx
                 $<TARGET_OBJECTS:objInfoLoggerTransport>
                 $<TARGET_OBJECTS:objCommonConfiguration>
                 $<TARGET_OBJECTS:objCommonSimpleLog>
                 $<TARGET_OBJECTS:objCommonDaemon>
                 $<TARGET_OBJECTS:objCommonThread>
                 src/InfoLoggerDispatch.cxx
                 src/ConfigInfoLoggerServer.cxx
                 src/InfoLoggerDispatchSQL.cxx
                 src/InfoLoggerDispatchBrowser.cxx
                 src/infoLoggerMessageDecode.c
                 src/InfoLoggerMessageHelper.cxx
                 src/InfoLoggerMessageList.cxx)
  target_include_directories(infoLoggerServer-s PUBLIC ${MYSQL_INCLUDE_DIRS})
  target_link_libraries(infoLoggerServer-s pthread ${MYSQL_LIBRARIES})
  add_executable(infoLoggerAdminDB-s
                 $<TARGET_OBJECTS:objCommonConfiguration>
                 $<TARGET_OBJECTS:objCommonSimpleLog>
                 src/infoLoggerAdminDB.cxx)
  target_include_directories(infoLoggerAdminDB-s PUBLIC ${MYSQL_INCLUDE_DIRS})
  target_link_libraries(infoLoggerAdminDB-s ${MYSQL_LIBRARIES})
  add_executable(log-s src/log.cxx $<TARGET_OBJECTS:objCommonLineBuffer>)
  target_link_libraries(log-s InfoLogger-standalone-static)
  set_target_properties(infoLoggerD-s
                        infoLoggerServer-s
                        infoLoggerAdminDB-s
                        log-s
                        PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                   "${CMAKE_BINARY_DIR}/bin-standalone")
  set_target_properties(infoLoggerD-s PROPERTIES OUTPUT_NAME infoLoggerD)
  set_target_properties(infoLoggerServer-s
                        PROPERTIES OUTPUT_NAME infoLoggerServer)
  set_target_properties(log-s PROPERTIES OUTPUT_NAME log)
  set_target_properties(infoLoggerAdminDB-s
                        PROPERTIES OUTPUT_NAME infoLoggerAdminDB)
