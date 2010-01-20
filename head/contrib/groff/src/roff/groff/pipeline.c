/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRERROR
#include <string.h>
#else
extern char *strerror();
#endif

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

#define error c_error

#ifdef __cplusplus
extern "C" {
#endif

extern void error(const char *, const char *, const char *, const char *);
extern void c_fatal(const char *, const char *, const char *, const char *);
extern const char *i_to_a(int);		/* from libgroff */

#ifdef __cplusplus
}
#endif

static void sys_fatal(const char *);
static const char *xstrsignal(int);


#if defined(__MSDOS__) \
    || (defined(_WIN32) && !defined(_UWIN) && !defined(__CYGWIN__)) \
    || defined(__EMX__)

#include <process.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "nonposix.h"

static const char *sh = "sh";
static const char *cmd = "cmd";
static const char *command = "command";

extern int strcasecmp(const char *, const char *);

char *sbasename(const char *path)
{
  char *base;
  const char *p1, *p2;

  p1 = path;
  if ((p2 = strrchr(p1, '\\'))
      || (p2 = strrchr(p1, '/'))
      || (p2 = strrchr(p1, ':')))
    p1 = p2 + 1;
  if ((p2 = strrchr(p1, '.'))
      && ((strcasecmp(p2, ".exe") == 0)
	  || (strcasecmp(p2, ".com") == 0)))
    ;
  else
    p2 = p1 + strlen(p1);

  base = malloc((size_t)(p2 - p1));
  strncpy(base, p1, p2 - p1);
  *(base + (p2 - p1)) = '\0';

  return(base);
}

/* Get the name of the system shell */
char *system_shell_name(void)
{
  const char *shell_name;

  /*
     Use a Unixy shell if it's installed.  Use SHELL if set; otherwise,
     let spawnlp try to find sh; if that fails, use COMSPEC if set; if
     not, try cmd.exe; if that fails, default to command.com.
   */

  if ((shell_name = getenv("SHELL")) != NULL)
    ;
  else if (spawnlp(_P_WAIT, sh, sh, "-c", ":", NULL) == 0)
    shell_name = sh;
  else if ((shell_name = getenv("COMSPEC")) != NULL)
    ;
  else if (spawnlp(_P_WAIT, cmd, cmd, "/c", ";", NULL) == 0)
    shell_name = cmd;
  else
    shell_name = command;

  return sbasename(shell_name);
}

const char *system_shell_dash_c(void)
{
  char *shell_name;
  const char *dash_c;

  shell_name = system_shell_name();

  /* Assume that if the shell name ends in "sh", it's Unixy */
  if (strcasecmp(shell_name + strlen(shell_name) - strlen("sh"), "sh") == 0)
    dash_c = "-c";
  else
    dash_c = "/c";

  free(shell_name);
  return dash_c;
}

int is_system_shell(const char *prog)
{
  int result;
  char *this_prog, *system_shell;

  if (!prog)	/* paranoia */
    return 0;

  this_prog = sbasename(prog);
  system_shell = system_shell_name();

  result = strcasecmp(this_prog, system_shell) == 0;

  free(this_prog);
  free(system_shell);

  return result;
}

#ifdef _WIN32

/*
  Windows 32 doesn't have fork(), so we need to start asynchronous child
  processes with spawn() rather than exec().  If there is more than one
  command, i.e., a pipeline, the parent must set up each child's I/O
  redirection prior to the spawn.  The original stdout must be restored
  before spawning the last process in the pipeline, and the original
  stdin must be restored in the parent after spawning the last process
  and before waiting for any of the children.
*/

