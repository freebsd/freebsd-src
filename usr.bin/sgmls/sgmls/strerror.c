/* strerror.c -
   ANSI C strerror() function.

      Written by James Clark (jjc@jclark.com).
*/

#include "config.h"

#ifdef STRERROR_MISSING
#include <stdio.h>

char *strerror(n)
int n;
{
     extern int sys_nerr;
     extern char *sys_errlist[];
     static char buf[sizeof("Error ") + 1 + 3*sizeof(int)];

     if (n >= 0 && n < sys_nerr && sys_errlist[n] != 0)
	  return sys_errlist[n];
     else {
	  sprintf(buf, "Error %d", n);
	  return buf;
     }
}

#endif /* STRERROR_MISSING */
/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
