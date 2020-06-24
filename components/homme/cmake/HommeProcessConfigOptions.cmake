# This file is responsible for processing the config options, which may trigger additional
# checks, generate/trigger additional variables, add compile definitions

##################################
#   Global compile definitions   #
##################################

# TODO: add_definition is not really the cmake 3.0 style.
#       See if you can use target_compile_definitions when you create execs/libs

# In Kokkos builds, tell Config.hpp that CMake is being used to build.
add_definitions(-DHOMMEXX_CONFIG_IS_CMAKE)

if (BUILD_HOMME_WITHOUT_PIOLIBRARY)
  add_definitions (-DHOMME_WITHOUT_PIOLIBRARY)
  message (STATUS "This configuration builds without PIO library and NetCDF calls")
endif()

##################################
#     Adjust enabled features    #
##################################

if (HOMME_QUAD_PREC) 
  # Check that the compiler supports quadruple precision.
  try_compile(COMPILE_RESULT_VAR
              ${HOMME_BINARY_DIR}/tests/compilerTests/
              ${HOMME_SOURCE_DIR}/cmake/compilerTests/quadTest.f90
              OUTPUT_VARIABLE COMPILE_OUTPUT)

  if (COMPILE_RESULT_VAR)
    message(STATUS "Quadruple-precision enabled, and supported.")
  else ()
    set (HOMME_QUAD_PREC FALSE)
    message(STATUS "Quadruple-precision requested but unavailable on this
                    system with this compiler. Turning it off.")
  endif ()
endif ()

# If OpenMP is turned off also turn off ENABLE_HORIZ_OPENMP
if (NOT ENABLE_OPENMP)
  set(ENABLE_HORIZ_OPENMP FALSE)
  set(ENABLE_COLUMN_OPENMP FALSE)
endif ()


# swim and prim executables require Trilinos compiler
if(BUILD_HOMME_SWIM OR BUILD_HOMME_PRIM)
  set (HOMME_USE_TRILINOS TRUE)
endif ()

# Tf trilinos or kokkos is needed, we need cxx
if (HOMME_USE_TRILINOS OR HOMME_USE_KOKKOS)
  set (HOMME_USE_CXX TRUE)
endif()

# If we need cxx, enable it (Homme doesn't enable CXX by default)
if (HOMME_USE_CXX)
  message (STATUS "This configuration of HOMME requires a C++ compiler")
  enable_language (CXX)

  # Handle Cuda.
  find_package(CUDA QUIET)
  if (CUDA_FOUND)
    string (FIND ${CMAKE_CXX_COMPILER} "nvcc" pos)
    if (${pos} GREATER -1)
      set (CUDA_BUILD TRUE)
    else ()
      message ("Cuda was found, but the C++ compiler is not nvcc_wrapper, so building without Cuda support.")
    endif ()
  endif ()
endif ()

# if kokkos is needed, then so is cxx
if (BUILD_HOMME_PREQX_KOKKOS)
  if (NOT BUILD_HOMME_PREQX AND HOMME_ENABLE_TESTING)
    # If we build preqx kokkos, we also build preqx, so we can compare
    message (STATUS "Setting manually disabled BUILD_HOMME_PREQX to ON, since BUILD_HOMME_PREQX_KOKKOS is ON.")

    # We need to compare against F90 implementation, so turn this on
    set (BUILD_HOMME_PREQX TRUE)
  endif ()
endif ()

if (HOMME_USE_KOKKOS)
  # Possibly build kokkos
  include(Kokkos)

  # Selection of Kokkos execution space
  string (TOUPPER ${HOMMEXX_EXEC_SPACE} HOMMEXX_EXEC_SPACE_UPPER)
  if ("${HOMMEXX_EXEC_SPACE_UPPER}" STREQUAL "CUDA")
    set (HOMMEXX_CUDA_SPACE ON)
  elseif ("${HOMMEXX_EXEC_SPACE_UPPER}" STREQUAL "OPENMP")
    set (HOMMEXX_OPENMP_SPACE ON)
  elseif ("${HOMMEXX_EXEC_SPACE_UPPER}" STREQUAL "THREADS")
    set (HOMMEXX_THREADS_SPACE ON)
  elseif ("${HOMMEXX_EXEC_SPACE_UPPER}" STREQUAL "SERIAL")
    set (HOMMEXX_SERIAL_SPACE ON)
  elseif ("${HOMMEXX_EXEC_SPACE_UPPER}" STREQUAL "DEFAULT")
    set (HOMMEXX_DEFAULT_SPACE ON)
  else()
    message (FATAL_ERROR "Invalid choice for 'HOMMEXX_EXEC_SPACE'. Valid options (case insensitive) are 'Cuda', 'OpenMP', 'Threads', 'Serial', 'Default'")
  endif()

  # Process Hommexx_config.h file
  string (TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
  if (CMAKE_BUILD_TYPE_UPPER MATCHES "DEBUG" OR CMAKE_BUILD_TYPE_UPPER MATCHES "RELWITHDEBINFO")
    set (HOMMEXX_DEBUG ON)
  endif()

  configure_file (${HOMME_SOURCE_DIR}/src/share/cxx/Hommexx_config.h.in
                  ${HOMME_BINARY_DIR}/src/share/cxx/Hommexx_config.h)
endif ()

if (HOMME_BUILD_EXECS AND HOMME_ENABLE_TESTING)
  # Turn on testing, if needed
  enable_testing()
endif()
