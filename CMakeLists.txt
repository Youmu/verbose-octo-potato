cmake_minimum_required(VERSION 3.20)

project(
  MMSMonitor
  VERSION 0.1.0
  DESCRIPTION "MMS Monitor"
  LANGUAGES CXX
)

set(MMS_MONITOR_DISPLAY_NAME "MMS Monitor")

option(MMS_MONITOR_BUILD_APP "Build CLI application target" ON)
option(MMS_MONITOR_BUILD_TESTS "Build tests" ON)
option(MMS_MONITOR_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mms_monitor STATIC
  src/mms_monitor.cpp
)
add_library(MMSMonitor::mms_monitor ALIAS mms_monitor)

target_include_directories(mms_monitor
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(mms_monitor PUBLIC cxx_std_20)

if(MMS_MONITOR_ENABLE_WARNINGS)
  if(MSVC)
    target_compile_options(mms_monitor PRIVATE /W4 /permissive-)
  else()
    target_compile_options(mms_monitor PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
  endif()
endif()

if(MMS_MONITOR_BUILD_APP)
  add_executable(mms_monitor_app src/main.cpp)
  target_link_libraries(mms_monitor_app PRIVATE mms_monitor)
endif()

include(CTest)
if(BUILD_TESTING AND MMS_MONITOR_BUILD_TESTS)
  add_subdirectory(tests)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  TARGETS mms_monitor
  EXPORT MMSMonitorTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(MMS_MONITOR_BUILD_APP)
  install(
    TARGETS mms_monitor_app
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/MMSMonitorConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MMSMonitorConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/MMSMonitorConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MMSMonitor"
)

install(
  EXPORT MMSMonitorTargets
  FILE MMSMonitorTargets.cmake
  NAMESPACE MMSMonitor::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MMSMonitor"
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MMSMonitorConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MMSMonitorConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MMSMonitor"
)

set(CPACK_PACKAGE_NAME "${MMS_MONITOR_DISPLAY_NAME}")
set(CPACK_PACKAGE_VENDOR "MMS Monitor Team")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ")
include(CPack)
