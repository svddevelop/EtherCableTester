// ===================================================================================
// Project:   Ethernet Cable Tester - Rotary Encoder with Center Button
// Class:     CEncoder (implementation)
// ===================================================================================
#include "CEncoder.h"
#include "system.h"
#include "gpio.h"
#include "config.h"

namespace {

  volatile int16_t s_delta     = 0;      // rotation steps, written by ISR
  volatile uint8_t s_irqEdge   = 0;      // button falling edge seen by ISR
  volatile uint32_t s_irqTime  = 0;      // time of that edge (SysTick ticks)

  volatile bool s_shortFlag    = false;  // short press event pending
  volatile bool s_longFlag     = false;  // long press event pending

  bool     s_down              = false;  // debounced button pressed
  bool     s_longFired         = false;  // long press already reported
  uint32_t s_pressTime         = 0;      // time of the debounced press

  const uint32_t TICKS_MS = F_CPU / 1000;
  const uint32_t TICKS_DEBOUNCE = ENC_DEBOUNCE_MS * TICKS_MS;
  const uint32_t TICKS_LONG     = ENC_LONGPRESS_MS * TICKS_MS;

  inline uint32_t ticks(void) { return STK->CNT; }
  inline bool past(uint32_t since, uint32_t len) { return (uint32_t)(ticks() - since) >= len; }

}

// ===================================================================================
// EXTI7_0 interrupt: quadrature decoding + button edge capture
// ===================================================================================
extern "C" void EXTI7_0_IRQHandler(void) __attribute__((interrupt));
extern "C" void EXTI7_0_IRQHandler(void) {

  // One step per rising edge of A (one detent on a typical EC11).
  // Direction comes from the level of B at that edge.
  if (PIN_INTFLAG_read(PIN_ENC_A)) {
    PIN_INTFLAG_clear(PIN_ENC_A);
    uint8_t a = PIN_read(PIN_ENC_A);
    uint8_t b = PIN_read(PIN_ENC_B);
    if (a) s_delta += b ? (int16_t)-1 : (int16_t)1;
  }

  // Button: remember edge, classification happens in poll()
  if (PIN_INTFLAG_read(PIN_ENC_BTN)) {
    PIN_INTFLAG_clear(PIN_ENC_BTN);
    s_irqTime  = ticks();
    s_irqEdge  = 1;
  }
}

void CEncoder::begin(void) {
  PIN_input_PU(PIN_ENC_A);                            // quadrature inputs with pullups
  PIN_input_PU(PIN_ENC_B);
  PIN_input_PU(PIN_ENC_BTN);                          // active low button

  PIN_INT_set(PIN_ENC_A,   PIN_INT_BOTH);             // EXTI line 5, both edges
  PIN_INT_set(PIN_ENC_BTN, PIN_INT_FALLING);          // EXTI line 7, falling edge

  PIN_INTFLAG_clear(PIN_ENC_A);
  PIN_INTFLAG_clear(PIN_ENC_BTN);
  PIN_INT_enable();
}

void CEncoder::poll(void) {
  // --- debounce: validate the edge captured by the ISR --------------------------
  if (s_irqEdge) {
    if (PIN_read(PIN_ENC_BTN)) {
      s_irqEdge = 0;                                  // released again -> bounce
    } else if (past(s_irqTime, TICKS_DEBOUNCE)) {
      s_irqEdge = 0;
      s_down      = true;
      s_longFired = false;
      s_pressTime = s_irqTime;
    }
  }

  if (s_down) {
    if (PIN_read(PIN_ENC_BTN)) {                      // released
      s_down = false;
      if (!s_longFired) s_shortFlag = true;           // short click on release
    } else if (!s_longFired && past(s_pressTime, TICKS_LONG)) {
      s_longFired = true;                             // long press confirmed
      s_longFlag  = true;
    }
  }
}

int16_t CEncoder::getDelta(void) {
  uint32_t m = __iSave();                             // s_delta is 16 bit -> atomic
  int16_t d = s_delta;
  s_delta = 0;
  __iRestore(&m);
  return d;
}

bool CEncoder::getPress(void) {
  uint32_t m = __iSave();
  bool r = s_shortFlag;
  s_shortFlag = false;
  __iRestore(&m);
  return r;
}

bool CEncoder::getLongPress(void) {
  uint32_t m = __iSave();
  bool r = s_longFlag;
  s_longFlag = false;
  __iRestore(&m);
  return r;
}

bool CEncoder::isDown(void) {
  return s_down;
}
