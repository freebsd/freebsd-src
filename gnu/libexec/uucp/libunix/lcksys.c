/* lcksys.c
   Lock and unlock a remote system.  */

#include "uucp.h"

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

/* Get the name of a system lock file.  */

static char *zssys_lock_name P((const struct uuconf_system *qsys, char *z));

#define LOCKNAMELEN (sizeof "LCK..12345678")

static char *
zssys_lock_name (qsys, z)
     const struct uuconf_system *qsys;
     char *z;
{
  strcpy (z, "LCK..");
  strncpy (z + sizeof "LCK.." - 1, qsys->uuconf_zname, 8);
  z[sizeof "LCK.." - 1 + 8] = '\0';
  return z;
}

/* Lock a remote system.  */

boolean
fsysdep_lock_system (qsys)
     const struct uuconf_system *qsys;
{
  char ab[LOCKNAMELEN];

  return fsdo_lock (zssys_lock_name (qsys, ab), FALSE, (boolean *) NULL);
}

/* Unlock a remote system.  */

boolean
fsysdep_unlock_system (qsys)
     const struct uuconf_system *qsys;
{
  char ab[LOCKNAMELEN];

  return fsdo_unlock (zssys_lock_name (qsys, ab), FALSE);
}
