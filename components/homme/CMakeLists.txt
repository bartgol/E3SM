# Best practice to put cmake required version at the top
cmake_minimum_required(VERSION 3.5)

# Check if we are built inside another project.
# We use this to set defaults for a few vars.

if (NOT DEFINED PROJECT_NAME)
  # Print the sha of the last commit,
  # useful to double check which version was build in the build folder.
  execute_process (COMMAND git rev-parse HEAD
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                   OUTPUT_VARIABLE LAST_GIT_COMMIT_SHA
                   OUTPUT_STRIP_TRAILING_WHITESPACE)
  set (LAST_GIT_COMMIT_SHA ${LAST_GIT_COMMIT_SHA} CACHE INTERNAL "The sha of the last git commit.")
  message (STATUS "The sha of the last commit is ${LAST_GIT_COMMIT_SHA}")

  # Build executables/tests
  set (HOMME_BUILD_EXECS_DEFAULT TRUE)
  set (HOMME_BUILD_LIBS_DEFAULT  FALSE)
else ()
  # Do not build executables/tests
  set (HOMME_BUILD_EXECS_DEFAULT FALSE)
  set (HOMME_BUILD_LIBS_DEFAULT  TRUE)
endif ()

# Note: it is perfectly fine to have a project inside another project.
project(HOMME C Fortran)

# Add Homme's cmake subfolder to cmake search path (to get Homme's cmake scripts)
list (APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Declare all possible homme config options in one place
include (HommeConfigOptions)

# Process config options (i.e., add cmake logic depending on values of options)
include (HommeProcessConfigOptions)

# Process Homme TPLs
include (HommeProcessTPLs)

# Set up some directories to facilitate finding netcdf and pnetcdf
# include(HommeMacros)

#############################
#  Global compiler options  #
#############################

# The flags set in the following script will be stored in HOMME_<LANG>_FLAGS,
# and will be appended to ALL targets.
include(${HOMME_SOURCE_DIR}/cmake/HommeSetCompilerFlags.cmake)

message(STATUS "Fortran Flags = ${HOMME_Fortran_FLAGS}")
message(STATUS "C Flags       = ${HOMME_C_FLAGS}")
message(STATUS "CXX Flags     = ${HOMME_CXX_FLAGS}")
message(STATUS "Linker Flags  = ${HOMME_EXE_LINKER_FLAGS}")

# CMAKE_CURRENT_SOURCE_DIR == homme
# set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
# set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/utils/cime/src/CMake" ${CMAKE_MODULE_PATH})
# set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/utils/externals/scorpio/cmake" ${CMAKE_MODULE_PATH})

####################################################
#  Add subdirectories, depending on configuration  #
####################################################

if (ENABLE_PREQX_KOKKOS_BFB_TESTS)
  # Add unit tests for C++ code
  add_subdirectory(test/unit_tests)
endif ()

# Add libcompose, consisting of just C++ files, so we can build it just once for
# all exes. It does not depend on compile-time sizes.
if (HOMME_ENABLE_COMPOSE)
  # TODO: replace add_definitions with either target_compile_definitions or
  #       with a cmakedefine inside config.h.cmake.in files
  add_definitions(-DHOMME_ENABLE_COMPOSE)
  set (COMPOSE_LIBRARY "compose")
  add_subdirectory(src/share/compose)
else ()
  message ("-- COMPOSE semi-Lagrangian transport was explicitly disabled")
endif ()

# Keep track of the execs we built. We will use this when creating all tests
set(EXEC_LIST ""  CACHE INTERNAL "List of configured executables")

if (BUILD_HOMME_SWEQX)
  add_subdirectory(src/sweqx)
endif()

if (BUILD_HOMME_PREQX)
  add_subdirectory(src/preqx)
endif()

if (BUILD_HOMME_THETA)
  add_subdirectory(src/theta-l)
endif()

if (BUILD_HOMME_PREQX_ACC)
  add_subdirectory(src/preqx_acc)
endif()

if (BUILD_HOMME_PREQX_KOKKOS)
  add_subdirectory(src/preqx_kokkos)
endif()

if (BUILD_HOMME_SWIM)
  add_subdirectory(src/swim)
endif()

if (BUILD_HOMME_PRIM)
  ADD_SUBDIRECTORY(src/prim)
endif()

if (BUILD_HOMME_TOOL)
  ADD_SUBDIRECTORY(src/tool)
endif()

# Tests and execs only if this is a standalone build
if (HOMME_BUILD_EXECS)
  add_subdirectory(test_execs)
  add_subdirectory(dcmip_tests)
endif()

# Create a "make clean-all" target to delete CMake cache and the cprnc build
add_custom_target(clean-all
   COMMAND ${CMAKE_BUILD_TOOL} clean
   COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clean-cmake-files.cmake"
)

if (HOMME_USE_KOKKOS)
  # Put this last so that tribits.cmake's 'check' target doesn't conflict with Homme's.
  # TODO: this expedient is no longer needed by current kokkos develop, since they
  #       got rid of their check target. By the way, also Homme, being built as part
  #       of a larger project, should avoid common names for targets, such as 'check'.
  build_kokkos_if_needed()
endif ()
