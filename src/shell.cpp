#include "shell.h"
#include "rtos.h"
#include "blackbox.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <strings.h>

static char lineBuf[48];
static uint8_t lineLen = 0;

static int findTask(const char* a) {
  if (!a) return -1;
  if (a[0] >= '0' && a[0] <= '9') {
    int id = atoi(a);
    return id < taskCount ? id : -1;
  }
  for (int i = 0; i < taskCount; i++)
    if (!strcasecmp(a, tasks[i].name)) return i;
  return -1;
}

static void cmdHelp() {
  Serial.println("Commands:");
  Serial.println("  ps            list tasks, states, run counts");
  Serial.println("  log           black-box: last 20 events");
  Serial.println("  crash         last crash report");
  Serial.println("  hang <task>   inject infinite loop (name or id)");
  Serial.println("  info          uptime, boots, heap, stack");
  Serial.println("  clear         erase black box");
  Serial.println("  reboot        soft reset");
}

static void cmdPs() {
  Serial.println("ID NAME       STATE       RUNS  HANGS PERIOD WAIT");
  for (int i = 0; i < taskCount; i++) {
    const Task& t = tasks[i];
    Serial.printf("%-2d %-10s %-11s %5lu %5u %6lu %s\r\n", i, t.name, stateName(t.state),
                  (unsigned long)t.runs, (unsigned)bb.restarts[i], (unsigned long)t.period,
                  (t.state == BLOCKED && t.waitName) ? t.waitName : "");
  }
}

static void cmdInfo() {
  Serial.printf("uptime %lu ms | boot #%u | free heap %lu | stack %u%% used\r\n",
                (unsigned long)millis(), (unsigned)bb.boots,
                (unsigned long)platformFreeHeap(), (unsigned)platformStackPct());
}

static void cmdHang(const char* arg) {
  int id = findTask(arg);
  if (id < 0) { Serial.println("usage: hang <task name or id>   (see 'ps')"); return; }
  tasks[id].inject = true;
  Serial.printf("OK: '%s' will hang on its next run. Watchdog limit %lu ms.\r\n",
                tasks[id].name, (unsigned long)tasks[id].timeout_ms);
}

static void runLine(char* line) {
  char* cmd = strtok(line, " ");
  if (!cmd) return;
  char* arg = strtok(nullptr, " ");

  if      (!strcasecmp(cmd, "help"))   cmdHelp();
  else if (!strcasecmp(cmd, "ps"))     cmdPs();
  else if (!strcasecmp(cmd, "log"))    bbPrintLog();
  else if (!strcasecmp(cmd, "crash"))  bbPrintCrash();
  else if (!strcasecmp(cmd, "hang"))   cmdHang(arg);
  else if (!strcasecmp(cmd, "info"))   cmdInfo();
  else if (!strcasecmp(cmd, "clear"))  { bbClear(); Serial.println("black box cleared"); }
  else if (!strcasecmp(cmd, "reboot")) { Serial.println("rebooting..."); Serial.flush(); platformReboot(); }
  else Serial.printf("unknown command '%s' (try 'help')\r\n", cmd);
}

void shellBegin() {
  Serial.println("Type 'help' for commands.");
  Serial.print("> ");
}

void shellTask() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (lineLen > 0) {
        lineBuf[lineLen] = 0;
        lineLen = 0;
        runLine(lineBuf);
        Serial.print("> ");
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}
