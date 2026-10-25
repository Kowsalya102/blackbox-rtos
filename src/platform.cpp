#ifndef HOST_TEST
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_system.h>
#include "platform.h"

#ifdef CONFIG_ARDUINO_LOOP_STACK_SIZE
#define LOOP_STACK_BYTES CONFIG_ARDUINO_LOOP_STACK_SIZE
#else
#define LOOP_STACK_BYTES 8192
#endif

static TaskHandle_t loopHandle = nullptr;
static void (*wdtCallback)() = nullptr;

static void timerThunk(void*) {
  if (wdtCallback) wdtCallback();
}

void platformStartWdt(void (*cb)()) {
  wdtCallback = cb;
  loopHandle = xTaskGetCurrentTaskHandle();     // setup() runs in the loop task

  esp_timer_create_args_t args = {};
  args.callback = timerThunk;
  args.name = "rtos_wdt";
  args.dispatch_method = ESP_TIMER_TASK;        // runs in a task that preempts loop()
  esp_timer_handle_t h;
  esp_timer_create(&args, &h);
  esp_timer_start_periodic(h, 100000);          // every 100 ms
}

void platformReboot() { esp_restart(); }

bool platformPowerOn() { return esp_reset_reason() == ESP_RST_POWERON; }

uint8_t platformStackPct() {
  if (!loopHandle) return 0;
  uint32_t freeBytes = uxTaskGetStackHighWaterMark(loopHandle);
  if (freeBytes > LOOP_STACK_BYTES) freeBytes = LOOP_STACK_BYTES;
  return (uint8_t)(((LOOP_STACK_BYTES - freeBytes) * 100UL) / LOOP_STACK_BYTES);
}

uint32_t platformFreeHeap() { return ESP.getFreeHeap(); }

void platformSpinForever() {
  volatile uint32_t x = 0;
  for (;;) { x++; }                              // real infinite loop
}

// 2 kHz tone on a buzzer pin. The LEDC API differs between Arduino core 2.x and 3.x.
void platformBuzzer(int pin, bool on) {
  static bool inited = false;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!inited) { ledcAttach(pin, 2000, 8); inited = true; }
  ledcWriteTone(pin, on ? 2000 : 0);
#else
  if (!inited) { ledcSetup(0, 2000, 8); ledcAttachPin(pin, 0); inited = true; }
  ledcWriteTone(0, on ? 2000 : 0);
#endif
}
#endif
