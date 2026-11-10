// ===================================================================================
// Project:   Ethernet Cable Tester - RJ45 Line Driver (implementation)
// Class:     CPins
// ===================================================================================
#include "CPins.h"
#include "system.h"
#include "gpio.h"
#include "ch32v003.h"
#include "config.h"

namespace {

  const uint8_t PINS[CPins::COUNT] = RJ45_PINS;       // contact 1..8

  volatile uint8_t s_pwmMask = 0;                     // pins currently in PWM mode
  volatile uint8_t s_phase   = 0;                     // 0..PWM_STEPS-1
  volatile uint8_t s_thrStep = PWM_STEPS / 2;         // duty threshold in steps
  bool s_pwmOn = false;

  void pwmStart(void) {
    if (s_pwmOn) return;
    RCC->APB1PCENR |= RCC_TIM2EN;                     // enable TIM2 clock
    TIM2->CTLR1  = 0;                                 // upcounter, no preload
    TIM2->PSC    = (F_CPU / 1000000) - 1;             // 1 MHz timer tick
    TIM2->ATRLR  = (uint16_t)((1000000UL / PWM_FREQ_HZ / PWM_STEPS) - 1);
    TIM2->CNT    = 0;
    TIM2->INTFR  = 0;
    TIM2->DMAINTENR = TIM_UIE;                        // update interrupt enable
    s_phase = 0;
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CTLR1 = TIM_CEN;                            // start
    s_pwmOn = true;
  }

  void pwmStop(void) {
    if (!s_pwmOn) return;
    TIM2->CTLR1 &= ~TIM_CEN;
    TIM2->DMAINTENR = 0;
    NVIC_DisableIRQ(TIM2_IRQn);
    s_pwmOn = false;
  }

}

// ===================================================================================
// TIM2 update interrupt - soft PWM heartbeat
// ===================================================================================
extern "C" void TIM2_IRQHandler(void) __attribute__((interrupt));
extern "C" void TIM2_IRQHandler(void) {
  TIM2->INTFR = (uint16_t)~TIM_UIF;
  CPins::irqTick();
}

void CPins::irqTick(void) {
  uint8_t ph = s_phase + 1;
  if (ph >= PWM_STEPS) ph = 0;
  s_phase = ph;

  uint8_t mask = s_pwmMask;
  if (!mask) return;
  uint8_t thr = s_thrStep;
  for (uint8_t i = 0; i < COUNT; i++) {
    if (mask & (uint8_t)(1 << i)) {
      if (ph < thr) PIN_high(PINS[i]);
      else          PIN_low(PINS[i]);
    }
  }
}

void CPins::begin(void) {
  toInputs();
}

void CPins::toInputs(void) {
  pwmStop();
  s_pwmMask = 0;
  for (uint8_t i = 0; i < COUNT; i++) PIN_input(PINS[i]);
}

void CPins::apply(const uint8_t* modes) {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < COUNT; i++) {
    uint8_t m = (modes[i] <= MODE_PWM) ? modes[i] : MODE_LOW;
    switch (m) {
      case MODE_HIGH: PIN_output(PINS[i]); PIN_high(PINS[i]); break;
      case MODE_PWM:  PIN_output(PINS[i]); PIN_low(PINS[i]);  mask |= (uint8_t)(1 << i); break;
      default:        PIN_output(PINS[i]); PIN_low(PINS[i]);  break;
    }
  }
  s_pwmMask = mask;
  if (mask) pwmStart(); else pwmStop();
}

uint8_t CPins::readLevels(void) {
  uint8_t v = 0;
  for (uint8_t i = 0; i < COUNT; i++) if (PIN_read(PINS[i])) v |= (uint8_t)(1 << i);
  return v;
}

void CPins::setDuty(uint8_t percent) {
  if (percent > 100) percent = 100;
  uint32_t t = ((uint32_t)percent * PWM_STEPS) / 100;
  if (t > PWM_STEPS) t = PWM_STEPS;
  s_thrStep = (uint8_t)t;
}
