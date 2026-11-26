// SPDX-License-Identifier: MIT
// Host tests of the ledm core against the fake matrix (make test, with ASan + UBSan).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "fake_matrix.h"
#include "ledm.h"
#include "ledm_text.h"
#include "test_ui.h"

static const uint8_t HEART[8] = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
static const uint8_t ARROW[8] = {0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x18, 0x18};

struct rig {
    fake_matrix f;
    ledm m;
    ledm_config cfg;
    explicit rig(int part = LEDM_COMMON_ANODE, int cfg_type = -1) : f(part)
    {
        ledm_config_default(&cfg, (uint8_t)(cfg_type < 0 ? part : cfg_type));
    }
    void start()
    {
        ledm_bus b = f.bus();
        CHECK(ledm_init(&m, &b, &cfg) == 0);
    }
    void frames(int n) { run(&m, f, (uint64_t)n * 8 * cfg.line_us); }
};

static bool same(const uint8_t a[8], const uint8_t b[8]) { return std::memcmp(a, b, 8) == 0; }

static void t_init()
{
    SECTION("init");
    fake_matrix f;
    ledm m;
    ledm_config cfg;
    ledm_config_default(&cfg, LEDM_COMMON_ANODE);
    ledm_bus nobus = {nullptr, nullptr};
    CHECK(ledm_init(&m, &nobus, &cfg) == -1);
    CHECK(ledm_init(nullptr, &nobus, &cfg) == -1);
    ledm_bus b = f.bus();
    CHECK(ledm_init(&m, &b, &cfg) == 0);
    CHECK(f.writes == 1 && f.count_on() == 0);
    CHECK(f.rows == 0x00 && f.cols == 0xFF); // common anode, all off
    CHECK(m.brightness == 15 && m.enabled);
    PASS("init writes the all-off levels, rejects a missing bus");
}

static void t_picture(int part, uint8_t scan, const char *what)
{
    rig r(part);
    r.cfg.scan = scan;
    r.start();
    ledm_set_bitmap(&r.m, HEART);
    r.frames(1);
    r.f.reset_counters();
    r.frames(10);
    uint8_t img[8];
    r.f.image(img);
    CHECK(same(img, HEART));
    CHECK(r.f.ghosts == 0);
    CHECK(r.f.max_lit <= 8);
    // every lit LED is on 1/8 of the time
    double worst = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            double want = (HEART[y] >> (7 - x)) & 1 ? 1.0 / 8 : 0.0;
            worst = std::fmax(worst, std::fabs(r.f.duty(y, x) - want));
        }
    CHECK(worst < 0.01);
    PASS(what);
}

static void t_pictures()
{
    SECTION("multiplexing");
    t_picture(LEDM_COMMON_ANODE, LEDM_SCAN_ROWS, "788BS (common anode), row scan: heart visible, 1/8 duty, no ghosting");
    t_picture(LEDM_COMMON_ANODE, LEDM_SCAN_COLS, "788BS, column scan: same picture");
    t_picture(LEDM_COMMON_CATHODE, LEDM_SCAN_ROWS, "788AS (common cathode), row scan");
    t_picture(LEDM_COMMON_CATHODE, LEDM_SCAN_COLS, "788AS, column scan");

    rig wrong(LEDM_COMMON_ANODE, LEDM_COMMON_CATHODE);
    wrong.start();
    ledm_set_bitmap(&wrong.m, HEART);
    wrong.f.reset_counters();
    wrong.frames(4);
    uint8_t img[8];
    wrong.f.image(img);
    CHECK(!same(img, HEART));
    PASS("wrong polarity setting does not show the picture (shows why the type matters)");

    rig r;
    r.start();
    ledm_set_bitmap(&r.m, ARROW);
    r.frames(1);
    r.f.reset_counters();
    r.frames(1);
    // 8 lines, each: blank + on = 16 writes per frame, nothing more
    CHECK(r.f.writes <= 16);
    r.f.reset_counters();
    ledm_clear(&r.m);
    r.frames(2);
    CHECK(r.f.writes <= 1);
    PASS("bus only gets writes when a level changes (16 per frame, 0 for a dark matrix)");

    rig nb;
    nb.cfg.no_blank = 1;
    nb.start();
    ledm_fill(&nb.m);
    nb.frames(1);
    nb.f.reset_counters();
    nb.frames(1);
    CHECK(nb.f.off_writes == 0 && nb.f.writes == 8);
    PASS("no_blank (latched 74HC595): 8 writes per frame, no blank in between");
}

