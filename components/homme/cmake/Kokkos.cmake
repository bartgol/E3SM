# Whether we need to build kokkos
set (KOKKOS_BUILD_NEEDED FALSE CACHE INTERNAL "")

if (NOT DEFINED E3SM_INTERNAL_KOKKOS_ALREADY_BUILT)
  set (E3SM_INTERNAL_KOKKOS_ALREADY_BUILT FALSE CACHE INTERNAL "")
endif ()

if (NOT DEFINED E3SM_KOKKOS_PATH)
  # Build kokkos submodule if user did not specify KOKKOS_PATH.
  set (KOKKOS_SRC_DIR ${HOMME_SOURCE_DIR}/../../externals/kokkos CACHE INTERNAL "")
  set (KOKKOS_BIN_DIR ${CMAKE_BINARY_DIR}/kokkos/build CACHE INTERNAL "")

  message ("Using E3SM-internal Kokkos, built in ${KOKKOS_BIN_DIR}")

  if (NOT E3SM_INTERNAL_KOKKOS_ALREADY_BUILT)
    # Nobody else already added kokkos subdirectory, so we will do it
    set (KOKKOS_BUILD_NEEDED TRUE)
    set (E3SM_INTERNAL_KOKKOS_ALREADY_BUILT TRUE)
  endif()
else ()
  find_package(Kokkos REQUIRED
               PATHS ${E3SM_KOKKOS_PATH})

  message (STATUS "Kokkos installation found in provided path '${E3SM_KOKKOS_PATH}'. Here are the details:")
  message ("    Kokkos_INCLUDE_DIRS:  ${Kokkos_INCLUDE_DIRS}")
  message ("    Kokkos_LIBRARY_DIRS:  ${Kokkos_LIBRARY_DIRS}")
  message ("    Kokkos_LIBRARIES:     ${Kokkos_LIBRARIES}")
  message ("    Kokkos_TPL_LIBRARIES: ${Kokkos_TPL_LIBRARIES}")
endif ()

macro (build_kokkos_if_needed)
  if (KOKKOS_BUILD_NEEDED)
    if (ENABLE_OPENMP)
      set (KOKKOS_ENABLE_OPENMP TRUE)
    endif ()
    add_subdirectory (${KOKKOS_SRC_DIR} ${KOKKOS_BIN_DIR})
  endif ()
endmacro()
