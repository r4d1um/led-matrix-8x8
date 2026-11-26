// SPDX-License-Identifier: MIT
// Host test of the Arduino adapter LedMatrix8x8 against fake GPIOs and a fake 74HC595 chain
// (tests/arduino/Arduino.h): checks the wiring the README documents, pin by pin.
#include <cmath>
#include <cstring>

#include "Arduino.h"
#include "LedMatrix8x8.h"
#include "fake_matrix.h"
#include "test_ui.h"

namespace fake {
uint8_t level[64], mode[64];
uint64_t us;
int writes;
int data_pin = -1, clock_pin = -1, latch_pin = -1;
uint16_t shift, latched;
int latches;
std::function<void()> on_change;
}

static const uint8_t HEART[8] = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
static const uint8_t ROWP[8] = {2, 3, 4, 5, 6, 7, 8, 9};
static const uint8_t COLP[8] = {10, 11, 12, 13, 14, 15, 16, 17};

static void reset_board()
{
    std::memset(fake::level, 0, sizeof fake::level);
    std::memset(fake::mode, 0, sizeof fake::mode);
    fake::us = 1000;
    fake::writes = fake::latches = 0;
    fake::data_pin = fake::clock_pin = fake::latch_pin = -1;
    fake::shift = fake::latched = 0;
    fake::on_change = nullptr;
}

// wire the fake matrix to the GPIOs: every pin change is seen at once (pin-by-pin, like real)
static void wire_gpio(fake_matrix &f)
{
    fake::on_change = [&f]() {
        uint8_t r = 0, c = 0;
        for (int i = 0; i < 8; i++) {
            r |= (uint8_t)(fake::level[ROWP[i]] << i);
            c |= (uint8_t)(fake::level[COLP[i]] << i);
        }
        f.now = fake::us;
        if (r != f.rows || c != f.cols)
            f.write(r, c);
    };
}

// wire it to the 595 outputs: chip 1 QA..QH = ROW1..8, chip 2 QA..QH = COL1..8
static void wire_595(fake_matrix &f)
{
    fake::data_pin = 20;
    fake::clock_pin = 21;
    fake::latch_pin = 22;
    fake::on_change = [&f]() {
        uint8_t r = (uint8_t)(fake::latched & 0xFF), c = (uint8_t)(fake::latched >> 8);
        f.now = fake::us;
        if (r != f.rows || c != f.cols)
            f.write(r, c);
    };
}

static void spin(LedMatrix8x8 &m, fake_matrix &f, uint64_t us)
{
    uint64_t end = fake::us + us;
    while (fake::us < end) {
        m.update();
        fake::us += 10;
        f.at(fake::us);
    }
}

static void t_gpio()
{
    SECTION("16 GPIOs");
    reset_board();
    fake_matrix f(LEDM_COMMON_ANODE);
    wire_gpio(f);
    LedMatrix8x8 m(ROWP, COLP);
    m.begin();
    for (int i = 0; i < 8; i++)
        CHECK(fake::mode[ROWP[i]] == OUTPUT && fake::mode[COLP[i]] == OUTPUT);
    CHECK(f.count_on() == 0);
    PASS("begin() makes all 16 pins outputs, matrix dark");

    m.setBitmap(HEART);
    spin(m, f, 10000);
    f.reset_counters();
    spin(m, f, 100000);
    uint8_t img[8];
    f.image(img);
    CHECK(std::memcmp(img, HEART, 8) == 0);
    CHECK(f.ghosts == 0);
    CHECK(std::fabs(f.duty(2, 0) - 1.0 / 8) < 0.01);
    PASS("heart on a 788BS via GPIOs, pin-by-pin writes without ghosting");

    int before = fake::writes;
    m.clear();
    spin(m, f, 10000);
    before = fake::writes;
    spin(m, f, 10000);
    CHECK(fake::writes == before);
    PASS("only changed pins are written (dark matrix: no writes at all)");

    reset_board();
    fake_matrix cc(LEDM_COMMON_CATHODE);
    wire_gpio(cc);
    LedMatrix8x8 m2(ROWP, COLP, LEDM_COMMON_CATHODE);
    m2.begin();
    m2.setPixel(7, 0);
    m2.setPixel(0, 7);
    spin(m2, cc, 20000);
    cc.reset_counters();
    spin(m2, cc, 40000);
    CHECK(cc.duty(0, 7) > 0.1 && cc.duty(7, 0) > 0.1 && cc.duty(0, 0) == 0.0);
    PASS("788AS (common cathode)");
}

static void t_595()
{
    SECTION("two 74HC595");
    reset_board();
    fake_matrix f(LEDM_COMMON_ANODE);
    wire_595(f);
    LedMatrix8x8 m(20, 21, 22);
    m.begin();
    CHECK(fake::latches == 1 && fake::latched == 0xFF00); // rows low, cols high: all off
    PASS("begin() latches the all-off levels (ROW = chip 1, COL = chip 2)");

    m.setPixel(0, 0);
    spin(m, f, 10000);
    f.reset_counters();
    spin(m, f, 50000);
    CHECK(f.duty(0, 0) > 0.1 && f.count_on() <= 1);
    uint8_t img[8];
    m.setBitmap(HEART);
    spin(m, f, 10000);
    f.reset_counters();
    spin(m, f, 100000);
    f.image(img);
    CHECK(std::memcmp(img, HEART, 8) == 0);
    CHECK(f.ghosts == 0 && f.max_lit <= 8);
    PASS("QA of chip 1 = ROW1, QA of chip 2 = COL1; heart, no ghosting without blanking (latched)");

    m.setBrightness(3);
    f.reset_counters();
    spin(m, f, 100000);
    CHECK(std::fabs(f.duty(2, 0) - 3.0 / 15 / 8) < 0.01);
    m.off();
    f.reset_counters();
    spin(m, f, 20000);
    CHECK(f.duty(2, 0) == 0.0);
    PASS("brightness and off() through the shift registers");
}

static void t_text()
{
    SECTION("scrolling and wait()");
    reset_board();
    fake_matrix f(LEDM_COMMON_ANODE);
    wire_gpio(f);
    LedMatrix8x8 m(ROWP, COLP);
    m.begin();
    m.scroll("Hi", 50);
    CHECK(m.scrolling());
    int cols = ledm_text_width("Hi") + 1 + 8;
    spin(m, f, (uint64_t)(cols - 1) * 50000 + 20000);
    CHECK(m.scrolling());
    spin(m, f, 60000);
    CHECK(!m.scrolling());
    PASS("scroll(\"Hi\", 50 ms) takes (width + 1 + 8) x 50 ms, then scrolling() is false");

    m.scroll("x", 1, true);
    spin(m, f, 300000);
    CHECK(m.scrolling());
    m.stopScroll();
    CHECK(!m.scrolling());
    PASS("loop scrolling runs until stopScroll()");

    m.showChar("A");
    uint64_t t0 = fake::us;
    m.wait(20);
    CHECK(fake::us - t0 >= 20000 && fake::us - t0 < 21000);
    CHECK(f.writes > 0);
    m.setRotation(2);
    m.setMirror(true);
    CHECK(m.core()->rotation == 2 && m.core()->mirror == 1);
    m.drawChar(0, 0, "\xE2\x82\xAC");
    CHECK(m.getRow(1) != 0);
    PASS("wait() keeps the matrix running; rotation, mirror, drawChar");
}

int main()
{
    banner("LedMatrix8x8 adapter  ·  fake GPIOs + 74HC595", LEDM_VERSION);
    t_gpio();
    t_595();
    t_text();
    return summary();
}
