enable_testing()

foreach(TEST testInfoLogger testInfoLoggerPerf)
  add_executable(${TEST} test/${TEST}.cxx)
  target_link_libraries(${TEST} PRIVATE InfoLogger)
  add_test(${TEST} ${TEST})
endforeach()

