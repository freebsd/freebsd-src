/*
 * mjs copyright
 */

#include <stand.h>
#include <btxv86.h>

/*
 * Return the time in seconds since the beginning of the day.
 *
 * If we pass midnight, don't wrap back to 0.
 *
 * XXX uses undocumented BCD support from libstand.
 */

time_t
time(time_t *t)
{
    static time_t	lasttime, now;
    int			hr, min, sec;
    
    v86.ctl = 0;
    v86.addr = 0x1a;		/* int 0x1a, function 2 */
    v86.eax = 0x0200;
    v86int();

    hr = bcd2bin((v86.ecx & 0xff00) >> 8);	/* hour in %ch */
    min = bcd2bin(v86.ecx & 0xff);		/* minute in %cl */
    sec = bcd2bin((v86.edx & 0xff00) >> 8);	/* second in %dh */
    
    now = hr * 3600 + min * 60 + sec;
    if (now < lasttime)
	now += 24 * 3600;
    lasttime = now;
    
    if (t != NULL)
	*t = now;
    return(now);
}

/*
 * Use the BIOS Wait function to pause for (period) microseconds.
 *
 * Resolution of this function is variable, but typically around
 * 1ms.
 */
void
delay(int period)
{
    v86.ctl = 0;
    v86.addr = 0x15;		/* int 0x1a, function 0x86 */
    v86.eax = 0x8600;
    v86.ecx = period >> 16;
    v86.edx = period & 0xffff;
    v86int();
}
