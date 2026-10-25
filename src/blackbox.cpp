#include "blackbox.h"
#include "rtos.h"
#include <string.h>

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

static_assert(MAX_TASKS <= BB_MAX_TASKS, "black box must have room for every task");

#define BB_MAGIC (0xB10CB0A7u ^ (uint32_t)sizeof(BlackBox))

RTC_NOINIT_ATTR BlackBox bb;

static const char* evName(uint8_t t) {
  switch (t) {
    case EV_BOOT:    return "BOOT";
    case EV_SWITCH:  return "RUN";
    case EV_BLOCK:   return "BLOCK";
    case EV_UNBLOCK: return "UNBLOCK";
    case EV_QFULL:   return "QFULL";
    case EV_HANG:    return "HANG";
    case EV_RESTART: return "RESTART";
    case EV_FAULT:   return "FAULT";
    case EV_ALARM:   return "ALARM";
  }
  return "?";
}

static const char* tName(uint8_t id) {
  if (id < taskCount) return tasks[id].name;
  return "kernel";
}

void bbInit() {
  // Cold power-on, or RTC memory holds garbage / data from another firmware
  if (platformPowerOn() || bb.magic != BB_MAGIC) {
    memset(&bb, 0, sizeof(bb));
    bb.magic = BB_MAGIC;
  }
  bb.boots++;
}

void bbClear() {
  uint16_t boots = bb.boots;
  memset(&bb, 0, sizeof(bb));
  bb.magic = BB_MAGIC;
  bb.boots = boots;
}

void bbLog(uint8_t type, uint8_t task, int16_t arg) {
  BBEvent& e = bb.ev[bb.head];
  e.t = millis();
  e.boot = bb.boots;
  e.type = type;
  e.task = task;
  e.arg = arg;
  bb.head = (bb.head + 1) % BB_EVENTS;
  if (bb.count < BB_EVENTS) bb.count++;
}

void bbOnHang(int task, uint32_t ran_ms, uint8_t stack_pct, uint32_t heap) {
  bbLog(EV_HANG, (uint8_t)task, (int16_t)(ran_ms > 32000 ? 32000 : ran_ms));
  CrashInfo& c = bb.crash;
  c.valid = 1;
  c.fresh = 1;
  c.task = (uint8_t)task;
  strncpy(c.name, tasks[task].name, sizeof(c.name) - 1);
  c.name[sizeof(c.name) - 1] = 0;
  c.boot = bb.boots;
  c.at_ms = millis();
  c.ran_ms = ran_ms;
  c.free_heap = heap;
  c.stack_pct = stack_pct;
  if (task < BB_MAX_TASKS) {
    bb.hung[task] = 1;
    if (bb.restarts[task] < 255) bb.restarts[task]++;
  }

  // Freeze the last events (including the HANG itself) so later activity
  // cannot overwrite the evidence.
  bb.snapCount = bb.count;
  for (uint16_t i = 0; i < bb.count; i++)
    bb.snap[i] = bb.ev[(bb.head + BB_EVENTS - bb.count + i) % BB_EVENTS];
}

static void printEvent(const BBEvent& e) {
  Serial.printf("[boot%-2u +%7lums] %-8s %s", (unsigned)e.boot, (unsigned long)e.t,
                evName(e.type), e.task == 255 ? "-" : tName(e.task));
  if (e.type == EV_HANG)    Serial.printf("  ran %d ms", (int)e.arg);
  if (e.type == EV_RESTART) Serial.printf("  restart #%d", (int)e.arg);
  if (e.type == EV_BOOT)    Serial.printf("  boot #%d", (int)e.arg);
  if (e.type == EV_ALARM)   Serial.printf("  sensor #%d", (int)e.arg);
  Serial.println();
}

void bbPrintLog() {
  Serial.printf("--- BLACK BOX: last %u events (oldest first) ---\r\n", (unsigned)bb.count);
  if (bb.count == 0) { Serial.println("(empty)"); return; }
  for (uint16_t i = 0; i < bb.count; i++)
    printEvent(bb.ev[(bb.head + BB_EVENTS - bb.count + i) % BB_EVENTS]);
}

void bbPrintCrash() {
  const CrashInfo& c = bb.crash;
  if (!c.valid) { Serial.println("No crash recorded."); return; }
  Serial.println("=========== CRASH REPORT ===========");
  Serial.printf("Task '%s' (id %u) HUNG in boot #%u at +%lu ms\r\n",
                c.name, (unsigned)c.task, (unsigned)c.boot, (unsigned long)c.at_ms);
  Serial.printf("It ran %lu ms without returning (limit %lu ms)\r\n",
                (unsigned long)c.ran_ms,
                (unsigned long)(c.task < taskCount ? tasks[c.task].timeout_ms : 0));
  Serial.printf("Stack: %u%% full   Free heap: %lu bytes\r\n",
                (unsigned)c.stack_pct, (unsigned long)c.free_heap);
  if (c.task < BB_MAX_TASKS) {
    Serial.printf("Hang count for this task: %u of %u allowed\r\n",
                  (unsigned)bb.restarts[c.task], (unsigned)MAX_RESTARTS);
  }
  Serial.printf("Last %u events before the crash:\r\n", (unsigned)bb.snapCount);
  for (uint16_t i = 0; i < bb.snapCount; i++) printEvent(bb.snap[i]);
  Serial.println("====================================");
}
