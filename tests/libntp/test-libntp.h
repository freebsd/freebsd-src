#ifndef TEST_LIBNTP_H
#define TEST_LIBNTP_H

#include "config.h"

#include "ntp_stdlib.h"
#include "ntp_calendar.h"

time_t timefunc(time_t *ptr);
void settime(int y, int m, int d, int H, int M, int S);
extern time_t nowtime;
#endif
