/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/*
Compile options are:

-DWCOREFLAG=0200 (or whatever)
-DHAVE_SYS_SIGLIST
-DSYS_SIGLIST_DECLARED
-DHAVE_UNISTD_H
*/

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef errno
extern int errno;
#endif

extern char *strerror();

#ifdef _POSIX_VERSION

#include <sys/wait.h>

#define PID_T pid_t

#else /* not _POSIX_VERSION */

/* traditional Unix */

#define WIFEXITED(s) (((s) & 0377) == 0)
#define WIFSTOPPED(s) (((s) & 0377) == 0177)
#define WIFSIGNALED(s) (((s) & 0377) != 0 && (((s) & 0377) != 0177))
#define WEXITSTATUS(s) (((s) >> 8) & 0377)
#define WTERMSIG(s) ((s) & 0177)
#define WSTOPSIG(s) (((s) >> 8) & 0377)

#ifndef WCOREFLAG
#define WCOREFLAG 0200
#endif

#define PID_T int

#endif /* not _POSIX_VERSION */

/* SVR4 uses WCOREFLG; Net 2 uses WCOREFLAG. */
#ifndef WCOREFLAG
#ifdef WCOREFLG
#define WCOREFLAG WCOREFLG
#endif /* WCOREFLG */
#endif /* not WCOREFLAG */

#ifndef WCOREDUMP
#ifdef WCOREFLAG
#define WCOREDUMP(s) ((s) & WCOREFLAG)
#else /* not WCOREFLAG */
#define WCOREDUMP(s) (0)
#endif /* WCOREFLAG */
#endif /* not WCOREDUMP */

#include "pipeline.h"

#ifdef __STDC__
#define P(parms) parms
#else
#define P(parms) ()
#define const /* as nothing */
#endif

#define error c_error
extern void error P((const char *, const char *, const char *, const char *));
extern void c_fatal P((const char *, const char *, const char *, const char *));

static void sys_fatal P((const char *));
static const char *xstrsignal P((int));
static char *itoa P((int));

int run_pipeline(ncommands, commands)
     int ncommands;
     char ***commands;
{
  int i;
  int last_input = 0;
  PID_T pids[MAX_COMMANDS];
  int ret = 0;
  int proc_count = ncommands;

  for (i = 0; i < ncommands; i++) {
      int pdes[2];
      PID_T pid;
      if (i != ncommands - 1) {
	if (pipe(pdes) < 0)
	  sys_fatal("pipe");
      }
      pid = fork();
      if (pid < 0)
	sys_fatal("fork");
      if (pid == 0) {
	/* child */
	if (last_input != 0) {
	  if (close(0) < 0)
	    sys_fatal("close");
	  if (dup(last_input) < 0)
	    sys_fatal("dup");
	  if (close(last_input) < 0)
	    sys_fatal("close");
	}
	if (i != ncommands - 1) {
	  if (close(1) < 0)
	    sys_fatal("close");
	  if (dup(pdes[1]) < 0)
	    sys_fatal("dup");
	  if (close(pdes[1]) < 0)
	    sys_fatal("close");
	  if (close(pdes[0]))
	    sys_fatal("close");
	}
	execvp(commands[i][0], commands[i]);
	error("couldn't exec %1: %2", commands[i][0],
	      strerror(errno), (char *)0);
	fflush(stderr);		/* just in case error() doesn't */
	_exit(EXEC_FAILED_EXIT_STATUS);
      }
      /* in the parent */
      if (last_input != 0) {
	if (close(last_input) < 0)
	  sys_fatal("close");
      }
      if (i != ncommands - 1) {
	if (close(pdes[1]) < 0)
	  sys_fatal("close");
	last_input = pdes[0];
      }
      pids[i] = pid;
    }
  while (proc_count > 0) {
    int status;
    PID_T pid = wait(&status);
    if (pid < 0)
      sys_fatal("wait");
    for (i = 0; i < ncommands; i++)
      if (pids[i] == pid) {
	pids[i] = -1;
	--proc_count;
	if (WIFSIGNALED(status)) {
	  int sig = WTERMSIG(status);
#ifdef SIGPIPE
	  if (sig == SIGPIPE) {
	    if (i == ncommands - 1) {

	      /* This works around a problem that occurred when using the
		 rerasterize action in gxditview.  What seemed to be
		 happening (on SunOS 4.1.1) was that pclose() closed the
		 pipe and waited for groff, gtroff got a SIGPIPE, but
		 gpic blocked writing to gtroff, and so groff blocked
		 waiting for gpic and gxditview blocked waiting for
		 groff.  I don't understand why gpic wasn't getting a
		 SIGPIPE. */
	      int j;
	      for (j = 0; j < ncommands; j++)
		if (pids[j] > 0)
		  (void)kill(pids[j], SIGPIPE);
	    }
	  }
	  else
#endif /* SIGPIPE */
	  {
	    error("%1: %2%3",
		  commands[i][0],
		  xstrsignal(sig),
		  WCOREDUMP(status) ? " (core dumped)" : "");
	    ret |= 2;
	  }
	}
	else if (WIFEXITED(status)) {
	  int exit_status = WEXITSTATUS(status);
	  if (exit_status == EXEC_FAILED_EXIT_STATUS)
	    ret |= 4;
	  else if (exit_status != 0)
	    ret |= 1;
	}
	else
	  error("unexpected status %1",
		itoa(status), (char *)0, (char *)0);
	break;
      }
  }
  return ret;
}

static void sys_fatal(s)
     const char *s;
{
  c_fatal("%1: %2", s, strerror(errno), (char *)0);
}

static char *itoa(n)
     int n;
{
  static char buf[12];
  sprintf(buf, "%d", n);
  return buf;
}

static const char *xstrsignal(n)
     int n;
{
  static char buf[sizeof("Signal ") + 1 + sizeof(int)*3];
#ifdef NSIG
#ifdef SYS_SIGLIST_DECLARED
  if (n >= 0 && n < NSIG && sys_siglist[n] != 0)
    return sys_siglist[n];
#endif /* SYS_SIGLIST_DECLARED */
#endif /* NSIG */
  sprintf(buf, "Signal %d", n);
  return buf;
}
