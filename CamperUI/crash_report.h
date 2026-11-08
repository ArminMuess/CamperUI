#pragma once
#include <stdint.h>
// Keeps the last crash (reason, PC, backtrace) across the reboot in RTC memory, so
// the next start can log it. Decode addresses with the matching CamperUI.ino.elf.
void crash_report_init();             // install the panic hook, log a stored crash once
const char *crash_report_summary();   // "" if the last start was not a crash
