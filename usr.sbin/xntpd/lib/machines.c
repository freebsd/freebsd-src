/*
 * provide special support for peculiar architectures
 *
 * Real bummers unite !
 */

#ifdef SYS_PTX
#include <sys/types.h>
#include <sys/procstats.h>
int settimeofday(tvp)
	struct timeval *tvp;
{
	return stime(&tvp->tv_sec);	/* lie as bad as SysVR4 */
}

int gettimeofday(tvp)
	struct timeval *tvp;
{
	/*
	 * hi, this is Sequents sneak path to get to a clock
	 * this is also the most logical syscall for such a function
	 */
	return get_process_stats(tvp, PS_SELF, (struct procstats *) 0,
				 (struct procstats *) 0);
}
#endif

#ifdef SYS_HPUX
/* hpux.c,v 3.1 1993/07/06 01:08:23 jbj Exp
 * hpux.c -- compatibility routines for HP-UX.
 * XXX many of these are not needed anymore.
 */
#include "ntp_machine.h"

#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>

#include "ntp_stdlib.h"

#if (SYS_HPUX < 8)
char 
*index(s, c)
register char *s;
register int c;
{
    return strchr (s, c);
}


char 
*rindex(s, c)
register char *s;
register int c;
{
    return strrchr (s, c);
}


int 
bcmp(a, b, count)
register char *a, *b;
register int count;
{
    return memcmp (a, b, count);
}


void 
bcopy(from, to, count)
register char *from;
register char *to;
register int count;
{
   if ((to == from) || (count <= 0))
       return;

   if ((to > from) && (to <= (from + count))) {
       to += count;
       from += count;

       do {
	   *--to = *--from;
       }   while (--count);
   }
   else {
       do {
	   *to++ = *from++;
       }   while (--count);
   }
}


void 
bzero(area, count)
register char *area;
register int count;
{
    memset(area, 0, count);
}
#endif


getdtablesize()
{
    return(sysconf(_SC_OPEN_MAX));
}


int 
setlinebuf(a_stream)
    FILE *a_stream;
{
    return setvbuf(a_stream, (char *) NULL, _IOLBF, 0);
}

#endif
