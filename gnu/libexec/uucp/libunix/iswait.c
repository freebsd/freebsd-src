/* iswait.c
   Wait for a process to finish.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"

#include <errno.h>

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/* We use a typedef wait_status for wait (waitpid, wait4) to put
   results into.  We define the POSIX examination functions we need if
   they are not already defined (if they aren't defined, I assume that
   we have a standard wait status).  */

#if HAVE_UNION_WAIT
typedef union wait wait_status;
#ifndef WIFEXITED
#define WIFEXITED(u) ((u).w_termsig == 0)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(u) ((u).w_retcode)
#endif
#ifndef WTERMSIG
#define WTERMSIG(u) ((u).w_termsig)
#endif
#else /* ! HAVE_UNION_WAIT */
typedef int wait_status;
#ifndef WIFEXITED
#define WIFEXITED(i) (((i) & 0xff) == 0)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(i) (((i) >> 8) & 0xff)
#endif
#ifndef WTERMSIG
#define WTERMSIG(i) ((i) & 0x7f)
#endif
#endif /* ! HAVE_UNION_WAIT */

/* Wait for a particular process to finish.  The ipid argument should
   be pid_t, but then we couldn't have a prototype.  If the zreport
   argument is not NULL, then a wait error will be logged, and if the
   exit status is non-zero it will be logged with zreport as the
   header of the log message.  If the zreport argument is NULL, no
   errors will be logged.  This function returns the exit status if
   the process exited normally, or -1 on error or if the process was
   killed by a signal (I don't just always return the exit status
   because then the calling code would have to prepared to handle
   union wait status vs. int status, and none of the callers care
   which signal killed the program anyhow).

   This functions keeps waiting until the process finished, even if it
   is interrupted by a signal.  I think this is right for all uses.
   The controversial one would be when called from uuxqt to wait for a
   requested process.  Hitting uuxqt with SIGKILL will approximate the
   actions taken if we return from here with an error anyhow.  If we
   do get a signal, we call ulog with a NULL argument to get it in the
   log file at about the right time.  */

int
ixswait (ipid, zreport)
     unsigned long ipid;
     const char *zreport;
{
  wait_status istat;

#if HAVE_WAITPID
  while (waitpid ((pid_t) ipid, (pointer) &istat, 0) < 0)
    {
      if (errno != EINTR)
	{
	  if (zreport != NULL)
	    ulog (LOG_ERROR, "waitpid: %s", strerror (errno));
	  return -1;
	}
      ulog (LOG_ERROR, (const char *) NULL);
    }
#else /* ! HAVE_WAITPID */
#if HAVE_WAIT4
  while (wait4 ((pid_t) ipid, (pointer) &istat, 0,
		(struct rusage *) NULL) < 0)
    {
      if (errno != EINTR)
	{
	  if (zreport != NULL)
	    ulog (LOG_ERROR, "wait4: %s", strerror (errno));
	  return -1;
	}
      ulog (LOG_ERROR, (const char *) NULL);
    }
#else /* ! HAVE_WAIT4 */
  pid_t igot;

  /* We could theoretically get the wrong child here if we're in some
     kind of weird pipeline, so we don't give any error messages for
     it.  */
  while ((igot = wait ((pointer) &istat)) != (pid_t) ipid)
    {
      if (igot < 0)
	{
	  if (errno != EINTR)
	    {
	      if (zreport != NULL)
		ulog (LOG_ERROR, "wait: %s", strerror (errno));
	      return -1;
	    }
	  ulog (LOG_ERROR, (const char *) NULL);
	}
    }
#endif /* ! HAVE_WAIT4 */
#endif /* ! HAVE_WAITPID */  

  DEBUG_MESSAGE2 (DEBUG_EXECUTE, "%s %d",
		  WIFEXITED (istat) ? "Exit status" : "Signal",
		  WIFEXITED (istat) ? WEXITSTATUS (istat) : WTERMSIG (istat));

  if (WIFEXITED (istat) && WEXITSTATUS (istat) == 0)
    return 0;

  if (zreport != NULL)
    {
      if (! WIFEXITED (istat))
	ulog (LOG_ERROR, "%s: Got signal %d", zreport, WTERMSIG (istat));
      else
	ulog (LOG_ERROR, "%s: Exit status %d", zreport,
	      WEXITSTATUS (istat));
    }

  if (WIFEXITED (istat))
    return WEXITSTATUS (istat);
  else
    return -1;
}
