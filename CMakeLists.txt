cmake_minimum_required( VERSION 3.16 )
project( grpy-cpp VERSION 1.0.0 LANGUAGES CXX )

# GRPY is a translation of GRPY.f (Copyright (C) 2017 Pawel Jan Zuk) and is GPLv3.
# See LICENSE and the provenance notes at the top of each header.

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

if( NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES )
   set( CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE )
endif()

# Eigen is header-only and is the only dependency. Point EIGEN_INCLUDE_DIR at a copy
# (UltraScan vendors one at us_somo/develop/include) or install Eigen3 system-wide.
set( EIGEN_INCLUDE_DIR "" CACHE PATH "Directory containing the Eigen/ header directory" )

if( EIGEN_INCLUDE_DIR AND EXISTS "${EIGEN_INCLUDE_DIR}/Eigen/Dense" )
   set( GRPY_EIGEN_INCLUDE "${EIGEN_INCLUDE_DIR}" )
else()
   find_package( Eigen3 3.3 QUIET NO_MODULE )
   if( TARGET Eigen3::Eigen )
      get_target_property( GRPY_EIGEN_INCLUDE Eigen3::Eigen INTERFACE_INCLUDE_DIRECTORIES )
   else()
      message( FATAL_ERROR
               "Eigen was not found. Install Eigen3, or configure with\n"
               "  -DEIGEN_INCLUDE_DIR=/path/to/eigen  (the directory holding Eigen/Dense)" )
   endif()
endif()
message( STATUS "GRPY: using Eigen headers from ${GRPY_EIGEN_INCLUDE}" )

find_package( Threads REQUIRED )

add_executable( grpy src/grpy_main.cpp )
target_include_directories( grpy PRIVATE include/grpy "${GRPY_EIGEN_INCLUDE}" )
target_link_libraries( grpy PRIVATE Threads::Threads )

# The numeric core is memory-bound and the factorization dominates; -O2 is the level every
# published validation of this port was measured at.
target_compile_options( grpy PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O2> )

# The headers are the library: a caller wanting GRPY in-process includes them directly.
add_library( grpy_headers INTERFACE )
target_include_directories( grpy_headers INTERFACE
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/grpy> )

install( TARGETS grpy RUNTIME DESTINATION bin )
install( DIRECTORY include/grpy DESTINATION include )

enable_testing()
add_test( NAME goldens
          COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh $<TARGET_FILE:grpy> )
