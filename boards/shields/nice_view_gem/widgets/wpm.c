#include <zephyr/kernel.h>
#include <stdio.h>
#include "wpm.h"
#include "../assets/custom_fonts.h"

/* Luna sprite data - 20 pixels wide, 16 rows tall.
 * Each uint32_t row: bit 19 = leftmost, bit 0 = rightmost (20-bit wide).
 * Drawn at 2x scale (40x32 on screen).
 */

#define LUNA_W 20
#define LUNA_H 16
#define LUNA_SCALE 2

/* Sitting frame 1 */
static const uint32_t luna_sit1[LUNA_H] = {
    0x00000, /*                     */
    0x00000, /*                     */
    0x60C00, /* .XX....XX..         */
    0x71C00, /* .XXX..XXX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x6DC00, /* .XX.XX.XX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x3F800, /* ..XXXXXXX..         */
    0x7FE00, /* .XXXXXXXXXX         */
    0xFFF00, /* XXXXXXXXXXXX        */
    0xFFF00, /* XXXXXXXXXXXX        */
    0x7FE00, /* .XXXXXXXXXX         */
    0x3FC00, /* ..XXXXXXXX.         */
    0x1F800, /* ...XXXXXX..         */
    0x19800, /* ...XX..XX..         */
    0x19800, /* ...XX..XX..         */
};

/* Sitting frame 2 (ears slightly different) */
static const uint32_t luna_sit2[LUNA_H] = {
    0x00000, /*                     */
    0x60C00, /* .XX....XX..         */
    0x71C00, /* .XXX..XXX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x6DC00, /* .XX.XX.XX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x3F800, /* ..XXXXXXX..         */
    0x7FE00, /* .XXXXXXXXXX         */
    0xFFF00, /* XXXXXXXXXXXX        */
    0xFFF00, /* XXXXXXXXXXXX        */
    0xFFF00, /* XXXXXXXXXXXX        */
    0x7FE00, /* .XXXXXXXXXX         */
    0x3FC00, /* ..XXXXXXXX.         */
    0x1F800, /* ...XXXXXX..         */
    0x19800, /* ...XX..XX..         */
    0x00000, /*                     */
};

/* Walking frame 1 */
static const uint32_t luna_walk1[LUNA_H] = {
    0x00000, /*                     */
    0x00000, /*                     */
    0x60C00, /* .XX....XX..         */
    0x71C00, /* .XXX..XXX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x6DC00, /* .XX.XX.XX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x3F800, /* ..XXXXXXX..         */
    0xFFF00, /* XXXXXXXXXXXX        */
    0xFFF00, /* XXXXXXXXXXXX        */
    0xFFF00, /* XXXXXXXXXXXX        */
    0x7FE00, /* .XXXXXXXXXX         */
    0x3FC00, /* ..XXXXXXXX.         */
    0x33600, /* ..XX.XX.XX.         */
    0x21200, /* ..X....X..X         */
    0x00000, /*                     */
};

/* Walking frame 2 */
static const uint32_t luna_walk2[LUNA_H] = {
    0x00000, /*                     */
    0x00000, /*                     */
    0x60C00, /* .XX....XX..         */
    0x71C00, /* .XXX..XXX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x6DC00, /* .XX.XX.XX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x3F800, /* ..XXXXXXX..         */
    0xFFF00, /* XXXXXXXXXXXX        */
    0xFFF00, /* XXXXXXXXXXXX        */
    0xFFF00, /* XXXXXXXXXXXX        */
    0x7FE00, /* .XXXXXXXXXX         */
    0x3FC00, /* ..XXXXXXXX.         */
    0x36C00, /* ..XX.XX.XX.         */
    0x24800, /* ..X..X..X..         */
    0x00000, /*                     */
};

/* Running frame 1 */
static const uint32_t luna_run1[LUNA_H] = {
    0x00000, /*                     */
    0x00000, /*                     */
    0x60C00, /* .XX....XX..         */
    0x71C00, /* .XXX..XXX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x6DC00, /* .XX.XX.XX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x3F800, /* ..XXXXXXX..         */
    0xFFF80, /* XXXXXXXXXXXXX       */
    0xFFFC0, /* XXXXXXXXXXXXXX      */
    0xFFF80, /* XXXXXXXXXXXXX       */
    0x7FF00, /* .XXXXXXXXXXX        */
    0x3FE00, /* ..XXXXXXXXX         */
    0x37600, /* ..XX.XXX.XX.        */
    0x63300, /* .XX..XX..XX.        */
    0x00000, /*                     */
};

/* Running frame 2 */
static const uint32_t luna_run2[LUNA_H] = {
    0x00000, /*                     */
    0x00000, /*                     */
    0x60C00, /* .XX....XX..         */
    0x71C00, /* .XXX..XXX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x6DC00, /* .XX.XX.XX..         */
    0x7FC00, /* .XXXXXXXXX.         */
    0x3F800, /* ..XXXXXXX..         */
    0x1FFC0, /* ...XXXXXXXXXX       */
    0x3FFE0, /* ..XXXXXXXXXXXX      */
    0x1FFC0, /* ...XXXXXXXXXX       */
    0x0FFE0, /* ....XXXXXXXXXXX     */
    0x07FC0, /* .....XXXXXXXXX      */
    0x06EC0, /* .....XX.XXX.X       */
    0x0CC60, /* ....XX..XX..XX      */
    0x00000, /*                     */
};

static uint32_t luna_frame_counter = 0;

static void draw_sprite(lv_obj_t *canvas, int x_off, int y_off, const uint32_t *sprite) {
    for (int row = 0; row < LUNA_H; row++) {
        uint32_t bits = sprite[row];
        for (int col = 0; col < LUNA_W; col++) {
            if (bits & (1u << (LUNA_W - 1 - col))) {
                for (int sy = 0; sy < LUNA_SCALE; sy++) {
                    for (int sx = 0; sx < LUNA_SCALE; sx++) {
                        lv_canvas_set_px_color(canvas,
                            x_off + col * LUNA_SCALE + sx,
                            y_off + row * LUNA_SCALE + sy,
                            LVGL_FOREGROUND);
                    }
                }
            }
        }
    }
}

void draw_luna(lv_obj_t *canvas, const struct status_state *state) {
    luna_frame_counter++;
    int frame = (luna_frame_counter / 4) & 1; /* alternate every 4 redraws */

    const uint32_t *sprite;
    uint8_t wpm = state->wpm;

    if (wpm > 40) {
        sprite = frame ? luna_run2 : luna_run1;
    } else if (wpm > 10) {
        sprite = frame ? luna_walk2 : luna_walk1;
    } else {
        sprite = frame ? luna_sit2 : luna_sit1;
    }

    /* Draw centered horizontally at 2x scale, at Y=52 */
    int draw_w = LUNA_W * LUNA_SCALE;
    int x_off = (SCREEN_WIDTH - draw_w) / 2;
    draw_sprite(canvas, x_off, 52, sprite);
}

void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);

    char wpm_text[16];
    snprintf(wpm_text, sizeof(wpm_text), "WPM:%" PRIu8, state->wpm);

    lv_canvas_draw_text(canvas, 0, 88, SCREEN_WIDTH, &label_dsc, wpm_text);
}
