/* SPDX-License-Identifier: MIT */
#include "ledm_text.h"
#include "ledm_font.h"

#include <string.h>

int ledm_font5x7_glyph(uint32_t cp, uint8_t out[5])
{
    const uint8_t *src = 0;
    uint8_t i;
    if (cp >= 0x20 && cp <= 0x7E) {
        src = ledm_font5x7_ascii[cp - 0x20];
    } else {
        int lo = 0, hi = (int)ledm_font5x7_extra_count - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            uint16_t c = LEDM_ROM_WORD(&ledm_font5x7_extra_cp[mid]);
            if (c == cp) {
                src = ledm_font5x7_extra[mid];
                break;
            }
            if (c < cp)
                lo = mid + 1;
            else
                hi = mid - 1;
        }
    }
    if (!src)
        return 0;
    for (i = 0; i < 5; i++)
        out[i] = LEDM_ROM_BYTE(src + i);
    return 1;
}

uint32_t ledm_utf8_next(const char *s, size_t len, size_t *pos)
{
    const uint8_t *p = (const uint8_t *)s;
    size_t i = *pos;
    uint32_t cp, min;
    unsigned need, k;
    uint8_t b;
    if (!s || i >= len)
        return 0;
    b = p[i];
    if (b < 0x80) {
        *pos = i + 1;
        return b;
    }
    if ((b & 0xE0) == 0xC0) {
        need = 1; cp = b & 0x1Fu; min = 0x80;
    } else if ((b & 0xF0) == 0xE0) {
        need = 2; cp = b & 0x0Fu; min = 0x800;
    } else if ((b & 0xF8) == 0xF0) {
        need = 3; cp = b & 0x07u; min = 0x10000;
    } else {
        *pos = i + 1;
        return '?';
    }
    if (i + need >= len) { /* truncated at the end */
        *pos = i + 1;
        return '?';
    }
    for (k = 1; k <= need; k++) {
        if ((p[i + k] & 0xC0) != 0x80) {
            *pos = i + 1;
            return '?';
        }
        cp = (cp << 6) | (p[i + k] & 0x3Fu);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        *pos = i + 1;
        return '?';
    }
    *pos = i + 1 + need;
    return cp;
}

uint8_t ledm_glyph(uint32_t cp, uint8_t cols[5], int trim)
{
    uint8_t first = 0, last = 4, i;
    if (!ledm_font5x7_glyph(cp, cols))
        ledm_font5x7_glyph('?', cols);
    if (!trim)
        return 5;
    while (first < 5 && cols[first] == 0)
        first++;
    if (first == 5) { /* space or empty glyph */
        cols[0] = cols[1] = 0;
        return 2;
    }
    while (cols[last] == 0)
        last--;
    for (i = first; i <= last; i++)
        cols[i - first] = cols[i];
    return (uint8_t)(last - first + 1);
}

void ledm_draw_char(ledm *m, int x, int y, uint32_t cp, uint8_t colour)
{
    uint8_t cols[5];
    int c, r;
    ledm_glyph(cp, cols, 0);
    for (c = 0; c < 5; c++)
        for (r = 0; r < 8; r++)
            if ((cols[c] >> r) & 1)
                ledm_set_pixel(m, x + c, y + r, colour);
}

void ledm_show_char(ledm *m, const char *utf8)
{
    uint8_t cols[5], n, c;
    size_t pos = 0;
    uint32_t cp = utf8 ? ledm_utf8_next(utf8, strlen(utf8), &pos) : 0;
    ledm_clear(m);
    if (!cp)
        return;
    n = ledm_glyph(cp, cols, 1);
    for (c = 0; c < n; c++)
        ledm_set_col(m, (8 - n) / 2 + c, cols[c]);
}

int ledm_text_width(const char *utf8)
{
    uint8_t cols[5];
    size_t pos = 0, len;
    uint32_t cp;
    int w = 0, n = 0;
    if (!utf8)
        return 0;
    len = strlen(utf8);
    while ((cp = ledm_utf8_next(utf8, len, &pos)) != 0) {
        w += ledm_glyph(cp, cols, 1);
        n++;
    }
    return n ? w + n - 1 : 0;
}

void ledm_scroll_start(ledm_scroll *s, const char *utf8, uint8_t flags)
{
    s->text = utf8 ? utf8 : "";
    s->len = strlen(s->text);
    s->pos = 0;
    s->ncols = s->ci = 0;
    s->tail = 8;
    s->flags = flags;
    s->running = 1;
}

/* next column of the text stream; 0 = finished */
static int next_col(ledm_scroll *s, uint8_t *col)
{
    for (;;) {
        uint32_t cp;
        if (s->ci < s->ncols) {
            *col = s->cols[s->ci++];
            return 1;
        }
        cp = ledm_utf8_next(s->text, s->len, &s->pos);
        if (cp) {
            uint8_t n = ledm_glyph(cp, s->cols, 1);
            s->cols[n] = 0; /* gap */
            s->ncols = (uint8_t)(n + 1);
            s->ci = 0;
            continue;
        }
        if (s->tail) {
            s->tail--;
            *col = 0;
            return 1;
        }
        if (s->flags & LEDM_SCROLL_LOOP && s->len) {
            s->pos = 0;
            s->tail = 8;
            continue;
        }
        return 0;
    }
}

int ledm_scroll_step(ledm *m, ledm_scroll *s)
{
    uint8_t col;
    if (!s->running)
        return 0;
    if (!next_col(s, &col)) {
        s->running = 0;
        return 0;
    }
    ledm_shift(m, LEDM_LEFT, col, 0);
    /* that was the last column: done now, not one step later */
    if (s->ci >= s->ncols && s->pos >= s->len && s->tail == 0 && !(s->flags & LEDM_SCROLL_LOOP && s->len))
        s->running = 0;
    return 1;
}
