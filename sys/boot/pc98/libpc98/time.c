/*
 * mjs copyright
 *
 * $FreeBSD$
 */

#include <stand.h>
#include <btxv86.h>
#ifdef PC98
#include <machine/cpufunc.h>
#endif

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
    
#ifdef PC98
    unsigned char	bios_time[6];
#endif

    v86.ctl = 0;
#ifdef PC98
    v86.addr = 0x1c;            /* int 0x1c, function 0 */
    v86.eax = 0x0000;
    v86.es  = VTOPSEG(bios_time);
    v86.ebx = VTOPOFF(bios_time);
#else
    v86.addr = 0x1a;		/* int 0x1a, function 2 */
    v86.eax = 0x0200;
#endif
    v86int();

#ifdef PC98
    hr = bcd2bin(bios_time[3]);
    min = bcd2bin(bios_time[4]);
    sec = bcd2bin(bios_time[5]);
#else
    hr = bcd2bin((v86.ecx & 0xff00) >> 8);	/* hour in %ch */
    min = bcd2bin(v86.ecx & 0xff);		/* minute in %cl */
    sec = bcd2bin((v86.edx & 0xff00) >> 8);	/* second in %dh */
#endif
    
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
#ifdef PC98
    int i;
    period = (period + 500) / 1000;
    for( ; period != 0 ; period--)
	for(i=800;i != 0; i--)
	    outb(0x5f,0);       /* wait 600ns */
#else
    v86.ctl = 0;
    v86.addr = 0x15;		/* int 0x15, function 0x86 */
    v86.eax = 0x8600;
    v86.ecx = period >> 16;
    v86.edx = period & 0xffff;
    v86int();
#endif
}
