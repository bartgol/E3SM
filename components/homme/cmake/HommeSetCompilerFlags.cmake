##############################################################################
# Compiler specific options
##############################################################################

set(HOMME_Fortran_FLAGS "")

message(STATUS "CMAKE_Fortran_COMPILER_ID = ${CMAKE_Fortran_COMPILER_ID}")
# Need this for a fix in repro_sum_mod
if (${CMAKE_Fortran_COMPILER_ID} STREQUAL XL)
  ADD_DEFINITIONS(-DnoI8)
endif ()

if (DEFINED BASE_FFLAGS)
  set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} ${BASE_FFLAGS}")
else ()
  if (CMAKE_Fortran_COMPILER_ID STREQUAL GNU)
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -ffree-line-length-none")
  elseif (CMAKE_Fortran_COMPILER_ID STREQUAL PGI)
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -Mextend -Mflushz")
    # Needed by csm_share
    ADD_DEFINITIONS(-DCPRPGI)
  elseif (CMAKE_Fortran_COMPILER_ID STREQUAL PathScale)
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -extend-source")
  elseif (CMAKE_Fortran_COMPILER_ID STREQUAL Intel)
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -assume byterecl")
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -fp-model fast -ftz")
    set(HOMME_C_FLAGS "${HOMME_C_FLAGS} -fp-model fast -ftz")
    set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -fp-model fast -ftz")
    #set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -fp-model fast -qopt-report=5 -ftz")
    #set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -mP2OPT_hpo_matrix_opt_framework=0 -fp-model fast -qopt-report=5 -ftz")

    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -diag-disable 8291")

    # remark #8291: Recommended relationship between field width 'W' and the number of fractional digits 'D' in this edit descriptor is 'W>=D+7'.

    # Needed by csm_share
    ADD_DEFINITIONS(-DCPRINTEL)
  elseif (CMAKE_Fortran_COMPILER_ID STREQUAL XL)
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -WF,-C! -qstrict -qnosave")
    # Needed by csm_share
    ADD_DEFINITIONS(-DCPRIBM)
  elseif (CMAKE_Fortran_COMPILER_ID STREQUAL NAG)
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -kind=byte -wmismatch=mpi_send,mpi_recv,mpi_bcast,mpi_allreduce,mpi_reduce,mpi_isend,mpi_irecv,mpi_irsend,mpi_rsend,mpi_gatherv,mpi_gather,mpi_scatterv,mpi_allgather,mpi_alltoallv,mpi_file_read_all,mpi_file_write_all,mpi_file_read_at")
#    set(OPT_FFLAGS "${OPT_FFLAGS} -ieee=full -O2")
    set(DEBUG_FFLAGS "${DEBUG_FFLAGS} -g -time -f2003 -ieee=stop")
    ADD_DEFINITIONS(-DHAVE_F2003_PTR_BND_REMAP)
    # Needed by both PIO and csm_share
    ADD_DEFINITIONS(-DCPRNAG)
  elseif (CMAKE_Fortran_COMPILER_ID STREQUAL Cray)
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -DHAVE_F2003_PTR_BND_REMAP")
    # Needed by csm_share
    ADD_DEFINITIONS(-DCPRCRAY)
 endif ()
endif ()

if (HOMME_USE_CXX)
  if (DEFINED BASE_CPPFLAGS)
    set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} ${BASE_CPPFLAGS}")
  endif ()

  # C++ Flags

  set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -g")

  include (CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG ("-cxxlib" IS_CXXLIB_SUPPORTED)
  set (CXXLIB_SUPPORTED ${IS_CXXLIB_SUPPORTED} CACHE INTERNAL "")
endif()

##############################################################################
# Optimization flags
# 1) OPT_FLAGS if specified sets the Fortran,C, and CXX optimization flags
# 2) OPT_FFLAGS if specified sets the Fortran optimization flags
# 3) OPT_CFLAGS if specified sets the C optimization flags
# 4) OPT_CXXFLAGS if specified sets the CXX optimization flags
##############################################################################
if (OPT_FLAGS)
  # Flags for Fortran C and CXX
  set (HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} ${OPT_FLAGS}")
  set (HOMME_C_FLAGS "${HOMME_C_FLAGS} ${OPT_FLAGS}")
  set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} ${OPT_FLAGS}")
