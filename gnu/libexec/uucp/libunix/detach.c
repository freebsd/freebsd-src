/* detach.c
   Detach from the controlling terminal.

   Copyright (C) 1992, 1993 Ian Lance Taylor

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
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucp.h"

#include "uudefs.h"
#include "system.h"
#include "sysdep.h"

#include <errno.h>

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef TIOCNOTTY
#define HAVE_TIOCNOTTY 1
#else
#define HAVE_TIOCNOTTY 0 
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#if HAVE_BROKEN_SETSID
#undef HAVE_SETSID
#define HAVE_SETSID 0
#endif

/* Detach from the controlling terminal.  This is called by uucico if
   it is calling out to another system, so that it can receive SIGHUP
   signals from the port it calls out on.  It is also called by uucico
   just before it starts uuxqt, so that uuxqt is completely
   independent of the terminal.  */

void
usysdep_detach ()
{
  pid_t igrp;

  /* Make sure we are not a process group leader.  */
#if HAVE_BSD_PGRP
  igrp = getpgrp (0);
#else
  igrp = getpgrp ();
#endif

  if (igrp == getpid ())
    {
      boolean fignored;
      pid_t ipid;

      /* Ignore SIGHUP, since our process group leader is about to
	 die.  */
      usset_signal (SIGHUP, SIG_IGN, FALSE, &fignored);

      ipid = ixsfork ();
      if (ipid < 0)
	ulog (LOG_FATAL, "fork: %s", strerror (errno));

      if (ipid != 0)
	_exit (EXIT_SUCCESS);

      /* We'll always wind up as a child of process number 1, right?
	 Right?  We have to wait for our parent to die before
	 reenabling SIGHUP.  */
      while (getppid () != 1)
	sleep (1);

      ipid = getpid ();
      ulog_id (ipid);

      /* Restore SIGHUP catcher if it wasn't being ignored.  */
      if (! fignored)
	usset_signal (SIGHUP, ussignal, TRUE, (boolean *) NULL);

      DEBUG_MESSAGE2 (DEBUG_PORT, "Forked; old PID %ld, new pid %ld",
		      (long) igrp, (long) ipid);
    }

#if ! HAVE_SETSID && HAVE_TIOCNOTTY
  /* Lose the original controlling terminal as well as our process
     group.  If standard input has been reopened to /dev/null, this
     will do no harm.  If another port has been opened to become the
     controlling terminal, it should have been detached when it was
     closed.  */
  (void) ioctl (0, TIOCNOTTY, (char *) NULL);
#endif /* ! HAVE_SETSID && HAVE_TIOCNOTTY */

  /* Close stdin, stdout and stderr and reopen them on /dev/null, to
     make sure we have no connection at all to the terminal.  */
  (void) close (0);
  (void) close (1);
  (void) close (2);
  if (open ((char *) "/dev/null", O_RDONLY) != 0
      || open ((char *) "/dev/null", O_WRONLY) != 1
      || open ((char *) "/dev/null", O_WRONLY) != 2)
    ulog (LOG_FATAL, "open (/dev/null): %s", strerror (errno));

#if HAVE_SETSID

  /* Under POSIX the setsid call creates a new session for which we
     are the process group leader.  It also detaches us from our
     controlling terminal.  */
  if (setsid () < 0)
    ulog (LOG_ERROR, "setsid: %s", strerror (errno));

#else /* ! HAVE_SETSID */

#if ! HAVE_SETPGRP
 #error Cannot detach from controlling terminal
#endif

  /* If we don't have setsid, we must use setpgrp.  On an old System V
     system setpgrp will make us the leader of a new process group and
     detach the controlling terminal.  On an old BSD system the call
     setpgrp (0, 0) will set our process group to 0 so that we can
     acquire a new controlling terminal (TIOCNOTTY may or may not have
     already done that anyhow).  */
#if HAVE_BSD_PGRP
  if (setpgrp (0, 0) < 0)
#else
  if (setpgrp () < 0)
#endif
    {
      /* Some systems seem to give EPERM errors inappropriately.  */
      if (errno != EPERM)
	ulog (LOG_ERROR, "setpgrp: %s", strerror (errno));
    }

#endif /* ! HAVE_SETSID */

  /* At this point we have completely detached from our controlling
     terminal.  The next terminal device we open will probably become
     our controlling terminal.  */
}
