/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_gesture

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zmk/hid.h>
#include <zmk/endpoints.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Gesture detection states */
enum gesture_state {
    GESTURE_STATE_IDLE,      /* No touch activity */
    GESTURE_STATE_TOUCHING,  /* Finger on pad, accumulating movement */
    GESTURE_STATE_TAP_WAIT,  /* After short touch, waiting for possible multi-tap */
};

struct gesture_config {
    int tap_timeout_ms;
    int tap_threshold;
    int double_tap_window_ms;
    int swipe_threshold;
    int touch_idle_timeout_ms;
};

struct gesture_data {
    const struct device *dev;
    enum gesture_state state;
    int64_t touch_start_ms;
    int64_t last_event_ms;
    int32_t accum_x;
    int32_t accum_y;
    int32_t pending_x;
    int32_t pending_y;
    int tap_count;
    struct k_work_delayable touch_timeout_work;
    struct k_work_delayable tap_decision_work;
};

/* --- Mouse button emission via ZMK HID --- */

static void emit_mouse_click(uint8_t button_bit) {
    zmk_hid_mouse_button_press(BIT(button_bit));
    zmk_endpoints_send_mouse_report();
}

static void emit_mouse_release(uint8_t button_bit) {
    zmk_hid_mouse_button_release(BIT(button_bit));
    zmk_endpoints_send_mouse_report();
}

/* --- Delayed work handlers (run in system workqueue) --- */

static void touch_timeout_handler(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct gesture_data *data = CONTAINER_OF(dwork, struct gesture_data, touch_timeout_work);
    const struct gesture_config *config = data->dev->config;

    if (data->state != GESTURE_STATE_TOUCHING) {
        return;
    }

    /* No events received for touch_idle_timeout_ms → finger lifted */
    int32_t total_movement = abs(data->accum_x) + abs(data->accum_y);
    int64_t touch_duration = k_uptime_get() - data->touch_start_ms;

    LOG_DBG("Gesture: touch ended, duration=%lld ms, movement=%d",
            touch_duration, total_movement);

    if (total_movement < config->tap_threshold &&
        touch_duration < config->tap_timeout_ms) {
        /* Qualifies as a tap */
        data->tap_count++;
        data->state = GESTURE_STATE_TAP_WAIT;

        /* Wait for possible follow-up tap */
        k_work_schedule(&data->tap_decision_work,
                        K_MSEC(config->double_tap_window_ms));

        LOG_DBG("Gesture: tap #%d detected, waiting for multi-tap", data->tap_count);
    } else if (total_movement >= config->swipe_threshold) {
        /* Qualifies as a swipe */
        int32_t abs_x = abs(data->accum_x);
        int32_t abs_y = abs(data->accum_y);

        if (abs_x > abs_y) {
            LOG_INF("Gesture: swipe %s (dx=%d)",
                    data->accum_x > 0 ? "RIGHT" : "LEFT", data->accum_x);
        } else {
            LOG_INF("Gesture: swipe %s (dy=%d)",
                    data->accum_y > 0 ? "DOWN" : "UP", data->accum_y);
        }
        data->state = GESTURE_STATE_IDLE;
    } else {
        /* Regular movement, not a gesture */
        data->state = GESTURE_STATE_IDLE;
    }

    data->accum_x = 0;
    data->accum_y = 0;
}

static void tap_decision_handler(struct k_work *work) {
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct gesture_data *data = CONTAINER_OF(dwork, struct gesture_data, tap_decision_work);

    if (data->state != GESTURE_STATE_TAP_WAIT) {
        return;
    }

    LOG_INF("Gesture: %d-tap → %s click",
            data->tap_count,
            data->tap_count == 1 ? "left" :
            data->tap_count == 2 ? "right" : "middle");

    uint8_t button;
    switch (data->tap_count) {
    case 1:
        button = 0; /* Left click */
        break;
    case 2:
        button = 1; /* Right click */
        break;
    default:
        button = 2; /* Middle click */
        break;
    }

    emit_mouse_click(button);
    k_msleep(30);
    emit_mouse_release(button);

    data->tap_count = 0;
    data->state = GESTURE_STATE_IDLE;
}

