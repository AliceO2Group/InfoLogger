# @author  Barthélémy von Haller
set(LIB_HEADERS
  "${CMAKE_CURRENT_BINARY_DIR}/../include/InfoLogger/Version.h" # Generated header
  ../include/InfoLogger/InfoLogger.hxx # Normal header
  ../include/InfoLogger/InfoLogger.h   # Normal header
)

set(LIB_SOURCES
  InfoLogger.cxx
  infoLoggerMessage.c
  utility.c
#  InfoLogger.c
  InfoLoggerClient.cxx
#  SimpleLog.cxx
  InfoLoggerMessageHelper
)

# Define two variables in order not to repeat ourselves.
set(LIBRARY_NAME_SHARED InfoLogger-shared)
set(LIBRARY_NAME_STATIC InfoLogger-static)

# Define the library
add_library(${LIBRARY_NAME_SHARED} SHARED ${LIB_SOURCES})
add_library(${LIBRARY_NAME_STATIC} STATIC ${LIB_SOURCES})

#SimpleLog.cxx Daemon.cxx
add_library(InfoLogger-utils-static STATIC  InfoLoggerMessageHelper.cxx)


# Set the build version. It will be used in the name of the lib, with corresponding
# symlinks created. SOVERSION could also be specified for api version. 
set_target_properties(${LIBRARY_NAME_SHARED} PROPERTIES
  VERSION ${VERSION}  # ${VERSION} was defined in the main CMakeLists.
  FRAMEWORK FALSE
  PUBLIC_HEADER "${LIB_HEADERS}" # specify the public headers
)
set_target_properties(${LIBRARY_NAME_STATIC} PROPERTIES
        VERSION ${VERSION}  # ${VERSION} was defined in the main CMakeLists.
        FRAMEWORK FALSE
        PUBLIC_HEADER "${LIB_HEADERS}" # specify the public headers
        )


  
#add_executable(infoLoggerClient infoLoggerClient.cxx)
#target_link_libraries(infoLoggerClient ${LIBRARY_NAME} InfoLogger-utils-static pthread Common)


set_source_files_properties(simplelog.c PROPERTIES LANGUAGE CXX )


add_library(infoLogger-legacy-utils STATIC utility.c transport_server.c transport_client.c transport_files.c simplelog.c transport_proxy.c permanentFIFO.c infoLoggerMessage.c)

add_executable(infoLoggerServer infoLoggerServer.cxx)
target_link_libraries(infoLoggerServer pthread Common InfoLogger-utils-static infoLogger-legacy-utils)


add_executable(infoLoggerD infoLoggerD.cxx)
target_link_libraries(infoLoggerD pthread Common InfoLogger-utils-static infoLogger-legacy-utils)

add_executable(log log.cxx)
target_link_libraries(log pthread Common InfoLogger-utils-static InfoLogger-static)

  
  
# Says how and where to install software
# Targets:
#   * <prefix>/lib/<libraries>
#   * header location after install: <prefix>/include/<project>/*.h
#   * headers can be included by C++ code `#<project>/Bar.hpp>`
install(TARGETS ${LIBRARY_NAME_SHARED} ${LIBRARY_NAME_STATIC} ${APPLICATION_NAME}
  EXPORT ${PROJECT_NAME}Targets            # for downstream dependencies
  ARCHIVE DESTINATION lib COMPONENT libs   # static lib
  RUNTIME DESTINATION bin COMPONENT libs   # binaries
  LIBRARY DESTINATION lib COMPONENT libs   # shared lib
  FRAMEWORK DESTINATION bin COMPONENT libs # for mac
  PUBLIC_HEADER DESTINATION include/${PROJECT_NAME} COMPONENT devel   # headers for mac (note the different component -> different package)
  INCLUDES DESTINATION include             # headers
)