int run_pipeline(int ncommands, char ***commands, int no_pipe)
{
  int i;
  int last_input = 0;	/* pacify some compilers */
  int save_stdin = 0;
  int save_stdout = 0;
  int ret = 0;
  char err_str[BUFSIZ];
  PID_T pids[MAX_COMMANDS];

  for (i = 0; i < ncommands; i++) {
    int pdes[2];
    PID_T pid;

    /* If no_pipe is set, just run the commands in sequence
       to show the version numbers */
    if (ncommands > 1 && !no_pipe) {
      /* last command doesn't need a new pipe */
      if (i < ncommands - 1) {
	if (pipe(pdes) < 0) {
	  sprintf(err_str, "%s: pipe", commands[i][0]);
	  sys_fatal(err_str);
	}
      }
      /* 1st command; writer */
      if (i == 0) {
	/* save stdin */
	if ((save_stdin = dup(STDIN_FILENO)) < 0)
	  sys_fatal("dup stdin");
	/* save stdout */
	if ((save_stdout = dup(STDOUT_FILENO)) < 0)
	  sys_fatal("dup stdout");

	/* connect stdout to write end of pipe */
	if (dup2(pdes[1], STDOUT_FILENO) < 0) {
	  sprintf(err_str, "%s: dup2(stdout)", commands[i][0]);
	  sys_fatal(err_str);
	}
	if (close(pdes[1]) < 0) {
	  sprintf(err_str, "%s: close(pipe[WRITE])", commands[i][0]);
	  sys_fatal(err_str);
	}
	/*
	   Save the read end of the pipe so that it can be connected to
	   stdin of the next program in the pipeline during the next
	   pass through the loop.
	*/
	last_input = pdes[0];
      }
      /* reader and writer */
      else if (i < ncommands - 1) {
	/* connect stdin to read end of last pipe */
	if (dup2(last_input, STDIN_FILENO) < 0) {
	  sprintf(err_str, " %s: dup2(stdin)", commands[i][0]);
	  sys_fatal(err_str);
	}
	if (close(last_input) < 0) {
	  sprintf(err_str, "%s: close(last_input)", commands[i][0]);
	  sys_fatal(err_str);
	}
	/* connect stdout to write end of new pipe */
	if (dup2(pdes[1], STDOUT_FILENO) < 0) {
	  sprintf(err_str, "%s: dup2(stdout)", commands[i][0]);
	  sys_fatal(err_str);
	}
	if (close(pdes[1]) < 0) {
	  sprintf(err_str, "%s: close(pipe[WRITE])", commands[i][0]);
	  sys_fatal(err_str);
	}
	last_input = pdes[0];
      }
      /* last command; reader */
      else {
	/* connect stdin to read end of last pipe */
	if (dup2(last_input, STDIN_FILENO) < 0) {
	  sprintf(err_str, "%s: dup2(stdin)", commands[i][0]);
	  sys_fatal(err_str);
	}
	if (close(last_input) < 0) {
	  sprintf(err_str, "%s: close(last_input)", commands[i][0]);
	  sys_fatal(err_str);
	}
	/* restore original stdout */
	if (dup2(save_stdout, STDOUT_FILENO) < 0) {
	  sprintf(err_str, "%s: dup2(save_stdout))", commands[i][0]);
	  sys_fatal(err_str);
	}
	/* close stdout copy */
	if (close(save_stdout) < 0) {
	  sprintf(err_str, "%s: close(save_stdout)", commands[i][0]);
 	  sys_fatal(err_str);
 	}
      }
    }
    if ((pid = spawnvp(_P_NOWAIT, commands[i][0], commands[i])) < 0) {
      error("couldn't exec %1: %2",
	    commands[i][0], strerror(errno), (char *)0);
      fflush(stderr);			/* just in case error() doesn't */
      _exit(EXEC_FAILED_EXIT_STATUS);
    }
    pids[i] = pid;
  }

  if (ncommands > 1 && !no_pipe) {
    /* restore original stdin if it was redirected */
    if (dup2(save_stdin, STDIN_FILENO) < 0) {
      sprintf(err_str, "dup2(save_stdin))");
      sys_fatal(err_str);
    }
    /* close stdin copy */
    if (close(save_stdin) < 0) {
      sprintf(err_str, "close(save_stdin)");
      sys_fatal(err_str);
    }
  }

  for (i = 0; i < ncommands; i++) {
    int status;
    PID_T pid;

    pid = pids[i];
    if ((pid = WAIT(&status, pid, _WAIT_CHILD)) < 0) {
      sprintf(err_str, "%s: wait", commands[i][0]);
      sys_fatal(err_str);
    }
    else if (status != 0)
      ret |= 1;
  }
  return ret;
}

#else  /* not _WIN32 */

/* MSDOS doesn't have `fork', so we need to simulate the pipe by running
   the programs in sequence with standard streams redirected to and
   from temporary files.
*/


/* A signal handler that just records that a signal has happened.  */
static int child_interrupted;

static RETSIGTYPE signal_catcher(int signo)
{
  child_interrupted++;
}

