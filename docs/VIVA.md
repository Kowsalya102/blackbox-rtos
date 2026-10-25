# Viva notes (Tanglish)

## 1. Project oru line-la
"Oru task hang aanaalum, OS adhai kandupidichu, enna nadandhuchu nu black box-la save panni, system-ah recover panni, reboot-ku apram report tharum."


## 1b. Application: Industrial Multi-Sensor Monitoring and Fault Recovery System

Namma RTOS-ku oru real maadhiri velai kudutthirukkom: oru plant-oda **temperature, pressure, level** monitor panradhu.

- 3 sensor tasks (Temp, Pressure, Level) readings-ah **queue** vazhiyaa Analyzer-ku anuppum.
- Analyzer limit paakkum: temp > 80 C, pressure > 7 bar, level < 10 % or > 90 % = **alarm** (red LED + buzzer). Alarm black box-layum record aagum.
- Heartbeat **semaphore** kudukkum, Report task 3 second-ku oru status line print pannum.
- Push button = fault injection. Oru sensor task hang aana: watchdog detect, black box save, reboot, crash report, andha sensor 3 s quarantine, mattha sensors keep reporting, "Temp=STALE" nu report-la theriyum, apram thirumba restart.

**Honest-ah sollunga:** Sensors real illa, potentiometers (simulation). Hang purposely inject pannadhu. "Industrial" = use-case scenario, certified industrial product illa.

**Demo order (Wokwi):**
1. Temperature pot-ah thirumbi 80 C-ku mela: ALARM, red LED + buzzer.
2. Thirumba kuraingga: "[OK] ... back to normal".
3. Red button press: hang inject, "[WDT] ... HUNG ... rebooting".
4. Reboot-ku apram CRASH REPORT, `ps` (Temp QUARANTINE), `log`.
5. 3 s apram "task 'Temp' restarted".

**Q: Real sensor-la hang eppadi varum?** I2C/SPI sensor respond pannaama wait panna loop-la maatturadhu, DHT-maadhiri sensor-oda pin wait loop. Namma buttons / `hang` adhai simulate pannudhu.

**Q: Alarm-ku safety guarantee irukka?** Illa. Cooperative, deadline guarantee illa. Safety-critical-ku certified RTOS venum.

## 2. Architecture (board-la ezhudhi vilakkalaam)

```
loop() -> rtosStep()  -> ovvoru task: state paathu run pannum
Timer (100 ms) -> rtosWdtCheck() -> task romba neram run aana "HUNG"
HUNG -> blackbox-la save -> soft reboot -> boot-la crash report -> task quarantine -> restart
```

## 3. Ovvoru file enna pannudhu
- `rtos.cpp`: scheduler, states, sleep, queue, semaphore, watchdog check.
- `blackbox.cpp`: last 20 events ring buffer, crash record. RTC memory-la irukku, adhanaala soft reset-la azhiyaadhu.
- `shell.cpp`: UART-la `ps`, `log`, `crash`, `hang` commands.
- `platform.cpp`: ESP32-specific (timer, reboot, stack, heap). Mattha code hardware-ku sambandham illa, adhanaala PC-la test panna mudiyudhu.
- `main.cpp`: demo tasks (LED, Sensor, Process, Heartbeat, Monitor, Shell).

## 4. Mukkiya concepts

**Cooperative scheduling:** Task run aagi return aana dhaan adutha task-ku control pogum. Context switch assembly vendaam, adhanaala simple. Kuraipaadu: task hang aana scheduler-ku control varaadhu.

**Task states:** READY (run aaga ready), RUNNING, SLEEPING (neram kaaththirukku), BLOCKED (queue/semaphore-kaaga kaaththirukku), QUARANTINE (hang aana apram thandanai), FAILED (3 thadavai hang aanadhu, off).

**Queue:** Circular buffer, size 4. Empty-na `qReceive` task-ah BLOCKED aakkum. Data vandha scheduler READY aakkum.

