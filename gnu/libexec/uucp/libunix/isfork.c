/* isfork.c 
   Retry fork several times before giving up.  */

#include "uucp.h"

#include "sysdep.h"

#include <errno.h>

pid_t
ixsfork ()
{
  int i;
  pid_t iret;

  for (i = 0; i < 10; i++)
    {
      iret = fork ();
      if (iret >= 0 || errno != EAGAIN)
	return iret;
      sleep (5);
    }

  return iret;
}
