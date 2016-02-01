# @author  Barthélémy von Haller
set(LIB_HEADERS
  "${CMAKE_CURRENT_BINARY_DIR}/../include/InfoLogger/Version.h" # Generated header
  ../include/InfoLogger/InfoLogger.hxx # Normal header
  ../include/InfoLogger/InfoLogger.h   # Normal header
)

set(LIB_SOURCES
  InfoLogger.cxx
  InfoLogger.c
)

# Define two variables in order not to repeat ourselves.
set(LIBRARY_NAME_SHARED InfoLogger-shared)
set(LIBRARY_NAME_STATIC InfoLogger-static)

# Define the library
add_library(${LIBRARY_NAME_SHARED} SHARED ${LIB_SOURCES})
add_library(${LIBRARY_NAME_STATIC} STATIC ${LIB_SOURCES})

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

