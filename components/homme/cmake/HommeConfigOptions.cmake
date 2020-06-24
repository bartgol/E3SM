# Here's a list of cmake configuration options that the user can set

# NOTE: here we only DECLARE options and their defaults. We do not perform any action depending
#       on the values of these options, except possibly exposing other options and setting some
#       internal variable needed to expose further options

##########################
#    Standard Options    #
##########################

# These options are always available and processed by CMake

# What kind of targets should be built (libs and/or execs)
option (HOMME_BUILD_EXECS ${HOMME_BUILD_EXECS_DEFAULT} "If ON, executables (and tests) will be built.")
option (HOMME_BUILD_LIBS  ${HOMME_BUILD_LIBS_DEFAULT} "If ON, build a library for each enabled target (preqx, preqx_kokkos, ...).")

# What equation should be built (shallow water, hydrostatic, theta, ...)
option (BUILD_HOMME_SWEQX        "Shallow water equations FEM"                  ON)
option (BUILD_HOMME_PREQX        "Primitive equations FEM"                      ON)
option (BUILD_HOMME_THETA        "SB81 NH model with theta and Exner pressure"  ON)
option (BUILD_HOMME_PREQX_ACC    "Primitive equations FEM with OpenACC"         ON)
option (BUILD_HOMME_PREQX_KOKKOS "Primitive equations FEM with Kokkos"          OFF)
option (BUILD_HOMME_SWIM         "Shallow water equations implicit"             OFF)
option (BUILD_HOMME_PRIM         "Primitive equations implicit"                 OFF)

# Additional targets that homme can build
option (BUILD_HOMME_TOOL  "Offline tool" ON)
option (HOMME_ENABLE_COMPOSE "Build COMPOSE semi-Lagrangian tracer transport code" ON)

# Whether homme can use quadruple precision in certain calculation
# If FALSE, all occurrences of longdouble will be resolved as double's
option (HOMME_QUAD_PREC "Default to try to use quadruple precision" TRUE)

# Build/compile options
option (ENABLE_INTEL_PHI      "Whether to build with Intel Xeon Phi (MIC) support." FALSE)
option (ENABLE_OPENMP         "OpenMP across elements"   TRUE)
option (ENABLE_HORIZ_OPENMP   "OpenMP across elements"   TRUE)
option (ENABLE_COLUMN_OPENMP  "OpenMP within an element" FALSE)


# Tpl options
option (PREFER_SHARED "Prefer linking with shared libraries." FALSE)

option (HOMME_USE_SCORPIO  "Use Scorpio as the I/O library (the default I/O library used is \"Scorpio classic\")" OFF)
option (BUILD_HOMME_WITHOUT_PIOLIBRARY "Option to build without pio and any netcdf in homme (no mesh runs either)" FALSE)
option (HOMME_USE_MKL "Whether to use Intel's MKL instead of blas/lapack. Ignored if compiler is not Intel" FALSE)


# Optional library to be linked against, for profiling purposes (e.g., VTUNE)
set (PERFORMANCE_PROFILE "" CACHE STRING "Whether to build and link with various profiler libraries")

# Optional tpls
set (ZOLTAN_DIR   "" CACHE STRING "Path to zoltan installation.")
set (ZOLTAN_DIR   "" CACHE STRING "Path to zoltan installation.")
set (SUNDIALS_DIR "" CACHE STRING "Path to sundials installation.")
set (CPRNC_DIR    "" CACHE STRING "Path to already existing cprnc build.")

# Options for the GPTL library
option (ENABLE_NANOTIMERS "Use nano timers in timing library" FALSE)
option (USE_BIT64 "Set BIT64 (for 64 bit arch) in timing library when ENABLE_NANOTIME" FALSE)

#############################
#    Conditional Options    #
#############################

# These are options that are only processed if a condition on other options is met
# The syntax of cmake_dependent_option is
#  cmake_dependent_option (opt_name "description" default_if_true "conditions" value_if_false)
# where conditions is a semi-colon separated list like "VAR1; NOT VAR2; VAR3"

include (CMakeDependentOption)

# We can build execs without configuring all the tests. By default, if execs are built, testing is on.
cmake_dependent_option (HOMME_ENABLE_TESTING
                        "Whether to create tests" ON "HOMME_BUILD_EXECS" OFF)