/* --- Input processor event handler --- */

static int gesture_handle_event(const struct device *dev, struct input_event *event,
                                uint32_t param1, uint32_t param2,
                                struct zmk_input_processor_state *state) {
    struct gesture_data *data = dev->data;
    const struct gesture_config *config = dev->config;

    /* Only process relative movement events from the trackpad */
    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* Accumulate deltas within the current event group */
    if (event->code == INPUT_REL_X) {
        data->pending_x = event->value;
    } else if (event->code == INPUT_REL_Y) {
        data->pending_y = event->value;
    }

    /* Process the accumulated group on sync */
    if (!event->sync) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int64_t now = k_uptime_get();

    switch (data->state) {
    case GESTURE_STATE_IDLE:
        /* First event after idle → new touch */
        data->state = GESTURE_STATE_TOUCHING;
        data->touch_start_ms = now;
        data->accum_x = data->pending_x;
        data->accum_y = data->pending_y;
        break;

    case GESTURE_STATE_TOUCHING:
        /* Ongoing touch → accumulate movement */
        data->accum_x += data->pending_x;
        data->accum_y += data->pending_y;
        break;

    case GESTURE_STATE_TAP_WAIT:
        /* New touch while waiting for multi-tap → could be another tap */
        k_work_cancel_delayable(&data->tap_decision_work);
        data->state = GESTURE_STATE_TOUCHING;
        data->touch_start_ms = now;
        data->accum_x = data->pending_x;
        data->accum_y = data->pending_y;
        break;
    }

    data->last_event_ms = now;
    data->pending_x = 0;
    data->pending_y = 0;

    /* (Re)schedule touch-end detection timer */
    k_work_reschedule(&data->touch_timeout_work,
                      K_MSEC(config->touch_idle_timeout_ms));

    /* Pass all movement events through unchanged */
    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api gesture_driver_api = {
    .handle_event = gesture_handle_event,
};

/* --- Device initialization --- */

static int gesture_init(const struct device *dev) {
    struct gesture_data *data = dev->data;
    const struct gesture_config *config = dev->config;

    data->dev = dev;
    data->state = GESTURE_STATE_IDLE;
    data->tap_count = 0;
    data->accum_x = 0;
    data->accum_y = 0;
    data->pending_x = 0;
    data->pending_y = 0;

    k_work_init_delayable(&data->touch_timeout_work, touch_timeout_handler);
    k_work_init_delayable(&data->tap_decision_work, tap_decision_handler);

    LOG_INF("Gesture processor: tap<%dms/%d, multi-tap<%dms, swipe>%d, idle=%dms",
            config->tap_timeout_ms, config->tap_threshold,
            config->double_tap_window_ms, config->swipe_threshold,
            config->touch_idle_timeout_ms);

    return 0;
}

/* --- Device instantiation macro --- */

#define GESTURE_INST(n)                                                        \
    static const struct gesture_config gesture_config_##n = {                  \
        .tap_timeout_ms = DT_INST_PROP_OR(n, tap_timeout_ms, 200),            \
        .tap_threshold = DT_INST_PROP_OR(n, tap_threshold, 50),               \
        .double_tap_window_ms = DT_INST_PROP_OR(n, double_tap_window_ms, 250),\
        .swipe_threshold = DT_INST_PROP_OR(n, swipe_threshold, 300),          \
        .touch_idle_timeout_ms = DT_INST_PROP_OR(n, touch_idle_timeout_ms, 80),\
    };                                                                         \
    static struct gesture_data gesture_data_##n = {};                          \
    DEVICE_DT_INST_DEFINE(n, gesture_init, NULL, &gesture_data_##n,            \
                          &gesture_config_##n, POST_KERNEL,                    \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                 \
                          &gesture_driver_api);

DT_INST_FOREACH_STATUS_OKAY(GESTURE_INST)
