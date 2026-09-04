#pragma once

#include <lvgl.h>
#include "util.h"

void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state);
void draw_bongocat(lv_obj_t *canvas, const struct status_state *state);
