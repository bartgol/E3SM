set(drv_sources
  cime_comp_mod.F90
  cime_driver.F90
  # component_type_mod.F90
  # map_glc2lnd_mod.F90
  # map_lnd2rof_irrig_mod.F90
  # seq_map_mod.F90
  # seq_map_type_mod.F90
)

add_executable (${E3SM_EXE_NAME} ${drv_sources})
message ("added executable ${E3SM_EXE_NAME}")
