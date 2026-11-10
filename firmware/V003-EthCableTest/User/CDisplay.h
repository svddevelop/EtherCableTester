// ===================================================================================
// Project:   Ethernet Cable Tester - SSD1306 128x64 OLED, three line UI
// Class:     CDisplay
// ===================================================================================
//
// The display shows three text lines of 8 cells (cell = 16x16 px, glyph is
// a 5x7 font rendered 2x = 10x14 px inside the cell):
//
//   line 0: pin numbers        "12345678"   (cursor = pin being edited)
//   line 1: per pin mode       inverted T  = drive logic 0
//                               '+'         = drive logic 1
//                               sine wave   = drive PWM signal
//           (in REC mode this line shows the sampled levels '0'/'1')
//   line 2: menu               REC / SEND / SAVE  (cursor = selected item)
//
// Special glyphs (5x7):
//   \x01 = inverted T (logic low), \x02 = sine wave (PWM)
//
// The OLED is driven by a software I2C master on PA1 (SDA) / PA2 (SCL) in
// open-drain fashion: logic 0 pulls the line low, logic 1 releases it to
// the (internal + module) pullups.
//
// Pixel buffer is 6 pages x 128 bytes = 768 bytes (only the 48 px used by
// the three lines; pages 6..7 are cleared once in begin() and left blank).
//
// ===================================================================================
#pragma once

#include <stdint.h>

class CDisplay {
public:
  static const uint8_t LINES    = 3;          // text lines
  static const uint8_t COLS     = 8;          // cells per line
  static const uint8_t SYM_ZERO = '\x01';     // inverted T  -> logic low
  static const uint8_t SYM_SINE = '\x02';     // sine wave   -> PWM

  CDisplay();

  void begin();                                // init OLED + clear screen
  void clear();                                // clear text and cursor
  void print(uint8_t line, const char* text);  // write up to COLS chars
  void setCursor(int8_t line, int8_t col);     // invert that cell (or -1/-1)
  void hideCursor();
  void flush();                                // render text into RAM and send to OLED

private:
  char    m_text[LINES][COLS + 1];
  int8_t  m_curLine;
  int8_t  m_curCol;
  uint8_t m_fb[LINES * 2 * 128];               // 6 pages x 128 bytes

  void render();
  void sendBuffer();
};