else ()

  if (OPT_FFLAGS)
    # User specified optimization flags
    set (HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} ${OPT_FFLAGS}")
  else ()
    # Defaults
    if (CMAKE_Fortran_COMPILER_ID STREQUAL GNU)
      set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -O3")
    elseif (CMAKE_Fortran_COMPILER_ID STREQUAL PGI)
      set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -O2")
    elseif (CMAKE_Fortran_COMPILER_ID STREQUAL PathScale)
    elseif (CMAKE_Fortran_COMPILER_ID STREQUAL Intel)
      set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -O3")
      #set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -mavx -DTEMP_INTEL_COMPILER_WORKAROUND_001")
    elseif (CMAKE_Fortran_COMPILER_ID STREQUAL XL)
      set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -O2 -qmaxmem=-1")
    elseif (CMAKE_Fortran_COMPILER_ID STREQUAL Cray)
      set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -O2")
    endif ()
  endif ()

  if (OPT_CFLAGS)
    set (HOMME_C_FLAGS "${HOMME_C_FLAGS} ${OPT_CFLAGS}")
  else ()
    if (CMAKE_C_COMPILER_ID STREQUAL GNU)
      set(HOMME_C_FLAGS "${HOMME_C_FLAGS} -O2")
    elseif (CMAKE_C_COMPILER_ID STREQUAL PGI)
    elseif (CMAKE_C_COMPILER_ID STREQUAL PathScale)
    elseif (CMAKE_C_COMPILER_ID STREQUAL Intel)
      set(HOMME_C_FLAGS "${HOMME_C_FLAGS} -O3")
      #set(HOMME_C_FLAGS "${HOMME_C_FLAGS} -mavx -DTEMP_INTEL_COMPILER_WORKAROUND_001")
    elseif (CMAKE_C_COMPILER_ID STREQUAL XL)
      set(HOMME_C_FLAGS "${HOMME_C_FLAGS} -O2 -qmaxmem=-1")
    elseif (CMAKE_C_COMPILER_ID STREQUAL Cray)
      set(HOMME_C_FLAGS "${HOMME_C_FLAGS} -O2")
    endif ()
  endif ()

  if (OPT_CXXFLAGS)
    set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} ${OPT_CXXFLAGS}")
  else ()
    if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
      set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -O3 -DNDEBUG")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL PGI)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL PathScale)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL Intel)
      set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -O3 -DNDEBUG")
      #set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -mavx -DTEMP_INTEL_COMPILER_WORKAROUND_001")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL XL)
      set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -O2 -DNDEBUG -qmaxmem=-1")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL Cray)
      set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -O2 -DNDEBUG")
    endif ()
  endif ()

endif ()

##############################################################################
# DEBUG flags
# 1) DEBUG_FLAGS if specified sets the Fortran,C, and CXX debug flags
# 2) DEBUG_FFLAGS if specified sets the Fortran debugflags
# 3) DEBUG_CFLAGS if specified sets the C debug flags
# 4) DEBUG_CXXFLAGS if specified sets the CXX debug flags
##############################################################################
if (DEBUG_FLAGS)
  set (HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} ${DEBUG_FLAGS}")
  set (HOMME_C_FLAGS "${HOMME_C_FLAGS} ${DEBUG_FLAGS}")
  set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} ${DEBUG_FLAGS}")
