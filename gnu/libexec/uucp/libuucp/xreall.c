/* xreall.c
   Realloc a block of memory without fail.  Supposedly some versions of
   realloc can't handle a NULL first argument, so we check for that
   here.  */

#include "uucp.h"

#include "uudefs.h"

pointer
xrealloc (p, c)
     pointer p;
     size_t c;
{
  pointer pret;

  if (p == NULL)
    return xmalloc (c);
  pret = realloc (p, c);
  if (pret == NULL && c != 0)
    ulog (LOG_FATAL, "Out of memory");
  return pret;
}
