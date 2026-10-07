#define LED_PIN 2

struct Task {
  const char* name;
  void (*fn)();
  uint32_t period;
  uint32_t next_run;
};

void ledTask() {
  static bool s = false;
  s = !s;
  digitalWrite(LED_PIN, s);
  Serial.println(s ? "[LED] ON" : "[LED] OFF");
}

void sensorTask() {
  Serial.printf("[Sensor] reading at %lu ms\n", millis());
}

Task tasks[] = {
  {"LED",    ledTask,    500,  0},
  {"Sensor", sensorTask, 1000, 0},
};
const int N = sizeof(tasks) / sizeof(tasks[0]);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("BlackBox RTOS v0.1 booting...");
}

void loop() {
  uint32_t now = millis();
  for (int i = 0; i < N; i++) {
    if ((int32_t)(now - tasks[i].next_run) >= 0) {
      tasks[i].fn();
      tasks[i].next_run = now + tasks[i].period;
    }
  }
}
