// SPDX-License-Identifier: MIT
// PinFinder: lights one LED after the other, row by row, and prints which ROW/COL (and module
// pins) it is on the serial monitor (115200 baud). If the dot doesn't walk left to right, top to
// bottom, a wire is swapped - or the matrix is the other type (try LEDM_COMMON_CATHODE).
#include <LedMatrix8x8.h>

const uint8_t ROWS[8] = {2, 3, 4, 5, 6, 7, 8, 9};
const uint8_t COLS[8] = {10, 11, 12, 13, A0, A1, A2, A3};
const uint8_t ROW_PIN[8] = LEDM_ROW_PINS;
const uint8_t COL_PIN[8] = LEDM_COL_PINS;

LedMatrix8x8 matrix(ROWS, COLS, LEDM_COMMON_ANODE);

void setup()
{
    Serial.begin(115200);
    matrix.begin();
}

void loop()
{
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            matrix.clear();
            matrix.setPixel(x, y);
            Serial.print("ROW");
            Serial.print(y + 1);
            Serial.print(" (module pin ");
            Serial.print(ROW_PIN[y]);
            Serial.print(")  COL");
            Serial.print(x + 1);
            Serial.print(" (module pin ");
            Serial.print(COL_PIN[x]);
            Serial.println(")");
            matrix.wait(400);
        }
    }
}
