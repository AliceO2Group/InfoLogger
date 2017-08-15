find_package(Boost COMPONENTS unit_test_framework program_options REQUIRED)
find_package(Git QUIET)
find_package(MySQL)
find_package(Common REQUIRED)

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
        ${Common_LIBRARIES}

        SYSTEMINCLUDE_DIRECTORIES
        ${Boost_INCLUDE_DIRS}
        ${Common_INCLUDE_DIRS}
)

o2_define_bucket(
        NAME
        o2_infologger_bucket_with_mysql

        DEPENDENCIES
        o2_infologger_bucket
        ${MYSQL_LIBRARY}
        ${MYSQL_LIBRARIES}

        SYSTEMINCLUDE_DIRECTORIES
        ${MYSQL_INCLUDE_DIR}
        ${MYSQL_INCLUDE_DIRS}
)