else ()
  if (DEBUG_FFLAGS)
    set (HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} ${DEBUG_FFLAGS}")
  else ()
    if(${CMAKE_Fortran_COMPILER_ID} STREQUAL PGI)
      set (HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -gopt")
    elseif(NOT ${CMAKE_Fortran_COMPILER_ID} STREQUAL Cray)
      set (HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -g")
    endif ()
  endif ()

  if (DEBUG_CFLAGS)
    set (HOMME_C_FLAGS "${HOMME_C_FLAGS} ${DEBUG_CFLAGS}")
  else ()
    if(${CMAKE_Fortran_COMPILER_ID} STREQUAL PGI)
      set (HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -gopt")
    else()
      set (HOMME_C_FLAGS "${HOMME_C_FLAGS} -g")
    endif()
  endif ()

  if (DEBUG_CXXFLAGS)
    set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} ${DEBUG_CXXFLAGS}")
  else ()
    if(${CMAKE_Fortran_COMPILER_ID} STREQUAL PGI)
      set (HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} -gopt")
    else()
      set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -g")
    endif ()
  endif ()

endif ()

OPTION(DEBUG_TRACE "Enables TRACE level debugging checks. Very slow" FALSE)
if (${DEBUG_TRACE})
  set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -DDEBUG_TRACE")
endif ()

##############################################################################
# OpenMP
# Two flavors:
#   1) HORIZ_OPENMP OpenMP over elements (standard OPENMP)
#   2) COLUMN_OPENMP OpenMP within an element (previously called ELEMENT_OPENMP)
# COLUMN_OPENMP will be disabled by the openACC exectuables.
#
# Kokkos does not distinguish between the two because it does not use
# nested OpenMP. Nested OpenMP is the reason the two are distinguished in the
# Fortran code.
##############################################################################

if (ENABLE_HORIZ_OPENMP OR ENABLE_COLUMN_OPENMP)
  if(NOT CMAKE_Fortran_COMPILER_ID STREQUAL Cray)
    find_package(OpenMP)
    if(OPENMP_FOUND)
      message(STATUS "Found OpenMP Flags")
      if (CMAKE_Fortran_COMPILER_ID STREQUAL XL)
        set(OpenMP_C_FLAGS "-qsmp=omp")
        if (ENABLE_COLUMN_OPENMP)
          set(OpenMP_C_FLAGS "-qsmp=omp:nested_par -qsuppress=1520-045:1506-793")
        endif ()
      endif ()
      # This file is needed for the timing library - this is currently
      # inaccessible from the timing CMake script

      message(STATUS "OpenMP_Fortran_FLAGS:    ${OpenMP_Fortran_FLAGS}")
      message(STATUS "OpenMP_C_FLAGS:          ${OpenMP_C_FLAGS}")
      message(STATUS "OpenMP_CXX_FLAGS:        ${OpenMP_CXX_FLAGS}")
      message(STATUS "OpenMP_EXE_LINKER_FLAGS: ${OpenMP_EXE_LINKER_FLAGS}")

      # The fortran openmp flag should be the same as the C Flag
      set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} ${OpenMP_C_FLAGS}")
      set(HOMME_C_FLAGS "${HOMME_C_FLAGS} ${OpenMP_C_FLAGS}")
      set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
      set(HOMME_LINKER_FLAGS "${HOMME_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    else ()
      message(FATAL_ERROR "Unable to find OpenMP")
    endif()
  else ()
    message ("USING CRAY? f compiler id: ${CMAKE_Fortran_COMPILER_ID}")
  endif()
  if (ENABLE_HORIZ_OPENMP)
   # Set this as global so it can be picked up by all executables
   set(HORIZ_OPENMP TRUE CACHE INTERNAL "Threading in the horizontal direction")
   message(STATUS "  Using HORIZ_OPENMP")
 endif ()

 if (${ENABLE_COLUMN_OPENMP})
   # Set this as global so it can be picked up by all executables
#   set(COLUMN_OPENMP TRUE CACHE BOOL "Threading in the horizontal direction")
   set(COLUMN_OPENMP TRUE BOOL "Threading in the horizontal direction")
   message(STATUS "  Using COLUMN_OPENMP")
 endif ()
else ()
  message (STATUS "openmp was disabled, man!")
endif ()

##############################################################################
# Intel Phi (MIC) specific flags - only supporting the Intel compiler
##############################################################################

if (ENABLE_INTEL_PHI)
  if (NOT "${CMAKE_Fortran_COMPILER_ID}" STREQUAL Intel)
    message(FATAL_ERROR "Intel Phi acceleration only supported through the Intel compiler")
  else ()
    set(INTEL_PHI_FLAGS "-mmic")

    # TODO: think about changing the line above with the following commented one
    # set(INTEL_PHI_FLAGS "-xMIC-AVX512")
    set(AVX_VERSION "512")
    set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} ${INTEL_PHI_FLAGS}")
    set(HOMME_C_FLAGS "${HOMME_C_FLAGS}  ${INTEL_PHI_FLAGS}")
    set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} ${INTEL_PHI_FLAGS}")

    # CMake magic for cross-compilation
    set (CMAKE_SYSTEM_NAME Linux)
    set (CMAKE_SYSTEM_PROCESSOR k1om)
    set (CMAKE_SYSTEM_VERSION 1)
    set (CMAKE_TOOLCHAIN_PREFIX  x86_64-k1om-linux-)

    # Specify the location of the target environment
    if (TARGET_ROOT_PATH)
      set (CMAKE_FIND_ROOT_PATH ${TARGET_ROOT_PATH})
    else ()
      set (CMAKE_FIND_ROOT_PATH /usr/linux-k1om-4.7)
    endif ()
  endif ()
