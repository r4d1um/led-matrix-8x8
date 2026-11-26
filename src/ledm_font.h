/* SPDX-License-Identifier: MIT */
/* Built-in 5x7 font: ASCII plus ° ± ² ³ µ · Ä Ö Ü ß ä ö ü € ← ↑ → ↓ █. Columns, bit 0 = top. */
#ifndef LEDM_FONT_H
#define LEDM_FONT_H

#include <stdint.h>

/* AVR: keep the font in flash (avr-libc, no Arduino headers needed) */
#ifdef __AVR__
#include <avr/pgmspace.h>
#define LEDM_ROM PROGMEM
#define LEDM_ROM_BYTE(p) pgm_read_byte(p)
#define LEDM_ROM_WORD(p) pgm_read_word(p)
#else
#define LEDM_ROM
#define LEDM_ROM_BYTE(p) (*(const uint8_t *)(p))
#define LEDM_ROM_WORD(p) (*(const uint16_t *)(p))
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t  ledm_font5x7_ascii[95][5] LEDM_ROM;
extern const uint16_t ledm_font5x7_extra_cp[] LEDM_ROM;
extern const uint8_t  ledm_font5x7_extra[][5] LEDM_ROM;
extern const uint8_t  ledm_font5x7_extra_count;

/* Copy the 5 column bytes (bit 0 = top row) of a code point to out. Returns 1, or 0 when the
 * font has no glyph for it (out untouched). */
int ledm_font5x7_glyph(uint32_t cp, uint8_t out[5]);

#ifdef __cplusplus
}
#endif
#endif /* LEDM_FONT_H */
