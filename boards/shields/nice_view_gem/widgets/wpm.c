#include <zephyr/kernel.h>
#include <stdio.h>
#include "wpm.h"
#include "../assets/custom_fonts.h"

/* Bongocat sprite data - 24 pixels wide, 14 rows tall.
 * Each uint32_t row: bit 23 = leftmost, bit 0 = rightmost (24-bit wide).
 * Drawn at 3x scale (72x42 on screen).
 *
 * Layout (top to bottom):
 *   rows 0-1:  ears
 *   rows 2-7:  head / face
 *   row  8:    table edge
 *   rows 9-12: paws
 *   row  13:   empty
 */

#define BONGO_W 24
#define BONGO_H 14
#define BONGO_SCALE 3

/* Idle frame 1 – both paws resting on the table */
static const uint32_t bongo_idle1[BONGO_H] = {
    0x0C0300, /* ....XX..........XX...... */
    0x1E0780, /* ...XXXX.......XXXX...... */
    0x1FFF80, /* ...XXXXXXXXXXXXXX....... */
    0x3FFFC0, /* ..XXXXXXXXXXXXXXXXXX.... */
    0x330CC0, /* ..XX..XX....XX..XX...... */
    0x3FFFC0, /* ..XXXXXXXXXXXXXXXXXX.... */
    0x1FDF80, /* ...XXXXXXX.XXXXXXX...... */
    0x0FFF00, /* ....XXXXXXXXXXXX........ */
    0xFFFFFF, /* XXXXXXXXXXXXXXXXXXXXXXXX */
    0x3801C0, /* ..XXX..........XXX...... */
    0x7C03E0, /* .XXXXX........XXXXX..... */
    0x7C03E0, /* .XXXXX........XXXXX..... */
    0x3801C0, /* ..XXX..........XXX...... */
    0x000000, /*                         */
};

/* Idle frame 2 – ears slightly raised */
static const uint32_t bongo_idle2[BONGO_H] = {
    0x1C0700, /* ...XXX.........XXX...... */
    0x1E0780, /* ...XXXX.......XXXX...... */
    0x1FFF80, /* ...XXXXXXXXXXXXXX....... */
    0x3FFFC0, /* ..XXXXXXXXXXXXXXXXXX.... */
    0x330CC0, /* ..XX..XX....XX..XX...... */
    0x3FFFC0, /* ..XXXXXXXXXXXXXXXXXX.... */
    0x1FDF80, /* ...XXXXXXX.XXXXXXX...... */
    0x0FFF00, /* ....XXXXXXXXXXXX........ */
    0xFFFFFF, /* XXXXXXXXXXXXXXXXXXXXXXXX */
    0x3801C0, /* ..XXX..........XXX...... */
    0x7C03E0, /* .XXXXX........XXXXX..... */
    0x7C03E0, /* .XXXXX........XXXXX..... */
    0x3801C0, /* ..XXX..........XXX...... */
    0x000000, /*                         */
};

/* Tap frame 1 – left paw hitting (left paw hidden, right paw on table) */
static const uint32_t bongo_tap1[BONGO_H] = {
    0x0C0300, /* ....XX..........XX...... */
    0x1E0780, /* ...XXXX.......XXXX...... */
    0x1FFF80, /* ...XXXXXXXXXXXXXX....... */
    0x3FFFC0, /* ..XXXXXXXXXXXXXXXXXX.... */
    0x330CC0, /* ..XX..XX....XX..XX...... */
    0x3FFFC0, /* ..XXXXXXXXXXXXXXXXXX.... */
    0x1FDF80, /* ...XXXXXXX.XXXXXXX...... */
    0x0FFF00, /* ....XXXXXXXXXXXX........ */
    0xFFFFFF, /* XXXXXXXXXXXXXXXXXXXXXXXX */
    0x0001C0, /* ...............XXX...... */
    0x0003E0, /* ..............XXXXX..... */
    0x0003E0, /* ..............XXXXX..... */
    0x0001C0, /* ...............XXX...... */
    0x000000, /*                         */
};

/* Tap frame 2 – right paw hitting (right paw hidden, left paw on table) */
static const uint32_t bongo_tap2[BONGO_H] = {
    0x0C0300, /* ....XX..........XX...... */
    0x1E0780, /* ...XXXX.......XXXX...... */
    0x1FFF80, /* ...XXXXXXXXXXXXXX....... */
    0x3FFFC0, /* ..XXXXXXXXXXXXXXXXXX.... */
    0x330CC0, /* ..XX..XX....XX..XX...... */
    0x3FFFC0, /* ..XXXXXXXXXXXXXXXXXX.... */
    0x1FDF80, /* ...XXXXXXX.XXXXXXX...... */
    0x0FFF00, /* ....XXXXXXXXXXXX........ */
    0xFFFFFF, /* XXXXXXXXXXXXXXXXXXXXXXXX */
    0x380000, /* ..XXX................... */
    0x7C0000, /* .XXXXX.................. */
    0x7C0000, /* .XXXXX.................. */
    0x380000, /* ..XXX................... */
    0x000000, /*                         */
};

static uint32_t bongo_frame_counter = 0;

static void draw_sprite(lv_obj_t *canvas, int x_off, int y_off, const uint32_t *sprite) {
    for (int row = 0; row < BONGO_H; row++) {
        uint32_t bits = sprite[row];
        for (int col = 0; col < BONGO_W; col++) {
            if (bits & (1u << (BONGO_W - 1 - col))) {
                for (int sy = 0; sy < BONGO_SCALE; sy++) {
                    for (int sx = 0; sx < BONGO_SCALE; sx++) {
                        lv_canvas_set_px_color(canvas,
                            x_off + col * BONGO_SCALE + sx,
                            y_off + row * BONGO_SCALE + sy,
                            LVGL_FOREGROUND);
                    }
                }
            }
        }
    }
}

void draw_bongocat(lv_obj_t *canvas, const struct status_state *state) {
    bongo_frame_counter++;
    int frame = (bongo_frame_counter / 4) & 1; /* alternate every 4 redraws */

    const uint32_t *sprite;
    uint8_t wpm = state->wpm;

    if (wpm > 10) {
        /* Typing – alternate paws */
        sprite = frame ? bongo_tap2 : bongo_tap1;
    } else {
        /* Idle – subtle ear animation */
        sprite = frame ? bongo_idle2 : bongo_idle1;
    }

    /* Draw centered horizontally at 3x scale, at Y=46 */
    int draw_w = BONGO_W * BONGO_SCALE;  /* 72 */
    int x_off = (SCREEN_WIDTH - draw_w) / 2;
    draw_sprite(canvas, x_off, 46, sprite);
}

void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);

    char wpm_text[16];
    snprintf(wpm_text, sizeof(wpm_text), "WPM:%" PRIu8, state->wpm);

    lv_canvas_draw_text(canvas, 0, 90, SCREEN_WIDTH, &label_dsc, wpm_text);
}