endif ()

##############################################################################
# Intel MKL flags
##############################################################################

if (HOMME_USE_MKL)
  string (APPEND HOMME_Fortran_FLAGS " -mkl")
  string (APPEND HOMME_C_FLAGS       " -mkl")
  string (APPEND HOMME_CXX_FLAGS     " -mkl")
  string (APPEND HOMME_LINKER_FLAGS  " -mkl")
endif ()

##############################################################################
# Compiler FLAGS for AVX1 and AVX2 (CXX compiler only)
##############################################################################
if (${HOMME_USE_CXX})
  if (NOT DEFINED AVX_VERSION)
    INCLUDE(FindAVX)
    FindAVX()
    if (AVX512_FOUND)
      set(AVX_VERSION "512")
    elseif (AVX2_FOUND)
      set(AVX_VERSION "2")
    elseif (AVX_FOUND)
      set(AVX_VERSION "1")
    else ()
      set(AVX_VERSION "0")
    endif ()
  endif ()

  if (AVX_VERSION STREQUAL "512")
    if (NOT ENABLE_INTEL_PHI)
      if (CMAKE_CXX_COMPILER_ID STREQUAL Intel)
        set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -xCORE-AVX512")
      endif()
    endif()
  elseif (AVX_VERSION STREQUAL "2")
    if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
      set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -mavx2")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL Intel)
      set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -xCORE-AVX2")
    endif()
  elseif (AVX_VERSION STREQUAL "1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
      set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -mavx")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL Intel)
      set (HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} -xAVX")
    endif()
  endif ()
endif ()

##############################################################################
# Allow the option to add compiler flags to those provided
##############################################################################
set(HOMME_Fortran_FLAGS "${HOMME_Fortran_FLAGS} ${ADD_Fortran_FLAGS}")
set(HOMME_C_FLAGS "${HOMME_C_FLAGS} ${ADD_C_FLAGS}")
set(HOMME_CXX_FLAGS "${HOMME_CXX_FLAGS} ${ADD_CXX_FLAGS}")
set(HOMME_LINKER_FLAGS "${HOMME_LINKER_FLAGS} ${ADD_LINKER_FLAGS}")

##############################################################################
# Allow the option to override compiler flags
##############################################################################
if (FORCE_Fortran_FLAGS)
  set(HOMME_Fortran_FLAGS ${FORCE_Fortran_FLAGS})
endif ()
if (FORCE_C_FLAGS)
  set(HOMME_C_FLAGS ${FORCE_C_FLAGS})
endif ()
if (FORCE_CXX_FLAGS)
  set(HOMME_CXX_FLAGS ${FORCE_CXX_FLAGS})
endif ()
if (FORCE_LINKER_FLAGS)
  set(HOMME_LINKER_FLAGS ${FORCE_LINKER_FLAGS})
endif ()
