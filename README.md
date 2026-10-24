# Black-Box RTOS

A small cooperative RTOS for ESP32 (Arduino framework, C++) that **detects a hung task, saves a black-box record of what happened, recovers, and tells you why** after the reboot.

## Problem

In embedded devices one hung task can freeze the whole system, and after a reboot the evidence of what happened is lost.

## What it does

| Feature | How |
|---|---|
| Cooperative scheduler | Round-robin task table, tasks run to completion (return = yield) |
| Task states | `READY`, `RUNNING`, `SLEEPING`, `BLOCKED`, `QUARANTINE`, `FAILED` |
| Sleep / delay | `sleepMs()`, per-task period |
| Queue | Fixed-size ring buffer, `qSend()` / `qReceive()`, receiver blocks when empty |
| Semaphore | `semGive()` / `semTake()`, taker blocks when no token |
| Per-task watchdog | 100 ms hardware-timer callback checks how long the current task has been running; over its limit = HUNG |
| Black box | Last 20 events (task run, block, unblock, queue full, hang, restart) kept in RTC memory, survives a soft reset |
| Crash report | After reboot: which task hung, how long it ran, stack %, free heap, and the 20 events before the crash |
| Recovery | Hung task is quarantined 3 s, then restarted; after 3 hangs it is marked `FAILED` and stays off. Other tasks keep running |
| UART shell | `help`, `ps`, `log`, `crash`, `hang <task>`, `info`, `clear`, `reboot` |
| Fault injection | `hang <task>` puts a real infinite loop into a task so you can demo the recovery |

## Files

```
src/main.cpp        demo application tasks + setup()/loop()
src/rtos.h/.cpp     scheduler, states, sleep, queue, semaphore, watchdog check
src/blackbox.h/.cpp event ring buffer, crash record, reports (RTC memory)
src/shell.h/.cpp    UART command shell
src/platform.h/.cpp ESP32-specific bits (timer, reboot, stack, heap)
tools/host_test/    PC test with a simulated hang + reboot
docs/               viva notes and demo script
```

## Build and run

- **PlatformIO (VS Code):** open the folder, click Build, then Upload, then Monitor (115200 baud).
- **Wokwi:** after a PlatformIO build, start the Wokwi simulator (uses `wokwi.toml` and `diagram.json`).

Try this in the serial monitor:

```
> ps
> hang Sensor        (task goes into an infinite loop)
[WDT] task 'Sensor' ran 1100 ms (limit 1000) -> HUNG. Saving black box, rebooting...
=========== CRASH REPORT ===========
Task 'Sensor' (id 1) HUNG ...
> ps                 (Sensor shows QUARANTINE, then runs again)
```

## Tests

PC test (no board needed) covers normal run, queue/semaphore blocking, hang detection, reboot with preserved black box, quarantine, restart, `FAILED` after 3 hangs, and power-cycle reset:

```
g++ -std=c++17 -DHOST_TEST -Itools/host_test -Isrc tools/host_test/host_test.cpp src/main.cpp src/rtos.cpp src/blackbox.cpp src/shell.cpp -o host_test && ./host_test
```

## Limitations (honest)

- Cooperative: no preemption, no priorities, no memory protection.
- A cooperative kernel cannot abandon a task that never returns, so recovery is a **soft reset of the whole chip**. The OS identifies the guilty task, remembers it across the reset, keeps it out for 3 s, and the other tasks resume. It does not restart one task while the others run uninterrupted.
- Hang detection is a time limit per run (1 s in the demo). A task that legitimately needs longer must have a larger limit in `rtosAdd()`.
- Stack % is the high-water mark of the Arduino loop task (all tasks share one stack).
- A learning project, not a production RTOS.

## Status

Complete: scheduler, states, sleep, queue, semaphore, watchdog, black box, crash report, shell, fault injection, host tests. History of the earlier days is in `archive/`.
