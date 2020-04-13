include (utils/genf90_utils)

macro (BuildCsmShare)
  set(SRC_ROOT_DIR ${CMAKE_SOURCE_DIR}/cime/src/share)
  set(BIN_DIR ${CMAKE_BINARY_DIR}/libs/csm_share)


  # Some files need to be preprocessed with genf90.
  # This process will create custom target, that need to be
  # run before creating the library
  set (GENF90_FILES_IN
    ${SRC_ROOT_DIR}/util/shr_assert_mod.F90.in
    ${SRC_ROOT_DIR}/util/shr_frz_mod.F90.in
    ${SRC_ROOT_DIR}/util/shr_infnan_mod.F90.in
  )
  process_genf90_source_list(
    ${BIN_DIR}
    "${GENF90_FILES_IN}"
    ${BIN_DIR}/genf90_output
    GENF90_FILES_OUT
  )

  set (SRCS
    ${GENF90_FILES_OUT}
    ${SRC_ROOT_DIR}/streams/shr_dmodel_mod.F90
    ${SRC_ROOT_DIR}/streams/shr_stream_mod.F90
    ${SRC_ROOT_DIR}/streams/shr_strdata_mod.F90
    ${SRC_ROOT_DIR}/streams/shr_tInterp_mod.F90
    ${SRC_ROOT_DIR}/util/mct_mod.F90
    ${SRC_ROOT_DIR}/util/shr_abort_mod.F90
    ${SRC_ROOT_DIR}/util/shr_cal_mod.F90
    ${SRC_ROOT_DIR}/util/shr_const_mod.F90
    ${SRC_ROOT_DIR}/util/shr_file_mod.F90
    ${SRC_ROOT_DIR}/util/shr_kind_mod.F90
    ${SRC_ROOT_DIR}/util/shr_log_mod.F90
    ${SRC_ROOT_DIR}/util/shr_map_mod.F90
    ${SRC_ROOT_DIR}/util/shr_mct_mod.F90
    ${SRC_ROOT_DIR}/util/shr_mem_mod.F90
    ${SRC_ROOT_DIR}/util/shr_mpi_mod.F90
    ${SRC_ROOT_DIR}/util/shr_ncread_mod.F90
    ${SRC_ROOT_DIR}/util/shr_nl_mod.F90
    ${SRC_ROOT_DIR}/util/shr_orb_mod.F90
    ${SRC_ROOT_DIR}/util/shr_pcdf_mod.F90
    ${SRC_ROOT_DIR}/util/shr_pio_mod.F90
    ${SRC_ROOT_DIR}/util/shr_reprosum_mod.F90
    ${SRC_ROOT_DIR}/util/shr_scam_mod.F90
    ${SRC_ROOT_DIR}/util/shr_strconvert_mod.F90
    ${SRC_ROOT_DIR}/util/shr_string_mod.F90
    ${SRC_ROOT_DIR}/util/shr_sys_mod.F90
    ${SRC_ROOT_DIR}/util/shr_taskmap_mod.F90
    ${SRC_ROOT_DIR}/util/shr_timer_mod.F90
  )

  if (NOT USE_ESMF_LIB)
    set (SRCS
      ${SRCS}
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_AlarmClockMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_AlarmMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_BaseMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_BaseTimeMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_CalendarMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_ClockMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_FractionMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_ShrTimeMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_Stubs.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_TimeIntervalMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/ESMF_TimeMod.F90
      ${SRC_ROOT_DIR}/esmf_wrf_timemgr/MeatMod.F90
    )
  endif()

  add_library (csm_share ${SRCS})
  target_include_directories(
    csm_share PUBLIC
    ${SHARED_INCLUDE_DIRS}
    ${BIN_DIR}
    ${NetCDF_Fortran_INCLUDE_DIRS}
    ${SRC_ROOT_DIR}/include
  )
  if (NOT USE_ESMF_LIB)
    target_include_directories(csm_share PUBLIC ${SRC_ROOT_DIR}/esmf_wrf_timemgr)
  endif()

  target_link_libraries(csm_share mct)

  set_target_properties(csm_share
      PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${BIN_DIR}"
      LIBRARY_OUTPUT_DIRECTORY "${BIN_DIR}"
      RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}"
      Fortran_MODULE_DIRECTORY "${BIN_DIR}/modules"
  )

  set (SHARED_LIBRARIES
    ${SHARED_LIBRARIES}
    csm_share
  )
  set (SHARED_INCLUDE_DIRS
    ${SHARED_INCLUDE_DIRS}
    ${BIN_DIR}/modules
  )

endmacro()
