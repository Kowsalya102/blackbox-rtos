# Black-Box RTOS

A lightweight cooperative mini RTOS kernel in C/C++ for ESP32,
with per-task watchdog recovery and a crash black-box logger.

## Problem
In embedded devices, one hung task can freeze the whole system.
After a reboot, the evidence of what happened is lost.

## Goal
- Detect a hung task using a per-task watchdog
- Restart only that task, other tasks keep running
- Record the last events (black box) and show a crash report after reboot

## Planned features
- [x] Cooperative scheduler (task table + tick)
- [ ] Sleep / delay, queue, semaphore
- [ ] Per-task watchdog + auto restart
- [ ] Black-box event log + crash report
- [ ] UART shell (ps, log, crash, hang)
- [ ] Fault injection tests

## Hardware / Tools
- ESP32 DevKit (or Wokwi simulator)
- VS Code + PlatformIO, Arduino framework

## Status
Week 1: LED blink, UART print, basic scheduler with 3 tasks.

## Limitations
Cooperative (no preemption), no priority, no memory protection.
This is a learning project, not a production RTOS.
