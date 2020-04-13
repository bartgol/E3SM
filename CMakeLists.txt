cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0079 NEW)

# Add local cmake dir to cmake search path (for cmake-files only)
set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  ${CMAKE_SOURCE_DIR}/cmake
)

set (CASEROOT   "./" CACHE STRING "The directory where the case is built")
set (COMPSET    "" CACHE STRING "The compset that we are running")
set (RESOLUTION "" CACHE STRING "The resolution we are running")
set (MACHINE    "" CACHE STRING "The machine on which we are running")
set (COMPILER   "" CACHE STRING "The compiler suite to be used")
set (E3SM_EXE_NAME   "e3sm.exe" CACHE STRING "The compiler suite to be used")
set (PIO_VERSION "0" CACHE STRING "The version of (scor)pio to use. 1 or 2")
set (GENF90 "${CMAKE_SOURCE_DIR}/cime/src/externals/genf90/genf90.pl" CACHE INTERNAL "The genf90 perl script")
option (USE_ESMF_LIB OFF "Whether we have an esmf lib available")

# Check that the above cache vars are valid
include(CheckUserInputs)
CheckUserInputs (${CASEROOT} ${COMPSET} ${RESOLUTION} ${MACHINE} ${COMPILER})

# Set compilers
include(SetCompilers)
SetCompilers (${MACHINE} ${COMPILER})

project (E3SM C Fortran CXX)

# First, process TPLs
include (tpls/ProcessTPLs)
ProcessTPLs ()

# Then, build all cime and external libs that are needed
# This macro will also define SHARED_LIBRARIES and SHARED_INCLUDE_DIRS
include(build_scripts/BuildSharedLibs)
BuildSharedLibs ()

set (COMPONENTS atm glc ice lnd ocn rof wav)

# Retrieve components list needed
include(CheckComponents)
GetActiveComponents(${COMPSET} ACTIVE_COMPONENTS)

# Build the active components
set (E3SM_COMPONENTS_LIBS)
set (E3SM_COMPONENTS_INC_DIRS)
foreach (COMP IN ${ACTIVE_COMPONENTS})
  add_subdirectory (components/${COMP})
endforeach()

include(build_scripts/BuildE3SM)
BuildE3SM()
