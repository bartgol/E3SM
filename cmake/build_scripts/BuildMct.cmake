macro (BuildMct)
  set (BIN_DIR ${CMAKE_BINARY_DIR}/libs/mct)

  add_subdirectory (${CMAKE_SOURCE_DIR}/cime/src/externals/mct
                    ${BIN_DIR})

  set (SHARED_LIBRARIES
    ${SHARED_LIBRARIES}
    mct
  )
  set (SHARED_INCLUDE_DIRS
    ${SHARED_INCLUDE_DIRS}
    ${BIN_DIR}/modules
  )
endmacro ()
