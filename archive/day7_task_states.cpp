#include <Arduino.h>

// ===== BlackBox RTOS v0.3 (task states + sleep) =====
#define LED_PIN 2

// ---- Task states ----
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

// ---- Task Control Block (TCB v2) ----
struct Task {
  const char* name;
  void (*fn)();
  uint32_t period;     // default sleep, run aana apram
  uint32_t next_run;   // eppo ezhumbanum
  uint32_t runs;
  State state;
};

extern Task tasks[];
extern const int N;
int current = 0;       // ippo edhu task run aagudhu

// ---- Kernel API ----
// task kulla irundhu call pannalaam: "naan ms neram thoonguren"
void sleepMs(uint32_t ms) {
  tasks[current].next_run = millis() + ms;
  tasks[current].state = SLEEPING;
}

// ---- Tasks ----
void ledTask() {
  static bool s = false;
  s = !s;
  digitalWrite(LED_PIN, s);
}

void sensorTask() {
  Serial.printf("[Sensor] reading at %lu ms\n", millis());
}

void heartbeatTask() {
  Serial.println("[Heartbeat] alive");
}

void statusTask() {
  Serial.println("--- STATUS (ps) ---");
  for (int i = 0; i < N; i++) {
    Serial.printf("%-10s %-9s runs=%lu\n",
                  tasks[i].name, stateName(tasks[i].state), tasks[i].runs);
  }
}

// ---- Task table ----
Task tasks[] = {
  {"LED",       ledTask,       500,  0, 0, SLEEPING},
  {"Sensor",    sensorTask,    1000, 0, 0, SLEEPING},
  {"Heartbeat", heartbeatTask, 3000, 0, 0, SLEEPING},
  {"Status",    statusTask,    5000, 0, 0, SLEEPING},
};
const int N = sizeof(tasks) / sizeof(tasks[0]);

// ---- Kernel ----
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("BlackBox RTOS v0.3 booting...");
}

void loop() {
  for (int i = 0; i < N; i++) {
    uint32_t now = millis();

    // 1. SLEEPING task-ku neram vandha, READY aakku
    if (tasks[i].state == SLEEPING && (int32_t)(now - tasks[i].next_run) >= 0) {
      tasks[i].state = READY;
    }

    // 2. READY task-ah run pannu
    if (tasks[i].state == READY) {
      current = i;
      tasks[i].state = RUNNING;
      tasks[i].fn();
      tasks[i].runs++;

      // 3. Task thaane sleepMs() call pannala-na, default period-ku thoonga vai
      if (tasks[i].state == RUNNING) {
        tasks[i].next_run = millis() + tasks[i].period;
        tasks[i].state = SLEEPING;
      }
    }
  }
}
