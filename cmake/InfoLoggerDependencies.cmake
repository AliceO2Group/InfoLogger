find_package(Boost COMPONENTS unit_test_framework program_options REQUIRED)
find_package(Git QUIET)

o2_define_bucket(
  NAME
  o2_infologger_bucket

  DEPENDENCIES
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  pthread
  Common

  SYSTEMINCLUDE_DIRECTORIES
  ${Boost_INCLUDE_DIRS}
)

