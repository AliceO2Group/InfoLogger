install(TARGETS InfoLogger EXPORT InfoLoggerTargets LIBRARY DESTINATION lib)

install(DIRECTORY include/ DESTINATION include FILES_MATCHING PATTERN "*.hxx")

install(EXPORT InfoLoggerTargets
        FILE InfoLoggerTargets.cmake
        NAMESPACE AliceO2::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/InfoLogger)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(InfoLoggerConfigVersion.cmake
                                 VERSION ${InfoLogger_VERSION}
                                 COMPATIBILITY SameMajorVersion)

configure_package_config_file(cmake/InfoLoggerConfig.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}/InfoLoggerConfig.cmake
                              INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/InfoLogger)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/InfoLoggerConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/InfoLoggerConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/InfoLogger)
