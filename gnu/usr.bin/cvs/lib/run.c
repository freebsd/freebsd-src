/* run.c --- routines for executing subprocesses.
   
   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "cvs.h"

#ifdef HAVE_VPRINTF
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#include <stdarg.h>
#define VA_START(args, lastarg) va_start(args, lastarg)
#else
#include <varargs.h>
#define VA_START(args, lastarg) va_start(args)
#endif
#else
#define va_alist a1, a2, a3, a4, a5, a6, a7, a8
#define va_dcl char *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
#endif

static void run_add_arg PROTO((const char *s));
static void run_init_prog PROTO((void));

extern char *strtok ();

/*
 * To exec a program under CVS, first call run_setup() to setup any initial
 * arguments.  The options to run_setup are essentially like printf(). The
 * arguments will be parsed into whitespace separated words and added to the
 * global run_argv list.
 * 
 * Then, optionally call run_arg() for each additional argument that you'd like
 * to pass to the executed program.
 * 
 * Finally, call run_exec() to execute the program with the specified arguments.
 * The execvp() syscall will be used, so that the PATH is searched correctly.
 * File redirections can be performed in the call to run_exec().
 */
static char *run_prog;
static char **run_argv;
static int run_argc;
static int run_argc_allocated;

/* VARARGS */
#if defined (HAVE_VPRINTF) && (defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__))
void 
run_setup (const char *fmt,...)
#else
void 
run_setup (fmt, va_alist)
    char *fmt;
    va_dcl
#endif
{
#ifdef HAVE_VPRINTF
    va_list args;
#endif
    char *cp;
    int i;

    run_init_prog ();

    /* clean out any malloc'ed values from run_argv */
    for (i = 0; i < run_argc; i++)
    {
	if (run_argv[i])
	{
	    free (run_argv[i]);
	    run_argv[i] = (char *) 0;
	}
    }
    run_argc = 0;

    /* process the varargs into run_prog */
#ifdef HAVE_VPRINTF
    VA_START (args, fmt);
    (void) vsprintf (run_prog, fmt, args);
    va_end (args);
#else
    (void) sprintf (run_prog, fmt, a1, a2, a3, a4, a5, a6, a7, a8);
#endif

    /* put each word into run_argv, allocating it as we go */
    for (cp = strtok (run_prog, " \t"); cp; cp = strtok ((char *) NULL, " \t"))
	run_add_arg (cp);
}

void
run_arg (s)
    const char *s;
{
    run_add_arg (s);
}

/* VARARGS */
#if defined (HAVE_VPRINTF) && (defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__))
void 
run_args (const char *fmt,...)
#else
void 
run_args (fmt, va_alist)
    char *fmt;
    va_dcl
#endif
{
#ifdef HAVE_VPRINTF
    va_list args;
#endif

    run_init_prog ();

    /* process the varargs into run_prog */
#ifdef HAVE_VPRINTF
    VA_START (args, fmt);
    (void) vsprintf (run_prog, fmt, args);
    va_end (args);
#else
    (void) sprintf (run_prog, fmt, a1, a2, a3, a4, a5, a6, a7, a8);
#endif

    /* and add the (single) argument to the run_argv list */
    run_add_arg (run_prog);
}

static void
run_add_arg (s)
    const char *s;
{
    /* allocate more argv entries if we've run out */
    if (run_argc >= run_argc_allocated)
    {
	run_argc_allocated += 50;
	run_argv = (char **) xrealloc ((char *) run_argv,
				     run_argc_allocated * sizeof (char **));
    }

    if (s)
	run_argv[run_argc++] = xstrdup (s);
    else
	run_argv[run_argc] = (char *) 0;	/* not post-incremented on purpose! */
}

static void
run_init_prog ()
{
    /* make sure that run_prog is allocated once */
    if (run_prog == (char *) 0)
	run_prog = xmalloc (10 * 1024);	/* 10K of args for _setup and _arg */
}