static void t_brightness()
{
    SECTION("brightness and on/off");
    for (int b : {15, 10, 5, 1}) {
        rig r;
        r.start();
        ledm_fill(&r.m);
        ledm_set_brightness(&r.m, (uint8_t)b);
        r.frames(1);
        r.f.reset_counters();
        r.frames(10);
        double want = b / 15.0 / 8.0;
        CHECK(std::fabs(r.f.duty(3, 3) - want) < 0.005);
        CHECK(r.f.ghosts == 0);
    }
    PASS("brightness 15/10/5/1 gives on-time 1/8 x level/15");
    rig r;
    r.start();
    ledm_fill(&r.m);
    ledm_set_brightness(&r.m, 0);
    r.f.reset_counters();
    r.frames(3);
    CHECK(r.f.duty(0, 0) == 0.0);
    ledm_set_brightness(&r.m, 99);
    CHECK(r.m.brightness == 15);
    PASS("brightness 0 = dark, values above 15 are clamped");

    ledm_set_brightness(&r.m, 15);
    r.frames(1);
    ledm_enable(&r.m, 0);
    CHECK(r.f.count_on() == 0);
    r.f.reset_counters();
    r.frames(3);
    CHECK(r.f.duty(4, 4) == 0.0);
    ledm_enable(&r.m, 1);
    r.f.reset_counters();
    r.frames(3);
    CHECK(r.f.duty(4, 4) > 0.1);
    PASS("off() darkens at once and keeps the picture for on()");
}

static void t_timing()
{
    SECTION("timing");
    rig r;
    r.start();
    ledm_fill(&r.m);
    // start just below the 32-bit wrap of micros()
    r.f.now = 0xFFFFFFFFull - 3000;
    r.f.reset_counters();
    uint64_t end = r.f.now + 8 * 1250 * 6;
    while (r.f.now < end) {
        ledm_service(&r.m, (uint32_t)r.f.now);
        r.f.now += 10;
        r.f.at(r.f.now);
    }
    CHECK(std::fabs(r.f.duty(0, 0) - 1.0 / 8) < 0.01 && std::fabs(r.f.duty(7, 7) - 1.0 / 8) < 0.01);
    PASS("survives the micros() wrap at 2^32");

    rig late;
    late.start();
    ledm_fill(&late.m);
    late.frames(1);
    late.f.now += 50000; // the sketch blocked for 50 ms
    late.f.at(late.f.now);
    uint8_t line = late.m.line;
    ledm_service(&late.m, (uint32_t)late.f.now);
    CHECK(late.m.line == ((line + 1) & 7)); // one step, no burst through all lines
    PASS("after a long block it moves on one line and restarts the timing");

    rig h;
    h.start();
    ledm_fill(&h.m);
    ledm_set_brightness(&h.m, 6);
    uint32_t wait = ledm_service(&h.m, 1000);
    CHECK(wait == 1250u * 6 / 15);
    wait = ledm_service(&h.m, 1000 + 500);
    CHECK(wait == 750);
    PASS("returns the time until the next call is needed");

    rig s;
    s.start();
    ledm_set_bitmap(&s.m, ARROW);
    uint8_t seen = 0;
    for (int i = 0; i < 8; i++) {
        ledm_step(&s.m);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
                if (s.f.on(y, x))
                    seen |= (uint8_t)(1u << y);
        CHECK(s.f.ghosts == 0);
    }
    CHECK(seen == 0xFF);
    PASS("ledm_step() (timer interrupt mode) walks through all 8 lines");
}

