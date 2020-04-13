include (config/Compsets)
include (config/Resolutions)
include (config/Machines)

function (CheckUserInputs CASEROOT COMPSET RESOLUTION MACHINE COMPILER)

  # Check that the user specified the basic inputs,
  # and that these inputs are in the list of valid inputs
  if ("${CASEROOT}" STREQUAL "")
    message (FATAL_ERROR "Error! You must specify a case folder.")
  else ()
    # Check if folder exists. If not, create it, otherwise
    # let user decide whether to abort or overwrite
  endif()

  if ("${COMPSET}" STREQUAL "")
    message (FATAL_ERROR "Error! You must specify a compset.")
  elseif (${COMPSET} IN_LIST COMPSETS_SHORT_NAMES)
  elseif (${COMPSET} IN_LIST COMPSETS_LONG_NAMES)
  else ()
    message (FATAL_ERROR "Error! Compset '${COMPSET}' was not found in the list of valid compsets.\n"
                         "A list of valid compsets can be found in\n  ${CMAKE_SOURCE_DIR}/cmake/config/Compsets.cmake\n")
  endif()

  if ("${RESOLUTION}" STREQUAL "")
    message (FATAL_ERROR "Error! You must specify a resolution.")
  elseif(NOT ${RESOLUTION} IN_LIST RESOLUTIONS)
    message ("valid res: ${RESOLUTIONS}")
    message (FATAL_ERROR "Error! Resolution '${RESOLUTION}' was not found in the list of valid resolutions.\n"
                         "       For a list of valid resolutions, see ${CMAKE_SOURCE_DIR}/cmake/config/Resolutions.cmake")

  else ()
    # Check resolution is supported by this compset
    ValidateResolution (${COMPSET} ${RESOLUTION})
  endif()

  if ("${MACHINE}" STREQUAL "")
    message (FATAL_ERROR "Error! You must specify a machine name.")
  elseif (NOT ${MACHINE} IN_LIST MACHINES)
    message (FATAL_ERROR "Error! Machine '${MACHINE}' was not found in the list of supported machines.\n"
                         "       For a list of valid machine names, see ${CMAKE_SOURCE_DIR}/cmake/config/Machines.cmake")
  endif()

  if (NOT "${COMPILER}" STREQUAL "")
    include (config/machines/${MACHINE})
    if (NOT ${COMPILER} IN_LIST ${MACHINE}_COMPILERS)
      message (FATAL_ERROR "Error! Compiler '${COMPILER}' not found in the list of compilers supported on machine '${MACHINE}'.\n"
                           "       For a list of valid compilers for this machine, see ${CMAKE_SOURCE_DIR}/cmake/config/machines/${MACHINE}.cmake")
    endif()
  endif()

endfunction()
