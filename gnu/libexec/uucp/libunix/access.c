/* access.c
   Check access to files by the user and by the daemon.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

/* See if the user has access to a file, to prevent the setuid uucp
   and uux programs handing out unauthorized access.  */

boolean
fsysdep_access (zfile)
     const char *zfile;
{
  if (access (zfile, R_OK) == 0)
    return TRUE;
  ulog (LOG_ERROR, "%s: %s", zfile, strerror (errno));
  return FALSE;
}

/* See if the daemon has access to a file.  This is called if a file
   is not being transferred to the spool directory, since if the
   daemon does not have access the later transfer will fail.  We
   assume that the daemon will have the same euid (or egid) as the one
   we are running under.  If our uid (gid) and euid (egid) are the
   same, we assume that we have access.  Note that is not important
   for security, since the check will be (implicitly) done again when
   the daemon tries to transfer the file.  This routine should work
   whether the UUCP programs are installed setuid or setgid.  */

boolean
fsysdep_daemon_access (zfile)
     const char *zfile;
{
  struct stat s;
  uid_t ieuid, iuid, iegid, igid;
  boolean fok;

  ieuid = geteuid ();
  if (ieuid == 0)
    return TRUE;
  iuid = getuid ();
  iegid = getegid ();
  igid = getgid ();

  /* If our effective uid and gid are the same as our real uid and
     gid, we assume the daemon will have access to the file.  */
  if (ieuid == iuid && iegid == igid)
    return TRUE;

 if (stat ((char *) zfile, &s) != 0)
     {
      ulog (LOG_ERROR, "stat (%s): %s", zfile, strerror (errno));
      return FALSE;
    }

  /* If our euid is not our uid, but it is the file's uid, see if the
     owner has read access.  Otherwise, if our egid is not our gid,
     but it is the file's gid, see if the group has read access.
     Otherwise, see if the world has read access.  We know from the
     above check that at least one of our euid and egid are different,
     so that is the only one we want to check.  This check could fail
     if the UUCP programs were both setuid and setgid, but why would
     they be?  */
  if (ieuid != iuid && ieuid == s.st_uid)
    fok = (s.st_mode & S_IRUSR) != 0;
  else if (iegid != igid && iegid == s.st_gid)
    fok = (s.st_mode & S_IRGRP) != 0;
  else
    fok = (s.st_mode & S_IROTH) != 0;

  if (! fok)
    {
      ulog (LOG_ERROR, "%s: cannot be read by daemon", zfile);
      return FALSE;
    }

  return TRUE;
}
