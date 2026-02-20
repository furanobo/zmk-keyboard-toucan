zephyr_include_directories(include ${CMAKE_SOURCE_DIR}/include)

if(CONFIG_ZMK_INPUT_PROCESSOR_SPEED_STEP)
  zephyr_library_named(zmk_speed_step)
  zephyr_library_sources(
    src/input_processor_speed_step.c
  )
  zephyr_library_include_directories(include ${CMAKE_SOURCE_DIR}/include)

  if(CONFIG_ZMK_BEHAVIOR_SPEED_STEP)
    zephyr_library_sources(
      src/behavior_speed_step.c
    )
  endif()
endif()

if(CONFIG_ZMK_INPUT_PROCESSOR_GESTURE)
  zephyr_library_named(zmk_gesture)
  zephyr_library_sources(
    src/input_processor_gesture.c
  )
  zephyr_library_include_directories(include ${CMAKE_SOURCE_DIR}/include)
endif()
