/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/device.h>

/**
 * @brief Change the speed level of a speed step input processor.
 *
 * @param dev The speed step input processor device.
 * @param delta The amount to change (positive = faster, negative = slower).
 * @return 0 on success, negative errno on failure.
 */
int speed_step_change_level(const struct device *dev, int delta);

/**
 * @brief Get the current speed level index.
 *
 * @param dev The speed step input processor device.
 * @return Current level index, or negative errno on failure.
 */
int speed_step_get_level(const struct device *dev);
