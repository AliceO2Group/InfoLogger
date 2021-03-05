include(FindDoxygen)

# put doc in a subdirectory with project name, if not already set so
if (NOT CMAKE_INSTALL_DOCDIR)
  set(CMAKE_INSTALL_DOCDIR "doc")
endif()
if (NOT "${CMAKE_INSTALL_DOCDIR}" MATCHES "${PROJECT_NAME}$")
  set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DOCDIR}/${PROJECT_NAME}")
endif()
set(DOC_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_DOCDIR}")

# log doc build(install paths)
message(STATUS "Documentation will be generated in ${DOC_OUTPUT_DIR}")
message(STATUS "Documentation will be installed in ${CMAKE_INSTALL_DOCDIR}")

# general files directly from source
install(
  DIRECTORY . 
  TYPE DOC
  FILES_MATCHING PATTERN "*.md" PATTERN "*.png"
)

# doxygen-generated files
if (DOXYGEN_FOUND)
  if(NOT DOXYGEN_DOT_FOUND)
    message(WARNING "dot not found. Please install Graphviz.")
  endif(NOT DOXYGEN_DOT_FOUND)

  # configure the doxygen config file with current cmake settings
  configure_file(doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/documentation-config.doxygen @ONLY)

  # build documentation with target "doc"
  add_custom_target(
    doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/documentation-config.doxygen
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation using doxygen for ${PROJECT_NAME}"
    VERBATIM
    DEPENDS doc-dir
  )

  # doxygen fails creating multi-level output directory, make sure it exists
  add_custom_target(
    doc-dir
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DOC_OUTPUT_DIR}"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Create the doxygen output directory"
    VERBATIM
  )

  # install generated files
  install(
    DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_DOCDIR}/html
    TYPE DOC
    OPTIONAL # because available only after "make doc"
  )
else (DO)
  message(WARNING "Doxygen not found")
endif (DOXYGEN_FOUND)

