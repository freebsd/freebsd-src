/* strdup.c
   Duplicate a string into memory.  */

#include "uucp.h"

char *
strdup (z)
     const char *z;
{
  size_t csize;
  char *zret;

  csize = strlen (z) + 1;
  zret = malloc (csize);
  if (zret != NULL)
    memcpy (zret, z, csize);
  return zret;
}
