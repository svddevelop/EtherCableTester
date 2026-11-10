// ===================================================================================
// Project:   Ethernet Cable Tester
// Version:   v1.0
// Year:      2026
// License:   MIT
// ===================================================================================
//
// Description:
// ------------
// Two part cable tester for Ethernet cables and patch cords. Each part is built
// around a CH32V003A4M6 and connects to one end of the cable via RJ45. The user
// interface consists of a rotary encoder with center button and an SSD1306
// 128x64 OLED (3 lines x 8 cells) on soft I2C.
//
// Display layout:
//   line 0: "12345678"          pin numbers, cursor selects the pin to edit
//   line 1: per pin drive mode  inverted T = '0', '+' = '1', sine = PWM
//           (in REC mode: sampled levels '0'/'1')
//   line 2: menu item           REC / SEND / SAVE, inverted while in menu
//
// Controls:
//   turn       - in EDIT: select pin 1..8 / in MENU: select menu item
//   short press- in EDIT: switch drive mode of selected pin (0 -> 1 -> PWM)
//               in MENU : run selected item (REC / SEND / SAVE)
//   long press - in EDIT: open menu / in MENU: cancel / in REC, SEND: leave mode
//
// On startup the configuration is loaded from the last flash page into the
// static structure g_cfg (CFlash::load). SAVE writes it back.
//
// ===================================================================================
#include "system.h"
#include "gpio.h"
#include "config.h"
#include "CFlash.h"
#include "CEncoder.h"
#include "CDisplay.h"
#include "CPins.h"

// ===================================================================================
// Structured data stored in flash
// ===================================================================================
struct CableConfig {
  uint8_t modes[RJ45_COUNT];      // drive mode per RJ45 contact
  uint8_t duty;                   // PWM duty in percent
  uint8_t menuSel;                // selected menu item: 0=REC 1=SEND 2=SAVE
  uint8_t reserved[5];
};

static_assert(sizeof(CableConfig) <= FLASH_PAGE_SIZE - 8, "record must fit into the flash page");

static CableConfig g_cfg;

// ===================================================================================
// UI state
// ===================================================================================
enum UiState : uint8_t { UI_EDIT, UI_MENU, UI_REC, UI_SEND };

static UiState s_ui = UI_EDIT;
static uint8_t s_pin = 0;                       // selected pin0..7

#ifndef __code
#define __code const __attribute__((section(".rodata")))
#endif


static __code char* const MENU_ITEMS[3] = { "RECEIVE", "SEND", "SAVE" };
static __code char MODE_CHARS[3] = { CDisplay::SYM_ZERO, '+', CDisplay::SYM_SINE };
static __code char C_ENUM1_8[] = "12345678";
static __code char C_SAVED[] = "Saved";
static __code char C_FAIL[] = "Fail";
static __code char C_MSG_ETHTEST[] = "ETH Test";

static CDisplay display;

// ===================================================================================
// Helpers
// ===================================================================================
static void setDefaults(void) {
  for (uint8_t i = 0; i < RJ45_COUNT; i++) g_cfg.modes[i] = CPins::MODE_LOW;
  g_cfg.duty    = 50;
  g_cfg.menuSel = 0;
  for (uint8_t i = 0; i < sizeof(g_cfg.reserved); i++) g_cfg.reserved[i] = 0;
}

static void sanitize(void) {
  for (uint8_t i = 0; i < RJ45_COUNT; i++)
    if (g_cfg.modes[i] > CPins::MODE_PWM) g_cfg.modes[i] = CPins::MODE_LOW;
  if (g_cfg.duty > 100)    g_cfg.duty = 50;
  if (g_cfg.menuSel > 2)   g_cfg.menuSel = 0;
}

static int8_t wrap8(int16_t v) {
  while (v < 0)               v += RJ45_COUNT;
  while (v >= RJ45_COUNT)     v -= RJ45_COUNT;
  return (int8_t)v;
}

static int8_t wrap3(int16_t v) {
  while (v < 0)   v += 3;
  while (v >= 3)  v -= 3;
  return (int8_t)v;
}

