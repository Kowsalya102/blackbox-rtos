// ===== BlackBox RTOS v0.2 (cooperative scheduler) =====
#define LED_PIN 2

// ---- Task Control Block (TCB v1) ----
struct Task {
  const char* name;      // task peru
  void (*fn)();          // task function pointer
  uint32_t period;       // evlo ms-ku oru dhadava run aaganum
  uint32_t next_run;     // adutha run neram (ms)
  uint32_t runs;         // ethana dhadava run aachu
};

// ---- Task table (forward declaration) ----
extern Task tasks[];
extern const int N;

// ---- Tasks (apps) ----
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
  Serial.println("--- STATUS ---");
  for (int i = 0; i < N; i++) {
    Serial.printf("%-10s runs=%lu\n", tasks[i].name, tasks[i].runs);
  }
}

// ---- Task table definition ----
Task tasks[] = {
  {"LED",       ledTask,       500,  0, 0},
  {"Sensor",    sensorTask,    1000, 0, 0},
  {"Heartbeat", heartbeatTask, 3000, 0, 0},
  {"Status",    statusTask,    5000, 0, 0},
};
const int N = sizeof(tasks) / sizeof(tasks[0]);

// ---- Kernel ----
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("BlackBox RTOS v0.2 booting...");
}

// loop() = scheduler: neram vandha task-ah run pannum
void loop() {
  uint32_t now = millis();
  for (int i = 0; i < N; i++) {
    if ((int32_t)(now - tasks[i].next_run) >= 0) {
      tasks[i].fn();                          // task run (cooperative)
      tasks[i].runs++;
      tasks[i].next_run = now + tasks[i].period;
    }
  }
}