**Semaphore:** Counter. `semGive` +1, `semTake` -1, 0-na task BLOCKED.

**Watchdog (mukkiyam):** Scheduler task-ah start pannum bodhu `run_start` note pannum. 100 ms timer (esp_timer, FreeRTOS task-la run aagum, adhanaala hang aana loop()-ah preempt pannum) `now - run_start > timeout` paakkum. Adhu kooda ESP32-oda hardware preemption use aagudhu, namma OS-oda cooperative scheduler-ku problem illa.

**Black box:** `RTC_NOINIT_ATTR` memory soft reset-la clear aagaadhu. Hang detect aana udane last 20 events-ah "snapshot" copy pannidum, apram thaan reboot. Illana reboot-ku apram pudhu events pazhaya evidence-ah azhichidum.

**Recovery:** Reboot-ku apram `bb.hung[i]` paathu andha task-ah 3 s QUARANTINE-la vaikkum. Mattha tasks udane run aagum. 3 thadavai hang aana FAILED.

## 5. Mudhalaye sollidunga (limitation)
"Cooperative OS-la never-return aagura task-ah vittu vera idam pogamudiyaadhu. Adhanaala recovery = soft reset. Aana OS guilty task-ah remember pannitu, adhai thaniya vaichu, mattha tasks-ah continue pannum. Preemptive context switch add pannina, reset illaama andha task-ah mattum kill pannalaam. Adhu future work."

## 6. Yaar kekkalaam: Q and A

**Q: Edhukku RTOS? Arduino loop() podhadhaa?**
A: loop()-la ovvoru task-oda timing, blocking, queue, hang detection ellam neengale ezhudhanum. RTOS idhai standard-ah tharum.

**Q: Cooperative vs preemptive?**
A: Cooperative: task thaane yield pannum, simple, context-switch bugs illa, aana oru task hang aana ellam hang. Preemptive: OS timer interrupt-la force-ah switch pannum, safe-aanaal complex (stack per task, assembly).

**Q: Hang eppadi detect pannura? Scheduler hang aagiduche?**
A: Hardware timer (esp_timer) vera FreeRTOS task-la run aagum, adhu loop task-ah preempt pannum. Adhu thaan `run_start`-ah check pannum.

**Q: Task restart pannuradhu eppadi?**
A: Soft reset. RTC memory-la `hung[]` flag irukkum. Boot-la adhai paathu andha task-ah quarantine pannum.

**Q: Reboot-la data poidaadhaa?**
A: RTC_NOINIT memory soft reset-la irukkum. Power off pannina poidum (cold boot-la OS thaane clear pannum).

**Q: Stack 90% eppadi theriyum?**
A: `uxTaskGetStackHighWaterMark` FreeRTOS function. Idhu "evlo minimum free irundhudhu" nu solludhu. Stack usage = total - free. Ellaa tasks-um oru stack share pannum (cooperative), adhanaala loop task-oda stack.

**Q: Idhu real-time-aa?**
A: Soft real-time. Hard guarantees illa, task-ku deadline miss aanaalum OS adhai nirutha maattaadhu (watchdog limit-ku mela dhaan).

**Q: Test eppadi panna?**
A: PC-la fake time + fake serial vachu `tools/host_test`. Hang inject panni, reboot simulate panni, quarantine / FAILED check panrom. Board-la `hang <task>` shell command.

**Q: Next level?**
A: Preemptive context switch (Cortex-M PendSV), priorities, mutex, stack per task, flash storage-la crash log.

## 7. Live demo order
1. `ps` : ellam SLEEPING/BLOCKED.
2. `hang Sensor` : "[WDT] task Sensor HUNG... rebooting".
3. Reboot-ku apram crash report: task, stack %, last 20 events.
4. `ps` : Sensor QUARANTINE. LED/Heartbeat odurathu paarunga.
5. 3 s apram "[OS] task 'Sensor' restarted".
6. `log`, `crash` show pannunga.
7. 3 thadavai `hang Sensor` : FAILED.
8. `clear` or power cycle.