# Whether we enable BF tests for preqx target
cmake_dependent_option (ENABLE_PREQX_KOKKOS_BFB_TESTS
                        "Turn on unit tests for XX and bfb for F vs XX" OFF
                        "HOMME_ENABLE_TESTING; BUILD_HOMME_PREQX_KOKKOS" OFF)

# Whether we print summary info about the tests
cmake_dependent_option (TEST_SUMMARY
                        "Print out information about the tests" OFF
                        "HOMME_ENABLE_TESTING" OFF)

# Wheter homme should look for a system blas/lapack library
cmake_dependent_option (HOMME_FIND_BLASLAPACK
                        "Whether to use system blas/lapack" FALSE
                        "NOT HOMME_USE_MKL" FALSE)

if (NOT HOMME_USE_MKL AND NOT HOMME_FIND_BLASLAPACK)
  set (HOMME_BLAS_SRC_DIR "libs/blas" CACHE STRING "Location of blas source code, to build as part of Homme.")
  set (HOMME_LAPACK_SRC_DIR "libs/lapack" CACHE STRING "Location of blas source code, to build as part of Homme.")
endif ()

# CMake does not have an equivalent to cmake_dependent_option for standard cache variables
if (HOMME_ENABLE_TESTING)
  # Note: the 'set_property' command has zero impact on cmake logic; cmake does not perform
  #       any check to ensure the value of the variable is in the given list of string.
  #       However, if someone is using a cmake gui, this can help identifying the possible options.
  set (HOMME_TESTING_PROFILE "nightly" CACHE STRING
       "Determine how long/pervasive the testing is. Currently available options: 'dev', 'short', 'nightly'.")
  set_property (CACHE HOMME_TESTING_PROFILE PROPERTY STRINGS "dev;short;nightly")
endif ()

# With the theta model, we have the option to use the ARKode package for time integration
cmake_dependent_option (HOMME_USE_ARKODE
                        "Use ARKode package from SUNDIALS" FALSE
                        "BUILD_HOMME_THETA" FALSE)

# If using scorpio, no need to build the scorpio tools.
cmake_dependent_option (PIO_ENABLE_TOOLS
                        "Disabling Scorpio tool build" OFF
                        "HOMME_USE_SCORPIO" OFF)

# Compose and preqx_kokkos require kokkos. Everything else doesn't.
if (BUILD_HOMME_PREQX_KOKKOS OR HOMME_ENABLE_COMPOSE)
  set (HOMME_USE_KOKKOS ON)
else ()
  set (HOMME_USE_KOKKOS OFF)
endif ()

if (HOMME_USE_KOKKOS)
  # The execution space we want to use from kokkos
  # Note: the 'set_property' command has zero impact on cmake logic; cmake does not perform
  #       any check to ensure the value of the variable is in the given list of string.
  #       However, if someone is using a cmake gui, this can help identifying the possible options.
  set (HOMMEXX_EXEC_SPACE "Default" CACHE STRING "The kokkos exec space to use.")
  set_property (CACHE HOMMEXX_EXEC_SPACE PROPERTY STRINGS "Cuda;OpenMP;Threads;Serial;Default")

  # Sowftware vector size to be used, unless we detect Cuda or AVX later on
  set (HOMMEXX_VECTOR_SIZE 8 CACHE STRING "If AVX or Cuda don't take priority, use this software vector size.")

  ### NOTE: the following options are irrelevant for non-GPU builds ###

  # Execution space parameters. 8 is a nice size for V100.
  set (HOMMEXX_CUDA_MIN_WARP_PER_TEAM  8 CACHE INT "Minimum number of warps to get 100% occoupancy on GPU")
  set (HOMMEXX_CUDA_MAX_WARP_PER_TEAM 16 CACHE INT "Maximum number of warps to get 100% occoupancy on GPU")

  # An option to allow to use GPU pointers for MPI calls.
  option (HOMMEXX_MPI_ON_DEVICE "Whether we want to use device pointers for MPI calls (relevant only for GPU builds)" ON)

  # Whether GPU runs should be bfb with CPU runs
  # Note: If you ask for GPU to be bfb with CPU, you should also ask for a strict fp model on cpu.
  option (HOMMEXX_GPU_BFB_WITH_CPU "Whether we want cpu and gpu solution to be bfb" OFF)
endif ()