static void t_orientation()
{
    SECTION("rotation and mirror");
    struct { uint8_t rot; int mirror; int px, py; } cases[] = {
        {0, 0, 0, 0}, {1, 0, 7, 0}, {2, 0, 7, 7}, {3, 0, 0, 7}, {0, 1, 7, 0}, {1, 1, 0, 0},
    };
    for (auto &c : cases) {
        rig r;
        r.start();
        ledm_set_rotation(&r.m, c.rot);
        ledm_set_mirror(&r.m, c.mirror);
        ledm_set_pixel(&r.m, 0, 0, LEDM_ON);
        r.frames(1);
        r.f.reset_counters();
        r.frames(2);
        uint8_t img[8], want[8] = {0};
        want[c.py] = (uint8_t)(0x80u >> c.px);
        r.f.image(img);
        CHECK(same(img, want));
    }
    PASS("pixel (0,0) lands on the right corner for 0/90/180/270 deg and mirrored");
}

static void t_framebuffer()
{
    SECTION("framebuffer");
    rig r;
    r.start();
    ledm *m = &r.m;
    ledm_set_pixel(m, 2, 5, LEDM_ON);
    CHECK(ledm_get_pixel(m, 2, 5) == 1 && ledm_get_row(m, 5) == 0x20);
    ledm_set_pixel(m, 2, 5, LEDM_INVERT);
    CHECK(ledm_get_pixel(m, 2, 5) == 0);
    ledm_set_pixel(m, -1, 0, LEDM_ON);
    ledm_set_pixel(m, 8, 8, LEDM_ON);
    CHECK(ledm_get_pixel(m, 8, 0) == 0 && ledm_get_row(m, 0) == 0);
    PASS("pixels, inversion, clipping");

    ledm_set_col(m, 1, 0x81);
    CHECK(ledm_get_col(m, 1) == 0x81 && ledm_get_row(m, 0) == 0x40 && ledm_get_row(m, 7) == 0x40);
    ledm_set_row(m, 3, 0xF0);
    CHECK(ledm_get_col(m, 0) == 0x08);
    PASS("rows (bit 7 = left) and columns (bit 0 = top)");

    ledm_set_bitmap(m, ARROW);
    ledm_shift(m, LEDM_LEFT, 0, 1);
    ledm_shift(m, LEDM_RIGHT, 0, 1);
    CHECK(same(m->fb, ARROW));
    ledm_shift(m, LEDM_UP, 0, 1);
    CHECK(m->fb[7] == ARROW[0] && m->fb[0] == ARROW[1]);
    ledm_shift(m, LEDM_DOWN, 0, 1);
    CHECK(same(m->fb, ARROW));
    ledm_shift(m, LEDM_DOWN, 0xAA, 0);
    CHECK(m->fb[0] == 0xAA && m->fb[1] == ARROW[0]);
    ledm_clear(m);
    ledm_shift(m, LEDM_LEFT, 0x0F, 0);
    CHECK(ledm_get_col(m, 7) == 0x0F && ledm_get_col(m, 6) == 0);
    PASS("shift in 4 directions, wrap and fill");

    ledm_clear(m);
    ledm_level(m, 0, 3);
    CHECK(ledm_get_col(m, 0) == 0xE0);
    ledm_level(m, 1, 99);
    CHECK(ledm_get_col(m, 1) == 0xFF);
    uint16_t v[4] = {0, 50, 100, 200};
    ledm_bars(m, v, 4, 100);
    CHECK(ledm_get_col(m, 0) == 0x00 && ledm_get_col(m, 1) == 0xF0 && ledm_get_col(m, 3) == 0xFF);
    ledm_invert(m);
    CHECK(ledm_get_col(m, 0) == 0xFF);
    PASS("levels and bar graph (scaled, clamped)");

    ledm_fill(m);
    ledm_set_bitmap_rom(m, HEART);
    CHECK(same(m->fb, HEART));
    PASS("bitmap from ROM");
}

