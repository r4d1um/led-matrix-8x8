// SPDX-License-Identifier: MIT
// Sample pictures for the README: every picture is drawn through the core, multiplexed into the
// fake matrix from tests/ for 20 frames, and each LED is painted with the brightness the fake
// measured (its share of lit time). So the PNG shows what the real module shows - including
// brightness levels. Labels use the library's own font.
//
//   screens <outdir>   ->  <outdir>/overview.png, <outdir>/scroll.png, one PNG per picture
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "fake_matrix.h"
#include "ledm.h"
#include "ledm_text.h"
#include "png.h"

static const uint32_t BG = 0x16181D, BODY = 0x0B0B0D, LABEL = 0xC9CDD6;
static const int PITCH = 18, DOT = 13, PAD = 10, TILE = 8 * PITCH + 2 * PAD;

struct pic {
    std::string name, label;
    uint8_t fb[8];
    uint8_t brightness;
    uint8_t rotation;
    double duty[8][8];
};

// multiplex fb through the core into the fake and measure every LED
static void measure(pic &p)
{
    fake_matrix f(LEDM_COMMON_ANODE);
    ledm m;
    ledm_config cfg;
    ledm_config_default(&cfg, LEDM_COMMON_ANODE);
    ledm_bus b = f.bus();
    ledm_init(&m, &b, &cfg);
    ledm_set_bitmap(&m, p.fb);
    ledm_set_brightness(&m, p.brightness);
    ledm_set_rotation(&m, p.rotation);
    run(&m, f, 8 * 1250 * 2);
    f.reset_counters();
    run(&m, f, 8 * 1250 * 20);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            p.duty[y][x] = f.duty(y, x) * 8.0; // 1.0 = full brightness
}

static uint32_t led_colour(double v)
{
    // off LEDs stay visible as dark red discs, lit ones go to bright red/orange
    double r = 0x2A + v * (0xFF - 0x2A), g = 0x08 + v * (0x3C - 0x08), bl = 0x08 + v * (0x28 - 0x08);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

static void draw_tile(rgb_image &img, int ox, int oy, const pic &p)
{
    img.rect(ox, oy, TILE, TILE, BODY);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            double v = p.duty[y][x];
            int cx = ox + PAD + x * PITCH + PITCH / 2, cy = oy + PAD + y * PITCH + PITCH / 2;
            // soft glow around lit LEDs
            if (v > 0.01)
                for (int j = -PITCH / 2; j <= PITCH / 2; j++)
                    for (int i = -PITCH / 2; i <= PITCH / 2; i++) {
                        double d = std::sqrt((double)(i * i + j * j));
                        if (d > DOT / 2.0 && d < PITCH / 2.0 + 1) {
                            double g = v * 0.35 * (1.0 - (d - DOT / 2.0) / (PITCH / 2.0 - DOT / 2.0 + 1));
                            if (g > 0)
                                img.set(cx + i, cy + j, ((uint32_t)(0x0B + g * 0xC0) << 16) | 0x0B0D);
                        }
                    }
            for (int j = -DOT / 2; j <= DOT / 2; j++)
                for (int i = -DOT / 2; i <= DOT / 2; i++)
                    if (i * i + j * j <= (DOT / 2) * (DOT / 2))
                        img.set(cx + i, cy + j, led_colour(v));
        }
}

// text with the library's own font, 2x scaled
static int text_px(const std::string &s) { return 2 * (ledm_text_width(s.c_str())); }
static void draw_text(rgb_image &img, int x, int y, const std::string &s, uint32_t col)
{
    size_t pos = 0;
    uint32_t cp;
    while ((cp = ledm_utf8_next(s.c_str(), s.size(), &pos)) != 0) {
        uint8_t cols[5];
        uint8_t n = ledm_glyph(cp, cols, 1);
        for (int c = 0; c < n; c++)
            for (int r = 0; r < 8; r++)
                if ((cols[c] >> r) & 1)
                    img.rect(x + 2 * c, y + 2 * r, 2, 2, col);
        x += 2 * (n + 1);
    }
}

