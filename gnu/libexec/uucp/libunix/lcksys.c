/* lcksys.c
   Lock and unlock a remote system.  */

#include "uucp.h"

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "system.h"

/* Do the actual lock or unlock.  */

static boolean fslock_sys P((boolean, const char *));

static boolean
fslock_sys (flock, zname)
     boolean flock;
     const char *zname;
{
  size_t clen;
  char *z;
  boolean fret;

  clen = strlen (zname);

#if ! HAVE_LONG_FILE_NAMES
  if (clen > 8)
    clen = 8;
#endif

  z = zbufalc (sizeof "LCK.." + clen);
  memcpy (z, "LCK..", sizeof "LCK.." - 1);
  memcpy (z + sizeof "LCK.." - 1, zname, clen);
  z[sizeof "LCK.." - 1 + clen] = '\0';

  if (flock)
    fret = fsdo_lock (z, FALSE, (boolean *) NULL);
  else
    fret = fsdo_unlock (z, FALSE);

  ubuffree (z);

  return fret;
}

/* Lock a remote system.  */

boolean
fsysdep_lock_system (qsys)
     const struct uuconf_system *qsys;
{
  return fslock_sys (TRUE, qsys->uuconf_zname);
}

/* Unlock a remote system.  */

boolean
fsysdep_unlock_system (qsys)
     const struct uuconf_system *qsys;
{
  return fslock_sys (FALSE, qsys->uuconf_zname);
}
