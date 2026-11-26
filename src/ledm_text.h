/* SPDX-License-Identifier: MIT */
/*
 * ledm_text - characters and scrolling text on an 8x8 matrix, with the built-in 5x7 font.
 *
 * Text is UTF-8. The font has ASCII plus ° ± ² ³ µ · Ä Ö Ü ß ä ö ü € ← ↑ → ↓ █; anything else
 * is shown as '?'. On the scroller and in ledm_text_width() glyphs are proportional: empty
 * columns at the sides are dropped, one blank column separates two characters and a space is
 * three columns wide - "Hi 23°C" reads better on 8 pixels that way.
 */
#ifndef LEDM_TEXT_H
#define LEDM_TEXT_H

#include "ledm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Decode the next code point of s[0..len) at *pos and advance *pos. Bad sequences give '?'
 * and advance by one byte. At the end: returns 0 and leaves *pos. */
uint32_t ledm_utf8_next(const char *s, size_t len, size_t *pos);

/* Glyph of cp as columns (bit 0 = top), '?' when the font lacks it. With trim set the empty
 * side columns are dropped (a space gives 2 empty columns). Returns the number of columns. */
uint8_t ledm_glyph(uint32_t cp, uint8_t cols[5], int trim);

/* Draw cp with its top-left corner at (x, y), full 5-column cell, clipped. Pixels of the glyph
 * get `colour` (LEDM_ON / LEDM_OFF / LEDM_INVERT), the rest is left as it is. */
void ledm_draw_char(ledm *m, int x, int y, uint32_t cp, uint8_t colour);

/* Clear and show the first character of utf8 centred. */
void ledm_show_char(ledm *m, const char *utf8);

/* Width in columns of utf8 on the scroller (proportional, one gap between characters). */
int ledm_text_width(const char *utf8);

/* Scroll text from right to left, one column per ledm_scroll_step(). */
#define LEDM_SCROLL_LOOP 0x01u   /* start over after the text (8 blank columns in between) */

typedef struct {
    const char *text;     /* not copied: must stay valid while scrolling */
    size_t len, pos;
    uint8_t cols[6];      /* current glyph plus its gap column */
    uint8_t ncols, ci;
    uint8_t tail;         /* blank columns still to send after the text */
    uint8_t flags;
    uint8_t running;
} ledm_scroll;

/* Start (or restart) scrolling utf8. The text enters from the right edge. */
void ledm_scroll_start(ledm_scroll *s, const char *utf8, uint8_t flags);
/* Shift the matrix one column left and feed the next text column. Returns 1 when it moved,
 * 0 when there was nothing left to scroll. s->running drops to 0 with the step that moves the
 * text off the screen completely (never with LEDM_SCROLL_LOOP). */
int ledm_scroll_step(ledm *m, ledm_scroll *s);

#ifdef __cplusplus
}
#endif
#endif /* LEDM_TEXT_H */
