// SPDX-License-Identifier: MIT
#include "LedMatrix8x8.h"

#include <string.h>

LedMatrix8x8::LedMatrix8x8(const uint8_t rowPins[8], const uint8_t colPins[8], uint8_t type)
{
    memcpy(pins_, rowPins, 8);
    memcpy(pins_ + 8, colPins, 8);
    mode_ = 0;
    bus_.write = writePins;
    bus_.ctx = this;
    init(type);
}

LedMatrix8x8::LedMatrix8x8(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin, uint8_t type)
{
    memset(pins_, 0, sizeof pins_);
    pins_[0] = dataPin;
    pins_[1] = clockPin;
    pins_[2] = latchPin;
    mode_ = 1;
    bus_.write = write595;
    bus_.ctx = this;
    init(type);
    cfg_.no_blank = 1; // the latch switches all 16 outputs at once
}

LedMatrix8x8::LedMatrix8x8(const ledm_bus &bus, uint8_t type)
{
    memset(pins_, 0, sizeof pins_);
    mode_ = 2;
    bus_ = bus;
    init(type);
}

void LedMatrix8x8::init(uint8_t type)
{
    ledm_config_default(&cfg_, type);
    memset(&m_, 0, sizeof m_);
    memset(&scroll_, 0, sizeof scroll_);
    haveLast_ = false;
    lastRows_ = lastCols_ = 0;
    scrollMs_ = 70;
    scrollT_ = 0;
}

void LedMatrix8x8::begin()
{
    uint8_t i;
    if (mode_ == 0) {
        for (i = 0; i < 16; i++)
            pinMode(pins_[i], OUTPUT);
    } else if (mode_ == 1) {
        for (i = 0; i < 3; i++) {
            pinMode(pins_[i], OUTPUT);
            digitalWrite(pins_[i], LOW);
        }
    }
    haveLast_ = false;
    ledm_init(&m_, &bus_, &cfg_); // writes the all-off levels
}

void LedMatrix8x8::writePins(void *ctx, uint8_t rows, uint8_t cols)
{
    LedMatrix8x8 *self = static_cast<LedMatrix8x8 *>(ctx);
    uint8_t i, changedR = self->haveLast_ ? (uint8_t)(rows ^ self->lastRows_) : 0xFF;
    uint8_t changedC = self->haveLast_ ? (uint8_t)(cols ^ self->lastCols_) : 0xFF;
    for (i = 0; i < 8; i++) {
        if (changedR & (1u << i))
            digitalWrite(self->pins_[i], (rows >> i) & 1 ? HIGH : LOW);
        if (changedC & (1u << i))
            digitalWrite(self->pins_[8 + i], (cols >> i) & 1 ? HIGH : LOW);
    }
    self->lastRows_ = rows;
    self->lastCols_ = cols;
    self->haveLast_ = true;
}

void LedMatrix8x8::write595(void *ctx, uint8_t rows, uint8_t cols)
{
    LedMatrix8x8 *self = static_cast<LedMatrix8x8 *>(ctx);
    // the first byte out ends up in the far chip: columns first, then rows; bit 0 -> QA
    shiftOut(self->pins_[0], self->pins_[1], MSBFIRST, cols);
    shiftOut(self->pins_[0], self->pins_[1], MSBFIRST, rows);
    digitalWrite(self->pins_[2], HIGH);
    digitalWrite(self->pins_[2], LOW);
}

void LedMatrix8x8::refresh()
{
    ledm_service(&m_, micros());
}

void LedMatrix8x8::update()
{
    if (scroll_.running) {
        uint32_t now = millis();
        if (now - scrollT_ >= scrollMs_) {
            scrollT_ += scrollMs_;
            if (now - scrollT_ >= scrollMs_) // fell behind: don't catch up in a burst
                scrollT_ = now;
            ledm_scroll_step(&m_, &scroll_);
        }
    }
    refresh();
}

void LedMatrix8x8::wait(uint32_t ms)
{
    uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        update();
        yield();
    }
}

void LedMatrix8x8::setBrightness(uint8_t level)
{
    ledm_set_brightness(&m_, level);
}

void LedMatrix8x8::setRotation(uint8_t quarterTurns)
{
    ledm_set_rotation(&m_, quarterTurns);
}

void LedMatrix8x8::setMirror(bool mirror)
{
    ledm_set_mirror(&m_, mirror ? 1 : 0);
}

void LedMatrix8x8::drawChar(int x, int y, const char *utf8, uint8_t colour)
{
    size_t pos = 0;
    uint32_t cp;
    if (!utf8)
        return;
    cp = ledm_utf8_next(utf8, strlen(utf8), &pos);
    if (cp)
        ledm_draw_char(&m_, x, y, cp, colour);
}

void LedMatrix8x8::scroll(const char *utf8, uint16_t msPerColumn, bool loop)
{
    ledm_scroll_start(&scroll_, utf8, loop ? LEDM_SCROLL_LOOP : 0);
    scrollMs_ = msPerColumn ? msPerColumn : 1;
    scrollT_ = millis();
}
