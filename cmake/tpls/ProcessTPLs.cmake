macro (ProcessTPLs)
  set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake/tpls
  )

  # For eac E3SM TPL, call its macro
  find_package(NetCDF COMPONENTS C Fortran)

endmacro ()
