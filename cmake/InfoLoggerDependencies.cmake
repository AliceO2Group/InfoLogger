find_package(Boost COMPONENTS unit_test_framework program_options REQUIRED)
find_package(Git QUIET)
find_package(MySQL)

if(NOT MYSQL_FOUND)
    message(WARNING "MySQL not found, the corresponding classes won't be built.")
else()
    add_definitions(-D_WITH_MYSQL)
endif()


o2_define_bucket(
  NAME
  o2_infologger_bucket

  DEPENDENCIES
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  pthread
  Common
  ${MYSQL_LIBRARY}
  ${MYSQL_LIBRARIES}

  SYSTEMINCLUDE_DIRECTORIES
  ${Boost_INCLUDE_DIRS}
  ${MYSQL_INCLUDE_DIR}
  ${MYSQL_INCLUDE_DIRS}
)

