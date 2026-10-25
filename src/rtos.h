#pragma once
// ===== BlackBox RTOS: cooperative kernel =====
#include <Arduino.h>

#define MAX_TASKS      12
#define QUARANTINE_MS  3000   // a hung task sits out this long after reboot
#define MAX_RESTARTS   3      // after this many hangs the task is FAILED for good
#define Q_SIZE         4

enum State : uint8_t { READY, RUNNING, SLEEPING, BLOCKED, QUARANTINE, FAILED };
const char* stateName(State s);

struct Task {
  const char* name;
  void (*fn)();
  uint32_t period;              // default sleep after each run (ms)
  uint32_t timeout_ms;          // watchdog limit for ONE run
  uint32_t next_run;
  uint32_t runs;
  State state;
  int* waitCount;               // what a BLOCKED task is waiting on
  const char* waitName;
  volatile uint32_t run_start;  // when the current run began (read by watchdog)
  bool inject;                  // fault injection: hang on next run
  bool quiet;                   // true = don't log every run in the black box
};

struct Semaphore {
  const char* name;
  int count;
};

struct Queue {
  const char* name;
  int buf[Q_SIZE];
  int head, tail, count;
};

extern Task tasks[MAX_TASKS];
extern int taskCount;
extern volatile int runningTask;   // -1 when scheduler itself is running

// --- setup ---
int  rtosAdd(const char* name, void (*fn)(), uint32_t period, uint32_t timeout_ms);
void rtosBegin();                  // call once after all rtosAdd()
void rtosReset();                  // clears task table (used by PC tests)
void rtosStep();                   // call from loop()
int  rtosCurrentTask();            // id of the task that is running now
void rtosWdtCheck();               // called by the 100 ms timer

// --- API for tasks ---
void sleepMs(uint32_t ms);
void semInit(Semaphore* s, const char* name);
void semGive(Semaphore* s);
bool semTake(Semaphore* s);        // false => caller became BLOCKED, just return
void qInit(Queue* q, const char* name);
bool qSend(Queue* q, int value);   // false => queue full
bool qReceive(Queue* q, int* v);   // false => caller became BLOCKED, just return
