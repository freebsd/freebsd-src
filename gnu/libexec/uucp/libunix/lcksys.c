/* lcksys.c
   Lock and unlock a remote system.  */

#include "uucp.h"

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

/* Lock a remote system.  */

boolean
fsysdep_lock_system (qsys)
     const struct uuconf_system *qsys;
{
  char *z;
  boolean fret;

  z = zbufalc (strlen (qsys->uuconf_zname) + sizeof "LCK..");
  sprintf (z, "LCK..%.8s", qsys->uuconf_zname);
  fret = fsdo_lock (z, FALSE, (boolean *) NULL);
  ubuffree (z);
  return fret;
}

/* Unlock a remote system.  */

boolean
fsysdep_unlock_system (qsys)
     const struct uuconf_system *qsys;
{
  char *z;
  boolean fret;

  z = zbufalc (strlen (qsys->uuconf_zname) + sizeof "LCK..");
  sprintf (z, "LCK..%.8s", qsys->uuconf_zname);
  fret = fsdo_unlock (z, FALSE);
  ubuffree (z);
  return fret;
}
