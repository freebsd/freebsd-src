/* detach.c
   Detach from the controlling terminal.

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

/* Detach from the controlling terminal.  This is called by uucico if
   it is calling out to another system, so that it can receive SIGHUP
   signals from the port it calls out on.  It is also called by uucico
   just before it starts uuxqt, so that uuxqt is completely
   independent of the terminal.  */

void
usysdep_detach ()
{
#if ! HAVE_BSD_PGRP || ! HAVE_TIOCNOTTY

  pid_t igrp;

  /* First make sure we are not a process group leader.  If we have
     TIOCNOTTY, this doesn't matter, since TIOCNOTTY sets our process
     group to 0 anyhow.  */

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

      ulog_id (getpid ());

      /* Restore SIGHUP catcher if it wasn't being ignored.  */
      if (! fignored)
	usset_signal (SIGHUP, ussignal, TRUE, (boolean *) NULL);
    }

#endif /* ! HAVE_BSD_PGRP || ! HAVE_TIOCNOTTY */

#if HAVE_TIOCNOTTY
  /* Lose the original controlling terminal.  If standard input has
     been reopened to /dev/null, this will do no harm.  If another
     port has been opened to become the controlling terminal, it
     should have been detached when it was closed.  */
  (void) ioctl (0, TIOCNOTTY, (char *) NULL);
#endif

  /* Close stdin, stdout and stderr and reopen them on /dev/null, to
     make sure we have no connection at all to the terminal.  */
  (void) close (0);
  (void) close (1);
  (void) close (2);
  if (open ((char *) "/dev/null", O_RDONLY) != 0
      || open ((char *) "/dev/null", O_WRONLY) != 1
      || open ((char *) "/dev/null", O_WRONLY) != 2)
    ulog (LOG_FATAL, "open (/dev/null): %s", strerror (errno));

#if HAVE_BSD_PGRP

  /* Make sure our process group ID is set to 0.  On BSD TIOCNOTTY
     should already have set it 0, so this will do no harm.  On System
     V we presumably did not execute the TIOCNOTTY call, but the
     System V setpgrp will detach the controlling terminal anyhow.
     This lets us use the same code on both BSD and System V, provided
     it compiles correctly, which life easier for the configure
     script.  We don't output an error if we got EPERM because some
     BSD variants don't permit this usage of setpgrp (which means they
     don't provide any way to pick up a new controlling terminal).  */

  if (setpgrp (0, 0) < 0)
    {
      if (errno != EPERM)
	ulog (LOG_ERROR, "setpgrp: %s", strerror (errno));
    }

#else /* ! HAVE_BSD_PGRP */

#if HAVE_SETSID

  /* Under POSIX the setsid call creates a new session for which we
     are the process group leader.  It also detaches us from our
     controlling terminal.  I'm using the BSD setpgrp call first
     because they should be equivalent for my purposes, but it turns
     out that on Ultrix 4.0 setsid prevents us from ever acquiring
     another controlling terminal (it does not change our process
     group, and Ultrix 4.0 prevents us from setting our process group
     to 0).  */
  (void) setsid ();

#else /* ! HAVE_SETSID */

#if HAVE_SETPGRP

  /* Now we assume we have the System V setpgrp, which takes no
     arguments, and we couldn't compile the HAVE_BSD_PGRP code above
     because there was a prototype somewhere in scope.  On System V
     setpgrp makes us the leader of a new process group and also
     detaches the controlling terminal.  */

  if (setpgrp () < 0)
    ulog (LOG_ERROR, "setpgrp: %s", strerror (errno));

#else /* ! HAVE_SETPGRP */

 #error Must detach from controlling terminal

#endif /* HAVE_SETPGRP */
#endif /* ! HAVE_SETSID */
#endif /* ! HAVE_BSD_PGRP */

  /* At this point we have completely detached from our controlling
     terminal.  The next terminal device we open will probably become
     our controlling terminal.  */
}
