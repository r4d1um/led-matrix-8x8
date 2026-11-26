// SPDX-License-Identifier: MIT
// CI only: a real firmware against the packaged Arduino library, built for every supported
// board (make firmware-check). Uses every part of the API so the linker pulls in all objects.
#include <LedMatrix8x8.h>

static const uint8_t ROWS[8] = {2, 3, 4, 5, 6, 7, 8, 9};
static const uint8_t COLS[8] = {10, 11, 12, 13, 14, 15, 16, 17};

static void own_write(void *, uint8_t rows, uint8_t cols)
{
    (void)rows;
    (void)cols;
}

LedMatrix8x8 gpio(ROWS, COLS);
LedMatrix8x8 chain(4, 5, 12, LEDM_COMMON_CATHODE);
LedMatrix8x8 own(ledm_bus{own_write, nullptr});

static const uint8_t ARROW[8] PROGMEM = {0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x18, 0x18};

void setup()
{
    Serial.begin(115200);
    gpio.config().scan = LEDM_SCAN_COLS;
    gpio.begin();
    chain.begin();
    own.begin();
    gpio.setBrightness(7);
    gpio.setRotation(1);
    gpio.setMirror(true);
    gpio.setBitmap_P(ARROW);
    gpio.setPixel(1, 1, LEDM_INVERT);
    gpio.setRow(0, gpio.getRow(1));
    gpio.setColumn(2, gpio.getColumn(3));
    gpio.shift(LEDM_UP, true);
    gpio.level(0, 5);
    const uint16_t v[3] = {1, 2, 3};
    gpio.bars(v, 3, 3);
    gpio.invert();
    gpio.drawChar(0, 0, "\xC3\xA4");
    gpio.showChar("\xE2\x82\xAC");
    gpio.wait(10);
    gpio.off();
    gpio.on();
    chain.fill();
    chain.scroll("23.5\xC2\xB0" "C", 50, true);
    own.clear();
    ledm_step(own.core());
    Serial.println(ledm_text_width("Hi"));
    Serial.println(gpio.getPixel(0, 0) ? "on" : "off");
    Serial.println(LEDM_VERSION);
}

void loop()
{
    gpio.update();
    chain.update();
    if (!chain.scrolling())
        chain.stopScroll();
    own.refresh();
}
