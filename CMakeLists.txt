zephyr_include_directories(include)

if(CONFIG_ZMK_INPUT_PROCESSOR_SPEED_STEP)
  zephyr_library_named(zmk_speed_step)
  zephyr_library_sources(
    src/input_processor_speed_step.c
  )
  zephyr_library_include_directories(include)

  if(CONFIG_ZMK_BEHAVIOR_SPEED_STEP)
    zephyr_library_sources(
      src/behavior_speed_step.c
    )
  endif()
endif()
