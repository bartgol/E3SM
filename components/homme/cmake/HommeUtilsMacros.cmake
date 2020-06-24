# This file contains some general-purpose utility macros

# Configure a config.h.in file, for both C and F90
#(comments have a different syntax in the two languages!)
macro (HommeConfigFile CONFIG_FILE_IN CONFIG_FILE_C CONFIG_FILE_F90)

  # Configure into a temp file, so we avoid updating the timestamp
  # of the actual output file if nothing changed. This avoids
  # triggering a build when nothing changed in practice.
  configure_file (${CONFIG_FILE_IN} ${CONFIG_FILE_C}.tmp)

  # Assume by default that config file is out of date
  set (OUT_OF_DATE TRUE)

  # If config file in binary dir exists, we check whether the new one would be different
  if (EXISTS ${CONFIG_FILE_C})

    # We rely on FILE macro rather than running diff, since it is
    # more portable (guaranteed to work regardless of underlying system)
    file (READ ${CONFIG_FILE_C} CONFIG_FILE_C_STR)
    file (READ ${CONFIG_FILE_C}.tmp CONFIG_FILE_C_TMP_STR)

    if (CONFIG_FILE_C_STR STREQUAL CONFIG_FILE_C_TMP_STR)
      # config file was present and appears unchanged
      set (OUT_OF_DATE FALSE)
    endif()

    file (REMOVE ${CONFIG_FILE_C}.tmp)
  endif ()

  # If out of date (either missing or different), adjust
  if (OUT_OF_DATE)

    # Run the configure macro
    configure_file (${CONFIG_FILE_IN} ${CONFIG_FILE_C})

    # run sed to change '/*...*/' comments into '!/*...*/'
    execute_process(COMMAND sed "s;^/;!/;g"
                    WORKING_DIRECTORY ${HOMME_BINARY_DIR}
                    INPUT_FILE ${CONFIG_FILE_C}
                    OUTPUT_FILE ${CONFIG_FILE_F90})
  endif()

endmacro (HommeConfigFile)
