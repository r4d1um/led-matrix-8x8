/* SPDX-License-Identifier: MIT */
#include "ledm.h"

#ifdef __AVR__
#include <avr/pgmspace.h>
#define ROM_BYTE(p) pgm_read_byte(p)
#else
#define ROM_BYTE(p) (*(const uint8_t *)(p))
#endif

void ledm_config_default(ledm_config *cfg, uint8_t type)
{
    if (!cfg)
        return;
    cfg->row_active_high = type == LEDM_COMMON_CATHODE ? 0 : 1;
    cfg->col_active_high = type == LEDM_COMMON_CATHODE ? 1 : 0;
    cfg->scan = LEDM_SCAN_ROWS;
    cfg->no_blank = 0;
    cfg->line_us = 1250;
}

static void out(ledm *m, uint8_t rows, uint8_t cols)
{
    if (m->have_last && rows == m->last_rows && cols == m->last_cols)
        return;
    m->bus.write(m->bus.ctx, rows, cols);
    m->last_rows = rows;
    m->last_cols = cols;
    m->have_last = 1;
}

void ledm_off_levels(const ledm *m, uint8_t *rows, uint8_t *cols)
{
    *rows = m->cfg.row_active_high ? 0x00 : 0xFF;
    *cols = m->cfg.col_active_high ? 0x00 : 0xFF;
}

static void out_off(ledm *m)
{
    uint8_t r, c;
    ledm_off_levels(m, &r, &c);
    out(m, r, c);
}

/* framebuffer -> LED image in scan order, with rotation and mirroring */
static void rebuild(ledm *m)
{
    uint8_t led[8] = {0}; /* led[ROW] bit COL */
    int x, y;
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int px, py;
            if (!(m->fb[y] & (0x80u >> x)))
                continue;
            switch (m->rotation & 3) {
            case 1: px = 7 - y; py = x; break;
            case 2: px = 7 - x; py = 7 - y; break;
            case 3: px = y; py = 7 - x; break;
            default: px = x; py = y; break;
            }
            if (m->mirror)
                px = 7 - px;
            led[py] |= (uint8_t)(1u << px);
        }
    }
    if (m->cfg.scan == LEDM_SCAN_COLS) {
        for (x = 0; x < 8; x++) {
            uint8_t b = 0;
            for (y = 0; y < 8; y++)
                if (led[y] & (1u << x))
                    b |= (uint8_t)(1u << y);
            m->phys[x] = b;
        }
    } else {
        for (y = 0; y < 8; y++)
            m->phys[y] = led[y];
    }
    m->dirty = 0;
}

void ledm_line_levels(ledm *m, uint8_t line, uint8_t *rows, uint8_t *cols)
{
    uint8_t sel, pat;
    if (m->dirty)
        rebuild(m);
    sel = (uint8_t)(1u << (line & 7));
    pat = m->phys[line & 7];
    if (m->cfg.scan == LEDM_SCAN_COLS) {
        *rows = m->cfg.row_active_high ? pat : (uint8_t)~pat;
        *cols = m->cfg.col_active_high ? sel : (uint8_t)~sel;
    } else {
        *rows = m->cfg.row_active_high ? sel : (uint8_t)~sel;
        *cols = m->cfg.col_active_high ? pat : (uint8_t)~pat;
    }
}

/* switch to m->line: blank first (no ghosting while the bus changes pins one by one), then on */
static void show_line(ledm *m, int full)
{
    uint8_t r, c;
    if (!m->cfg.no_blank || !m->enabled)
        out_off(m);
    m->lit = 0;
    if (!m->enabled || (!full && m->brightness == 0) || m->phys[m->line] == 0) {
        out_off(m);
        return;
    }
    ledm_line_levels(m, m->line, &r, &c);
    out(m, r, c);
    m->lit = 1;
}

int ledm_init(ledm *m, const ledm_bus *bus, const ledm_config *cfg)
{
    int i;
    if (!m || !bus || !bus->write || !cfg)
        return -1;
    m->bus = *bus;
    m->cfg = *cfg;
    if (m->cfg.line_us == 0)
        m->cfg.line_us = 1250;
    for (i = 0; i < 8; i++)
        m->fb[i] = m->phys[i] = 0;
    m->dirty = 0;
    m->rotation = 0;
    m->mirror = 0;
    m->brightness = LEDM_BRIGHTNESS_MAX;
    m->enabled = 1;
    m->line = 0;
    m->lit = 0;
    m->started = 0;
    m->have_last = 0;
    m->t_line = 0;
    out_off(m);
    return 0;
}

static uint32_t on_time(const ledm *m)
{
    return (uint32_t)m->cfg.line_us * m->brightness / LEDM_BRIGHTNESS_MAX;
}

uint32_t ledm_service(ledm *m, uint32_t now_us)
{
    uint32_t el, on, slot = m->cfg.line_us;
    if (m->dirty)
        rebuild(m);
    if (!m->enabled) {
        out_off(m);
        m->lit = 0;
        return slot;
    }
    if (!m->started) {
        m->started = 1;
        m->line = 0;
        m->t_line = now_us;
        show_line(m, 0);
    }
    el = now_us - m->t_line;
    if (el >= slot) {
        m->line = (uint8_t)((m->line + 1) & 7);
        /* late by more than a slot: restart the timing instead of racing to catch up */
        m->t_line = el >= 2 * slot ? now_us : m->t_line + slot;
        show_line(m, 0);
        el = now_us - m->t_line;
    }
    on = on_time(m);
    if (m->lit && el >= on && on < slot) {
        out_off(m);
        m->lit = 0;
    }
    if (m->lit)
        return on - el;
    return slot - el;
}

