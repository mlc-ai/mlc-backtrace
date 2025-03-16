cmake_minimum_required(VERSION 3.15)
project(mlc_backtrace
  VERSION 0.1.0
  DESCRIPTION "MLC backtrace support"
  LANGUAGES C CXX
)
include(cmake/CPM.cmake)
include(ExternalProject)
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.13.0")

add_library(
  mlc_backtrace-static STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cpp/traceback_win.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/cpp/traceback.cc
)
target_include_directories(mlc_backtrace-static PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(mlc_backtrace-static PRIVATE MLC_BACKTRACE_EXPORTS)
set_target_properties(
  mlc_backtrace-static PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_STANDARD 17
  CXX_EXTENSIONS OFF
  CXX_STANDARD_REQUIRED ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
if(MSVC)
  set_target_properties(
    mlc_backtrace-static
    PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
  )
endif()

# Define target `libbacktrace` if not on Windows
if(NOT WIN32)
  set(_libbacktrace_source ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbacktrace)
  set(_libbacktrace_prefix ${CMAKE_CURRENT_BINARY_DIR}/libbacktrace)
  if(CMAKE_SYSTEM_NAME MATCHES "Darwin" AND (CMAKE_C_COMPILER MATCHES "^/Library" OR CMAKE_C_COMPILER MATCHES "^/Applications"))
    set(_cmake_c_compiler "/usr/bin/cc")
  else()
    set(_cmake_c_compiler "${CMAKE_C_COMPILER}")
  endif()
  file(MAKE_DIRECTORY ${_libbacktrace_prefix}/include)
  file(MAKE_DIRECTORY ${_libbacktrace_prefix}/lib)
  ExternalProject_Add(project_libbacktrace
    PREFIX libbacktrace
    SOURCE_DIR ${_libbacktrace_source}
    BINARY_DIR ${_libbacktrace_prefix}
    CONFIGURE_COMMAND
    ${_libbacktrace_source}/configure
    "--prefix=${_libbacktrace_prefix}"
    "--with-pic"
    "CC=${_cmake_c_compiler}"
    "CPP=${_cmake_c_compiler} -E"
    "CFLAGS=${CMAKE_C_FLAGS}"
    "LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}"
    "NM=${CMAKE_NM}"
    "STRIP=${CMAKE_STRIP}"
    BUILD_COMMAND make -j
    BUILD_BYPRODUCTS ${_libbacktrace_prefix}/lib/libbacktrace.a ${_libbacktrace_prefix}/include/backtrace.h
    INSTALL_DIR ${_libbacktrace_prefix}
    INSTALL_COMMAND make install
    LOG_CONFIGURE ON
    LOG_INSTALL ON
    LOG_BUILD ON
    LOG_OUTPUT_ON_FAILURE ON
  )
  ExternalProject_Add_Step(project_libbacktrace checkout DEPENDERS configure DEPENDEES download)
  set_target_properties(project_libbacktrace PROPERTIES EXCLUDE_FROM_ALL TRUE)
  target_include_directories(mlc_backtrace-static PRIVATE ${_libbacktrace_prefix}/include)
  add_dependencies(mlc_backtrace-static project_libbacktrace)
endif()

if(APPLE)
  # On macOS, use libtool -static to merge the libraries
  execute_process(COMMAND xcrun -sdk ${CMAKE_OSX_SYSROOT} -find libtool
    OUTPUT_VARIABLE _mlc_libtool_val
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(MLC_LIBTOOL ${_mlc_libtool_val} CACHE FILEPATH "Libtool")
  message(STATUS "Using libtool ${MLC_LIBTOOL}")
  add_custom_command(TARGET mlc_backtrace-static POST_BUILD
    COMMAND ${MLC_LIBTOOL} -static -o $<TARGET_FILE:mlc_backtrace-static>.tmp $<TARGET_FILE:mlc_backtrace-static> ${_libbacktrace_prefix}/lib/libbacktrace.a
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mlc_backtrace-static>.tmp $<TARGET_FILE:mlc_backtrace-static>
    COMMAND ${CMAKE_COMMAND} -E remove $<TARGET_FILE:mlc_backtrace-static>.tmp
    COMMENT "Merging libbacktrace.a into libmlc_backtrace-static.a (macOS)..."
  )
elseif(WIN32)
  # On Windows, no merging is performed (libbacktrace not used or not supported)
  # If using MinGW (GNU on Windows) with libbacktrace.a available, you could handle similarly to Linux.
  # message(STATUS "Skipping libbacktrace merge on Windows.")
else() # Linux and other UNIX (not macOS)
  # On Linux/Unix, use ar to extract and merge object files
  add_custom_command(TARGET mlc_backtrace-static POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/backtrace_objs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/backtrace_objs
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/backtrace_objs ${CMAKE_AR} x ${_libbacktrace_prefix}/lib/libbacktrace.a
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/backtrace_objs ${CMAKE_AR} rcs $<TARGET_FILE:mlc_backtrace-static> "${CMAKE_CURRENT_BINARY_DIR}/backtrace_objs/*.o"
    COMMENT "Merging libbacktrace.a into libmlc_backtrace-static.a (Linux/Unix)..."
  )
endif()

packageProject(
  NAME mlc_backtrace-static
  VERSION ${PROJECT_VERSION}
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include
  NAMESPACE mlc
  CPACK YES
)
