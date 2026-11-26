// SPDX-License-Identifier: MIT
// Minimal Arduino stand-in for the host test of the adapter: GPIOs are an array, time is a
// variable the test moves, and a fake chain of two 74HC595 listens on three pins.
#ifndef FAKE_ARDUINO_H
#define FAKE_ARDUINO_H

#include <cstdint>
#include <functional>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define MSBFIRST 1
#define LSBFIRST 0

namespace fake {
extern uint8_t level[64];
extern uint8_t mode[64];
extern uint64_t us;
extern int writes;
// 74HC595 chain: stage 0 = QA of the chip fed by the MCU, stage 15 = QH of the second chip
extern int data_pin, clock_pin, latch_pin;
extern uint16_t shift, latched;
extern int latches;
extern std::function<void()> on_change; // after every pin change / latch
}

inline void pinMode(uint8_t pin, uint8_t m) { fake::mode[pin] = m; }
inline void digitalWrite(uint8_t pin, uint8_t v)
{
    uint8_t old = fake::level[pin];
    fake::level[pin] = v ? 1 : 0;
    fake::writes++;
    if (!old && v) {
        if (pin == fake::clock_pin)
            fake::shift = (uint16_t)((fake::shift << 1) | fake::level[fake::data_pin]);
        if (pin == fake::latch_pin) {
            fake::latched = fake::shift;
            fake::latches++;
        }
    }
    if (fake::on_change)
        fake::on_change();
}
inline void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t bit = bitOrder == LSBFIRST ? (val >> i) & 1 : (val >> (7 - i)) & 1;
        digitalWrite(dataPin, bit);
        digitalWrite(clockPin, HIGH);
        digitalWrite(clockPin, LOW);
    }
}
inline unsigned long micros() { return (unsigned long)(uint32_t)fake::us; }
inline unsigned long millis() { return (unsigned long)(uint32_t)(fake::us / 1000); }
inline void yield() { fake::us += 10; }

#endif
