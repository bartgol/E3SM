# This file is responsible to make sure that all requested/needed TPLs are found

# This F-C interface header is needed by gptl and pio (but not scorpio)
include (FortranCInterface)
include_directories (${CMAKE_CURRENT_BINARY_DIR})
FortranCInterface_HEADER(cmake_fortran_c_interface.h
                         MACRO_NAMESPACE  "FCI_")

##################################
#        BLAS/LAPACK/MKL         #
##################################

# TODO: alternatively, set BLA_VENDOR=Intel10_64lp, then simply do find_package(BLAS)
#       and find_package(LAPACK). Drawback: this way, if MKLROOT is not set, we would
#       fall back on any blas/lapack found in LD_LIBRARY_PATH, even if not MKL.

if (HOMME_FIND_BLASLAPACK)
  find_package (BLAS REQUIRED)
  find_package (LAPACK REQUIRED)
elseif (HOMME_USE_MKL)
  # Check we are using Intel, otherwise this makes no sense
  if (NOT "${CMAKE_Fortran_COMPILER_ID}" STREQUAL "Intel")
    message (FATAL_ERROR "Error! MKL is only available with Intel compilers.")
  endif ()
else ()
  add_subdirectory (${HOMME_BLAS_SRC_DIR}   ${HOMME_BINARY_DIR}/libs/blas)
  add_subdirectory (${HOMME_LAPACK_SRC_DIR} ${HOMME_BINARY_DIR}/libs/lapack)
endif ()

##################################
#   Zoltan partitioning library  #
##################################

if (ZOLTAN_DIR)
  message (STATUS "Building with Zoltan")
  find_package (Zoltan REQUIRED)
  set (HAVE_ZOLTAN TRUE)
endif ()

##################################
#         ExtraE library         #
##################################

# TODO: add some documentation. What is this library?
if (EXTRAE_DIR)
  message (STATUS "Building with Extrae")
  find_package (Extrae REQUIRED)
  set (HAVE_EXTRAE TRUE)
endif ()

#########################################
#  SUNDIALS/ARKODE for time-integration #
#########################################

if (SUNDIALS_DIR)
  # TODO: these are GLOBAL commands (this folder and below)
  #       Replace with target-specific commands
  link_directories (${SUNDIALS_DIR}/lib)
  include_directories (${SUNDIALS_DIR}/include)
endif ()

# Option to use ARKode package from SUNDIALS
if (HOMME_USE_ARKODE)
  message(STATUS "Using ARKode, adding -DARKODE")
  # TODO: get rid of this 'add_definition', and replace with cmakedefine logic in the config.h.cmake.in files
  add_definitions(-DARKODE)
endif ()

if (HOMME_USE_TRILINOS)
  message (STATUS "This configuration of requires Trilinos")
  find_package (Trilinos REQUIRED)
  set (HAVE_TRILINOS TRUE)

  if("${Trilinos_PACKAGE_LIST}" MATCHES ".*Zoltan2*.")
    message (STATUS "Trilinos is compiled with Zoltan2 Trilinos_PACKAGE_LIST:${Trilinos_PACKAGE_LIST}")
    message (STATUS "ENABLING ZOLTAN2")
    set (TRILINOS_HAVE_ZOLTAN2 TRUE)
  endif ()
endif ()

###############################
#     GPTL timing library     #
###############################

add_subdirectory (utils/cime/src/share/timing)

# This is required in timing to set HAVE_MPI
target_compile_definitions (timing PUBLIC SPMD)

# Additional optional pre-processor definitions
if (ENABLE_NANOTIMERS)
  target_compile_definitions (timing PUBLIC HAVE_NANOTIME)
  if (USE_BIT64)
    target_compile_definitions (timing PUBLIC BIT64)
  endif ()
endif ()

###############################
#           Scorpio           #
###############################

# set search paths for SCORPIO's findNetCDF
# HOMME machine files set NETCDF_DIR, PNETCDF_DIR which we copy
# to variables used by SCORPIO.  Newer machine files sould direclty set
# necessary SCORPIO variables:

# TODO: I think this CPP definition was used by pio1, but not by scorpio/scorpio_classic
# add_definitions(-D_NO_MPI_RSEND)

if(NOT BUILD_HOMME_WITHOUT_PIOLIBRARY)
  set (NetCDF_PATH ${NETCDF_DIR})
  set (PnetCDF_PATH ${PNETCDF_DIR})

  message("** Configuring SCORPIO with:")
  message("-- NetCDF_PATH = ${NetCDF_PATH}")
  message("-- PnetCDF_PATH = ${PnetCDF_PATH}")
  message("-- NetCDF_Fortran_PATH = ${NetCDF_Fortran_PATH}")
  message("-- NetCDF_C_PATH = ${NetCDF_C_PATH}")
  message("-- PnetCDF_Fortran_PATH = ${PnetCDF_Fortran_PATH}")
  message("-- PnetCDF_C_PATH = ${PnetCDF_C_PATH}")

  # pio needs cime/externals/genf90/genf90.pl
  set(GENF90_PATH ${HOMME_SOURCE_DIR}/utils/cime/src/externals/genf90)
  if (HOMME_USE_SCORPIO)
    add_subdirectory(utils/externals/scorpio)
  else ()
    # The default I/O library used in "Scorpio classic"
    add_subdirectory(utils/externals/scorpio_classic)
  endif ()
endif()

###############################
#           CPRNC             #
###############################

# If the location of a pre-built CPRNC binary is given, use that.
# Otherwise, **if PIO is built**, build it from source.
if (CPRNC_DIR)
  # location of CPRNC binary passed in from CIME or specified in machine file. skip build
  find_program(CPRNC_BINARY cprnc ${CPRNC_DIR})
  if ( CPRNC_BINARY )
     add_executable( cprnc IMPORTED)
     message("-- CPRNC_BINARY = ${CPRNC_BINARY}")
  else()
     message(WARNING "cprnc not found in CPRNC_DIR")
     set(CPRNC_DIR "")
  endif ()
elseif (NOT BUILD_HOMME_WITHOUT_PIOLIBRARY)
  # compile CPRNC from CIME source code. Requires CIME support for machine
  message("-- CPRNC_BINARY = will compile from source")
  message("-- If cmake aborts, set CPRNC_DIR to location of external cprnc executable")

  # cprnc's cmake wont search for netcdf, so we have to find it first:
  find_package(NetCDF "4.0" COMPONENTS C Fortran)
  set(NETCDF_LIBRARIES ${NetCDF_Fortran_LIBRARIES})
  set(NETCDF_INCLUDE_DIR ${NetCDF_Fortran_INCLUDE_DIRS})

  # needed for CPRNC build system
  set (cprnc_dummy_file "${CMAKE_CURRENT_SOURCE_DIR}/utils/cime/tools/cprnc/Macros.cmake")
  if (NOT EXISTS "${cprnc_dummy_file}")
    file(WRITE "${cprnc_dummy_file}" "#dummy Macros file for non-CIME machines")
  endif ()

  set (CPRNC_INSTALL_DIR ${HOMME_BINARY_DIR}/utils/cime/tools/cprnc)
  set (CPRNC_BINARY ${HOMME_BINARY_DIR}/utils/cime/tools/cprnc/cprnc)
  add_subdirectory(utils/cime/tools/cprnc)
endif ()
