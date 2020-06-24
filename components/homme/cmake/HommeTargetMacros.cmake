# String representation of pound since it is a cmake comment char.
string(ASCII 35 POUND)

function (prc var)
  message("${var} ${${var}}")
endfunction ()

# Macro to create config file. The macro creates a temporary config
# file first. If the config file in the build directory does not
# exist or it is different from the temporary one, then the config
# file is updated. This avoid rebuilding the (almost) entire homme
# every time cmake is run.

# Macro to create the individual tests
macro(createTestExec execName execType macroNP macroNC
                     macroPLEV macroUSE_PIO macroWITH_ENERGY macroQSIZE_D)

# before calling this macro, be sure that these are set locally:
# EXEC_INCLUDE_DIRS
# EXEC_SOURCES

  # Set the include directories
  set(EXEC_MODULE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${execName}_modules")
  include_directories(${CMAKE_CURRENT_BINARY_DIR}
                      ${EXEC_INCLUDE_DIRS}
                      ${EXEC_MODULE_DIR}
                      )

  message(STATUS "Building ${execName} derived from ${execType} with:")
  message(STATUS "  NP = ${macroNP}")
  message(STATUS "  PLEV = ${macroPLEV}")
  message(STATUS "  QSIZE_D = ${macroQSIZE_D}")
  message(STATUS "  PIO = ${macroUSE_PIO}")
  message(STATUS "  ENERGY = ${macroWITH_ENERGY}")

  # Set the variable to the macro variables
  set(NUM_POINTS ${macroNP})
  set(NUM_CELLS ${macroNC})
  set(NUM_PLEV ${macroPLEV})

  if (${macroUSE_PIO})
    set(PIO TRUE)
    set(PIO_INTERP)
  else ()
    set(PIO)
    set(PIO_INTERP TRUE)
  endif ()

  if(BUILD_HOMME_WITHOUT_PIOLIBRARY AND (NOT PIO_INTERP))
    message(ERROR "For building without PIO library set PREQX_USE_PIO to false")
  endif ()

  if (${macroWITH_ENERGY})
    set(ENERGY_DIAGNOSTICS TRUE)
  else()
    set(ENERGY_DIAGNOSTICS)
  endif ()

  if (${macroQSIZE_D})
    set(QSIZE_D ${macroQSIZE_D})
  else()
    set(QSIZE_D)
  endif ()


  # This is needed to compile the test executables with the correct options
  set(THIS_CONFIG_IN ${HOMME_SOURCE_DIR}/src/${execType}/config.h.cmake.in)
  set(THIS_CONFIG_HC ${CMAKE_CURRENT_BINARY_DIR}/config.h.c)
  set(THIS_CONFIG_H ${CMAKE_CURRENT_BINARY_DIR}/config.h)

  # First configure the file (which formats the file as C)
  HommeConfigFile (${THIS_CONFIG_IN} ${THIS_CONFIG_HC} ${THIS_CONFIG_H} )

  add_definitions(-DHAVE_CONFIG_H)

  add_executable(${execName} ${EXEC_SOURCES})
  set_target_properties(${execName} PROPERTIES LINKER_LANGUAGE Fortran)

  if (CXXLIB_SUPPORTED_CACHE)
    message(STATUS "   Linking Fortran with -cxxlib")
    target_link_libraries(${execName} -cxxlib)
  endif ()

  string(TOUPPER "${PERFORMANCE_PROFILE}" PERF_PROF_UPPER)
  if ("${PERF_PROF_UPPER}" STREQUAL "VTUNE")
    target_link_libraries(${execName} ittnotify)
  endif ()

  # Add this executable to a list
  set(EXEC_LIST ${EXEC_LIST} ${execName} CACHE INTERNAL "List of configured executables")

  target_link_libraries(${execName} timing ${COMPOSE_LIBRARY} ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES})
  if(NOT BUILD_HOMME_WITHOUT_PIOLIBRARY)
    if(HOMME_USE_SCORPIO)
      target_link_libraries(${execName} piof pioc)
    else ()
      target_link_libraries(${execName} pio)
    endif ()
  endif ()

  # Link to kokkos if necessary
  if (HOMME_USE_KOKKOS)
    target_link_libraries (${execName} kokkos)
  endif ()

  # Enforce C++11 standard, and add CUDA specific flags
  if (HOMME_USE_CXX)
    set_property(TARGET ${execName} PROPERTY CXX_STANDARD 11)
    set_property(TARGET ${execName} PROPERTY CXX_STANDARD_REQUIRED ON)
    if (CUDA_BUILD)
      target_compile_options (${execName}  BEFORE PUBLIC $<$<COMPILE_LANGUAGE:C>:--expt-extended-lambda>)
    endif ()
  endif ()

  # Move the module files out of the way so the parallel build
  # doesn't have a race condition
  set_target_properties(${execName}
                        PROPERTIES Fortran_MODULE_DIRECTORY ${EXEC_MODULE_DIR})

  # If MKL, then the -mkl flag should already be added to link flags
  if (NOT HOMME_USE_MKL)
    target_link_libraries(${execName} lapack blas)
  endif()

  if (HAVE_EXTRAE)
    target_link_libraries(${execName} ${Extrae_LIBRARY})
  endif ()

  if (HOMME_USE_TRILINOS)
    target_link_libraries(${execName} ${Trilinos_LIBRARIES} ${Trilinos_TPL_LIBRARIES})
  endif()

  if (HOMME_USE_ARKODE AND execType STREQUAL "theta-l")
    target_link_libraries(${execName} sundials_farkode)
    target_link_libraries(${execName} sundials_arkode)
    target_link_libraries(${execName} sundials_nvecserial)
    target_link_libraries(${execName} sundials_fnvecserial)
  endif ()

  # Add all HOMME_<LANG>_FLAGS
  target_compile_options(${execName} PUBLIC $<$<COMPILE_LANGUAGE:C>:${HOMME_C_FLAGS}>)
  target_compile_options(${execName} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${HOMME_CXX_FLAGS}>)
  target_compile_options(${execName} PUBLIC $<$<COMPILE_LANGUAGE:Fortran>:${HOMME_Fortran_FLAGS}>)

  # Swap the following IF we can rely on cmake version 3.13 or higher
  # target_link_options(${execName} PUBLIC ${HOMME_LINKER_FLAGS})
  set_target_properties(${execName} PROPERTIES
                        LINK_FLAGS ${HOMME_LINKER_FLAGS})

  install(TARGETS ${execName} RUNTIME DESTINATION tests)

