set (linux-generic_COMPILERS
  gnu
)

# For each compiler, define macro ${MACHINE}-${COMPILER}.
# The macro should set compiler name, as well as machine-specific
# flags for this compiler
macro (linux-generic-gnu)

  # Find gcc, g++, gfortran, mpicc, mpicxx, mpifort
  find_program (SCC  gcc)
  find_program (SCXX g++)
  find_program (SFC  gfortran)

  find_program (MPICC  mpicc)
  find_program (MPICXX mpicxx)
  find_program (MPIFC  mpifort)

endmacro()

macro (SetMachineCompilers COMPILER)
  string(TOUPPER COMPILER COMPILER_CI)
  
  if ("${COMPILER_CI}" STREQUAL "GNU")
    linux-generic-gnu()
  else ()
    message (FATAL_ERROR
             "Error! Unrecognized compiler '${COMPILER}'.\n"
             "       Note: you should have already gotten an error.\n"
             "             Please, contact developers.")
  endif()
  unset (COMPILER_CI)
endmacro()
