/* SPDX-License-Identifier: MIT */
/*
 * ledm - driver core for bare 8x8 LED dot matrices (788BS, 788AS, 1088BS, 1088AS, 1588 and other
 * 16-pin single-colour 8x8 modules), C99.
 *
 * A bare matrix has no controller: 8 row lines and 8 column lines, one LED at every crossing.
 * The picture has to be multiplexed - one scan line at a time, fast enough that the eye sees a
 * still image. This core does that part:
 *
 *   - keeps an 8x8 framebuffer (one byte per row, bit 7 = leftmost pixel),
 *   - turns it into the electrical levels of the 16 lines for the current scan line, with the
 *     polarity of the module (common anode "BS" / common cathode "AS") and of any driver
 *     transistors applied,
 *   - times the scan lines and the brightness (on-time per line) from a microsecond clock the
 *     caller passes in, and blanks all lines between two scan lines so nothing ghosts.
 *
 * It has no hardware access: the caller passes a bus with one write callback that sets the 16
 * lines. The Arduino adapter LedMatrix8x8.h ships two ready-made buses (16 GPIOs, or two
 * 74HC595). No malloc, no float, no Arduino headers.
 *
 *     static ledm m;
 *     ledm_config cfg;
 *     ledm_config_default(&cfg, LEDM_COMMON_ANODE);     // 788BS
 *     ledm_init(&m, &bus, &cfg);
 *     ledm_set_pixel(&m, 3, 4, LEDM_ON);
 *     for (;;) ledm_service(&m, micros());             // as often as possible
 *
 * Coordinates: x = 0..7 left to right (COL1..COL8), y = 0..7 top to bottom (ROW1..ROW8), before
 * rotation. Rotation and mirroring only change how the framebuffer is mapped to the LEDs.
 */
#ifndef LEDM_H
#define LEDM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEDM_VERSION "0.1.0"

/* Module pin numbers (1..16; pins 1..8 are on the side with the printed part number, pin 1 at
 * the left end when you look at the LED face with that side towards you; pins 9..16 run back
 * on the other side, 16 opposite 1) of ROW1..ROW8 and
 * COL1..COL8. The same for 788xS, 1088xS and 1588xS; "BS" = common anode (ROW = anode),
 * "AS" = common cathode (ROW = cathode). */
#define LEDM_ROW_PINS { 9, 14, 8, 12, 1, 7, 2, 5 }
#define LEDM_COL_PINS { 13, 3, 4, 10, 6, 11, 15, 16 }

/* Module types */
enum {
    LEDM_COMMON_ANODE = 0,   /* 788BS, 1088BS, 1588BS: LED on = ROW high, COL low */
    LEDM_COMMON_CATHODE = 1  /* 788AS, 1088AS, 1588AS: LED on = ROW low, COL high */
};

/* Scan direction: which lines are switched on one after another. The other 8 lines carry the
 * pixel pattern and get the current-limiting resistors. The active scan line carries the
 * current of up to 8 LEDs (see README, "Current"). */
enum {
    LEDM_SCAN_ROWS = 0,      /* one row at a time, resistors on the COL lines (default) */
    LEDM_SCAN_COLS = 1       /* one column at a time, resistors on the ROW lines */
};

/* Pixel colours */
enum { LEDM_OFF = 0, LEDM_ON = 1, LEDM_INVERT = 2 };

/* Shift directions for ledm_shift() */
enum { LEDM_LEFT = 0, LEDM_RIGHT = 1, LEDM_UP = 2, LEDM_DOWN = 3 };

#define LEDM_BRIGHTNESS_MAX 15

/*
 * Bus: sets all 16 lines. rows bit i = electrical level of ROW(i+1), cols bit i = level of
 * COL(i+1); 1 = HIGH. Polarity is already applied, the callback only copies bits to pins.
 * Called from ledm_service() only when a level changes. Keep it fast: with the default timing
 * it runs up to 1600 times per second.
 */
typedef struct {
    void (*write)(void *ctx, uint8_t rows, uint8_t cols);
    void *ctx;
} ledm_bus;

typedef struct {
    uint8_t row_active_high; /* 1: a ROW line is HIGH when its LEDs light (common anode) */
    uint8_t col_active_high; /* 1: a COL line is HIGH when its LEDs light (common cathode) */
    uint8_t scan;            /* LEDM_SCAN_ROWS / LEDM_SCAN_COLS */
    uint8_t no_blank;        /* 1: skip the all-off write between scan lines (only if the bus
                                sets all 16 lines at once, e.g. 74HC595 with a latch) */
    uint16_t line_us;        /* time per scan line; 8 lines = one frame. Default 1250 (100 Hz) */
} ledm_config;

