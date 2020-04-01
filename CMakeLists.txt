cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0057 NEW)

# Add local cmake dir to cmake search path (for cmake-files only)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set (CASEROOT   "" CACHE STRING "The case we are building")
set (COMPSET    "" CACHE STRING "The compset that we are running")
set (RESOLUTION "" CACHE STRING "The resolution we are running")
set (MACHINE    "" CACHE STRING "The machine on which we are running")
set (COMPILER   "" CACHE STRING "The compiler suite to be used")

# Check that the above cache vars are valid
CheckUserInputs (CASEROOT COMPSET RESOLUTION MACHINE COMPILER)

# Set compilers
SetCompilers (MACHINE COMPILER)

project (E3SM C Fortran CXX)

# Retrieve components list needed
GetCompsetComponents(COMPSET COMP_LIST)

# Build the components
foreach (COMP IN COMP_LIST)
  add_subdirectory (components/${COMP})
endforeach()

# Build e3sm exe
add_subdirectory(cime/src/drivers/mct/main)
