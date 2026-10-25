#include "rtos.h"
#include "blackbox.h"
#include "platform.h"
#include <string.h>

Task tasks[MAX_TASKS];
int taskCount = 0;
volatile int runningTask = -1;
static int currentTask = 0;

const char* stateName(State s) {
  switch (s) {
    case READY:      return "READY";
    case RUNNING:    return "RUNNING";
    case SLEEPING:   return "SLEEPING";
    case BLOCKED:    return "BLOCKED";
    case QUARANTINE: return "QUARANTINE";
    case FAILED:     return "FAILED";
  }
  return "?";
}

// ---------- task table ----------
void rtosReset() {
  memset(tasks, 0, sizeof(tasks));
  taskCount = 0;
  runningTask = -1;
  currentTask = 0;
}

int rtosAdd(const char* name, void (*fn)(), uint32_t period, uint32_t timeout_ms) {
  if (taskCount >= MAX_TASKS) return -1;
  Task& t = tasks[taskCount];
  t.name = name;
  t.fn = fn;
  t.period = period;
  t.timeout_ms = timeout_ms;
  t.next_run = 0;
  t.runs = 0;
  t.state = SLEEPING;           // becomes READY on the first step
  t.waitCount = nullptr;
  t.waitName = nullptr;
  t.run_start = 0;
  t.inject = false;
  t.quiet = false;
  return taskCount++;
}

// ---------- boot: black box + crash recovery ----------
void rtosBegin() {
  bbInit();
  bbLog(EV_BOOT, 255, (int16_t)bb.boots);

  // Did a task hang in the previous boot? Quarantine it, or give up on it.
  for (int i = 0; i < taskCount && i < BB_MAX_TASKS; i++) {
    if (!bb.hung[i]) continue;
    bb.hung[i] = 0;
    if (bb.restarts[i] >= MAX_RESTARTS) {
      tasks[i].state = FAILED;
    } else {
      tasks[i].state = QUARANTINE;
      tasks[i].next_run = millis() + QUARANTINE_MS;
    }
  }

  if (bb.crash.valid && bb.crash.fresh) {
    bbPrintCrash();
    bb.crash.fresh = 0;
    for (int i = 0; i < taskCount; i++) {
      if (tasks[i].state == QUARANTINE)
        Serial.printf("[OS] '%s' quarantined for %d ms, other tasks keep running\r\n",
                      tasks[i].name, QUARANTINE_MS);
      if (tasks[i].state == FAILED)
        Serial.printf("[OS] '%s' hung %d times -> FAILED, disabled until power cycle\r\n",
                      tasks[i].name, MAX_RESTARTS);
    }
  }

  platformStartWdt(rtosWdtCheck);
}

// ---------- watchdog (runs from the 100 ms timer, preempts a hung task) ----------
void rtosWdtCheck() {
  int r = runningTask;
  if (r < 0 || r >= taskCount) return;
  Task& t = tasks[r];
  uint32_t ran = millis() - t.run_start;
  if (ran <= t.timeout_ms) return;

  bbOnHang(r, ran, platformStackPct(), platformFreeHeap());
  Serial.printf("\r\n[WDT] task '%s' ran %lu ms (limit %lu) -> HUNG. Saving black box, rebooting...\r\n",
                t.name, (unsigned long)ran, (unsigned long)t.timeout_ms);
  Serial.flush();
  platformReboot();
}

// ---------- scheduler ----------
void rtosStep() {
  for (int i = 0; i < taskCount; i++) {
    Task& t = tasks[i];
    uint32_t now = millis();

    if (t.state == FAILED) continue;

    if (t.state == QUARANTINE) {
      if ((int32_t)(now - t.next_run) < 0) continue;
      t.state = READY;
      bbLog(EV_RESTART, (uint8_t)i, bb.restarts[i]);
      Serial.printf("[OS] task '%s' restarted\r\n", t.name);
    }

    if (t.state == SLEEPING && (int32_t)(now - t.next_run) >= 0) t.state = READY;

    if (t.state == BLOCKED && t.waitCount && *t.waitCount > 0) {
      t.state = READY;
      t.waitCount = nullptr;
      t.waitName = nullptr;
      bbLog(EV_UNBLOCK, (uint8_t)i, 0);
    }

    if (t.state != READY) continue;

    currentTask = i;
    t.state = RUNNING;
    if (!t.quiet) bbLog(EV_SWITCH, (uint8_t)i, 0);

    t.run_start = millis();
    runningTask = i;                 // watchdog starts timing now

    if (t.inject) {                  // fault injection from the shell: "hang <task>"
      t.inject = false;
      bbLog(EV_FAULT, (uint8_t)i, 0);
      Serial.printf("[FAULT] injecting infinite loop in '%s'\r\n", t.name);
      Serial.flush();
      platformSpinForever();
    }

    t.fn();

    runningTask = -1;
    if (t.state != BLOCKED) t.runs++;
    if (t.state == RUNNING) {
      t.next_run = millis() + t.period;
      t.state = SLEEPING;
    }
  }
}

// ---------- API used by tasks ----------
int rtosCurrentTask() { return currentTask; }

void sleepMs(uint32_t ms) {
  tasks[currentTask].next_run = millis() + ms;
  tasks[currentTask].state = SLEEPING;
}

static void blockCurrent(int* counter, const char* name) {
  tasks[currentTask].state = BLOCKED;
  tasks[currentTask].waitCount = counter;
  tasks[currentTask].waitName = name;
  bbLog(EV_BLOCK, (uint8_t)currentTask, 0);
}

void semInit(Semaphore* s, const char* name) { s->name = name; s->count = 0; }
void semGive(Semaphore* s) { s->count++; }
bool semTake(Semaphore* s) {
  if (s->count > 0) { s->count--; return true; }
  blockCurrent(&s->count, s->name);
  return false;
}

void qInit(Queue* q, const char* name) {
  memset(q, 0, sizeof(*q));
  q->name = name;
}

bool qSend(Queue* q, int value) {
  if (q->count >= Q_SIZE) {
    bbLog(EV_QFULL, (uint8_t)currentTask, 0);
    return false;
  }
  q->buf[q->tail] = value;
  q->tail = (q->tail + 1) % Q_SIZE;
  q->count++;
  return true;
}

bool qReceive(Queue* q, int* value) {
  if (q->count > 0) {
    *value = q->buf[q->head];
    q->head = (q->head + 1) % Q_SIZE;
    q->count--;
    return true;
  }
  blockCurrent(&q->count, q->name);
  return false;
}
