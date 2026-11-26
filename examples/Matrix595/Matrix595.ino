// SPDX-License-Identifier: MIT
// Matrix595: a 788BS on two daisy-chained 74HC595 - only 3 pins, fits a D1 mini or an ESP32.
// A small counter shows how to mix text and drawing.
//
//   MCU data  -> SER (14) of 595 #1;  QH' (9) of #1 -> SER (14) of #2
//   MCU clock -> SRCLK (11) of both;  MCU latch -> RCLK (12) of both
//   OE (13) to GND, SRCLR (10) to VCC, 100 nF at each chip
//   595 #1 QA..QH -> ROW1..ROW8 = module pins  9, 14,  8, 12,  1,  7,  2,  5
//   595 #2 QA..QH -> COL1..COL8 = module pins 13,  3,  4, 10,  6, 11, 15, 16  (1 kOhm each)
// On 3.3 V boards power the 595s from 3.3 V (or use 74HCT595 at 5 V).
#include <LedMatrix8x8.h>

#if defined(ARDUINO_ESP8266_WEMOS_D1MINI) || defined(ESP8266)
LedMatrix8x8 matrix(D7, D5, D6);   // data, clock, latch
#elif defined(ESP32)
LedMatrix8x8 matrix(23, 18, 5);
#else
LedMatrix8x8 matrix(11, 13, 10);
#endif

uint8_t count = 0;
uint32_t last = 0;

void setup()
{
    matrix.begin();
    matrix.scroll("Matrix595 \xE2\x86\x92 ", 60);   // once, with an arrow
}

void loop()
{
    matrix.update();
    if (matrix.scrolling() || millis() - last < 400)
        return;
    last = millis();
    char digit[2] = {(char)('0' + count % 10), 0};
    matrix.showChar(digit);
    matrix.level(7, count % 9);                    // a little bar at the right edge
    count++;
}
