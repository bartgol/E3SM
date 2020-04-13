macro (BuildGptl)
  set (BIN_DIR ${CMAKE_BINARY_DIR}/libs/gptl)

  add_subdirectory(cime/src/share/timing ${BIN_DIR})
  set (SHARED_LIBRARIES
    ${SHARED_LIBRARIES}
    timing
  )
  set (SHARED_INCLUDE_DIRS
    ${SHARED_INCLUDE_DIRS}
    ${BIN_DIR}
  )
endmacro()
