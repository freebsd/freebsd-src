/* xmalloc.c
   Allocate a block of memory without fail.  */

#include "uucp.h"

#include "uudefs.h"

pointer
xmalloc (c)
     size_t c;
{
  pointer pret;

  pret = malloc (c);
  if (pret == NULL && c != 0)
    ulog (LOG_FATAL, "Out of memory");
  return pret;
}
