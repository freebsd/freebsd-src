/* remove.c
   Remove a file (Unix specific implementation).  */

#include "uucp.h"

#include "sysdep.h"

int
remove (z)
     const char *z;
{
  return unlink (z);
}
