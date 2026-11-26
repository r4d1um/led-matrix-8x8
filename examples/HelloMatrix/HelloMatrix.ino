// SPDX-License-Identifier: MIT
// HelloMatrix: a 788BS (common anode) wired straight to 16 pins of an Uno or a Pico.
// Shows a heart, a bar graph and then scrolls a text forever.
//
// Wiring (module pin -> board pin), a 1 kOhm resistor in every COL line:
//   ROW1..ROW8 = module pins  9, 14,  8, 12,  1,  7,  2,  5
//   COL1..COL8 = module pins 13,  3,  4, 10,  6, 11, 15, 16
#include <LedMatrix8x8.h>

#if defined(ARDUINO_ARCH_RP2040)
const uint8_t ROWS[8] = {2, 3, 4, 5, 6, 7, 8, 9};          // GP2..GP9
const uint8_t COLS[8] = {10, 11, 12, 13, 14, 15, 16, 17};  // GP10..GP17
#else
const uint8_t ROWS[8] = {2, 3, 4, 5, 6, 7, 8, 9};          // D2..D9
const uint8_t COLS[8] = {10, 11, 12, 13, A0, A1, A2, A3};
#endif

LedMatrix8x8 matrix(ROWS, COLS);   // LEDM_COMMON_CATHODE for a 788AS / 1088AS

const uint8_t HEART[8] PROGMEM = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};

void setup()
{
    matrix.begin();
    matrix.setBrightness(10);      // 0..15
    // matrix.setRotation(1);      // if the picture is turned on your breadboard

    matrix.setBitmap_P(HEART);
    matrix.wait(1500);             // like delay(), but the matrix keeps running

    const uint16_t values[8] = {10, 30, 55, 80, 100, 70, 40, 20};
    matrix.clear();
    matrix.bars(values, 8, 100);
    matrix.wait(1500);

    matrix.clear();
    matrix.scroll("Hello 23\xC2\xB0" "C ", 70, true);   // UTF-8, 70 ms per column, loop
}

void loop()
{
    matrix.update();               // as often as possible, no delay() in loop
}
