/* ultspl.c
   See whether there is an Ultrix spool directory for a system.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

boolean
fsultrix_has_spool (zsystem)
     const char *zsystem;
{
  char *z;
  boolean fret;

  z = zsysdep_in_dir ("sys", zsystem);
  fret = fsysdep_directory (z);
  ubuffree (z);
  return fret;
}