/* Fills cfg for a module type (LEDM_COMMON_ANODE / LEDM_COMMON_CATHODE) wired straight to the
 * MCU or to 74HC595 outputs: row scan, 1250 us per line. Driver transistors that invert a line
 * (PNP high side, ULN2803 low side) need the matching *_active_high flipped afterwards. */
void ledm_config_default(ledm_config *cfg, uint8_t type);

typedef struct {
    ledm_bus bus;
    ledm_config cfg;
    uint8_t fb[8];        /* framebuffer: fb[y] bit (7 - x) */
    uint8_t phys[8];      /* LED image: phys[scan line] bit k = LED on the k-th crossing line */
    uint8_t dirty;        /* fb changed since phys was built */
    uint8_t rotation;     /* 0..3, quarter turns clockwise */
    uint8_t mirror;       /* 1: mirror left/right (after rotation) */
    uint8_t brightness;   /* 0..15 */
    uint8_t enabled;      /* 0 = display off (all lines inactive) */
    uint8_t line;         /* current scan line 0..7 */
    uint8_t lit;          /* current line is switched on */
    uint8_t started;      /* timing initialised */
    uint8_t last_rows, last_cols, have_last;
    uint32_t t_line;      /* start of the current scan line (us) */
} ledm;

/* Sets up m (framebuffer cleared, brightness max, display on) and switches all lines off.
 * Returns 0, or -1 for a NULL argument or a missing write callback. */
int ledm_init(ledm *m, const ledm_bus *bus, const ledm_config *cfg);

/*
 * Multiplexing. Call as often as possible with a free-running microsecond clock (wraps at 2^32
 * are fine). Switches the current line off when its on-time is over and moves to the next line
 * when its slot is over. Returns the microseconds until the next call is needed (0 = now);
 * calling earlier is harmless. Flicker or uneven rows mean the calls come too seldom - call it
 * from a timer interrupt or a tight loop then.
 */
uint32_t ledm_service(ledm *m, uint32_t now_us);

/* One scan step for a fixed-rate timer interrupt instead of ledm_service(): shows the next
 * line at full brightness. Call it every line_us microseconds. */
void ledm_step(ledm *m);

/* Brightness 0..15 (0 = dark, 15 = each line on for its whole slot). Values above 15 = 15. */
void ledm_set_brightness(ledm *m, uint8_t level);
/* Display on/off. Off keeps the framebuffer and switches all lines inactive at once. */
void ledm_enable(ledm *m, int on);
/* Orientation: rotation 0..3 quarter turns clockwise, mirror left/right. */
void ledm_set_rotation(ledm *m, uint8_t quarter_turns);
void ledm_set_mirror(ledm *m, int mirror);

/* Framebuffer */
void ledm_clear(ledm *m);
void ledm_fill(ledm *m);
void ledm_invert(ledm *m);
void ledm_set_pixel(ledm *m, int x, int y, uint8_t colour);     /* outside 0..7: ignored */
int ledm_get_pixel(const ledm *m, int x, int y);                 /* outside: 0 */
void ledm_set_row(ledm *m, int y, uint8_t bits);                 /* bit 7 = x 0 */
uint8_t ledm_get_row(const ledm *m, int y);
void ledm_set_col(ledm *m, int x, uint8_t bits);                 /* bit 0 = y 0 (top) */
uint8_t ledm_get_col(const ledm *m, int x);
/* Whole picture from 8 row bytes (bit 7 = left), e.g. { 0x3C, 0x42, ... }. */
void ledm_set_bitmap(ledm *m, const uint8_t rows[8]);
/* Same, from flash on AVR (PROGMEM); elsewhere identical to ledm_set_bitmap. */
void ledm_set_bitmap_rom(ledm *m, const uint8_t *rows);
/* Shift the picture one pixel; the new edge gets `fill` (row/column bits as in set_row /
 * set_col), or the pixels that fell off when wrap is set. */
void ledm_shift(ledm *m, uint8_t dir, uint8_t fill, int wrap);
/* Vertical bar in column x: the bottom `height` pixels on (0..8), the rest off. */
void ledm_level(ledm *m, int x, int height);
/* Bar graph over all columns: values[0..n-1] scaled to 0..8 against max (n <= 8). */
void ledm_bars(ledm *m, const uint16_t *values, int n, uint16_t max);

/* For tests and tools: the levels the bus gets for scan line `line` (0..7) when it is lit. */
void ledm_line_levels(ledm *m, uint8_t line, uint8_t *rows, uint8_t *cols);
/* The all-off levels. */
void ledm_off_levels(const ledm *m, uint8_t *rows, uint8_t *cols);

#ifdef __cplusplus
}
#endif
#endif /* LEDM_H */
