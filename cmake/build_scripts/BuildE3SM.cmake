include (CheckComponents)

macro (BuildE3SM)
  # Build stub/data components
  GetDataComponents(${COMPSET} DATA_COMPONENTS)
  foreach (comp IN ${DATA_COMPONENTS})
    # DO SOMETHING
  endforeach()


  # Build e3sm exe
  add_subdirectory(cime/src/drivers/mct/main
                   ${CMAKE_BINARY_DIR}/driver)

  # Set all target properties (e.g., include and link libs/dirs) to e3sm.exe
  target_include_directories(${E3SM_EXE_NAME} PUBLIC "${E3SM_COMPONENTS_INC_DIRS};${SHARED_INCLUDE_DIRS}")
  target_link_libraries(${E3SM_EXE_NAME} ${SHARED_LIBRARIES})
endmacro ()