int run_pipeline(int ncommands, char ***commands, int no_pipe)
{
  int save_stdin = dup(0);
  int save_stdout = dup(1);
  char *tmpfiles[2];
  int infile  = 0;
  int outfile = 1;
  int i, f, ret = 0;

  /* Choose names for a pair of temporary files to implement the pipeline.
     Microsoft's `tempnam' uses the directory specified by `getenv("TMP")'
     if it exists; in case it doesn't, try the GROFF alternatives, or
     `getenv("TEMP")' as last resort -- at least one of these had better
     be set, since Microsoft's default has a high probability of failure. */
  char *tmpdir;
  if ((tmpdir = getenv("GROFF_TMPDIR")) == NULL
      && (tmpdir = getenv("TMPDIR")) == NULL)
    tmpdir = getenv("TEMP");

  /* Don't use `tmpnam' here: Microsoft's implementation yields unusable
     file names if current directory is on network share with read-only
     root. */
  tmpfiles[0] = tempnam(tmpdir, NULL);
  tmpfiles[1] = tempnam(tmpdir, NULL);

  for (i = 0; i < ncommands; i++) {
    int exit_status;
    RETSIGTYPE (*prev_handler)(int);

    if (i && !no_pipe) {
      /* redirect stdin from temp file */
      f = open(tmpfiles[infile], O_RDONLY|O_BINARY, 0666);
      if (f < 0)
	sys_fatal("open stdin");
      if (dup2(f, 0) < 0)
	sys_fatal("dup2 stdin"); 
      if (close(f) < 0)
	sys_fatal("close stdin");
    }
    if ((i < ncommands - 1) && !no_pipe) {
      /* redirect stdout to temp file */
      f = open(tmpfiles[outfile], O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0666);
      if (f < 0)
	sys_fatal("open stdout");
      if (dup2(f, 1) < 0)
	sys_fatal("dup2 stdout");
      if (close(f) < 0)
	sys_fatal("close stdout");
    }
    else if (dup2(save_stdout, 1) < 0)
      sys_fatal("restore stdout");

    /* run the program */
    child_interrupted = 0;
    prev_handler = signal(SIGINT, signal_catcher);
    exit_status = spawnvp(P_WAIT, commands[i][0], commands[i]);
    signal(SIGINT, prev_handler);
    if (child_interrupted) {
      error("%1: Interrupted", commands[i][0], (char *)0, (char *)0);
      ret |= 2;
    }
    else if (exit_status < 0) {
      error("couldn't exec %1: %2",
	    commands[i][0], strerror(errno), (char *)0);
      fflush(stderr);			/* just in case error() doesn't */
      ret |= 4;
    }
    if (exit_status != 0)
      ret |= 1;
    /* There's no sense to continue with the pipe if one of the
       programs has ended abnormally, is there? */
    if (ret != 0)
      break;
    /* swap temp files: make output of this program be input for the next */
    infile = 1 - infile;
    outfile = 1 - outfile;
  }
  if (dup2(save_stdin, 0) < 0)
    sys_fatal("restore stdin");
  unlink(tmpfiles[0]);
  unlink(tmpfiles[1]);
  return ret;
}

#endif /* not _WIN32 */

#else /* not __MSDOS__, not _WIN32 */

int run_pipeline(int ncommands, char ***commands, int no_pipe)
{
  int i;
  int last_input = 0;
  PID_T pids[MAX_COMMANDS];
  int ret = 0;
  int proc_count = ncommands;

  for (i = 0; i < ncommands; i++) {
    int pdes[2];
    PID_T pid;

    if ((i != ncommands - 1) && !no_pipe) {
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
      if ((i != ncommands - 1) && !no_pipe) {
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
      error("couldn't exec %1: %2",
	    commands[i][0], strerror(errno), (char *)0);
      fflush(stderr);			/* just in case error() doesn't */
      _exit(EXEC_FAILED_EXIT_STATUS);
    }
    /* in the parent */
    if (last_input != 0) {
      if (close(last_input) < 0)
	sys_fatal("close");
    }
    if ((i != ncommands - 1) && !no_pipe) {
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
	  error("unexpected status %1",	i_to_a(status), (char *)0, (char *)0);
	break;
      }
  }
  return ret;
}

#endif /* not __MSDOS__, not _WIN32 */

static void sys_fatal(const char *s)
{
  c_fatal("%1: %2", s, strerror(errno), (char *)0);
}

static const char *xstrsignal(int n)
{
  static char buf[sizeof("Signal ") + 1 + sizeof(int) * 3];

#ifdef NSIG
#if HAVE_DECL_SYS_SIGLIST
  if (n >= 0 && n < NSIG && sys_siglist[n] != 0)
    return sys_siglist[n];
#endif /* HAVE_DECL_SYS_SIGLIST */
#endif /* NSIG */
  sprintf(buf, "Signal %d", n);
  return buf;
}
