/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_speed_step

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include <dt-bindings/zmk/speed_step.h>
#include <speed_step/speed_step.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct speed_step_behavior_config {
    const struct device *processor;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (!dev) {
        LOG_ERR("Speed step behavior device not found");
        return -ENODEV;
    }

    const struct speed_step_behavior_config *config = dev->config;
    int delta = (binding->param1 == SPEED_STEP_UP) ? 1 : -1;

    LOG_DBG("Speed step %s at position %d", delta > 0 ? "UP" : "DN", event.position);

    return speed_step_change_level(config->processor, delta);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api speed_step_behavior_driver_api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define SPEED_STEP_BEHAVIOR_INST(n)                                                \
    static const struct speed_step_behavior_config behavior_config_##n = {         \
        .processor = DEVICE_DT_GET(DT_INST_PHANDLE(n, processor)),                 \
    };                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &behavior_config_##n, POST_KERNEL, \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                   \
                            &speed_step_behavior_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SPEED_STEP_BEHAVIOR_INST)
