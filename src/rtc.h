#ifndef RTC_H
#define RTC_H
#include "types.h"

// Initialize RTC to 2Hz:
void init_rtc();

void rtc_handler();

// Timeout timer:
extern uint32_t ticks_since_kb;

#endif