int
run_exec (stin, stout, sterr, flags)
    char *stin;
    char *stout;
    char *sterr;
    int flags;
{
    int shin, shout, sherr;
    int mode_out, mode_err;
    int status;
    int rc = -1;
    int rerrno = 0;
    int pid, w;

#ifdef POSIX_SIGNALS
    sigset_t sigset_mask, sigset_omask;
    struct sigaction act, iact, qact;

#else
#ifdef BSD_SIGNALS
    int mask;
    struct sigvec vec, ivec, qvec;

#else
    RETSIGTYPE (*istat) (), (*qstat) ();
#endif
#endif

    if (trace)
    {
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> system(", (server_active) ? 'S' : ' ');
#else
	(void) fprintf (stderr, "-> system(");
#endif
	run_print (stderr);
	(void) fprintf (stderr, ")\n");
    }
    if (noexec && (flags & RUN_REALLY) == 0)
	return (0);

    /* make sure that we are null terminated, since we didn't calloc */
    run_add_arg ((char *) 0);

    /* setup default file descriptor numbers */
    shin = 0;
    shout = 1;
    sherr = 2;

    /* set the file modes for stdout and stderr */
    mode_out = mode_err = O_WRONLY | O_CREAT;
    mode_out |= ((flags & RUN_STDOUT_APPEND) ? O_APPEND : O_TRUNC);
    mode_err |= ((flags & RUN_STDERR_APPEND) ? O_APPEND : O_TRUNC);

    if (stin && (shin = open (stin, O_RDONLY)) == -1)
    {
	rerrno = errno;
	error (0, errno, "cannot open %s for reading (prog %s)",
	       stin, run_argv[0]);
	goto out0;
    }
    if (stout && (shout = open (stout, mode_out, 0666)) == -1)
    {
	rerrno = errno;
	error (0, errno, "cannot open %s for writing (prog %s)",
	       stout, run_argv[0]);
	goto out1;
    }
    if (sterr && (flags & RUN_COMBINED) == 0)
    {
	if ((sherr = open (sterr, mode_err, 0666)) == -1)
	{
	    rerrno = errno;
	    error (0, errno, "cannot open %s for writing (prog %s)",
		   sterr, run_argv[0]);
	    goto out2;
	}
    }

    /* Make sure we don't flush this twice, once in the subprocess.  */
    fflush (stdout);
    fflush (stderr);

    /* The output files, if any, are now created.  Do the fork and dups */
#ifdef HAVE_VFORK
    pid = vfork ();
#else
    pid = fork ();
#endif
    if (pid == 0)
    {
	if (shin != 0)
	{
	    (void) dup2 (shin, 0);
	    (void) close (shin);
	}
	if (shout != 1)
	{
	    (void) dup2 (shout, 1);
	    (void) close (shout);
	}
	if (flags & RUN_COMBINED)
	    (void) dup2 (1, 2);
	else if (sherr != 2)
	{
	    (void) dup2 (sherr, 2);
	    (void) close (sherr);
	}

	/* dup'ing is done.  try to run it now */
	(void) execvp (run_argv[0], run_argv);
	error (0, errno, "cannot exec %s", run_argv[0]);
	_exit (127);
    }
    else if (pid == -1)
    {
	rerrno = errno;
	goto out;
    }

    /* the parent.  Ignore some signals for now */
#ifdef POSIX_SIGNALS
    if (flags & RUN_SIGIGNORE)
    {
	act.sa_handler = SIG_IGN;
	(void) sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	(void) sigaction (SIGINT, &act, &iact);
	(void) sigaction (SIGQUIT, &act, &qact);
    }
    else
    {
	(void) sigemptyset (&sigset_mask);
	(void) sigaddset (&sigset_mask, SIGINT);
	(void) sigaddset (&sigset_mask, SIGQUIT);
	(void) sigprocmask (SIG_SETMASK, &sigset_mask, &sigset_omask);
    }
#else
#ifdef BSD_SIGNALS
    if (flags & RUN_SIGIGNORE)
    {
	memset ((char *) &vec, 0, sizeof (vec));
	vec.sv_handler = SIG_IGN;
	(void) sigvec (SIGINT, &vec, &ivec);
	(void) sigvec (SIGQUIT, &vec, &qvec);
    }
    else
	mask = sigblock (sigmask (SIGINT) | sigmask (SIGQUIT));
#else
    istat = signal (SIGINT, SIG_IGN);
    qstat = signal (SIGQUIT, SIG_IGN);
#endif
#endif

    /* wait for our process to die and munge return status */
#ifdef POSIX_SIGNALS
    while ((w = waitpid (pid, &status, 0)) == -1 && errno == EINTR)
	;
#else
    while ((w = wait (&status)) != pid)
    {
	if (w == -1 && errno != EINTR)
	    break;
    }
#endif
    if (w == -1)
    {
	rc = -1;
	rerrno = errno;
    }
    else if (WIFEXITED (status))
	rc = WEXITSTATUS (status);
    else if (WIFSIGNALED (status))
    {
	if (WTERMSIG (status) == SIGPIPE)
	    error (1, 0, "broken pipe");
	rc = 2;
    }
    else
	rc = 1;

    /* restore the signals */
#ifdef POSIX_SIGNALS
    if (flags & RUN_SIGIGNORE)
    {
	(void) sigaction (SIGINT, &iact, (struct sigaction *) NULL);
	(void) sigaction (SIGQUIT, &qact, (struct sigaction *) NULL);
    }
    else
	(void) sigprocmask (SIG_SETMASK, &sigset_omask, (sigset_t *) NULL);
#else
#ifdef BSD_SIGNALS
    if (flags & RUN_SIGIGNORE)
    {
	(void) sigvec (SIGINT, &ivec, (struct sigvec *) NULL);
	(void) sigvec (SIGQUIT, &qvec, (struct sigvec *) NULL);
    }
    else
	(void) sigsetmask (mask);
#else
    (void) signal (SIGINT, istat);
    (void) signal (SIGQUIT, qstat);
#endif
#endif

    /* cleanup the open file descriptors */
  out:
    if (sterr)
	(void) close (sherr);
  out2:
    if (stout)
	(void) close (shout);
  out1:
    if (stin)
	(void) close (shin);

  out0:
    if (rerrno)
	errno = rerrno;
    return (rc);
}

void
run_print (fp)
    FILE *fp;
{
    int i;

    for (i = 0; i < run_argc; i++)
    {
	(void) fprintf (fp, "'%s'", run_argv[i]);
	if (i != run_argc - 1)
	    (void) fprintf (fp, " ");
    }
}

FILE *
Popen (cmd, mode)
    const char *cmd;
    const char *mode;
{
    if (trace)
#ifdef SERVER_SUPPORT
	(void) fprintf (stderr, "%c-> Popen(%s,%s)\n",
			(server_active) ? 'S' : ' ', cmd, mode);
#else
	(void) fprintf (stderr, "-> Popen(%s,%s)\n", cmd, mode);
#endif
    if (noexec)
	return (NULL);

    return (popen (cmd, mode));
}

extern int evecvp PROTO((char *file, char **argv));

int
piped_child (command, tofdp, fromfdp)
     char **command;
     int *tofdp;
     int *fromfdp;
{
    int pid;
    int to_child_pipe[2];
    int from_child_pipe[2];

    if (pipe (to_child_pipe) < 0)
	error (1, errno, "cannot create pipe");
    if (pipe (from_child_pipe) < 0)
	error (1, errno, "cannot create pipe");

    pid = fork ();
    if (pid < 0)
	error (1, errno, "cannot fork");
    if (pid == 0)
    {
	if (dup2 (to_child_pipe[0], STDIN_FILENO) < 0)
	    error (1, errno, "cannot dup2");
	if (close (to_child_pipe[1]) < 0)
	    error (1, errno, "cannot close");
	if (close (from_child_pipe[0]) < 0)
	    error (1, errno, "cannot close");
	if (dup2 (from_child_pipe[1], STDOUT_FILENO) < 0)
	    error (1, errno, "cannot dup2");

	execvp (command[0], command);
	error (1, errno, "cannot exec");
    }
    if (close (to_child_pipe[0]) < 0)
	error (1, errno, "cannot close");
    if (close (from_child_pipe[1]) < 0)
	error (1, errno, "cannot close");

    *tofdp = to_child_pipe[1];
    *fromfdp = from_child_pipe[0];
    return pid;
}


void
close_on_exec (fd)
     int fd;
{
#if defined (FD_CLOEXEC) && defined (F_SETFD)
  if (fcntl (fd, F_SETFD, 1))
    error (1, errno, "can't set close-on-exec flag on %d", fd);
#endif
}

/*
 * dir = 0 : main proc writes to new proc, which writes to oldfd
 * dir = 1 : main proc reads from new proc, which reads from oldfd
 */

int
filter_stream_through_program (oldfd, dir, prog, pidp)
     int oldfd, dir;
     char **prog;
     pid_t *pidp;
{
    int p[2], newfd;
    pid_t newpid;

    if (pipe (p))
	error (1, errno, "cannot create pipe");
    newpid = fork ();
    if (pidp)
	*pidp = newpid;
    switch (newpid)
    {
      case -1:
	error (1, errno, "cannot fork");
      case 0:
	/* child */
	if (dir)
	{
	    /* write to new pipe */
	    close (p[0]);
	    dup2 (oldfd, 0);
	    dup2 (p[1], 1);
	}
	else
	{
	    /* read from new pipe */
	    close (p[1]);
	    dup2 (p[0], 0);
	    dup2 (oldfd, 1);
	}
	/* Should I be blocking some signals here?  */
	execvp (prog[0], prog);
	error (1, errno, "couldn't exec %s", prog[0]);
      default:
	/* parent */
	close (oldfd);
	if (dir)
	{
	    /* read from new pipe */
	    close (p[1]);
	    newfd = p[0];
	}
	else
	{
	    /* write to new pipe */
	    close (p[0]);
	    newfd = p[1];
	}
	close_on_exec (newfd);
	return newfd;
    }
}
