#include <Arduino.h>
#include "rtos.h"
#include "blackbox.h"
#include "shell.h"
#include "platform.h"

// ===== Industrial Multi-Sensor Monitoring and Fault Recovery System =====
// Application running on the BlackBox RTOS (ESP32, simulated in Wokwi).
//
//   3 analog sensors (potentiometers stand in for real transmitters):
//     Temperature  GPIO34   0..150.0 C   alarm above 80.0 C
//     Pressure     GPIO35   0..10.0 bar  alarm above 7.0 bar
//     Level        GPIO32   0..100 %     alarm below 10 % or above 90 %
//   Outputs: green status LED (GPIO2), red alarm LED (GPIO26), buzzer (GPIO27)
//   Push button (GPIO4): injects a hang into the next sensor task, so you can
//   watch the watchdog detect it, the black box record it and the system recover.

#define PIN_STATUS_LED 2
#define PIN_ALARM_LED  26
#define PIN_BUZZER     27
#define PIN_BUTTON     4
#define PIN_TEMP       34
#define PIN_PRESSURE   35
#define PIN_LEVEL      32

enum SensorId { S_TEMP = 0, S_PRESS = 1, S_LEVEL = 2, S_COUNT = 3 };
static const char* sensorName[S_COUNT]  = {"Temperature", "Pressure", "Level"};
static const char* sensorShort[S_COUNT] = {"Temp", "Press", "Level"};

#define TEMP_ALARM_DC   800   // 80.0 C   (value is in 0.1 C)
#define PRESS_ALARM_DB  70    // 7.0 bar  (value is in 0.1 bar)
#define LEVEL_LOW_PCT   10
#define LEVEL_HIGH_PCT  90
#define STALE_MS        3000  // no new reading for this long = STALE

Queue     sensorQ;      // sensor tasks -> Analyzer   (message = id << 16 | value)
Semaphore reportSem;    // Heartbeat -> Report

static int      latest[S_COUNT];
static bool     alarmOn[S_COUNT];
static uint32_t lastUpdate[S_COUNT];
static int      sensorTaskId[S_COUNT];
static int      droppedMsgs = 0;

// ---------------- helpers ----------------
static int readScaled(int pin, int fullScale) {
  return (int)((long)analogRead(pin) * fullScale / 4095);   // ESP32 ADC = 12 bit
}

static void publish(int id, int value) {
  if (!qSend(&sensorQ, (id << 16) | (value & 0xFFFF))) droppedMsgs++;
}

static bool outOfRange(int id, int v) {
  switch (id) {
    case S_TEMP:  return v > TEMP_ALARM_DC;
    case S_PRESS: return v > PRESS_ALARM_DB;
    default:      return v < LEVEL_LOW_PCT || v > LEVEL_HIGH_PCT;
  }
}

static void printValue(int id, int v) {
  if (id == S_TEMP)       Serial.printf("%d.%d C", v / 10, v % 10);
  else if (id == S_PRESS) Serial.printf("%d.%d bar", v / 10, v % 10);
  else                    Serial.printf("%d %%", v);
}

// ---------------- tasks ----------------
void statusLedTask() {                       // green LED = "RTOS is alive"
  static bool on = false;
  on = !on;
  digitalWrite(PIN_STATUS_LED, on);
}

void tempTask()     { publish(S_TEMP,  readScaled(PIN_TEMP, 1500)); }      // 0.1 C
void pressureTask() { publish(S_PRESS, readScaled(PIN_PRESSURE, 100)); }    // 0.1 bar
void levelTask()    { publish(S_LEVEL, readScaled(PIN_LEVEL, 100)); }       // %

