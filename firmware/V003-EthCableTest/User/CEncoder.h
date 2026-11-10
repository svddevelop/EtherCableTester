// ===================================================================================
// Project:   Ethernet Cable Tester - Rotary Encoder with Center Button
// Class:     CEncoder
// ===================================================================================
//
// Event driven encoder service based on external interrupts (EXTI):
//
//   - quadrature input A (PD5, EXTI line 5, both edges) - a step is counted
//     on the rising edge only, direction from the level of B (PD6);
//   - push button (PD7, EXTI line 7, falling edge) - press timestamp is
//     captured in the ISR, debounce / short / long press classification is
//     done in poll(), which must be called from the main loop.
//
// Public API:
//   CEncoder::begin();         // setup pins and interrupts, call once
//   CEncoder::poll();          // call regularly from the main loop
//   CEncoder::getDelta();      // accumulated steps since last call (+ right / - left)
//   CEncoder::getPress();      // true once per short click (auto reset)
//   CEncoder::getLongPress();  // true once per long press (auto reset)
//
// ===================================================================================
#pragma once

#include <stdint.h>

class CEncoder {
public:
  static void     begin();          // init pins + EXTI, call once at startup
  static void     poll();           // debounce / classification, call in main loop

  static int16_t  getDelta();       // consume accumulated rotation steps
  static bool     getPress();       // consume short press event
  static bool     getLongPress();   // consume long press event
  static bool     isDown();         // debounced button state
};