// ===================================================================================
// Redraw the whole UI according to the current state
// ===================================================================================
static void render(void) {
  char buf[9];

  // line 0: pin numbers
  display.print(0, C_ENUM1_8 );

  // line 1: drive modes - or sampled levels while receiving
  if (s_ui == UI_REC) {
    uint8_t v = CPins::readLevels();
    for (uint8_t i = 0; i < RJ45_COUNT; i++) buf[i] = (v & (uint8_t)(1 << i)) ? '1' : '0';
    buf[RJ45_COUNT] = 0;
    display.print(1, buf);
  } else {
    for (uint8_t i = 0; i < RJ45_COUNT; i++) buf[i] = MODE_CHARS[g_cfg.modes[i] % 3];
    buf[RJ45_COUNT] = 0;
    display.print(1, buf);
  }

  // line 2: menu
  display.print(2, MENU_ITEMS[g_cfg.menuSel]);

  // cursor: pin in EDIT, active menu item otherwise
  if (s_ui == UI_EDIT) display.setCursor(0, (int8_t)s_pin);
  else                 display.setCursor(2, 0);

  display.flush();
}

// ===================================================================================
// Run the selected menu item
// ===================================================================================
static void activateMenu(void) {
  switch (g_cfg.menuSel) {
    case 0:                                             // REC - listen to the pins
      CPins::toInputs();
      s_ui = UI_REC;
      break;

    case 1:                                             // SEND - drive per config
      CPins::setDuty(g_cfg.duty);
      CPins::apply(g_cfg.modes);
      s_ui = UI_SEND;
      break;

    case 2:                                             // SAVE - store in flash
      display.hideCursor();
      if (CFlash::save(g_cfg)) display.print(2, C_SAVED);
      else                     display.print(2, C_FAIL);
      display.flush();
      DLY_ms(900);
      s_ui = UI_EDIT;
      break;
  }
  render();
}

// ===================================================================================
// Main
// ===================================================================================
int main(void) {
  // peripherals
  CEncoder::begin();
  CPins::begin();
  display.begin();

  // startup splash
  display.print(0, C_MSG_ETHTEST);
  display.flush();
  DLY_ms(600);

  // load structured data from the last flash page into the static structure
  if (!CFlash::load(g_cfg)) setDefaults();
  sanitize();
  CPins::setDuty(g_cfg.duty);

  uint32_t recRefresh = STK->CNT;
  render();

  while (1) {
    CEncoder::poll();

    switch (s_ui) {

      case UI_EDIT: {
        int16_t d = CEncoder::getDelta();
        if (d) {
          s_pin = (uint8_t)wrap8((int16_t)s_pin + d);
          render();
        }
        if (CEncoder::getPress()) {                    // next drive mode of pin
          g_cfg.modes[s_pin] = (uint8_t)((g_cfg.modes[s_pin] + 1) % 3);
          render();
        }
        if (CEncoder::getLongPress()) {                // open menu
          s_ui = UI_MENU;
          render();
        }
        break;
      }

      case UI_MENU: {
        int16_t d = CEncoder::getDelta();
        if (d) {
          g_cfg.menuSel = (uint8_t)wrap3((int16_t)g_cfg.menuSel + d);
          render();
        }
        if (CEncoder::getPress()) activateMenu();      // run item
        if (CEncoder::getLongPress()) {                // cancel
          s_ui = UI_EDIT;
          render();
        }
        break;
      }

      default: {                                       // UI_REC or UI_SEND
        CEncoder::getDelta();                          // discard rotation
        CEncoder::getPress();                          // discard click
        if (CEncoder::getLongPress()) {                // leave mode
          CPins::toInputs();
          s_ui = UI_EDIT;
          render();
        }
        if (s_ui == UI_REC) {                          // live update 150 ms
          if ((uint32_t)(STK->CNT - recRefresh) >= (uint32_t)(150 * (F_CPU / 1000))) {
            recRefresh = STK->CNT;
            render();
          }
        }
        break;
      }
    }
  }
}
