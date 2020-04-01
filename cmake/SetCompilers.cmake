macro (SetCompilers MACHINE COMPILER)

  include (config/machines/${MACHINE}.cmake)

  if ("${COMPILER}" STREQUAL "")
    # Set the default compiler for this machine
    set (COMPILER ${DEFAULT_COMPILER})
  endif()

  # Grab basic compiler flags
  include (config/compilers/${COMPILER}.cmake)

  # Each machine file should have a macro that receives the compiler name
  # as argument, and sets SCC, SCXX, SFC (the scalar C,CXX,Fortran compilers)
  # as well as MPICC, MPICXX, MPIFC (the mpi C,CXX,Fortran compilers)
  SetMachineCompilers(${COMPILER}) 

endmacro()
