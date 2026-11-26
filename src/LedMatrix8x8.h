// SPDX-License-Identifier: MIT
//
// LedMatrix8x8 - Arduino adapter for the ledm core: bare 8x8 LED matrices (788BS, 788AS,
// 1088BS, 1088AS, ...) on 16 GPIOs or on two 74HC595 shift registers.
//
//   const uint8_t rows[8] = {2, 3, 4, 5, 6, 7, 8, 9};      // MCU pins for ROW1..ROW8
//   const uint8_t cols[8] = {10, 11, 12, 13, A0, A1, A2, A3}; // MCU pins for COL1..COL8
//   LedMatrix8x8 matrix(rows, cols);                      // 788BS = common anode (default)
//   // LedMatrix8x8 matrix(dataPin, clockPin, latchPin);  // two 74HC595
//
//   void setup() { matrix.begin(); matrix.scroll("Hello", 70, true); }
//   void loop()  { matrix.update(); }                    // as often as possible, no delay()
//
// Module pins of ROW1..8 / COL1..8: LEDM_ROW_PINS / LEDM_COL_PINS in ledm.h, table in README.
#ifndef LED_MATRIX_8X8_H
#define LED_MATRIX_8X8_H

#include <Arduino.h>

#include "ledm.h"
#include "ledm_text.h"

class LedMatrix8x8 {
public:
    // 16 GPIOs. rowPins[i] = MCU pin wired to ROW(i+1), colPins[i] = MCU pin for COL(i+1).
    // type: LEDM_COMMON_ANODE (788BS, 1088BS) or LEDM_COMMON_CATHODE (788AS, 1088AS).
    LedMatrix8x8(const uint8_t rowPins[8], const uint8_t colPins[8],
                 uint8_t type = LEDM_COMMON_ANODE);
    // Two daisy-chained 74HC595: the chip whose SER comes from the MCU drives ROW1..ROW8 on
    // QA..QH, the next chip (SER from QH' of the first) drives COL1..COL8 on QA..QH.
    LedMatrix8x8(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin,
                 uint8_t type = LEDM_COMMON_ANODE);
    // Your own bus (e.g. port registers, SPI, an I/O expander).
    explicit LedMatrix8x8(const ledm_bus &bus, uint8_t type = LEDM_COMMON_ANODE);

    // Settings for begin(): polarity, scan direction, line time (see ledm_config in ledm.h).
    // Change them before begin(), e.g. config().col_active_high = 1 for a ULN2803 on the columns.
    ledm_config &config() { return cfg_; }

    void begin();
    // Multiplexing plus text scrolling. Call as often as possible; long delay() calls make the
    // matrix flicker or go dark - use wait() instead.
    void update();
    // Only the multiplexing (e.g. from a timer callback).
    void refresh();
    // Like delay(), but keeps the matrix running.
    void wait(uint32_t ms);

    void setBrightness(uint8_t level);       // 0..15
    uint8_t brightness() const { return m_.brightness; }
    void on() { ledm_enable(&m_, 1); }
    void off() { ledm_enable(&m_, 0); }
    void setRotation(uint8_t quarterTurns);  // 0..3 clockwise
    void setMirror(bool mirror);

    void clear() { ledm_clear(&m_); }
    void fill() { ledm_fill(&m_); }
    void invert() { ledm_invert(&m_); }
    void setPixel(int x, int y, uint8_t colour = LEDM_ON) { ledm_set_pixel(&m_, x, y, colour); }
    bool getPixel(int x, int y) const { return ledm_get_pixel(&m_, x, y) != 0; }
    void setRow(int y, uint8_t bits) { ledm_set_row(&m_, y, bits); }        // bit 7 = left
    uint8_t getRow(int y) const { return ledm_get_row(&m_, y); }
    void setColumn(int x, uint8_t bits) { ledm_set_col(&m_, x, bits); }     // bit 0 = top
    uint8_t getColumn(int x) const { return ledm_get_col(&m_, x); }
    void setBitmap(const uint8_t rows[8]) { ledm_set_bitmap(&m_, rows); }
    void setBitmap_P(const uint8_t *rows) { ledm_set_bitmap_rom(&m_, rows); } // PROGMEM on AVR
    void shift(uint8_t dir, bool wrap = false) { ledm_shift(&m_, dir, 0, wrap); }
    void level(int x, int height) { ledm_level(&m_, x, height); }           // 0..8 from bottom
    void bars(const uint16_t *values, int n, uint16_t max) { ledm_bars(&m_, values, n, max); }

    // Text (UTF-8; ASCII, ° ± ² ³ µ · Ä Ö Ü ß ä ö ü € ← ↑ → ↓ █)
    void drawChar(int x, int y, const char *utf8, uint8_t colour = LEDM_ON);
    void showChar(const char *utf8) { ledm_show_char(&m_, utf8); }           // centred
    // Scroll text in from the right, msPerColumn per pixel step. The string is not copied.
    void scroll(const char *utf8, uint16_t msPerColumn = 70, bool loop = false);
    bool scrolling() const { return scroll_.running != 0; }
    void stopScroll() { scroll_.running = 0; }

    ledm *core() { return &m_; }

private:
    static void writePins(void *ctx, uint8_t rows, uint8_t cols);
    static void write595(void *ctx, uint8_t rows, uint8_t cols);
    void init(uint8_t type);

    ledm m_;
    ledm_config cfg_;
    ledm_bus bus_;
    ledm_scroll scroll_;
    uint8_t pins_[16];       // GPIO bus: ROW1..8, COL1..8; 595 bus: data, clock, latch
    uint8_t mode_;           // 0 = GPIO, 1 = 74HC595, 2 = own bus
    uint8_t lastRows_, lastCols_;
    bool haveLast_;
    uint16_t scrollMs_;
    uint32_t scrollT_;
};

#endif // LED_MATRIX_8X8_H
