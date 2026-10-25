# Industrial Multi-Sensor Monitoring and Fault Recovery System

Built on **BlackBox RTOS**: a small cooperative RTOS for ESP32 (Arduino framework, C++) that detects a hung task, saves a black-box record of what happened, recovers, and reports the cause after the reboot.

## Problem

In an industrial monitor, one stuck sensor task can freeze the whole system, and after a reboot the evidence of what happened is lost.

## The application

Three analog sensors are read by separate RTOS tasks, analysed, and drive alarms. In the simulation potentiometers stand in for real transmitters.

| Sensor | Pin | Range | Alarm |
|---|---|---|---|
| Temperature | GPIO34 | 0 to 150.0 C | above 80.0 C |
| Pressure | GPIO35 | 0 to 10.0 bar | above 7.0 bar |
| Level | GPIO32 | 0 to 100 % | below 10 % or above 90 % |

Outputs: green status LED (GPIO2, RTOS alive), red alarm LED (GPIO26), buzzer (GPIO27). A push button (GPIO4) injects a hang into the next sensor task, one press per sensor in turn.

Task flow: `Temp`, `Pressure`, `Level` send readings through a **queue** to `Analyzer`, which checks limits and logs alarms. `Alarm` blinks the red LED and sounds the buzzer. `Heartbeat` gives a **semaphore** every 3 s, `Report` takes it and prints a status line. `Button` and `Shell` handle fault injection and commands.

## RTOS features

| Feature | How |
|---|---|
| Cooperative scheduler | Round-robin task table, tasks run to completion (return = yield) |
| Task states | `READY`, `RUNNING`, `SLEEPING`, `BLOCKED`, `QUARANTINE`, `FAILED` |
| Sleep, queue, semaphore | `sleepMs()`, ring-buffer queue, counting semaphore; receivers block when empty |
| Per-task watchdog | 100 ms hardware-timer callback; a task running longer than its limit is HUNG |
| Black box | Last 20 events (run, block, unblock, alarm, queue full, hang, restart) in RTC memory, survives a soft reset |
| Crash report | After reboot: which task hung, how long, stack %, free heap, and the 20 events before the crash |
| Recovery | The hung task is quarantined 3 s, then restarted. After 3 hangs it is `FAILED` and stays off. The other sensors keep reporting |
| UART shell | `help`, `ps`, `log`, `crash`, `hang <task>`, `info`, `clear`, `reboot` |

## Files

```
src/main.cpp        the monitoring application (tasks, thresholds, pins)
src/rtos.h/.cpp     scheduler, states, sleep, queue, semaphore, watchdog check
src/blackbox.h/.cpp event ring buffer, crash record, reports (RTC memory)
src/shell.h/.cpp    UART command shell
src/platform.h/.cpp ESP32-specific bits (timer, reboot, stack, heap, buzzer)
diagram.json        Wokwi circuit (3 pots, 2 LEDs, buzzer, button)
tools/host_test/    PC test with fake sensors, button, hang and reboot
docs/               viva notes and demo script
archive/            day-by-day history of the RTOS
```

## Build and run

- **PlatformIO (VS Code):** open the folder, Build, then Upload and Monitor (115200 baud) on a real ESP32.
- **Wokwi (no board):** after a PlatformIO build, `Ctrl+Shift+P` then `Wokwi: Start Simulator`. For typing commands, connect the PlatformIO monitor to `rfc2217://localhost:4000`.

## Demo script

1. Turn the Temperature pot above 80 C: `[ALARM] Temperature = ...`, red LED and buzzer blink.
2. Turn it back: `[OK] Temperature back to normal`.
3. Press the red button: `[BUTTON] fault injected into Temperature sensor task`.
4. `[WDT] task 'Temp' ran 1100 ms (limit 1000) -> HUNG ... rebooting`.
5. After reboot the CRASH REPORT shows the task, stack and the last 20 events. `ps` shows `Temp QUARANTINE` while Pressure and Level keep running, and the report line shows `Temp=STALE`.
6. 3 s later `[OS] task 'Temp' restarted`.
7. Repeat with the shell (`hang Temp`) until `Temp` has hung 3 times: it becomes `FAILED` and stays off until a power cycle, while the other sensors keep reporting.

## Tests

PC test (no board needed), 47 checks: normal readings, alarms and outputs, queue and semaphore blocking, button-injected hang, reboot with preserved black box, quarantine, restart, `FAILED` after 3 hangs, power-cycle reset.

```
g++ -std=c++17 -DHOST_TEST -Itools/host_test -Isrc tools/host_test/host_test.cpp src/main.cpp src/rtos.cpp src/blackbox.cpp src/shell.cpp -o host_test && ./host_test
```

## Limitations (honest)

- The sensors are simulated (potentiometers on an ADC). The hang is injected on purpose; it is not a real sensor fault.
- Cooperative: no preemption, no priorities, no memory protection, no real-time deadline guarantee.
- A cooperative kernel cannot abandon a task that never returns, so recovery is a **soft reset of the whole chip**. The OS identifies the guilty task, remembers it across the reset, keeps it out for 3 s, and the other tasks resume. It does not restart one task while the others run uninterrupted.
- Hang detection is a time limit per run (1 s). A task that legitimately needs longer needs a larger limit in `rtosAdd()`.
- Stack % is the high-water mark of the Arduino loop task (all tasks share one stack).
- A learning project, not a production RTOS.
