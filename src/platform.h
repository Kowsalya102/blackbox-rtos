#pragma once
// Thin hardware layer. Real ESP32 code is in platform.cpp.
// For PC testing (HOST_TEST) the test program provides these functions.
#include <Arduino.h>

// Start a 100 ms periodic timer that runs cb() in a high-priority context.
// Must be called from setup() (it remembers the loop task for stack stats).
void platformStartWdt(void (*cb)());

void platformReboot();          // soft reset (RTC memory survives)
bool platformPowerOn();         // true if this boot was a cold power-on
uint8_t platformStackPct();     // % of loop-task stack used (high-water mark)
uint32_t platformFreeHeap();
void platformSpinForever();     // used by fault injection: never returns