void analyzerTask() {                        // one message per run; BLOCKED while the queue is empty
  int msg;
  if (!qReceive(&sensorQ, &msg)) return;

  int id = msg >> 16;
  int v  = msg & 0xFFFF;
  if (id < 0 || id >= S_COUNT) return;

  latest[id] = v;
  lastUpdate[id] = millis();

  bool bad = outOfRange(id, v);
  if (bad != alarmOn[id]) {
    alarmOn[id] = bad;
    if (bad) {
      bbLog(EV_ALARM, (uint8_t)rtosCurrentTask(), (int16_t)id);
      Serial.printf("[ALARM] %s = ", sensorName[id]);
      printValue(id, v);
      Serial.println(" (out of range)");
    } else {
      Serial.printf("[OK] %s back to normal: ", sensorName[id]);
      printValue(id, v);
      Serial.println();
    }
  }
}

void alarmTask() {                           // red LED + buzzer beep while any alarm is active
  static bool phase = false;
  bool any = alarmOn[0] || alarmOn[1] || alarmOn[2];
  if (any) {
    phase = !phase;
    digitalWrite(PIN_ALARM_LED, phase);
    platformBuzzer(PIN_BUZZER, phase);
  } else if (phase) {
    phase = false;
    digitalWrite(PIN_ALARM_LED, LOW);
    platformBuzzer(PIN_BUZZER, false);
  }
}

void heartbeatTask() {
  semGive(&reportSem);                       // wake the Report task
}

void reportTask() {
  if (!semTake(&reportSem)) return;          // no token -> BLOCKED
  Serial.print("[REPORT] ");
  for (int i = 0; i < S_COUNT; i++) {
    Serial.printf("%s=", sensorShort[i]);
    if (lastUpdate[i] == 0 || millis() - lastUpdate[i] > STALE_MS) Serial.print("STALE");
    else printValue(i, latest[i]);
    if (alarmOn[i]) Serial.print("(!)");
    Serial.print("  ");
  }
  Serial.printf("| uptime %lu s\r\n", (unsigned long)(millis() / 1000));
}

void buttonTask() {                          // each press = inject a hang in the next sensor task
  static bool prev = true;                   // pull-up: HIGH when not pressed
  static int next = 0;
  bool level = digitalRead(PIN_BUTTON);
  if (prev && !level) {
    int id = next;
    next = (next + 1) % S_COUNT;
    tasks[sensorTaskId[id]].inject = true;
    Serial.printf("[BUTTON] fault injected into %s sensor task\r\n", sensorName[id]);
  }
  prev = level;
}

// ---------------- kernel start ----------------
void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_ALARM_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  digitalWrite(PIN_ALARM_LED, LOW);

  qInit(&sensorQ, "sensorQ");
  semInit(&reportSem, "reportSem");

  //                          name         function        period  watchdog limit (ms)
  int ledId = rtosAdd("StatusLED", statusLedTask, 500, 1000);
  sensorTaskId[S_TEMP]  = rtosAdd("Temp",      tempTask,      500,  1000);
  sensorTaskId[S_PRESS] = rtosAdd("Pressure",  pressureTask,  500,  1000);
  sensorTaskId[S_LEVEL] = rtosAdd("Level",     levelTask,     1000, 1000);
  rtosAdd("Analyzer",  analyzerTask,  100,  1000);
  rtosAdd("Alarm",     alarmTask,     250,  1000);
  int hbId = rtosAdd("Heartbeat", heartbeatTask, 3000, 1000);
  rtosAdd("Report",    reportTask,    100,  1000);
  int btnId   = rtosAdd("Button",    buttonTask,    50,   1000);
  int shellId = rtosAdd("Shell",     shellTask,     20,   1000);

  tasks[hbId].next_run = millis() + 2000;    // first report 2 s after boot, once readings exist
  tasks[ledId].quiet = true;                 // fast housekeeping tasks stay out of the black box
  tasks[btnId].quiet = true;
  tasks[shellId].quiet = true;

  Serial.println("\r\nIndustrial Multi-Sensor Monitoring and Fault Recovery System");
  Serial.println("running on BlackBox RTOS v1.1");
  rtosBegin();          // black box init, crash report, watchdog timer
  shellBegin();
}

void loop() {
  rtosStep();           // the scheduler
}
