/* sync.c
   Sync a file to disk, if FSYNC_ON_CLOSE is set.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

boolean
fsysdep_sync (e, zmsg)
     openfile_t e;
     const char *zmsg;
{
  int o;

#if USE_STDIO
  if (fflush (e) == EOF)
    {
      ulog (LOG_ERROR, "%s: fflush: %s", zmsg, strerror (errno));
      return FALSE;
    }
#endif

#if USE_STDIO
  o = fileno (e);
#else
  o = e;
#endif

#if FSYNC_ON_CLOSE
  if (fsync (o) < 0)
    {
      ulog (LOG_ERROR, "%s: fsync: %s", zmsg, strerror (errno));
      return FALSE;
    }
#endif

  return TRUE;
}
