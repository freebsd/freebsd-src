/* strerr.c
   Return a string for a Unix errno value.  */

#include "uucp.h"

#include <errno.h>

#ifndef sys_nerr
extern int sys_nerr;
#endif
#ifndef sys_errlist
extern char *sys_errlist[];
#endif

#undef strerror

char *
strerror (ierr)
     int ierr;
{
  if (ierr >= 0 && ierr < sys_nerr)
    return sys_errlist[ierr];
  return (char *) "unknown error";
}
