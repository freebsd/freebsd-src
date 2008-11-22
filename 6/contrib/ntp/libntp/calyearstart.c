/*
 * calyearstart - determine the NTP time at midnight of January 1 in
 *		  the year of the given date.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

u_long
calyearstart(u_long ntp_time)
{
    struct calendar jt;

    caljulian(ntp_time,&jt);
    jt.yearday  = 1;
    jt.monthday = 1;
    jt.month    = 1;
    jt.hour = jt.minute = jt.second = 0;
    return caltontp(&jt);
}
