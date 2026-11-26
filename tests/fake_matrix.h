// SPDX-License-Identifier: MIT
// Fake bare 8x8 matrix for the host tests: gets the 16 line levels with a timestamp and adds up
// how long every LED was lit - what an eye (or a camera with a long exposure) would see.
// It also counts "ghost" moments where LEDs of more than one scan line were lit at once.
#ifndef FAKE_MATRIX_H
#define FAKE_MATRIX_H

#include <cstdint>
#include <cstring>

#include "ledm.h"

struct fake_matrix {
    int type;                 // LEDM_COMMON_ANODE / LEDM_COMMON_CATHODE: the real part
    uint8_t rows = 0, cols = 0;
    uint64_t now = 0, last = 0;
    uint64_t lit[8][8];       // microseconds lit per LED [ROW][COL]
    uint64_t total = 0;       // observed time
    int writes = 0;
    int off_writes = 0;       // writes that switched everything off
    int ghosts = 0;           // writes after which LEDs of two lines were lit at once
    int max_lit = 0;          // most LEDs lit at the same time

    explicit fake_matrix(int t = LEDM_COMMON_ANODE) : type(t)
    {
        std::memset(lit, 0, sizeof lit);
        set_levels(t == LEDM_COMMON_ANODE ? 0x00 : 0xFF, t == LEDM_COMMON_ANODE ? 0xFF : 0x00);
    }
    void set_levels(uint8_t r, uint8_t c) { rows = r; cols = c; }

    bool on(int r, int c) const
    {
        bool rh = (rows >> r) & 1, ch = (cols >> c) & 1;
        return type == LEDM_COMMON_ANODE ? (rh && !ch) : (!rh && ch);
    }
    int count_on() const
    {
        int n = 0;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                n += on(r, c);
        return n;
    }
    // time passes up to t with the current levels
    void at(uint64_t t)
    {
        if (t <= last)
            return;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (on(r, c))
                    lit[r][c] += t - last;
        total += t - last;
        last = t;
    }
    void write(uint8_t r, uint8_t c)
    {
        at(now);
        set_levels(r, c);
        writes++;
        int n = count_on();
        if (n == 0)
            off_writes++;
        if (n > max_lit)
            max_lit = n;
        uint8_t lr = 0, lc = 0;
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                if (on(i, j)) {
                    lr |= (uint8_t)(1u << i);
                    lc |= (uint8_t)(1u << j);
                }
        // more than one row AND more than one column lit = two scan lines at once
        if ((lr & (lr - 1)) && (lc & (lc - 1)))
            ghosts++;
    }
    void reset_counters()
    {
        std::memset(lit, 0, sizeof lit);
        total = 0;
        writes = off_writes = ghosts = max_lit = 0;
        last = now;
    }
    // share of the time an LED was lit, 0..1
    double duty(int r, int c) const { return total ? (double)lit[r][c] / (double)total : 0.0; }
    // LEDs lit more than `threshold` of the time, as 8 row bytes (bit 7 = COL1) like a bitmap
    void image(uint8_t out[8], double threshold = 0.02) const
    {
        for (int r = 0; r < 8; r++) {
            out[r] = 0;
            for (int c = 0; c < 8; c++)
                if (duty(r, c) > threshold)
                    out[r] |= (uint8_t)(0x80u >> c);
        }
    }

    static void bus_write(void *ctx, uint8_t r, uint8_t c) { static_cast<fake_matrix *>(ctx)->write(r, c); }
    ledm_bus bus() { return ledm_bus{bus_write, this}; }
};

// Run the multiplexing for `us` microseconds, calling ledm_service every `step` us.
static inline void run(ledm *m, fake_matrix &f, uint64_t us, uint32_t step = 10)
{
    uint64_t end = f.now + us;
    while (f.now < end) {
        ledm_service(m, (uint32_t)f.now);
        f.now += step;
        f.at(f.now);
    }
}

#endif // FAKE_MATRIX_H
