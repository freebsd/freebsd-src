/* priv.c
   See if a user is privileged.  */

#include "uucp.h"

#include "sysdep.h"
#include "system.h"

/* See whether the user is privileged (for example, only privileged
   users are permitted to kill arbitrary jobs with uustat).  This is
   true only for root and uucp.  We check for uucp by seeing if the
   real user ID and the effective user ID are the same; this works
   because we should be suid to uucp, so our effective user ID will
   always be uucp while our real user ID will be whoever ran the
   program.  */

boolean
fsysdep_privileged ()
{
  uid_t iuid;

  iuid = getuid ();
  return iuid == 0 || iuid == geteuid ();
}
