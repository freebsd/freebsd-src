/* unknwn.c
   Check remote.unknown shell script.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

/* Run the remote.unknown shell script.  If it succeeds, we return
   FALSE because that means that the system is not permitted to log
   in.  If the execution fails, we return TRUE.  */

boolean
fsysdep_unknown_caller (zscript, zsystem)
     const char *zscript;
     const char *zsystem;
{
  const char *azargs[3];
  int aidescs[3];
  pid_t ipid;

  azargs[0] = zscript;
  azargs[1] = zsystem;
  azargs[2] = NULL;

  aidescs[0] = SPAWN_NULL;
  aidescs[1] = SPAWN_NULL;
  aidescs[2] = SPAWN_NULL;

  ipid = ixsspawn (azargs, aidescs, TRUE, TRUE, (const char *) NULL, FALSE,
		   TRUE, (const char *) NULL, (const char *) NULL,
		   (const char *) NULL);
  if (ipid < 0)
    {
      ulog (LOG_ERROR, "ixsspawn: %s", strerror (errno));
      return FALSE;
    }

  return ixswait ((unsigned long) ipid, (const char *) NULL) != 0;
}
