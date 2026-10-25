#pragma once
// ===== Black box: event ring buffer + crash record in RTC memory =====
// RTC_NOINIT memory is NOT cleared by a software reset, so the evidence of a
// crash is still there after the reboot.
#include <Arduino.h>
#include "platform.h"

#define BB_EVENTS     20
#define BB_MAX_TASKS  12

enum EvType : uint8_t {
  EV_BOOT = 1, EV_SWITCH, EV_BLOCK, EV_UNBLOCK,
  EV_QFULL, EV_HANG, EV_RESTART, EV_FAULT, EV_ALARM
};

struct BBEvent {
  uint32_t t;        // ms since that boot
  uint16_t boot;     // boot number
  uint8_t  type;
  uint8_t  task;     // task id, 255 = kernel
  int16_t  arg;
};

struct CrashInfo {
  uint8_t  valid;
  uint8_t  fresh;    // not yet shown in boot report
  uint8_t  task;
  char     name[16];
  uint16_t boot;
  uint32_t at_ms;
  uint32_t ran_ms;
  uint32_t free_heap;
  uint8_t  stack_pct;
};

struct BlackBox {
  uint32_t magic;
  uint16_t boots;
  uint16_t head;     // next write index
  uint16_t count;
  BBEvent  ev[BB_EVENTS];
  CrashInfo crash;
  BBEvent  snap[BB_EVENTS];      // frozen copy of the ring at crash time (oldest first)
  uint16_t snapCount;
  uint8_t  restarts[BB_MAX_TASKS];
  uint8_t  hung[BB_MAX_TASKS];   // set at crash, consumed at next boot
};

extern BlackBox bb;

void bbInit();
void bbLog(uint8_t type, uint8_t task, int16_t arg);
void bbOnHang(int task, uint32_t ran_ms, uint8_t stack_pct, uint32_t heap);
void bbPrintLog();
void bbPrintCrash();
void bbClear();
