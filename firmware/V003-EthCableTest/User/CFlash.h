// ===================================================================================
// Project:   Ethernet Cable Tester - Structured Flash Storage
// Class:     CFlash
// ===================================================================================
//
// Stores a structured data record in the LAST flash page of the CH32V003
// (16 KB flash, 64-byte pages => page at 0x08003FC0). The linker scripts keep
// code out of that page. Fast page erase / page program is used (PAGE_ER,
// PAGE_PG): the standard PG bit programs a half-word, not a 32-bit word.
//
// The record is passed by reference:
//
//   struct MyRec { uint8_t modes[8]; ... };
//   static MyRec rec;
//   CFlash::save(rec);        // const MyRec& overload via template
//   CFlash::load(rec);        // read back on startup
//
// Layout of the page:
//   offset 0 : header  { uint32 magic; uint16 size; uint16 crc; }  (8 bytes)
//   offset 8 : raw record bytes (sizeof(T))
//
// The header is checked on load (magic, size, CRC16-CCITT), so a blank or
// interrupted write is rejected safely.
//
// ===================================================================================
#pragma once

#include <stdint.h>
#include <string.h>
#include "system.h"
#include "config.h"

class CFlash {
  static const uint32_t MAGIC   = 0x43544845UL;   // "ETHC" in little endian
  static const uint16_t HDR     = 8;
  static const uint16_t MAXREC  = FLASH_PAGE_SIZE - HDR;

public:
  // Save a structured record (passed by reference) to the last flash page.
  template <typename T>
  static bool save(const T& data) {
    static_assert(sizeof(T) <= MAXREC, "CFlash: record too big for one flash page");

    alignas(4) uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));
    Hdr h;
    h.magic = MAGIC;
    h.size  = (uint16_t)sizeof(T);
    h.crc   = crc16((const uint8_t*)&data, sizeof(T));
    memcpy(page, &h, HDR);
    memcpy(page + HDR, &data, sizeof(T));

    // flash write must not be interrupted; the core stalls on flash fetch while BSY
    uint32_t m = __iSave();
    unlock();
    bool ok = erasePage() && programPage(page);
    lock();
    __iRestore(&m);
    if (!ok) return false;

    return 0 == memcmp((const void*)FLASH_LAST_PAGE, page, FLASH_PAGE_SIZE);
  }

  // Load a structured record from the last flash page, false if invalid.
  template <typename T>
  static bool load(T& data) {
    static_assert(sizeof(T) <= MAXREC, "CFlash: record too big for one flash page");

    const Hdr* h = (const Hdr*)FLASH_LAST_PAGE;
    if (h->magic != MAGIC) return false;
    if (h->size  != (uint16_t)sizeof(T)) return false;
    if (h->crc   != crc16((const uint8_t*)(FLASH_LAST_PAGE + HDR), sizeof(T))) return false;

    memcpy(&data, (const void*)(FLASH_LAST_PAGE + HDR), sizeof(T));
    return true;
  }

  // Check whether a valid record is currently stored in flash.
  static bool available(uint16_t size) {
    const Hdr* h = (const Hdr*)FLASH_LAST_PAGE;
    if (h->magic != MAGIC) return false;
    if (h->size  != size)  return false;
    return h->crc == crc16((const uint8_t*)(FLASH_LAST_PAGE + HDR), size);
  }

private:
  struct Hdr {
    uint32_t magic;
    uint16_t size;
    uint16_t crc;
  };

  static uint16_t crc16(const uint8_t* p, uint32_t n) {
    uint16_t c = 0xFFFF;
    while (n--) {
      c ^= (uint16_t)*p++ << 8;
      for (uint8_t i = 0; i < 8; i++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
  }

  static const uint32_t KEY1 = 0x45670123;
  static const uint32_t KEY2 = 0xCDEF89AB;
  static const uint32_t ERR  = FLASH_STATR_WRPRTERR | FLASH_STATR_PGERR;

  static void unlock(void) {
    FLASH->KEYR = KEY1;
    FLASH->KEYR = KEY2;
    FLASH->MODEKEYR = KEY1;                        // fast-program unlock
    FLASH->MODEKEYR = KEY2;
  }

  static void lock(void) {
    FLASH->CTLR = FLASH_CTLR_LOCK | FLASH_CTLR_FLOCK;
  }

  static void waitBusy(void) {
    while (FLASH->STATR & FLASH_STATR_BSY);
  }

  static bool failed(void) {
    if (FLASH->STATR & ERR) {
      FLASH->STATR = ERR;                          // write 1 to clear
      return true;
    }
    return false;
  }

  // Erase the 64-byte page at FLASH_LAST_PAGE.
  static bool erasePage(void) {
    FLASH->STATR = FLASH_STATR_EOP | ERR;
    FLASH->CTLR  = FLASH_CTLR_PAGE_ER;
    FLASH->ADDR  = FLASH_LAST_PAGE;
    FLASH->CTLR  = FLASH_CTLR_PAGE_ER | FLASH_CTLR_STRT;
    waitBusy();
    bool bad = failed();
    FLASH->CTLR = 0;
    return !bad;
  }

  // Load 16 words into the page buffer, then commit them.
  static bool programPage(const uint8_t* page) {
    FLASH->STATR = FLASH_STATR_EOP | ERR;
    FLASH->CTLR  = FLASH_CTLR_PAGE_PG;
    FLASH->CTLR  = FLASH_CTLR_PAGE_PG | FLASH_CTLR_BUF_RST;
    waitBusy();

    for (uint32_t i = 0; i < (FLASH_PAGE_SIZE / 4); i++) {
      uint32_t addr = FLASH_LAST_PAGE + i * 4;
      const uint8_t* p = page + i * 4;
      uint32_t w = (uint32_t)p[0] | (uint32_t)p[1] << 8 |
                   (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
      *(volatile uint32_t*)addr = w;               // offset selects the buffer slot
      FLASH->CTLR = FLASH_CTLR_PAGE_PG | FLASH_CTLR_BUF_LOAD;
      waitBusy();
    }

    FLASH->CTLR = FLASH_CTLR_PAGE_PG;
    FLASH->ADDR = FLASH_LAST_PAGE;
    FLASH->CTLR = FLASH_CTLR_PAGE_PG | FLASH_CTLR_STRT;
    waitBusy();
    bool bad = failed();
    FLASH->CTLR = 0;
    return !bad;
  }
};
