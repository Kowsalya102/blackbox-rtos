#include <Arduino.h>
#include "rtos.h"
#include "blackbox.h"
#include "shell.h"

// ===== BlackBox RTOS v1.0 =====
// Cooperative RTOS for ESP32: task states, sleep, queue, semaphore,
// per-task watchdog with recovery, black-box log and UART shell.

#define LED_PIN 2

Queue     sensorQ;      // Sensor -> Process
Semaphore beatSem;      // Heartbeat -> Monitor

// ---- application tasks (each runs to completion = cooperative yield) ----
void ledTask() {
  static bool on = false;
  on = !on;
  digitalWrite(LED_PIN, on);
}

void sensorTask() {
  int value = 20 + (millis() / 1000) % 15;          // fake temperature
  if (qSend(&sensorQ, value))
    Serial.printf("[Sensor] sent %d (queue=%d)\r\n", value, sensorQ.count);
  else
    Serial.println("[Sensor] queue FULL, value dropped");
}

void processTask() {
  int value;
  if (!qReceive(&sensorQ, &value)) return;          // empty -> BLOCKED
  Serial.printf("[Process] got %d\r\n", value);
}

void heartbeatTask() {
  Serial.println("[Heartbeat] alive");
  semGive(&beatSem);                                // wake Monitor
}

void monitorTask() {
  if (!semTake(&beatSem)) return;                   // no token -> BLOCKED
  Serial.printf("[Monitor] system ok, uptime %lu s\r\n", (unsigned long)(millis() / 1000));
}

// ---- kernel start ----
void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(LED_PIN, OUTPUT);

  qInit(&sensorQ, "sensorQ");
  semInit(&beatSem, "beatSem");

  //       name         function       period  watchdog limit (ms)
  rtosAdd("LED",        ledTask,        500,   1000);
  rtosAdd("Sensor",     sensorTask,    1000,   1000);
  rtosAdd("Process",    processTask,    100,   1000);
  rtosAdd("Heartbeat",  heartbeatTask, 3000,   1000);
  rtosAdd("Monitor",    monitorTask,    100,   1000);
  int shellId = rtosAdd("Shell", shellTask, 20,   1000);
  tasks[shellId].quiet = true;      // runs every 20 ms: keep it out of the black box

  Serial.println("\r\nBlackBox RTOS v1.0 booting...");
  rtosBegin();          // black box init, crash report, watchdog timer
  shellBegin();
}

void loop() {
  rtosStep();           // the scheduler
}
