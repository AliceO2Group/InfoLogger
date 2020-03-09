####################################
# cmake InfoLogger
####################################

# cmake settings
CMAKE_MINIMUM_REQUIRED(VERSION 3.8.0 FATAL_ERROR)
cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})

# define project
project(
  InfoLogger
  DESCRIPTION "O2 Logging library and services"
  LANGUAGES CXX C
)

# global compilation options
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# define directories
# build-time binaries
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
# installation
include(GNUInstallDirs)
set(CMAKE_INSTALL_LIBDIR lib)

# add local modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# add subdirectories
add_subdirectory(doc)

# dependencies to build the InfoLogger components
# NB: cmake/InfoLoggerConfig.cmake defines dependencies for clients using the library
#find_package(Common REQUIRED)
find_package(Boost REQUIRED)
find_package(MySQL)
find_package(SWIG 3.0.12)

# flag to build only client library
#set(INFOLOGGER_BUILD_LIBONLY 1)
if(INFOLOGGER_BUILD_LIBONLY) 
  message("Enabling libInfologger only.") 
else()
  message("Enabling all InfoLogger modules.")
endif()



##################################
# Common-O2
##################################

# reuse locally some of the Common files to embed in standalone library
include(ExternalProject)
externalproject_add (Common-standalone
  GIT_REPOSITORY "https://github.com/AliceO2Group/Common.git"
  GIT_TAG "v1.4.9"
  LOG_DOWNLOAD 1
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  BUILD_COMMAND ""
  CONFIGURE_COMMAND ""
  TEST_COMMAND ""
  INSTALL_COMMAND ""
)
externalproject_get_property(Common-standalone source_dir)
set(COMMON_STANDALONE_SRC_DIR ${source_dir})
set(COMMON_STANDALONE_INCLUDE_DIRS ${COMMON_STANDALONE_SRC_DIR}/include ${Boost_INCLUDE_DIRS})
foreach (modul Configuration Daemon LineBuffer SimpleLog Thread Timer)
  add_library(
    objCommon${modul} OBJECT
    ${COMMON_STANDALONE_SRC_DIR}/src/${modul}.cxx
  )
  set_source_files_properties(${COMMON_STANDALONE_SRC_DIR}/src/${modul}.cxx PROPERTIES GENERATED TRUE)
  target_include_directories(objCommon${modul}
    PRIVATE ${COMMON_STANDALONE_INCLUDE_DIRS}
  )
  add_dependencies(objCommon${modul} Common-standalone)   
endforeach (modul)



# define include dirs
#get_target_property(COMMON_INCLUDE_DIRS AliceO2::Common INTERFACE_INCLUDE_DIRECTORIES)
if(NOT COMMON_INCLUDE_DIRS)
  set(COMMON_INCLUDE_DIRS ${COMMON_STANDALONE_INCLUDE_DIRS})
