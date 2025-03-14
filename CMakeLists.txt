cmake_minimum_required(VERSION 3.18)
project(mlc_backtrace VERSION 0.1.0
  DESCRIPTION "MLC backtrace support library"
  LANGUAGES C CXX)

# Option to use system-installed libbacktrace instead of building the bundled one
option(USE_SYSTEM_LIBBACKTRACE "Use system-installed libbacktrace if available" OFF)

# If not on Windows, we will either find or build libbacktrace
if(NOT WIN32)
  if(USE_SYSTEM_LIBBACKTRACE)
    find_path(LIBBACKTRACE_INCLUDE_DIR backtrace.h) # look for backtrace.h
    find_library(LIBBACKTRACE_LIBRARY NAMES backtrace) # look for libbacktrace.a or .so
    if(LIBBACKTRACE_INCLUDE_DIR AND LIBBACKTRACE_LIBRARY)
      message(STATUS "Found system libbacktrace")
      add_library(libbacktrace STATIC IMPORTED)
      set_target_properties(libbacktrace PROPERTIES
        IMPORTED_LOCATION "${LIBBACKTRACE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBBACKTRACE_INCLUDE_DIR}")
    else()
      message(FATAL_ERROR
        "USE_SYSTEM_LIBBACKTRACE is ON, but libbacktrace was not found on your system.")
    endif()
  else()
    message(STATUS "Using bundled libbacktrace (ExternalProject build)")
    include(ExternalProject)
    set(LIBBACKTRACE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbacktrace")
    set(LIBBACKTRACE_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/libbacktrace")

    file(MAKE_DIRECTORY "${LIBBACKTRACE_BUILD_DIR}/include" "${LIBBACKTRACE_BUILD_DIR}/lib")

    set(_cc "${CMAKE_C_COMPILER}")
    set(_libbt_configure
      "${LIBBACKTRACE_SRC_DIR}/configure"
      "--prefix=${LIBBACKTRACE_BUILD_DIR}"
      "--with-pic"
      "CC=${_cc}"
      "CPP=${_cc} -E"
      "CFLAGS=${CMAKE_C_FLAGS}"
      "LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}"
      "NM=${CMAKE_NM}"
      "STRIP=${CMAKE_STRIP}")

    if(CMAKE_CROSSCOMPILING)
      if(DEFINED MACHINE_NAME)
        list(APPEND _libbt_configure "--host=${MACHINE_NAME}")
      else()
        message(WARNING "Cross-compiling but MACHINE_NAME not set. libbacktrace configure may fail to detect the target correctly.")
      endif()
    endif()

    ExternalProject_Add(project_libbacktrace
      SOURCE_DIR "${LIBBACKTRACE_SRC_DIR}"
      BINARY_DIR "${LIBBACKTRACE_BUILD_DIR}"
      CONFIGURE_COMMAND ${_libbt_configure}
      BUILD_COMMAND make -j
      BUILD_BYPRODUCTS "${LIBBACKTRACE_BUILD_DIR}/lib/libbacktrace.a"
      "${LIBBACKTRACE_BUILD_DIR}/include/backtrace.h"
      INSTALL_COMMAND make install
      LOG_CONFIGURE ON
      LOG_BUILD ON
      LOG_INSTALL ON
      LOG_OUTPUT_ON_FAILURE ON
    )
    set_target_properties(project_libbacktrace PROPERTIES EXCLUDE_FROM_ALL TRUE)
    add_library(libbacktrace OBJECT IMPORTED)
    set_target_properties(libbacktrace PROPERTIES
      IMPORTED_OBJECTS "${LIBBACKTRACE_BUILD_DIR}/lib/libbacktrace.a"
      INTERFACE_INCLUDE_DIRECTORIES "${LIBBACKTRACE_BUILD_DIR}/include")
    add_dependencies(libbacktrace project_libbacktrace)
  endif()
endif()

# Define mlc_backtrace library
add_library(mlc_backtrace STATIC)
target_sources(mlc_backtrace PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/cpp/traceback.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/cpp/traceback_win.cc"
)

target_include_directories(mlc_backtrace
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_definitions(mlc_backtrace PRIVATE MLC_BACKTRACE_EXPORTS)
target_compile_features(mlc_backtrace PUBLIC cxx_std_17)
set_target_properties(mlc_backtrace PROPERTIES CXX_EXTENSIONS OFF)

# Embed libbacktrace into mlc_backtrace if applicable
if(NOT WIN32)
  target_sources(mlc_backtrace PRIVATE
    "${LIBBACKTRACE_BUILD_DIR}/lib/libbacktrace.a")
endif()

# Ensure visibility and symbol export handling
set_target_properties(mlc_backtrace PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  POSITION_INDEPENDENT_CODE ON)

# MacOS and Linux linker options to hide symbols
if(APPLE)
  file(WRITE "${CMAKE_BINARY_DIR}/empty_exports.txt" "")
  target_link_options(mlc_backtrace INTERFACE
    "-Wl,-exported_symbols_list,${CMAKE_BINARY_DIR}/empty_exports.txt")
elseif(UNIX)
  target_link_options(mlc_backtrace INTERFACE "-Wl,--exclude-libs,ALL")
endif()

# Packaging and installation using TheLartians/PackageProject
include(cmake/CPM.cmake)
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.13.0")

packageProject(
  NAME mlc_backtrace
  VERSION ${PROJECT_VERSION}
  NAMESPACE mlc
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include
  COMPATIBILITY SameMajorVersion
  CPACK_GENERATOR "TGZ;ZIP"
  CPACK YES
)
