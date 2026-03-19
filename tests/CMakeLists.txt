add_executable(mms_monitor_tests
  test_smoke.cpp
)

target_link_libraries(mms_monitor_tests PRIVATE mms_monitor)

add_test(
  NAME mms_monitor_smoke_test
  COMMAND mms_monitor_tests
)