endif()
set(INFOLOGGER_INCLUDE_DIRS_PUBLIC include)
set(INFOLOGGER_INCLUDE_DIRS_PRIVATE ${COMMON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INFOLOGGER_INCLUDE_DIRS ${INFOLOGGER_INCLUDE_DIRS_PUBLIC} ${INFOLOGGER_INCLUDE_DIRS_PRIVATE})


#####################################
# infoLogger client library
#####################################

# object file: client (to infoLoggerD)
add_library (objInfoLoggerClient OBJECT
  src/InfoLogger.cxx
  src/InfoLoggerScripting.cxx
  src/InfoLoggerContext.cxx
  src/InfoLoggerClient.cxx
  src/infoLoggerMessageDecode.c
  src/InfoLoggerMessageHelper.cxx
  src/utility.c
  src/simplelog.cxx
)
target_include_directories(objInfoLoggerClient
  PRIVATE ${INFOLOGGER_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
)
add_dependencies(objInfoLoggerClient Common-standalone)

# list of object files for the library
set(INFOLOGGER_LIB_OBJECTS
  $<TARGET_OBJECTS:objInfoLoggerClient>
  $<TARGET_OBJECTS:objCommonConfiguration>
  $<TARGET_OBJECTS:objCommonSimpleLog>
  $<TARGET_OBJECTS:objCommonLineBuffer>
)

# shared library
add_library (InfoLogger SHARED
  $<TARGET_OBJECTS:objInfoLoggerClient>  
  ${INFOLOGGER_LIB_OBJECTS}
)
set_target_properties(InfoLogger PROPERTIES
  OUTPUT_NAME InfoLogger
)
target_include_directories(InfoLogger
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${INFOLOGGER_INCLUDE_DIRS_PUBLIC}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(InfoLogger
  PUBLIC
#  AliceO2::Common
  Boost::boost
  pthread
)
install(TARGETS InfoLogger
  EXPORT InfoLoggerTargets
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# export targets for shared library
install(EXPORT InfoLoggerTargets
  FILE InfoLoggerTargets.cmake
  NAMESPACE AliceO2::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/InfoLogger
)
install(FILES cmake/InfoLoggerConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/InfoLogger)

# install include files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/InfoLogger DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


##########################################
# infoLogger - services and other modules
if(NOT INFOLOGGER_BUILD_LIBONLY)
##########################################


# object file: transport (to infoLoggerS)
add_library (objInfoLoggerTransport OBJECT
  src/permanentFIFO.c
  src/simplelog.cxx
  src/transport_client.c
  src/transport_files.c
  src/transport_proxy.c
  src/transport_server.c
  src/utility.c
)
target_include_directories(objInfoLoggerTransport
  PRIVATE ${INFOLOGGER_INCLUDE_DIRS}
)
add_dependencies(objInfoLoggerTransport Common-standalone)

# static library
add_library (libInfoLogger-static STATIC
  ${INFOLOGGER_LIB_OBJECTS}
)
set_target_properties(libInfoLogger-static PROPERTIES
  OUTPUT_NAME InfoLogger
)
target_include_directories(libInfoLogger-static
  INTERFACE
  ${INFOLOGGER_INCLUDE_DIRS_PUBLIC}
)
add_dependencies(libInfoLogger-static Common-standalone)
target_link_libraries(libInfoLogger-static pthread)


# executable: log (command line tool to inject log messages)
add_executable(
  log
  src/log.cxx  
)
target_link_libraries(
  log
  libInfoLogger-static
)
target_include_directories(
  log
  PRIVATE
  ${COMMON_STANDALONE_INCLUDE_DIRS}
)


# executable: infoLoggerD
add_executable(
  infoLoggerD
  src/infoLoggerD.cxx
  $<TARGET_OBJECTS:objInfoLoggerTransport>
  $<TARGET_OBJECTS:objCommonConfiguration>
  $<TARGET_OBJECTS:objCommonSimpleLog>
  $<TARGET_OBJECTS:objCommonDaemon>
)
target_link_libraries(
  infoLoggerD
  pthread        
)
target_include_directories(
  infoLoggerD
  PRIVATE
  ${COMMON_STANDALONE_INCLUDE_DIRS}
)


# executable: infoLoggerServer
if(MYSQL_FOUND)
  add_definitions(-DWITH_MYSQL)
endif()
add_executable(
  infoLoggerServer
  $<TARGET_OBJECTS:objInfoLoggerTransport>
  $<TARGET_OBJECTS:objCommonConfiguration>
  $<TARGET_OBJECTS:objCommonSimpleLog>
  $<TARGET_OBJECTS:objCommonDaemon>
  $<TARGET_OBJECTS:objCommonThread>
  src/infoLoggerServer.cxx
  src/InfoLoggerDispatch.cxx
  src/InfoLoggerDispatchBrowser.cxx
  src/ConfigInfoLoggerServer.cxx  
  src/infoLoggerMessageDecode.c
  src/InfoLoggerMessageHelper.cxx
  src/InfoLoggerMessageList.cxx
  $<$<BOOL:${MYSQL_FOUND}>:src/InfoLoggerDispatchSQL.cxx>
)
target_include_directories(
  infoLoggerServer
  PRIVATE
  ${INFOLOGGER_INCLUDE_DIRS_PUBLIC}
  ${COMMON_STANDALONE_INCLUDE_DIRS}
  ${MYSQL_INCLUDE_DIRS}
)
target_link_libraries(
  infoLoggerServer
  pthread
  ${MYSQL_LIBRARIES}
)


# executable: infoLoggerAdminDB
add_executable(
        infoLoggerAdminDB
        $<TARGET_OBJECTS:objCommonConfiguration>
        $<TARGET_OBJECTS:objCommonSimpleLog>
        src/infoLoggerAdminDB.cxx
)
target_include_directories(
        infoLoggerAdminDB
        PRIVATE
        ${COMMON_STANDALONE_INCLUDE_DIRS}
        ${MYSQL_INCLUDE_DIRS}
)
target_link_libraries(
        infoLoggerAdminDB
        ${MYSQL_LIBRARIES}
)

# generate libraries for other languages with SWIG
if(SWIG_FOUND)
  include(${SWIG_USE_FILE})
  message("Generating extra languages libraries with SWIG")

  set_property(SOURCE src/infoLogger.i PROPERTY CPLUSPLUS ON)

  # bug in UseSwig.cmake, this one does not work, argument not passed to swig command:
  # set_property(SOURCE src/infoLogger.i PROPERTY SWIG_MODULE_NAME infoLoggerModuleName)
  # see https://gitlab.kitware.com/cmake/cmake/issues/18374
  # workaround: add -module option to variable CMAKE_SWIG_FLAGS

  # library for TCL
  find_package(TCL)
  if (TCL_FOUND)
  message("Bindings for Tcl will be generated")
  set(CMAKE_SWIG_FLAGS -module infoLoggerForTcl)
  swig_add_library(infoLoggerForTcl LANGUAGE tcl SOURCES src/infoLogger.i  ${INFOLOGGER_LIB_OBJECTS})
  target_include_directories(${SWIG_MODULE_infoLoggerForTcl_REAL_NAME} PRIVATE ${INFOLOGGER_INCLUDE_DIRS} ${TCL_INCLUDE_PATH})
  target_link_libraries(${SWIG_MODULE_infoLoggerForTcl_REAL_NAME} ${TCL_LIBRARY})
  set_target_properties(${SWIG_MODULE_infoLoggerForTcl_REAL_NAME} PROPERTIES OUTPUT_NAME infoLoggerForTcl)
  install(TARGETS ${SWIG_MODULE_infoLoggerForTcl_REAL_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
  endif()
  
  # library for Python
  find_package(PythonLibs)
  if (PYTHONLIBS_FOUND)
  message("Bindings for Python will be generated")
  set(CMAKE_SWIG_FLAGS -module infoLoggerForPython)
  swig_add_library(infoLoggerForPython LANGUAGE python SOURCES src/infoLogger.i  ${INFOLOGGER_LIB_OBJECTS})
  target_include_directories(${SWIG_MODULE_infoLoggerForPython_REAL_NAME} PRIVATE ${INFOLOGGER_INCLUDE_DIRS} ${PYTHON_INCLUDE_PATH})
  target_link_libraries(${SWIG_MODULE_infoLoggerForPython_REAL_NAME} ${PYTHON_LIBRARIES})
  message("linking with ${PYTHON_LIBRARIES}")
  set_target_properties(${SWIG_MODULE_infoLoggerForPython_REAL_NAME} PROPERTIES OUTPUT_NAME _infoLoggerForPython)
  install(TARGETS ${SWIG_MODULE_infoLoggerForPython_REAL_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/infoLoggerForPython.py DESTINATION ${CMAKE_INSTALL_LIBDIR})
  endif()
  
  #library for Go
  find_program(GO_EXECUTABLE go)
  if (GO_EXECUTABLE)
    message("Bindings for Go will be generated")
    message("Using ${GO_EXECUTABLE}")
    set(GO_PATH ${CMAKE_CURRENT_BINARY_DIR}/go_workspace)
    message("Using GOPATH=${GO_PATH}")
    file(MAKE_DIRECTORY ${GO_PATH})
    file(MAKE_DIRECTORY ${GO_PATH}/src)
    set( ENV{GOPATH} ${GO_PATH})
    set(CMAKE_SWIG_FLAGS -module infoLoggerForGo -cgo -intgosize 64)
    swig_add_library(infoLoggerForGo LANGUAGE go TYPE STATIC SOURCES src/infoLogger.i  ${INFOLOGGER_LIB_OBJECTS})
    target_include_directories(${SWIG_MODULE_infoLoggerForGo_REAL_NAME} PRIVATE ${INFOLOGGER_INCLUDE_DIRS})
    install(TARGETS ${SWIG_MODULE_infoLoggerForGo_REAL_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/infoLoggerForGo.go DESTINATION ${CMAKE_INSTALL_LIBDIR})
  endif()  

  #library for Javascript / node.js
  find_program(NODE_EXECUTABLE node)
  execute_process(COMMAND ${NODE_EXECUTABLE} --version COMMAND tr -d 'v' OUTPUT_VARIABLE NODEJS_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
  get_filename_component(NODE_DIR ${NODE_EXECUTABLE} DIRECTORY)
  find_path(NODEJS_INCLUDE_DIRS NAMES node.h NO_DEFAULT_PATH PATHS ${Nodejs_ROOT} ${Nodejs_ROOT}/include ${NODE_DIR}/../include/node)
  if (NODEJS_INCLUDE_DIRS AND "${NODEJS_VERSION}" VERSION_GREATER_EQUAL "8.9")
    message("Bindings for node.js will be generated")
    message("Using NODEJS_INCLUDE_DIRS=${NODEJS_INCLUDE_DIRS}")
    message("Node.js version ${NODEJS_VERSION}")
    if (${SWIG_VERSION} VERSION_LESS "4.0.0")
      message("SWIG>=4.0.0 needed to generate node.js wrapper, reusing cached one")
      set(SWIG_MODULE_infoLoggerForNodejs_REAL_NAME infoLoggerForNodejs)
      add_library (infoLoggerForNodejs SHARED src/infoLoggerJAVASCRIPT_wrap.cxx ${INFOLOGGER_LIB_OBJECTS})
      target_include_directories(${SWIG_MODULE_infoLoggerForNodejs_REAL_NAME} PRIVATE ${INFOLOGGER_INCLUDE_DIRS} ${NODEJS_INCLUDE_DIRS})
      set_target_properties(${SWIG_MODULE_infoLoggerForNodejs_REAL_NAME} PROPERTIES PREFIX "")
    else()
      set(CMAKE_SWIG_FLAGS -module infoLoggerForNodejs -v8)
      swig_add_library(infoLoggerForNodejs LANGUAGE javascript SOURCES src/infoLogger.i  ${INFOLOGGER_LIB_OBJECTS})
    endif()
    target_include_directories(${SWIG_MODULE_infoLoggerForNodejs_REAL_NAME} PRIVATE ${INFOLOGGER_INCLUDE_DIRS} ${NODEJS_INCLUDE_DIRS})
    target_compile_definitions(${SWIG_MODULE_infoLoggerForNodejs_REAL_NAME} PRIVATE -DBUILDING_NODE_EXTENSION)
    set_target_properties(${SWIG_MODULE_infoLoggerForNodejs_REAL_NAME} PROPERTIES OUTPUT_NAME infoLogger SUFFIX ".node")
    set(no_lookup_compilers "Clang" "AppleClang")
    if ( ${CMAKE_CXX_COMPILER_ID} IN_LIST no_lookup_compilers)
      set_target_properties(${SWIG_MODULE_infoLoggerForNodejs_REAL_NAME} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif()
    install(TARGETS ${SWIG_MODULE_infoLoggerForNodejs_REAL_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
  endif()
endif()


# test programs
message("Generating test executables")
enable_testing()
set(TEST_SRCS
  test/testInfoLogger.c
  test/testInfoLogger.cxx
  test/testInfoLoggerPerf.cxx
)
foreach (f ${TEST_SRCS})
  get_filename_component(test_name ${f} NAME)
  string(REGEX REPLACE ".cxx" "" test_name ${test_name})
  set(exe "${test_name}.exe")
  message ("${exe}")
  add_executable(${exe} ${f} ${INFOLOGGER_LIB_OBJECTS} $<TARGET_OBJECTS:objCommonTimer>)
  target_link_libraries(${exe} InfoLogger)
  target_include_directories(${exe} PRIVATE ${COMMON_STANDALONE_INCLUDE_DIRS} src)
  add_test(NAME ${test_name} COMMAND ${exe})
endforeach()


# define what should be built
# exclude modules requiring optional dependencies not found
if(NOT MYSQL_FOUND)
  message("MySQL not found, some infoLogger modules are disabled for this build")
  set_target_properties(infoLoggerAdminDB PROPERTIES EXCLUDE_FROM_ALL 1)
endif()


# Install
set (INSTALL_TARGETS log infoLoggerD infoLoggerServer infoLoggerAdminDB libInfoLogger-static)

# Install has undefined behavior for properties with EXLUDE_FROM_ALL property set
# Here we want to skip such targets
foreach (t ${INSTALL_TARGETS})
  get_target_property(IS_EXCLUDED ${t} EXCLUDE_FROM_ALL)
  if(NOT ${IS_EXCLUDED})
    install(TARGETS ${t} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  endif()
endforeach()

install(PROGRAMS src/infoBrowser.tcl DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME infoBrowser)
install(PROGRAMS src/infoLoggerTester.tcl DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME infoLoggerTester)
install(PROGRAMS newMysql.sh DESTINATION ${CMAKE_INSTALL_BINDIR})

#####################################
endif()
# (NOT INFOLOGGER_BUILD_LIBONLY)
#####################################
