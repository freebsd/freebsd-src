/* spool.c
   See whether a filename is legal for the spool directory.  */

#include "uucp.h"

#include <ctype.h>

#include "uudefs.h"

/* See whether a file is a spool file.  Spool file names are specially
   crafted to hand around to other UUCP packages.  They always begin
   with 'C', 'D' or 'X', and the second character is always a period.
   The remaining characters may be any printable characters, since
   they may include a grade set by another system.  */

boolean
fspool_file (zfile)
     const char *zfile;
{
  const char *z;

  if (*zfile != 'C' && *zfile != 'D' && *zfile != 'X')
    return FALSE;
  if (zfile[1] != '.')
    return FALSE;
  for (z = zfile + 2; *z != '\0'; z++)
    if (*z == '/' || ! isprint (BUCHAR (*z)) || isspace (BUCHAR (*z)))
      return FALSE;
  return TRUE;
}