static void t_text()
{
    SECTION("text");
    const char *s = "A\xC2\xB0\xE2\x82\xAC\xFF\xC3";
    size_t pos = 0, len = std::strlen(s);
    CHECK(ledm_utf8_next(s, len, &pos) == 'A');
    CHECK(ledm_utf8_next(s, len, &pos) == 0xB0);
    CHECK(ledm_utf8_next(s, len, &pos) == 0x20AC);
    CHECK(ledm_utf8_next(s, len, &pos) == '?');   // 0xFF
    CHECK(ledm_utf8_next(s, len, &pos) == '?');   // truncated
    CHECK(ledm_utf8_next(s, len, &pos) == 0 && pos == len);
    const char *overlong = "\xC0\xAF";
    pos = 0;
    CHECK(ledm_utf8_next(overlong, 2, &pos) == '?');
    PASS("UTF-8: °, €, bad bytes, truncation and overlong forms");

    uint8_t cols[5];
    CHECK(ledm_glyph('I', cols, 1) == 3);
    CHECK(ledm_glyph(' ', cols, 1) == 2 && cols[0] == 0);
    CHECK(ledm_glyph('W', cols, 0) == 5);
    uint8_t q[5], z[5];
    ledm_glyph('?', q, 0);
    ledm_glyph(0x4E2D, z, 0);
    CHECK(std::memcmp(q, z, 5) == 0);
    CHECK(ledm_text_width("") == 0 && ledm_text_width("I") == 3 && ledm_text_width("II") == 7);
    CHECK(ledm_text_width("I I") == 3 + 1 + 2 + 1 + 3);
    PASS("proportional glyphs, unknown characters as '?', text width");

    rig r;
    r.start();
    ledm_show_char(&r.m, "\xC2\xB0");
    CHECK(ledm_get_row(&r.m, 0) == 0x18 && ledm_get_row(&r.m, 1) == 0x24);
    ledm_clear(&r.m);
    ledm_draw_char(&r.m, 6, 0, 'H', LEDM_ON);
    CHECK(ledm_get_col(&r.m, 6) == 0x7F && ledm_get_col(&r.m, 7) == 0x08);
    PASS("show_char centres a glyph, draw_char clips at the edge");

    ledm_scroll sc;
    ledm_clear(&r.m);
    ledm_scroll_start(&sc, "Hi", 0);
    int steps = 0, max_on = 0;
    while (ledm_scroll_step(&r.m, &sc)) {
        steps++;
        int on = 0;
        for (int y = 0; y < 8; y++)
            on += __builtin_popcount(ledm_get_row(&r.m, y));
        if (on > max_on)
            max_on = on;
        if (steps == 5) // "H" fully on screen, starting at x = 3
            CHECK(ledm_get_col(&r.m, 3) == 0x7F && ledm_get_col(&r.m, 7) == 0x7F);
    }
    CHECK(steps == ledm_text_width("Hi") + 1 + 8);
    CHECK(max_on > 10);
    for (int y = 0; y < 8; y++)
        CHECK(ledm_get_row(&r.m, y) == 0);
    CHECK(!sc.running && ledm_scroll_step(&r.m, &sc) == 0);
    PASS("scroller: text enters right, leaves left, then reports done");

    ledm_scroll_start(&sc, "ab", LEDM_SCROLL_LOOP);
    for (int i = 0; i < 500; i++)
        CHECK(ledm_scroll_step(&r.m, &sc) == 1);
    ledm_scroll_start(&sc, "", LEDM_SCROLL_LOOP);
    int n = 0;
    while (ledm_scroll_step(&r.m, &sc) && n < 100)
        n++;
    CHECK(n == 8);
    ledm_scroll_start(&sc, nullptr, 0);
    n = 0;
    while (ledm_scroll_step(&r.m, &sc) && n < 100)
        n++;
    CHECK(n == 8);
    PASS("loop mode never ends; empty or NULL text ends after clearing the screen");
}

int main()
{
    banner("ledm core  ·  fake 8x8 matrix", LEDM_VERSION);
    t_init();
    t_pictures();
    t_brightness();
    t_timing();
    t_orientation();
    t_framebuffer();
    t_text();
    return summary();
}
