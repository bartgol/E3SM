macro (BuildScorpio)
  set (BIN_DIR ${CMAKE_BINARY_DIR}/libs/pio)

  if ("${PIO_VERSION}" STREQUAL "1")
    add_subdirectory(externals/scorpio_classic ${BIN_DIR})
    set (SHARED_LIBRARIES
      ${SHARED_LIBRARIES}
      pio
    )
    set (SHARED_INCLUDE_DIRS
      ${SHARED_INCLUDE_DIRS}
      ${BIN_DIR}/pio
    )
  elseif ("${PIO_VERSION}" STREQUAL "2")
    add_subdirectory(externals/scorpio ${BIN_DIR})
  else ()
    message (FATAL_ERROR "Error! Invalid pio version: ${PIO_VERSION}")
  endif()

endmacro()