static pic make(const char *name, const char *label, const uint8_t fb[8], uint8_t br = 15, uint8_t rot = 0)
{
    pic p;
    p.rotation = rot;
    p.name = name;
    p.label = label;
    std::memcpy(p.fb, fb, 8);
    p.brightness = br;
    measure(p);
    return p;
}

static pic from_core(const char *name, const char *label, void (*draw)(ledm *), uint8_t br = 15)
{
    // draw with the core API into a scratch matrix, then take its framebuffer
    fake_matrix f;
    ledm m;
    ledm_config cfg;
    ledm_config_default(&cfg, LEDM_COMMON_ANODE);
    ledm_bus b = f.bus();
    ledm_init(&m, &b, &cfg);
    draw(&m);
    return make(name, label, m.fb, br);
}

int main(int argc, char **argv)
{
    std::string out = argc > 1 ? argv[1] : ".";
    static const uint8_t HEART[8] = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
    static const uint8_t SMILE[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};
    static const uint8_t ARROW[8] = {0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x18, 0x18};
    static const uint8_t FULL[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    std::vector<pic> pics;
    pics.push_back(make("heart", "heart", HEART));
    pics.push_back(make("smile", "bitmap", SMILE));
    pics.push_back(make("arrow", "arrow", ARROW));
    pics.push_back(from_core("degree", "°", [](ledm *m) { ledm_show_char(m, "\xC2\xB0"); }));
    pics.push_back(from_core("euro", "€", [](ledm *m) { ledm_show_char(m, "\xE2\x82\xAC"); }));
    pics.push_back(from_core("bars", "bars", [](ledm *m) {
        const uint16_t v[8] = {12, 30, 55, 80, 100, 70, 40, 18}; // made-up sample values
        ledm_bars(m, v, 8, 100);
    }));
    pics.push_back(make("bright15", "15", FULL, 15));
    pics.push_back(make("bright8", "8", FULL, 8));
    pics.push_back(make("bright3", "3", FULL, 3));
    pics.push_back(from_core("rot90", "rot 90", [](ledm *m) { ledm_show_char(m, "A"); }));
    pics.back().rotation = 1;
    measure(pics.back()); // through the core's rotation

    const int cols = 5, label_h = 30, gap = 14;
    int rows = ((int)pics.size() + cols - 1) / cols;
    rgb_image ov(cols * TILE + (cols + 1) * gap, rows * (TILE + label_h) + (rows + 1) * gap - 4, BG);
    for (size_t i = 0; i < pics.size(); i++) {
        int c = (int)i % cols, r = (int)i / cols;
        int ox = gap + c * (TILE + gap), oy = gap + r * (TILE + label_h + gap);
        draw_tile(ov, ox, oy, pics[i]);
        draw_text(ov, ox + (TILE - text_px(pics[i].label)) / 2, oy + TILE + 8, pics[i].label, LABEL);
        rgb_image one(TILE, TILE, BODY);
        draw_tile(one, 0, 0, pics[i]);
        write_png((out + "/" + pics[i].name + ".png").c_str(), one);
    }
    write_png((out + "/overview.png").c_str(), ov);

    // scroll: frames of "Hi 23°C" every 4 columns, side by side
    fake_matrix f;
    ledm m;
    ledm_config cfg;
    ledm_config_default(&cfg, LEDM_COMMON_ANODE);
    ledm_bus b = f.bus();
    ledm_init(&m, &b, &cfg);
    ledm_scroll sc;
    ledm_scroll_start(&sc, "Hi 23\xC2\xB0" "C", 0);
    std::vector<pic> frames;
    int step = 0;
    while (ledm_scroll_step(&m, &sc) && frames.size() < 10)
        if (++step % 4 == 0 && step >= 8)
            frames.push_back(make("f", "", m.fb));
    rgb_image strip((int)frames.size() * (TILE + 6) + 6, TILE + 12, BG);
    for (size_t i = 0; i < frames.size(); i++)
        draw_tile(strip, 6 + (int)i * (TILE + 6), 6, frames[i]);
    write_png((out + "/scroll.png").c_str(), strip);
    std::printf("%zu pictures + overview.png + scroll.png (%zu frames) in %s\n", pics.size(),
                frames.size(), out.c_str());
    return 0;
}
