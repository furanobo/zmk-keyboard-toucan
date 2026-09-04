#include <zephyr/kernel.h>
#include "modifiers.h"
#include "../assets/custom_fonts.h"

/* HID modifier bit positions */
#define MOD_LCTL 0x01
#define MOD_LSFT 0x02
#define MOD_LALT 0x04
#define MOD_LGUI 0x08
#define MOD_RCTL 0x10
#define MOD_RSFT 0x20
#define MOD_RALT 0x40
#define MOD_RGUI 0x80

void draw_modifier_status(lv_obj_t *canvas, uint8_t mods) {
    lv_draw_label_dsc_t label_active;
    init_label_dsc(&label_active, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);

    /* Draw each modifier label; active ones get inverted (filled rect + dark text) */
    const char *names[] = {"C", "S", "A", "G"};
    const uint8_t masks[] = {
        MOD_LCTL | MOD_RCTL,
        MOD_LSFT | MOD_RSFT,
        MOD_LALT | MOD_RALT,
        MOD_LGUI | MOD_RGUI,
    };

    int total_w = 4 * 14 + 3 * 6; /* 4 boxes of 14px + 3 gaps of 6px = 74px */
    int x_start = (SCREEN_WIDTH - total_w) / 2;
    int y = 102;

    for (int i = 0; i < 4; i++) {
        int x = x_start + i * 20;
        bool active = (mods & masks[i]) != 0;

        if (active) {
            /* Draw filled rect background */
            lv_draw_rect_dsc_t rect_dsc;
            init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
            lv_canvas_draw_rect(canvas, x, y, 14, 12, &rect_dsc);

            /* Dark text on white background */
            lv_draw_label_dsc_t label_inv;
            init_label_dsc(&label_inv, LVGL_BACKGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);
            lv_canvas_draw_text(canvas, x, y + 2, 14, &label_inv, names[i]);
        } else {
            /* Just draw the letter in foreground color */
            lv_canvas_draw_text(canvas, x, y + 2, 14, &label_active, names[i]);
        }
    }
}
