// ===================================================================================
// Project:   Ethernet Cable Tester - Pin and System Configuration
// Chip:      CH32V003A4M6 (SOP-16)
// ===================================================================================
//
// Pin allocation (14 GPIO available, SWIO/PD1 reserved for programming only):
//
//   RJ45 contact 1 -> PC0   |  Encoder A   -> PD5   (EXTI line 5)
//   RJ45 contact 2 -> PC1   |  Encoder B   -> PD6   (EXTI line 6, input only)
//   RJ45 contact 3 -> PC2   |  Encoder SW  -> PD7   (EXTI line 7)
//   RJ45 contact 4 -> PC3   |
//   RJ45 contact 5 -> PC4   |  I2C SDA     -> PA1   (soft I2C, SSD1306)
//   RJ45 contact 6 -> PC6   |  I2C SCL     -> PA2   (soft I2C, SSD1306)
//   RJ45 contact 7 -> PC7   |
//   RJ45 contact 8 -> PD4   |  SWIO (PD1)  -> WCH-LinkE programmer ONLY
//
// Notes:
// - All three encoder inputs use EXTI lines with different pin numbers
//   (5, 6, 7), which is required by the single EXTI port multiplexer.
// - PC0..PC4, PC6, PD4 occupy physically adjacent package pins (16,1,2,3,4,5,8),
//   which keeps the RJ45 wiring short and uniform. PD1 (pin 7) sits between
//   PC7 and PD4 but is not routed to the RJ45 socket.
// - PD0/PD2/PD3 are not bonded out on the SOP-16 package, PC5 does not exist
//   on this chip variant.
//
// ===================================================================================
#pragma once

#include "gpio.h"

// --- RJ45 contacts 1..8 -----------------------------------------------------------
#define PIN_RJ1     PC0
#define PIN_RJ2     PC1
#define PIN_RJ3     PC2
#define PIN_RJ4     PC3
#define PIN_RJ5     PC4
//#define PIN_RJ6     PC6
#define PIN_RJ6     PC5
//#define PIN_RJ7     PC7
#define PIN_RJ7     PC6
//#define PIN_RJ8     PD4
#define PIN_RJ8     PC7


//#define RJ45_PINS   { PC0, PC1, PC2, PC3, PC4, PC6, PC7, PD4 }
#define RJ45_PINS   { PC0, PC1, PC2, PC3, PC4, PC5, PC6, PC7 }
#define RJ45_COUNT  8

// --- Rotary encoder with center button --------------------------------------------
#define PIN_ENC_A   PD5                 // EXTI line 5, quadrature input A
#define PIN_ENC_B   PD6                 // EXTI line 6 is free; B is read as plain input
#define PIN_ENC_BTN PD4                 // EXTI line 7, push button (active low)

#define ENC_DEBOUNCE_MS    20           // button debounce time [ms]
#define ENC_LONGPRESS_MS  700           // long press threshold [ms]

// --- SSD1306 128x64 OLED over soft I2C --------------------------------------------
#define PIN_I2C_SDA  PA1
#define PIN_I2C_SCL  PA2
#define OLED_ADDR    0x3C               // 7-bit I2C address of SSD1306 (0x78 write)
#define I2C_DELAY_US   3                // half SCL period -> about 100..150 kHz

// --- Software PWM (used for the '~' pin mode) --------------------------------------
#define PWM_FREQ_HZ   1000              // carrier frequency [Hz]
#define PWM_STEPS     50                // duty resolution (2% per step)

// --- Flash storage ------------------------------------------------------------------
// CH32V003 main flash is 16 KB, organized as 64-byte pages. The controller
// programs and erases them in the 0x08000000 alias (same array as 0x00000000).
#define FLASH_PAGE_SIZE   64
#define FLASH_BASE_ADDR   0x08000000UL
#define FLASH_SIZE        (16 * 1024)
#define FLASH_LAST_PAGE   (FLASH_BASE_ADDR + FLASH_SIZE - FLASH_PAGE_SIZE)