void ledm_step(ledm *m)
{
    if (m->dirty)
        rebuild(m);
    m->line = (uint8_t)((m->line + 1) & 7);
    show_line(m, 1);
}

void ledm_set_brightness(ledm *m, uint8_t level)
{
    m->brightness = level > LEDM_BRIGHTNESS_MAX ? LEDM_BRIGHTNESS_MAX : level;
}

void ledm_enable(ledm *m, int on)
{
    m->enabled = on ? 1 : 0;
    if (!on) {
        out_off(m);
        m->lit = 0;
    }
}

void ledm_set_rotation(ledm *m, uint8_t quarter_turns)
{
    m->rotation = quarter_turns & 3;
    m->dirty = 1;
}

void ledm_set_mirror(ledm *m, int mirror)
{
    m->mirror = mirror ? 1 : 0;
    m->dirty = 1;
}

void ledm_clear(ledm *m)
{
    int i;
    for (i = 0; i < 8; i++)
        m->fb[i] = 0;
    m->dirty = 1;
}

void ledm_fill(ledm *m)
{
    int i;
    for (i = 0; i < 8; i++)
        m->fb[i] = 0xFF;
    m->dirty = 1;
}

void ledm_invert(ledm *m)
{
    int i;
    for (i = 0; i < 8; i++)
        m->fb[i] = (uint8_t)~m->fb[i];
    m->dirty = 1;
}

void ledm_set_pixel(ledm *m, int x, int y, uint8_t colour)
{
    uint8_t bit;
    if (x < 0 || y < 0 || x > 7 || y > 7)
        return;
    bit = (uint8_t)(0x80u >> x);
    if (colour == LEDM_INVERT)
        m->fb[y] ^= bit;
    else if (colour)
        m->fb[y] |= bit;
    else
        m->fb[y] &= (uint8_t)~bit;
    m->dirty = 1;
}

int ledm_get_pixel(const ledm *m, int x, int y)
{
    if (x < 0 || y < 0 || x > 7 || y > 7)
        return 0;
    return (m->fb[y] >> (7 - x)) & 1;
}

void ledm_set_row(ledm *m, int y, uint8_t bits)
{
    if (y < 0 || y > 7)
        return;
    m->fb[y] = bits;
    m->dirty = 1;
}

uint8_t ledm_get_row(const ledm *m, int y)
{
    return (y < 0 || y > 7) ? 0 : m->fb[y];
}

void ledm_set_col(ledm *m, int x, uint8_t bits)
{
    int y;
    if (x < 0 || x > 7)
        return;
    for (y = 0; y < 8; y++)
        ledm_set_pixel(m, x, y, (bits >> y) & 1);
}

uint8_t ledm_get_col(const ledm *m, int x)
{
    uint8_t b = 0;
    int y;
    for (y = 0; y < 8; y++)
        b |= (uint8_t)(ledm_get_pixel(m, x, y) << y);
    return b;
}

void ledm_set_bitmap(ledm *m, const uint8_t rows[8])
{
    int i;
    for (i = 0; i < 8; i++)
        m->fb[i] = rows[i];
    m->dirty = 1;
}

void ledm_set_bitmap_rom(ledm *m, const uint8_t *rows)
{
    int i;
    for (i = 0; i < 8; i++)
        m->fb[i] = ROM_BYTE(rows + i);
    m->dirty = 1;
}

void ledm_shift(ledm *m, uint8_t dir, uint8_t fill, int wrap)
{
    int i;
    uint8_t edge;
    switch (dir) {
    case LEDM_LEFT:
        edge = ledm_get_col(m, 0);
        for (i = 0; i < 8; i++)
            m->fb[i] = (uint8_t)(m->fb[i] << 1);
        ledm_set_col(m, 7, wrap ? edge : fill);
        break;
    case LEDM_RIGHT:
        edge = ledm_get_col(m, 7);
        for (i = 0; i < 8; i++)
            m->fb[i] = (uint8_t)(m->fb[i] >> 1);
        ledm_set_col(m, 0, wrap ? edge : fill);
        break;
    case LEDM_UP:
        edge = m->fb[0];
        for (i = 0; i < 7; i++)
            m->fb[i] = m->fb[i + 1];
        m->fb[7] = wrap ? edge : fill;
        break;
    case LEDM_DOWN:
        edge = m->fb[7];
        for (i = 7; i > 0; i--)
            m->fb[i] = m->fb[i - 1];
        m->fb[0] = wrap ? edge : fill;
        break;
    default:
        return;
    }
    m->dirty = 1;
}

void ledm_level(ledm *m, int x, int height)
{
    uint8_t bits;
    if (height < 0)
        height = 0;
    if (height > 8)
        height = 8;
    bits = (uint8_t)(0xFF00u >> height); /* bottom `height` bits, bit 7 = bottom row */
    ledm_set_col(m, x, bits);
}

void ledm_bars(ledm *m, const uint16_t *values, int n, uint16_t max)
{
    int i;
    if (!values || n <= 0)
        return;
    if (n > 8)
        n = 8;
    for (i = 0; i < n; i++) {
        uint32_t v = values[i];
        int h;
        if (max == 0)
            h = 0;
        else {
            if (v > max)
                v = max;
            h = (int)((v * 8u + max / 2u) / max);
        }
        ledm_level(m, i, h);
    }
}
