#pragma once
// UART shell: help, ps, log, crash, hang, clear, info, reboot
void shellBegin();
void shellTask();   // register with rtosAdd(), period ~20 ms
