# Set compiler basic flags

add_definitions (-DCPRGNU)

set (CMAKE_Fortran_FLAGS
  "${CMAKE_Fortran_FLAGS} -ffree-line-length-none"
)
