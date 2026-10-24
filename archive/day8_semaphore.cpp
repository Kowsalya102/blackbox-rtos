#include <Arduino.h>

// ===== BlackBox RTOS v0.4 (semaphore + BLOCKED) =====
#define LED_PIN 2

enum State { READY, RUNNING, SLEEPING, BLOCKED };

const char* stateName(State s) {
  switch (s) {
    case READY:    return "READY";
    case RUNNING:  return "RUNNING";
    case SLEEPING: return "SLEEPING";
    case BLOCKED:  return "BLOCKED";
  }
  return "?";
}

// ---- Semaphore ----
struct Semaphore {
  const char* name;
  int count;
};

// ---- Task Control Block (TCB v3) ----
struct Task {
  const char* name;
  void (*fn)();
  uint32_t period;
  uint32_t next_run;
  uint32_t runs;
  State state;
  Semaphore* waitOn;   // NEW: edhukku kaaththirukku
};

extern Task tasks[];
extern const int N;
int current = 0;

// ---- Kernel API ----
void sleepMs(uint32_t ms) {
  tasks[current].next_run = millis() + ms;
  tasks[current].state = SLEEPING;
}

void semGive(Semaphore* s) {
  s->count++;
}

// token irundha edukkum (true). Illa-na task-ah BLOCKED aakkum (false)
bool semTake(Semaphore* s) {
  if (s->count > 0) {
    s->count--;
    return true;
  }
  tasks[current].state = BLOCKED;
  tasks[current].waitOn = s;
  return false;
}

Semaphore dataReady = {"dataReady", 0};

// ---- Tasks ----
void ledTask() {
  static bool s = false;
  s = !s;
  digitalWrite(LED_PIN, s);
}

void sensorTask() {
  Serial.printf("[Sensor] new data at %lu ms\n", millis());
  semGive(&dataReady);            // token kudu
}

void processTask() {
  if (!semTake(&dataReady)) return;   // token illa -> BLOCKED
  Serial.println("[Process] got data, processing...");
}

void heartbeatTask() {
  Serial.println("[Heartbeat] alive");
}

void statusTask() {
  Serial.println("--- STATUS (ps) ---");
  for (int i = 0; i < N; i++) {
    Serial.printf("%-10s %-9s runs=%lu", tasks[i].name,
                  stateName(tasks[i].state), tasks[i].runs);
    if (tasks[i].state == BLOCKED && tasks[i].waitOn)
      Serial.printf("  waiting:%s", tasks[i].waitOn->name);
    Serial.println();
  }
}

// ---- Task table ----
Task tasks[] = {
  {"LED",       ledTask,       500,  0, 0, SLEEPING, nullptr},
  {"Sensor",    sensorTask,    1000, 0, 0, SLEEPING, nullptr},
  {"Process",   processTask,   100,  0, 0, SLEEPING, nullptr},
  {"Heartbeat", heartbeatTask, 3000, 0, 0, SLEEPING, nullptr},
  {"Status",    statusTask,    5000, 0, 0, SLEEPING, nullptr},
};
const int N = sizeof(tasks) / sizeof(tasks[0]);

// ---- Kernel ----
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("BlackBox RTOS v0.4 booting...");
}

void loop() {
  for (int i = 0; i < N; i++) {
    uint32_t now = millis();

    // 1. SLEEPING: neram vandha READY
    if (tasks[i].state == SLEEPING && (int32_t)(now - tasks[i].next_run) >= 0) {
      tasks[i].state = READY;
    }

    // 2. BLOCKED: token vandha READY (NEW)
    if (tasks[i].state == BLOCKED && tasks[i].waitOn && tasks[i].waitOn->count > 0) {
      tasks[i].state = READY;
      tasks[i].waitOn = nullptr;
    }

    // 3. READY: run pannu
    if (tasks[i].state == READY) {
      current = i;
      tasks[i].state = RUNNING;
      tasks[i].fn();

      if (tasks[i].state != BLOCKED) tasks[i].runs++;   // block aana run count vendaam

      if (tasks[i].state == RUNNING) {
        tasks[i].next_run = millis() + tasks[i].period;
        tasks[i].state = SLEEPING;
      }
    }
  }
}