endmacro(createTestExec)

macro (copyDirFiles testDir)
  # Copy all of the files into the binary dir
  foreach (singleFile ${NAMELIST_FILES})
    # Some namelist contain cmake variable, to generate
    # multiple testcases with different ne or ndays,
    # so use CONFIGURE_FILE, to replace variables with values
    GET_FILENAME_COMPONENT(fileName ${singleFile} NAME)
    CONFIGURE_FILE(${singleFile} ${testDir}/${fileName})
  endforeach ()
  foreach (singleFile ${VCOORD_FILES})
    FILE(COPY ${singleFile} DESTINATION ${testDir}/vcoord)
  endforeach ()
  foreach (singleFile ${MESH_FILES})
    FILE(COPY ${singleFile} DESTINATION ${testDir})
  endforeach ()
  foreach (singleFile ${NCL_FILES})
    FILE(COPY ${singleFile} DESTINATION ${testDir})
  endforeach ()
  foreach (singleFile ${MESH_FILES})
    FILE(COPY ${singleFile} DESTINATION ${testDir})
  endforeach ()
  foreach (singleFile ${TRILINOS_XML_FILE})
    FILE(COPY ${singleFile} DESTINATION ${testDir})
  endforeach ()




  # Need to create the movie directory for output
  EXECUTE_PROCESS(COMMAND mkdir -p ${testDir}/movies
    RESULT_VARIABLE Homme_result
    OUTPUT_VARIABLE Homme_output
    ERROR_VARIABLE Homme_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  # Need to create the restart directory for restart files
  EXECUTE_PROCESS(COMMAND mkdir -p ${testDir}/restart
    RESULT_VARIABLE Homme_result
    OUTPUT_VARIABLE Homme_output
    ERROR_VARIABLE Homme_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

endmacro (copyDirFiles)

macro (setUpTestDir TEST_DIR)

  set(TEST_DIR_LIST ${TEST_DIR_LIST} ${TEST_DIR})
  set(THIS_BASELINE_TEST_DIR ${HOMME_BINARY_DIR}/tests/baseline/${TEST_NAME})

  copyDirFiles(${TEST_DIR})
  copyDirFiles(${THIS_BASELINE_TEST_DIR})

  # Create a run script
  set(THIS_TEST_SCRIPT ${TEST_DIR}/${TEST_NAME}.sh)

  FILE(WRITE  ${THIS_TEST_SCRIPT} "${POUND}!/bin/bash\n")
  FILE(APPEND ${THIS_TEST_SCRIPT} "${POUND}===============================\n")
  FILE(APPEND ${THIS_TEST_SCRIPT} "${POUND} Test script generated by CMake\n")
  FILE(APPEND ${THIS_TEST_SCRIPT} "${POUND}===============================\n")
  FILE(APPEND ${THIS_TEST_SCRIPT} "${POUND} Used for running ${TEST_NAME}\n")
  FILE(APPEND ${THIS_TEST_SCRIPT} "${POUND}===============================\n")

  FILE(APPEND ${THIS_TEST_SCRIPT} "\n") # new line
  if ("${NUM_CPUS}" STREQUAL "")
    message(FATAL_ERROR "In test ${TEST_NAME} NUM_CPUS not defined. Quitting")
  endif ()
  if (NOT ${HOMME_QUEUING})
    if (NOT ${USE_NUM_PROCS} STREQUAL "")
    #if (USE_NUM_PROCS)
      #FILE(APPEND ${THIS_TEST_SCRIPT} "num_cpus=${USE_NUM_PROCS}\n") # new line
      set(NUM_CPUS ${USE_NUM_PROCS})
    elseif (${NUM_CPUS} GREATER ${MAX_NUM_PROCS})
      ##message(STATUS "For ${TEST_NAME} the requested number of CPU processes is larger than the number available")
      ##message(STATUS "  Changing NUM_CPU from ${NUM_CPUS} to ${MAX_NUM_PROCS}")
      ##set(NUM_CPUS ${MAX_NUM_PROCS})
      #FILE(APPEND ${THIS_TEST_SCRIPT} "num_cpus=${MAX_NUM_PROCS}\n") # new line
      set(NUM_CPUS ${MAX_NUM_PROCS})
    endif ()
  endif ()
  FILE(APPEND ${THIS_TEST_SCRIPT} "NUM_CPUS=${NUM_CPUS}\n") # new line
  FILE(APPEND ${THIS_TEST_SCRIPT} "\n") # new line
  set (TEST_INDEX 1)
  foreach (singleFile ${NAMELIST_FILES})
    GET_FILENAME_COMPONENT(fileName ${singleFile} NAME)
    FILE(APPEND ${THIS_TEST_SCRIPT} "TEST_${TEST_INDEX}=\"${CMAKE_CURRENT_BINARY_DIR}/${EXEC_NAME}/${EXEC_NAME} < ${fileName}\"\n")
    FILE(APPEND ${THIS_TEST_SCRIPT} "\n") # new line
    MATH(EXPR TEST_INDEX "${TEST_INDEX} + 1")
  endforeach ()
  MATH(EXPR TEST_INDEX "${TEST_INDEX} - 1")
  FILE(APPEND ${THIS_TEST_SCRIPT} "NUM_TESTS=${TEST_INDEX}\n") # new line
  FILE(APPEND ${THIS_TEST_SCRIPT} "\n") # new line

  # openMP runs

  if (EXEC_NAME MATCHES "kokkos$")
    #not doing logic around omp_num_mpi...
    FILE(APPEND ${THIS_TEST_SCRIPT} "OMP_NUMBER_THREADS_KOKKOS=${OMP_NUM_THREADS}\n") # new line
  endif()

  if (NOT "${OMP_NAMELIST_FILES}" STREQUAL "")
    if (${ENABLE_HORIZ_OPENMP})
      FILE(APPEND ${THIS_TEST_SCRIPT} "${POUND}===============================\n")
      FILE(APPEND ${THIS_TEST_SCRIPT} "${POUND} OpenMP Tests\n")
      FILE(APPEND ${THIS_TEST_SCRIPT} "${POUND}===============================\n")
      FILE(APPEND ${THIS_TEST_SCRIPT} "\n") # new line
      MATH(EXPR OMP_MOD "${NUM_CPUS} % ${OMP_NUM_THREADS}")
      if (NOT ${OMP_MOD} EQUAL 0)
      # MT: why is this a fatal error?  i'm just trying to build preqx and not run regression tests.
        message(STATUS "In test ${TEST_NAME} NUM_CPUS not divisible by OMP_NUM_THREADS. Quitting.")
      endif ()
      MATH(EXPR OMP_NUM_MPI "${NUM_CPUS} / ${OMP_NUM_THREADS}")
      FILE(APPEND ${THIS_TEST_SCRIPT} "OMP_NUM_MPI=${OMP_NUM_MPI}\n") # new line
      FILE(APPEND ${THIS_TEST_SCRIPT} "OMP_NUMBER_THREADS=${OMP_NUM_THREADS}\n") # new line
      FILE(APPEND ${THIS_TEST_SCRIPT} "\n") # new line
      set (TEST_INDEX 1)
      foreach (singleFile ${OMP_NAMELIST_FILES})
        FILE(APPEND ${THIS_TEST_SCRIPT} "OMP_TEST_${TEST_INDEX}=\"${CMAKE_CURRENT_BINARY_DIR}/${EXEC_NAME}/${EXEC_NAME} < ${singleFile}\"\n")
        FILE(APPEND ${THIS_TEST_SCRIPT} "\n") # new line
        MATH(EXPR TEST_INDEX "${TEST_INDEX} + 1")
      endforeach ()
      MATH(EXPR TEST_INDEX "${TEST_INDEX} - 1")
      FILE(APPEND ${THIS_TEST_SCRIPT} "OMP_NUM_TESTS=${TEST_INDEX}\n") # new line
    else ()
      message(STATUS "  Not including OpenMP tests")
    endif()
  endif ()

  # Add this test to the list of tests
  MATH(EXPR NUM_TEST_FILES "${NUM_TEST_FILES} + 1")
  FILE (APPEND ${HOMME_TEST_LIST} "TEST_FILE_${NUM_TEST_FILES}=${THIS_TEST_SCRIPT}\n")

  FILE(APPEND ${THIS_TEST_SCRIPT} "\n") # new line

  # Deal with the Netcdf output files
  FILE(APPEND ${THIS_TEST_SCRIPT} "NC_OUTPUT_FILES=\"")
  foreach (singleFile ${NC_OUTPUT_FILES})
    FILE(APPEND ${THIS_TEST_SCRIPT} "${singleFile} ")
  endforeach ()
  # Add the OPENMP netcdf outpuf files
  if (${ENABLE_HORIZ_OPENMP})
    foreach (singleFile ${OMP_NC_OUTPUT_FILES})
      FILE(APPEND ${THIS_TEST_SCRIPT} "${singleFile} ")
    endforeach ()
  endif()
  FILE(APPEND ${THIS_TEST_SCRIPT} "\"\n")

  FILE(APPEND ${THIS_TEST_SCRIPT} "NC_OUTPUT_REF=\"")
  foreach (singleFile ${NC_OUTPUT_REF})
    FILE(APPEND ${THIS_TEST_SCRIPT} "${singleFile} ")
  endforeach ()
  FILE(APPEND ${THIS_TEST_SCRIPT} "\"\n")

  FILE(APPEND ${THIS_TEST_SCRIPT} "NC_OUTPUT_CHECKREF=\"")
  foreach (singleFile ${NC_OUTPUT_CHECKREF})
    FILE(APPEND ${THIS_TEST_SCRIPT} "${singleFile} ")
  endforeach ()
  FILE(APPEND ${THIS_TEST_SCRIPT} "\"\n")

  FILE(APPEND ${THIS_TEST_SCRIPT} "TESTCASE_REF_TOL=\"")
  FILE(APPEND ${THIS_TEST_SCRIPT} "${TESTCASE_REF_TOL}")
  FILE(APPEND ${THIS_TEST_SCRIPT} "\"\n")



endmacro (setUpTestDir)

macro(resetTestVariables)
  # Reset the variables
  set(TEST_NAME)
  set(EXEC_NAME)
  set(NAMELIST_FILES)
  set(VCOORD_FILES)
  set(MESH_FILES)
  set(NCL_FILES)
  set(MESH_FILES)
  set(NC_OUTPUT_FILES)
  set(OMP_NC_OUTPUT_FILES)
  set(NC_OUTPUT_REF)
  set(NC_OUTPUT_CHECKREF)
  set(TESTCASE_REF_TOL)
  set(NUM_CPUS)
  set(OMP_NAMELIST_FILES)
  set(OMP_NUM_THREADS)
  set(TRILINOS_XML_FILE)
endmacro(resetTestVariables)

macro(printTestSummary)
  message(STATUS "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%")
  message(STATUS "Summary of test ${TEST_NAME}")
  if (NOT "${NAMELIST_FILES}" STREQUAL "")
    message(STATUS "  namelist_files=")
    foreach (singleFile ${NAMELIST_FILES})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${VCOORD_FILES}" STREQUAL "")
    message(STATUS "  vcoord_files=")
    foreach (singleFile ${VCOORD_FILES})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${MESH_FILES}" STREQUAL "")
    message(STATUS "  mesh_files=")
    foreach (singleFile ${MESH_FILES})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${NCL_FILES}" STREQUAL "")
    message(STATUS "  ncl_files=")
    foreach (singleFile ${NCL_FILES})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${NC_OUTPUT_FILES}" STREQUAL "")
    message(STATUS "  nc_output_files=")
    foreach (singleFile ${NC_OUTPUT_FILES})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${NC_OUTPUT_REF}" STREQUAL "")
    message(STATUS "  nc_output_files=")
    foreach (singleFile ${NC_OUTPUT_REF})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${NC_OUTPUT_CHECKREF}" STREQUAL "")
    message(STATUS "  nc_output_files=")
    foreach (singleFile ${NC_OUTPUT_CHECKREF})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${TESTCASE_REF_TOL}" STREQUAL "")
    message(STATUS "  testcase_ref_tol=")
    message(STATUS "  ${TESTCASE_REF_TOL}")
  endif ()
  if (NOT "${MESH_FILES}" STREQUAL "")
    message(STATUS "  mesh_files=")
    foreach (singleFile ${MESH_FILES})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${OMP_NAMELIST_FILES}" STREQUAL "")
    message(STATUS "  omp_namelist_files=")
    foreach (singleFile ${OMP_NAMELIST_FILES})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${OMP_NC_OUTPUT_FILES}" STREQUAL "")
    message(STATUS "  omp_nc_output_files=")
    foreach (singleFile ${OMP_NC_OUTPUT_FILES})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()
  if (NOT "${TRILINOS_XML_FILE}" STREQUAL "")
    message(STATUS "  trilinos_xml_file=")
    foreach (singleFile ${TRILINOS_XML_FILE})
      message(STATUS "    ${singleFile}")
    endforeach ()
  endif ()

endmacro(printTestSummary)

macro (set_homme_tests_parameters testFile profile)
#example on how to customize tests lengths
#  if ("${testFile}" MATCHES ".*-moist.*")
  if ("${testFile}" MATCHES ".*-tensorhv-*")
    if ("${profile}" STREQUAL "dev")
      set (HOMME_TEST_NE 2)
      set (HOMME_TEST_NDAYS 1)
    elseif ("${profile}" STREQUAL "short")
      set (HOMME_TEST_NE 4)
      set (HOMME_TEST_NDAYS 3)
    else ()
      set (HOMME_TEST_NE 12)
      set (HOMME_TEST_NDAYS 3)
    endif ()
  else ()
    if ("${profile}" STREQUAL "dev")
      set (HOMME_TEST_NE 2)
      set (HOMME_TEST_NDAYS 1)
    elseif ("${profile}" STREQUAL "short")
      set (HOMME_TEST_NE 4)
      set (HOMME_TEST_NDAYS 9)
    else ()
      set (HOMME_TEST_NE 12)
      set (HOMME_TEST_NDAYS 9)
    endif ()
  endif()
endmacro ()

# Macro to create the individual tests
macro(createTest testFile)

  set (THIS_TEST_INPUT ${HOMME_SOURCE_DIR}/test/reg_test/run_tests/${testFile})

  resetTestVariables()

  set(HOMME_ROOT ${HOMME_SOURCE_DIR})

  include(${THIS_TEST_INPUT})

  if (DEFINED PROFILE)
    set_homme_tests_parameters(${testFile} ${PROFILE})
    set (TEST_NAME "${TEST_NAME}-ne${HOMME_TEST_NE}-ndays${HOMME_TEST_NDAYS}")
  endif ()

  FILE(GLOB NAMELIST_FILES ${NAMELIST_FILES})
  FILE(GLOB VCOORD_FILES ${VCOORD_FILES})
  FILE(GLOB NCL_FILES ${NCL_FILES})
  FILE(GLOB MESH_FILES ${MESH_FILES})
  FILE(GLOB OMP_NAMELIST_FILES ${OMP_NAMELIST_FILES})
  FILE(GLOB TRILINOS_XML_FILE ${TRILINOS_XML_FILE})

  # Determine if the executable this tests depeds upon is built
  LIST(FIND EXEC_LIST ${EXEC_NAME} FIND_INDEX)

  if (${FIND_INDEX} LESS 0)
    message(STATUS "Not configuring test ${TEST_NAME} since it depends upon the executable ${EXEC_NAME}
                    which isn't built with this configuration")
  else ()

    message(STATUS "Adding test: ${TEST_NAME}, using exec ${EXEC_NAME}")

    if (${TEST_SUMMARY})
      printTestSummary()
    endif()

    # Set up the directory
    set(THIS_TEST_DIR ${HOMME_BINARY_DIR}/tests/${TEST_NAME})

    # Set up the test directory for both the baseline and the comparison tests
    setUpTestDir(${THIS_TEST_DIR})
    #setUpTestDir(${THIS_BASELINE_TEST_DIR})

    # The test (not the baseline) run script
    set(THIS_TEST_RUN_SCRIPT "${THIS_TEST_DIR}/${TEST_NAME}-run.sh")

    if (${HOMME_QUEUING})

      set(THIS_TEST "${TEST_NAME}-diff")

      # When run through the queue the runs are submitted and ran in
      #   submitAndRunTests, and diffed in the subsequent tests
      ADD_TEST(NAME ${THIS_TEST}
               COMMAND ${HOMME_BINARY_DIR}/tests/diff_output.sh ${TEST_NAME})

      set_tests_properties(${THIS_TEST} PROPERTIES DEPENDS submitAndRunTests)


    else ()

      set(THIS_TEST "${TEST_NAME}")

      # When not run through a queue each run is ran and then diffed. This is handled by
      #  the submit_tests.sh script
      ADD_TEST(NAME ${THIS_TEST}
               COMMAND ${HOMME_BINARY_DIR}/tests/submit_tests.sh "${THIS_TEST_RUN_SCRIPT}" "${TEST_NAME}"
               DEPENDS ${EXEC_NAME})

    endif ()

    # Force cprnc to be built when the individual test is run
    set_tests_properties(${THIS_TEST} PROPERTIES DEPENDS cprnc)

    if (DEFINED PROFILE)
      set_tests_properties(${THIS_TEST} PROPERTIES LABELS ${PROFILE})
    endif()

    # Individual target to rerun and diff the tests
    set(THIS_TEST_INDIV "test-${TEST_NAME}")

    ADD_CUSTOM_TARGET(${THIS_TEST_INDIV}
             COMMAND ${HOMME_BINARY_DIR}/tests/submit_tests.sh "${THIS_TEST_RUN_SCRIPT}" "${TEST_NAME}")

    ADD_DEPENDENCIES(${THIS_TEST_INDIV} ${EXEC_NAME})

    # test execs
    ADD_DEPENDENCIES(test-execs ${EXEC_NAME})

    # Check target
    ADD_DEPENDENCIES(check ${EXEC_NAME})

    # Baseline target
    ADD_DEPENDENCIES(baseline ${EXEC_NAME})

    # Force cprnc to be built when the individual test is run
    ADD_DEPENDENCIES(${THIS_TEST_INDIV} cprnc)

    # This helped in some builds on GPU, where the test hanged for a VERY long time
    if (NOT "${TIMEOUT}" STREQUAL "")
      set_tests_properties(${THIS_TEST} PROPERTIES TIMEOUT ${TIMEOUT})
    endif ()

    # Now make the Individual targets
    #ADD_CUSTOM_COMMAND(TARGET ${THIS_TEST_INDIV}
    #                   COMMENT "Running the HOMME regression test: ${THIS_TEST}"
    #                   POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND} ARGS --output-on-failure -R ${THIS_TEST_INDIV}
    #                   WORKING_DIRECTORY ${HOMME_BINARY_DIR})

  endif ()
endmacro(createTest)

macro (createTests testList)
  foreach (test ${${testList}})
    createTest(${test})
  endforeach ()
endmacro (createTests)

macro(createTestsWithProfile testList testsProfile)
  set (PROFILE ${testsProfile})
  foreach (test ${${testList}})
    createTest(${test})
  endforeach ()
  UNSET(PROFILE)
endmacro(createTestsWithProfile)

# Make a list of all testing profiles no more intensive than the given profile.
function (make_profiles_up_to profile profiles)
  string (TOLOWER "${profile}" profile_ci)
  set (tmp)
  if ("${profile_ci}" STREQUAL "dev")
    list (APPEND tmp "dev")
  elseif ("${profile_ci}" STREQUAL "short")
    list (APPEND tmp "dev")
    list (APPEND tmp "short")
  elseif ("${profile_ci}" STREQUAL "nightly")
    list (APPEND tmp "dev")
    list (APPEND tmp "short")
    list (APPEND tmp "nightly")
  else ()
    message (FATAL_ERROR "Testing profile '${profile}' not implemented.")
  endif ()
  set (profiles "${tmp}" PARENT_SCOPE)
endfunction ()

MACRO(CREATE_CXX_VS_F90_TESTS_WITH_PROFILE TESTS_LIST testProfile)

  foreach (TEST ${${TESTS_LIST}})
    set (TEST_FILE_F90 "${TEST}.cmake")

    set_homme_tests_parameters(${TEST} ${testProfile})
    set (PROFILE ${testProfile})
    include (${HOMME_SOURCE_DIR}/test/reg_test/run_tests/${TEST_FILE_F90})

    set (TEST_NAME_SUFFIX "ne${HOMME_TEST_NE}-ndays${HOMME_TEST_NDAYS}")
    set (F90_TEST_NAME "${TEST}-${TEST_NAME_SUFFIX}")
    set (CXX_TEST_NAME "${TEST}-kokkos-${TEST_NAME_SUFFIX}")
    set (F90_DIR ${HOMME_BINARY_DIR}/tests/${F90_TEST_NAME})
    set (CXX_DIR ${HOMME_BINARY_DIR}/tests/${CXX_TEST_NAME})

    # Compare netcdf output files bit-for-bit AND compare diagnostic lines
    # in the raw output files
    set (TEST_NAME "${TEST}-${TEST_NAME_SUFFIX}_cxx_vs_f90")
    message ("-- Creating cxx-f90 comparison test ${TEST_NAME}")

    CONFIGURE_FILE (${HOMME_SOURCE_DIR}/cmake/CxxVsF90.cmake.in
                    ${HOMME_BINARY_DIR}/tests/${CXX_TEST_NAME}/CxxVsF90.cmake @ONLY)

    ADD_TEST (NAME "${TEST_NAME}"
              COMMAND ${CMAKE_COMMAND} -P CxxVsF90.cmake
              WORKING_DIRECTORY ${HOMME_BINARY_DIR}/tests/${CXX_TEST_NAME})

    set_tests_properties(${TEST_NAME} PROPERTIES DEPENDS "${F90_TEST_NAME};${CXX_TEST_NAME}"
      LABELS ${testProfile})
  endforeach ()
endmacro(CREATE_CXX_VS_F90_TESTS_WITH_PROFILE)


macro(setCustomCompilerFlags CUSTOM_FLAGS_FILE SRCS_ALL)

  # Locally reset the compiler flags
  #   This only changes the flags for preqx
  #   these variables get reset outside of this subdir
  set(CMAKE_Fortran_FLAGS_ORIG "${CMAKE_Fortran_FLAGS}")
  set(CMAKE_Fortran_FLAGS "")

  # Need to put the OpenMP flag back on the compile line since it
  #   is used for linking
  if (${ENABLE_OPENMP})
    set(CMAKE_Fortran_FLAGS "${OpenMP_Fortran_FLAGS}")
  endif ()

  # Need to put the -mmic flag back on the compile line since it
  #   is used for linking
  if (${ENABLE_INTEL_PHI})
    set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} ${INTEL_PHI_FLAGS}")
  endif ()

  # This file should declare the list of files to be exclude
  #   from default compilation and declare compiler options for them
  include(${${CUSTOM_FLAGS_FILE}})

  # Remove the custom files from the list of all files
  foreach (CUSTOM_FILE ${CUSTOM_FLAG_FILES})
    message(STATUS "Applying custom compiler flags to ${CUSTOM_FILE}")
    get_source_file_property(THIS_CUSTOM_FLAGS ${CUSTOM_FILE} COMPILE_FLAGS)
    message(STATUS "  ${THIS_CUSTOM_FLAGS}")
    list(REMOVE_ITEM ${SRCS_ALL} ${CUSTOM_FILE})
  endforeach()

  # Compile the rest of the files with the original flags
  set_source_files_properties(${${SRCS_ALL}} PROPERTIES COMPILE_FLAGS
                              "${CMAKE_Fortran_FLAGS_ORIG}")

  # Add the custom files back in to the list of all files
  set(${SRCS_ALL} ${${SRCS_ALL}} ${CUSTOM_FLAG_FILES})

endmacro(setCustomCompilerFlags)
