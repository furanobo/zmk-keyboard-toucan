/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_speed_step

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif

#include <speed_step/speed_step.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct speed_step_config {
    uint8_t type;
    size_t codes_len;
    const uint16_t *codes;
    size_t num_levels;
    const int32_t *multipliers;
    int32_t divisor;
    int default_level;
    const char *settings_name;
};

struct speed_step_data {
    int current_level;
};

static int speed_step_handle_event(const struct device *dev, struct input_event *event,
                                   uint32_t param1, uint32_t param2,
                                   struct zmk_input_processor_state *state) {
    const struct speed_step_config *config = dev->config;
    struct speed_step_data *data = dev->data;

    if (event->type != config->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    for (int i = 0; i < config->codes_len; i++) {
        if (config->codes[i] == event->code) {
            int32_t mul = config->multipliers[data->current_level];
            int32_t div = config->divisor;

            int16_t value_mul = event->value * (int16_t)mul;

            if (state && state->remainder) {
                value_mul += *state->remainder;
            }

            int16_t scaled = value_mul / (int16_t)div;

            if (state && state->remainder) {
                *state->remainder = value_mul - (scaled * (int16_t)div);
            }

            LOG_DBG("speed_step [%s] level=%d mul=%d/%d: %d -> %d",
                    config->settings_name, data->current_level,
                    mul, div, event->value, scaled);

            event->value = scaled;
            return ZMK_INPUT_PROC_CONTINUE;
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api speed_step_driver_api = {
    .handle_event = speed_step_handle_event,
};

/* --- Public API for behavior to call --- */

int speed_step_change_level(const struct device *dev, int delta) {
    const struct speed_step_config *config = dev->config;
    struct speed_step_data *data = dev->data;

    int new_level = data->current_level + delta;

    if (new_level < 0) {
        new_level = 0;
    }
    if (new_level >= (int)config->num_levels) {
        new_level = config->num_levels - 1;
    }

    if (new_level == data->current_level) {
        LOG_INF("Speed [%s] already at %s (level %d, mul %d/%d)",
                config->settings_name,
                delta > 0 ? "max" : "min",
                data->current_level,
                config->multipliers[data->current_level],
                config->divisor);
        return 0;
    }

    data->current_level = new_level;

    LOG_INF("Speed [%s] changed to level %d/%d (mul %d/%d)",
            config->settings_name,
            new_level, (int)config->num_levels - 1,
            config->multipliers[new_level],
            config->divisor);

#if IS_ENABLED(CONFIG_SETTINGS)
    char key[24];
    snprintf(key, sizeof(key), "spd/%s", config->settings_name);
    settings_save_one(key, &data->current_level, sizeof(data->current_level));
#endif

    return 0;
}

int speed_step_get_level(const struct device *dev) {
    struct speed_step_data *data = dev->data;
    return data->current_level;
}

/* --- Settings persistence --- */

#if IS_ENABLED(CONFIG_SETTINGS)

/* Registry of all speed step devices for settings restore */
#define MAX_SPEED_STEP_INSTANCES 4
static const struct device *ss_devices[MAX_SPEED_STEP_INSTANCES];
static int ss_device_count = 0;

static int speed_step_settings_set(const char *name, size_t len,
                                   settings_read_cb read_cb, void *cb_arg) {
    for (int i = 0; i < ss_device_count; i++) {
        const struct speed_step_config *config = ss_devices[i]->config;
        if (strcmp(name, config->settings_name) == 0) {
            struct speed_step_data *data = ss_devices[i]->data;
            int level;
            int rc = read_cb(cb_arg, &level, sizeof(level));
            if (rc <= 0) {
                LOG_ERR("Failed to read speed setting '%s' (err %d)", name, rc);
                return rc;
            }
            if (level >= 0 && level < (int)config->num_levels) {
                data->current_level = level;
                LOG_INF("Restored speed [%s] to level %d (mul %d/%d)",
                        config->settings_name, level,
                        config->multipliers[level], config->divisor);
            } else {
                LOG_WRN("Invalid saved speed level %d for '%s', keeping default %d",
                        level, name, data->current_level);
            }
            return 0;
        }
    }

    LOG_WRN("Unknown speed setting: %s", name);
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(speed_step, "spd", NULL, speed_step_settings_set, NULL, NULL);

#endif /* CONFIG_SETTINGS */

/* --- Device instantiation --- */

static int speed_step_init(const struct device *dev) {
    const struct speed_step_config *config = dev->config;
    struct speed_step_data *data = dev->data;

    data->current_level = config->default_level;

    if (data->current_level < 0 || data->current_level >= (int)config->num_levels) {
        LOG_ERR("Invalid default-level %d for speed step '%s'",
                data->current_level, config->settings_name);
        data->current_level = 0;
    }

#if IS_ENABLED(CONFIG_SETTINGS)
    if (ss_device_count < MAX_SPEED_STEP_INSTANCES) {
        ss_devices[ss_device_count++] = dev;
    } else {
        LOG_ERR("Too many speed step instances (max %d)", MAX_SPEED_STEP_INSTANCES);
    }
#endif

    LOG_INF("Speed step [%s] initialized: %d levels, default=%d (mul %d/%d)",
            config->settings_name, (int)config->num_levels, data->current_level,
            config->multipliers[data->current_level], config->divisor);

    return 0;
}

#define SPEED_STEP_INST(n)                                                         \
    static const uint16_t speed_step_codes_##n[] = DT_INST_PROP(n, codes);         \
    static const int32_t speed_step_muls_##n[] = DT_INST_PROP(n, speed_multipliers); \
    static const struct speed_step_config speed_step_config_##n = {                \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                            \
        .codes = speed_step_codes_##n,                                             \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                  \
        .multipliers = speed_step_muls_##n,                                        \
        .num_levels = DT_INST_PROP_LEN(n, speed_multipliers),                      \
        .divisor = DT_INST_PROP_OR(n, speed_divisor, 100),                         \
        .default_level = DT_INST_PROP(n, default_level),                           \
        .settings_name = DT_INST_PROP(n, settings_name),                           \
    };                                                                             \
    static struct speed_step_data speed_step_data_##n = {};                        \
    DEVICE_DT_INST_DEFINE(n, speed_step_init, NULL, &speed_step_data_##n,          \
                          &speed_step_config_##n, POST_KERNEL,                     \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                     \
                          &speed_step_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SPEED_STEP_INST)
