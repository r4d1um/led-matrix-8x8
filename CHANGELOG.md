# Changelog

## 0.1.0

- First release: C99 core for bare 8x8 LED matrices (788BS/788AS, 1088BS/1088AS and other
  16-pin modules): multiplexing by rows or columns with blanking between lines (no ghosting),
  brightness 0..15 by on-time, timing from a microsecond clock (`ledm_service`) or a timer
  interrupt (`ledm_step`), common anode / common cathode and inverting drivers, rotation and
  mirroring.
- Framebuffer: pixels, rows, columns, bitmaps (also from PROGMEM), shifting with wrap or fill,
  levels and bar graphs.
- Text: own proportional 5x7 font, UTF-8 (° ± ² ³ µ · Ä Ö Ü ß ä ö ü € arrows █), single
  characters and a scroller with loop mode.
- Arduino adapter `LedMatrix8x8` for 16 GPIOs or two 74HC595, precompiled core for ESP8266,
  ESP32, RP2040 (cortex-m0plus), ATmega328P and ATmega2560.
