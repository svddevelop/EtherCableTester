// ===================================================================================
// Project:   Ethernet Cable Tester - SSD1306 128x64 OLED (implementation)
// Class:     CDisplay
// ===================================================================================
#include "CDisplay.h"
#include <string.h>
#include "system.h"
#include "gpio.h"
#include "config.h"

namespace {

  // =================================================================================
  // Glyph table: 5x7 pixels + 1 blank column, LSB = top row
  // =================================================================================
  struct Glyph { uint8_t code; uint8_t col[5]; };

  const Glyph GLYPHS[] = {
    { '0', { 0x3E, 0x51, 0x49, 0x45, 0x3E } },
    { '1', { 0x00, 0x42, 0x7F, 0x40, 0x00 } },
    { '2', { 0x42, 0x61, 0x51, 0x49, 0x46 } },
    { '3', { 0x21, 0x41, 0x45, 0x4B, 0x31 } },
    { '4', { 0x18, 0x14, 0x12, 0x7F, 0x10 } },
    { '5', { 0x27, 0x45, 0x45, 0x45, 0x39 } },
    { '6', { 0x3C, 0x4A, 0x49, 0x49, 0x30 } },
    { '7', { 0x01, 0x71, 0x09, 0x05, 0x03 } },
    { '8', { 0x36, 0x49, 0x49, 0x49, 0x36 } },
    { '9', { 0x06, 0x49, 0x49, 0x29, 0x1E } },
    { 'A', { 0x7E, 0x11, 0x11, 0x11, 0x7E } },
    { 'B', { 0x7F, 0x49, 0x49, 0x49, 0x36 } },
    { 'C', { 0x3E, 0x41, 0x41, 0x41, 0x22 } },
    { 'D', { 0x7F, 0x41, 0x41, 0x22, 0x1C } },
    { 'E', { 0x7F, 0x49, 0x49, 0x49, 0x41 } },
    { 'H', { 0x7F, 0x08, 0x08, 0x08, 0x7F } },
    { 'I', { 0x00, 0x41, 0x7F, 0x41, 0x00 } },
    { 'L', { 0x7F, 0x40, 0x40, 0x40, 0x40 } },
    { 'N', { 0x7F, 0x04, 0x08, 0x10, 0x7F } },
    { 'P', { 0x7F, 0x09, 0x09, 0x09, 0x06 } },
    { 'R', { 0x7F, 0x09, 0x19, 0x29, 0x46 } },
    { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
    { 'T', { 0x01, 0x01, 0x7F, 0x01, 0x01 } },
    { 'V', { 0x1F, 0x40, 0x40, 0x40, 0x1F } },
    { '+', { 0x08, 0x08, 0x3E, 0x08, 0x08 } },
    { '>', { 0x08, 0x14, 0x22, 0x41, 0x00 } },
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
    // inverted T: horizontal bar on row 6, vertical bar rows 3..6 in center col
    { CDisplay::SYM_ZERO, { 0x40, 0x40, 0x78, 0x40, 0x40 } },
    // sine wave: mid, high, mid, low, mid
    { CDisplay::SYM_SINE, { 0x30, 0x0C, 0x30, 0x60, 0x30 } },
  };

  const uint8_t GLYPH_COUNT = sizeof(GLYPHS) / sizeof(GLYPHS[0]);
  const uint8_t BLANK[5]    = { 0, 0, 0, 0, 0 };

  const uint8_t* glyph(uint8_t c) {
    for (uint8_t i = 0; i < GLYPH_COUNT; i++) if (GLYPHS[i].code == c) return GLYPHS[i].col;
    return BLANK;
  }

  // =================================================================================
  // Software I2C master (open drain style)
  // =================================================================================
  void sdaHi(void)  { PIN_input_PU(PIN_I2C_SDA); }               // release, pullup
  void sclHi(void)  { PIN_input_PU(PIN_I2C_SCL); }
  void sdaLo(void)  { PIN_low(PIN_I2C_SDA); PIN_output(PIN_I2C_SDA); }  // drive low
  void sclLo(void)  { PIN_low(PIN_I2C_SCL); PIN_output(PIN_I2C_SCL); }

  void i2cDelay(void) { DLY_us(I2C_DELAY_US); }

  void i2cStart(void) {
    sdaHi(); sclHi(); i2cDelay();
    sdaLo(); i2cDelay();
    sclLo(); i2cDelay();
  }

  void i2cStop(void) {
    sdaLo(); i2cDelay();
    sclHi(); i2cDelay();
    sdaHi(); i2cDelay();
  }

  void i2cByte(uint8_t b) {
    for (uint8_t m = 0x80; m; m >>= 1) {
      if (b & m) sdaHi(); else sdaLo();
      i2cDelay();
      sclHi(); i2cDelay();
      sclLo(); i2cDelay();
    }
    sdaHi();                                               // release for ACK
    i2cDelay();
    sclHi(); i2cDelay();
    sclLo(); i2cDelay();                                   // ACK bit ignored
  }

  // 0x80 = Co=1, D/C#=0: the following byte is a single command.
  // A bare 0x00 would switch the controller into "rest of packet is commands",
  // so a second control byte would be executed as command 0x00.
  void oledCmd(uint8_t cmd) { i2cByte(0x80); i2cByte(cmd); }

  void oledCmdN(const uint8_t* p, uint8_t n) {
    while (n--) oledCmd(*p++);
  }

  // Horizontal window. pages is the last page index (inclusive).
  void oledWindow(uint8_t lastPage) {
    i2cStart();
    i2cByte(OLED_ADDR << 1);
    const uint8_t cmd[] = { 0x20, 0x00, 0x21, 0x00, 0x7F, 0x22, 0x00, lastPage };
    oledCmdN(cmd, sizeof(cmd));
    i2cStop();
  }

  void oledInit(void) {
    i2cStart();
    i2cByte(OLED_ADDR << 1);
    const uint8_t init[] = {
      0xAE,                          // display off
      0xD5, 0x80,                    // clock divide
      0xA8, 0x3F,                    // multiplex 64
      0xD3, 0x00,                    // display offset
      0x40,                          // start line 0
      0x8D, 0x14,                    // charge pump on
      0x20, 0x00,                    // horizontal addressing
      0xA1,                          // segment remap
      0xC8,                          // COM scan direction
      0xDA, 0x12,                    // COM pins
      0x81, 0x7F,                    // contrast
      0xD9, 0xF1,                    // precharge
      0xDB, 0x40,                    // VCOMH
      0xA4,                          // resume from RAM
      0xA6,                          // normal (not inverted)
      0xAF                           // display on
    };
    oledCmdN(init, sizeof(init));
    i2cStop();
  }

}

// ===================================================================================
// Construction / text handling
// ===================================================================================
CDisplay::CDisplay() : m_curLine(-1), m_curCol(-1) {
  clear();
}

void CDisplay::begin(void) {
  sdaHi();                                                 // idle high
  sclHi();
  DLY_ms(20);                                              // OLED power-up time
  oledInit();

  // clear whole VRAM once (pages 6..7 stay blank afterwards)
  oledWindow(7);
  i2cStart(); i2cByte(OLED_ADDR << 1); i2cByte(0x40);
  for (uint16_t i = 0; i < 8 * 128; i++) i2cByte(0);
  i2cStop();
}

void CDisplay::clear(void) {
  memset(m_text, ' ', sizeof(m_text));
  for (uint8_t l = 0; l < LINES; l++) m_text[l][COLS] = 0;
  m_curLine = -1;
  m_curCol  = -1;
}

void CDisplay::print(uint8_t line, const char* text) {
  if (line >= LINES) return;
  uint8_t i = 0;
  while (i < COLS && text[i]) { m_text[line][i] = text[i]; i++; }
  while (i < COLS) m_text[line][i++] = ' ';
  m_text[line][COLS] = 0;
}

void CDisplay::setCursor(int8_t line, int8_t col) {
  m_curLine = line;
  m_curCol  = col;
}

void CDisplay::hideCursor(void) {
  m_curLine = -1;
  m_curCol  = -1;
}

// ===================================================================================
// Render text into pixel buffer (5x7 glyph scaled x2 -> 10x14, cell 16x16)
// ===================================================================================
void CDisplay::render(void) {
  memset(m_fb, 0, sizeof(m_fb));

  for (uint8_t l = 0; l < LINES; l++) {
    for (uint8_t c = 0; c < COLS; c++) {
      const uint8_t* g = glyph((uint8_t)m_text[l][c]);
      uint8_t x0 = (uint8_t)(c * 16 + 2);                 // 12 px wide, centered
      uint8_t y0 = (uint8_t)(l * 16);
      for (uint8_t gx = 0; gx < 5; gx++) {
        uint8_t bits = g[gx];
        if (!bits) continue;
        for (uint8_t gy = 0; gy < 7; gy++) {
          if (!(bits & (uint8_t)(1 << gy))) continue;
          for (uint8_t sx = 0; sx < 2; sx++) {
            uint8_t px = (uint8_t)(x0 + gx * 2 + sx);
            for (uint8_t sy = 0; sy < 2; sy++) {
              uint8_t py = (uint8_t)(y0 + gy * 2 + sy);
              m_fb[(py >> 3) * 128 + px] |= (uint8_t)(1 << (py & 7));
            }
          }
        }
      }
    }
  }

  // invert the cursor cell (16x16 block)
  if (m_curLine >= 0 && m_curLine < LINES && m_curCol >= 0 && m_curCol < COLS) {
    uint8_t x0 = (uint8_t)(m_curCol * 16);
    uint8_t y0 = (uint8_t)(m_curLine * 16);
    for (uint8_t y = 0; y < 16; y++) {
      uint8_t* row = &m_fb[((y0 + y) >> 3) * 128 + x0];
      for (uint8_t x = 0; x < 16; x++) row[x] ^= (uint8_t)(1 << ((y0 + y) & 7));
    }
  }
}

void CDisplay::sendBuffer(void) {
  // 3 lines x 16 px = pages 0..5. Reset the write pointer every frame,
  // otherwise horizontal mode continues from where the previous burst stopped.
  oledWindow(5);
  i2cStart();
  i2cByte(OLED_ADDR << 1);
  i2cByte(0x40);                                           // data mode, rest of packet
  for (uint16_t i = 0; i < sizeof(m_fb); i++) i2cByte(m_fb[i]);
  i2cStop();
}

void CDisplay::flush(void) {
  render();
  sendBuffer();
}
