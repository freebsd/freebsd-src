/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Various useful functions for the CVS support code.
 */

#include "cvs.h"

#ifndef lint
static char rcsid[] = "$CVSid: @(#)subr.c 1.64 94/10/07 $";
USE(rcsid)
#endif

#ifdef _MINIX
#undef	POSIX		/* Minix 1.6 doesn't support POSIX.1 sigaction yet */
#endif

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

/*
 * I don't know of a convenient way to test this at configure time, or else
 * I'd certainly do it there.
 */
#if defined(NeXT)
#define LOSING_TMPNAM_FUNCTION
#ifndef	_POSIX_SOURCE
/*
 * NeXT doesn't define these without _POSIX_SOURCE,
 * but that changes a lot of things.
 */
#define	WEXITSTATUS(x)	((x).w_retcode)
#define	WTERMSIG(x)	((x).w_termsig)
#endif
#endif

static void run_add_arg PROTO((char *s));
static void run_init_prog PROTO((void));

extern char *getlogin ();
extern char *strtok ();

/*
 * Copies "from" to "to". mallocs a buffer large enough to hold the entire
 * file and does one read/one write to do the copy.  This is reasonable,
 * since source files are typically not too large.
 */
void
copy_file (from, to)
    char *from;
    char *to;
{
    struct stat sb;
    struct utimbuf t;
    int fdin, fdout;
    char *buf;

    if (trace)
	(void) fprintf (stderr, "-> copy(%s,%s)\n", from, to);
    if (noexec)
	return;

    if ((fdin = open (from, O_RDONLY)) < 0)
	error (1, errno, "cannot open %s for copying", from);
    if (fstat (fdin, &sb) < 0)
	error (1, errno, "cannot fstat %s", from);
    if ((fdout = creat (to, (int) sb.st_mode & 07777)) < 0)
	error (1, errno, "cannot create %s for copying", to);
    if (sb.st_size > 0)
    {
	buf = xmalloc ((int) sb.st_size);
	if (read (fdin, buf, (int) sb.st_size) != (int) sb.st_size)
	    error (1, errno, "cannot read file %s for copying", from);
	if (write (fdout, buf, (int) sb.st_size) != (int) sb.st_size
#ifdef HAVE_FSYNC
	    || fsync (fdout) == -1
#endif
	    )
	{
	    error (1, errno, "cannot write file %s for copying", to);
	}
	free (buf);
    }
    (void) close (fdin);
    if (close (fdout) < 0)
	error (1, errno, "cannot close %s", to);

    /* now, set the times for the copied file to match those of the original */
    memset ((char *) &t, 0, sizeof (t));
    t.actime = sb.st_atime;
    t.modtime = sb.st_mtime;
    (void) utime (to, &t);
}

/* FIXME-krp: these functions would benefit from caching the char * &
   stat buf.  */

/*
 * Returns non-zero if the argument file is a directory, or is a symbolic
 * link which points to a directory.
 */
int
isdir (file)
    char *file;
{
    struct stat sb;

    if (stat (file, &sb) < 0)
	return (0);
    return (S_ISDIR (sb.st_mode));
}

/*
 * Returns non-zero if the argument file is a symbolic link.
 */
int
islink (file)
    char *file;
{
#ifdef S_ISLNK
    struct stat sb;

    if (lstat (file, &sb) < 0)
	return (0);
    return (S_ISLNK (sb.st_mode));
#else
    return (0);
#endif
}

/*
 * Returns non-zero if the argument file exists.
 */
int
isfile (file)
    char *file;
{
    struct stat sb;

    if (stat (file, &sb) < 0)
	return (0);
    return (1);
}

/*
 * Returns non-zero if the argument file is readable.
 * XXX - must be careful if "cvs" is ever made setuid!
 */
int
isreadable (file)
    char *file;
{
    return (access (file, R_OK) != -1);
}

/*
 * Returns non-zero if the argument file is writable
 * XXX - muct be careful if "cvs" is ever made setuid!
 */
int
iswritable (file)
    char *file;
{
    return (access (file, W_OK) != -1);
}

/*
 * Open a file and die if it fails
 */
FILE *
open_file (name, mode)
    char *name;
    char *mode;
{
    FILE *fp;

    if ((fp = fopen (name, mode)) == NULL)
	error (1, errno, "cannot open %s", name);
    return (fp);
}

/*
 * Open a file if allowed and return.
 */
FILE *
Fopen (name, mode)
    char *name;
    char *mode;
{
    if (trace)
	(void) fprintf (stderr, "-> fopen(%s,%s)\n", name, mode);
    if (noexec)
	return (NULL);

    return (fopen (name, mode));
}

/*
 * Make a directory and die if it fails
 */
void
make_directory (name)
    char *name;
{
    struct stat buf;

    if (stat (name, &buf) == 0 && (!S_ISDIR (buf.st_mode)))
	    error (0, 0, "%s already exists but is not a directory", name);
    if (!noexec && mkdir (name, 0777) < 0)
	error (1, errno, "cannot make directory %s", name);
}

/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
void
make_directories (name)
    char *name;
{
    char *cp;

    if (noexec)
	return;

    if (mkdir (name, 0777) == 0 || errno == EEXIST)
	return;
    if (errno != ENOENT)
    {
	error (0, errno, "cannot make path to %s", name);
	return;
    }
    if ((cp = strrchr (name, '/')) == NULL)
	return;
    *cp = '\0';
    make_directories (name);
    *cp++ = '/';
    if (*cp == '\0')
	return;
    (void) mkdir (name, 0777);
}

/*
 * malloc some data and die if it fails
 */
char *
xmalloc (bytes)
    size_t bytes;
{
    char *cp;

    if ((cp = malloc (bytes)) == NULL)
	error (1, 0, "can not allocate %lu bytes", (unsigned long) bytes);
    return (cp);
}

/*
 * realloc data and die if it fails [I've always wanted to have "realloc" do
 * a "malloc" if the argument is NULL, but you can't depend on it.  Here, I
 * can *force* it.
 */
char *
xrealloc (ptr, bytes)
    char *ptr;
    size_t bytes;
{
    char *cp;

    if (!ptr)
	cp = malloc (bytes);
    else
	cp = realloc (ptr, bytes);

    if (cp == NULL)
	error (1, 0, "can not reallocate %lu bytes", (unsigned long) bytes);
    return (cp);
}

/*
 * Duplicate a string, calling xmalloc to allocate some dynamic space
 */
char *
xstrdup (str)
    char *str;
{
    char *s;

    if (str == NULL)
	return ((char *) NULL);
    s = xmalloc (strlen (str) + 1);
    (void) strcpy (s, str);
    return (s);
}

/*
 * Change the mode of a file, either adding write permissions, or removing
 * all write permissions.  Adding write permissions honors the current umask
 * setting.
 */
void
xchmod (fname, writable)
    char *fname;
    int writable;
{
    struct stat sb;
    mode_t mode, oumask;

    if (stat (fname, &sb) < 0)
    {
	if (!noexec)
	    error (0, errno, "cannot stat %s", fname);
	return;
    }
    if (writable)
    {
	oumask = umask (0);
	(void) umask (oumask);
	mode = sb.st_mode | ((S_IWRITE | S_IWGRP | S_IWOTH) & ~oumask);
    }
    else
    {
	mode = sb.st_mode & ~(S_IWRITE | S_IWGRP | S_IWOTH);
    }

    if (trace)
	(void) fprintf (stderr, "-> chmod(%s,%o)\n", fname, mode);
    if (noexec)
	return;

    if (chmod (fname, mode) < 0)
	error (0, errno, "cannot change mode of file %s", fname);
}

/*
 * Rename a file and die if it fails
 */
void
rename_file (from, to)
    char *from;
    char *to;
{
    if (trace)
	(void) fprintf (stderr, "-> rename(%s,%s)\n", from, to);
    if (noexec)
	return;

    if (rename (from, to) < 0)
	error (1, errno, "cannot rename file %s to %s", from, to);
}

/*
 * link a file, if possible.
 */
int
link_file (from, to)
    char *from, *to;
{
    if (trace)
	(void) fprintf (stderr, "-> link(%s,%s)\n", from, to);
    if (noexec)
	return (0);

    return (link (from, to));
}

/*
 * unlink a file, if possible.
 */
int
unlink_file (f)
    char *f;
{
    if (trace)
	(void) fprintf (stderr, "-> unlink(%s)\n", f);
    if (noexec)
	return (0);

    return (unlink (f));
}

/*
 * Compare "file1" to "file2". Return non-zero if they don't compare exactly.
 * 
 * mallocs a buffer large enough to hold the entire file and does two reads to
 * load the buffer and calls memcmp to do the cmp. This is reasonable, since
 * source files are typically not too large.
 */

/* richfix: this *could* exploit mmap. */

int
xcmp (file1, file2)
    char *file1;
    char *file2;
{
    register char *buf1, *buf2;
    struct stat sb;
    off_t size;
    int ret, fd1, fd2;

    if ((fd1 = open (file1, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", file1);
    if ((fd2 = open (file2, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", file2);
    if (fstat (fd1, &sb) < 0)
	error (1, errno, "cannot fstat %s", file1);
    size = sb.st_size;
    if (fstat (fd2, &sb) < 0)
	error (1, errno, "cannot fstat %s", file2);
    if (size == sb.st_size)
    {
	if (size == 0)
	    ret = 0;
	else
	{
	    buf1 = xmalloc ((int) size);
	    buf2 = xmalloc ((int) size);
	    if (read (fd1, buf1, (int) size) != (int) size)
		error (1, errno, "cannot read file %s for comparing", file1);
	    if (read (fd2, buf2, (int) size) != (int) size)
		error (1, errno, "cannot read file %s for comparing", file2);
	    ret = memcmp(buf1, buf2, (int) size);
	    free (buf1);
	    free (buf2);
	}
    }
    else
	ret = 1;
    (void) close (fd1);
    (void) close (fd2);
    return (ret);
}

/*
 * Recover the space allocated by Find_Names() and line2argv()
 */
void
free_names (pargc, argv)
    int *pargc;
    char *argv[];
{
    register int i;

    for (i = 0; i < *pargc; i++)
    {					/* only do through *pargc */
	free (argv[i]);
    }
    *pargc = 0;				/* and set it to zero when done */
}

/*
 * Convert a line into argc/argv components and return the result in the
 * arguments as passed.  Use free_names() to return the memory allocated here
 * back to the free pool.
 */
void
line2argv (pargc, argv, line)
    int *pargc;
    char *argv[];
    char *line;
{
    char *cp;

    *pargc = 0;
    for (cp = strtok (line, " \t"); cp; cp = strtok ((char *) NULL, " \t"))
    {
	argv[*pargc] = xstrdup (cp);
	(*pargc)++;
    }
}

/*
 * Returns the number of dots ('.') found in an RCS revision number
 */
int
numdots (s)
    char *s;
{
    char *cp;
    int dots = 0;

    for (cp = s; *cp; cp++)
    {
	if (*cp == '.')
	    dots++;
    }
    return (dots);
}

/*
 * Get the caller's login from his uid. If the real uid is "root" try LOGNAME
 * USER or getlogin(). If getlogin() and getpwuid() both fail, return
 * the uid as a string.
 */
char *
getcaller ()
{
    static char uidname[20];
    struct passwd *pw;
    char *name;
    uid_t uid;

    uid = getuid ();
    if (uid == (uid_t) 0)
    {
	/* super-user; try getlogin() to distinguish */
	if (((name = getenv("LOGNAME")) || (name = getenv("USER")) ||
	     (name = getlogin ())) && *name)
	    return (name);
    }
    if ((pw = (struct passwd *) getpwuid (uid)) == NULL)
    {
	(void) sprintf (uidname, "uid%d", (unsigned long) uid);
	return (uidname);
    }
    return (pw->pw_name);
}

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
run_setup (char *fmt,...)
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
    char *s;
{
    run_add_arg (s);
}

/* VARARGS */
#if defined (HAVE_VPRINTF) && (defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__))
void 
run_args (char *fmt,...)
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
    char *s;
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
	run_argv[run_argc] = (char *) 0;/* not post-incremented on purpose! */
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
#if	defined(NeXT) && !defined(_POSIX_SOURCE)
    union wait status;
#else
    int status;
#endif
    int rc = -1;
    int rerrno = 0;
    int pid, w;

#ifdef POSIX
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
	(void) fprintf (stderr, "-> system(");
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
#ifdef POSIX
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
#ifdef POSIX
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
#ifdef POSIX
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
	(void) fprintf (fp, "%s", run_argv[i]);
	if (i != run_argc - 1)
	    (void) fprintf (fp, " ");
    }
}

FILE *
Popen (cmd, mode)
    char *cmd, *mode;
{
    if (trace)
	(void) fprintf (stderr, "-> Popen(%s,%s)\n", cmd, mode);
    if (noexec)
	return (NULL);
    return (popen (cmd, mode));
}

#ifdef lint
#ifndef __GNUC__
/* ARGSUSED */
time_t
get_date (date, now)
    char *date;
    struct timeb *now;
{
    time_t foo = 0;

    return (foo);
}
#endif
#endif

/* Given two revisions, find their greatest common ancestor.  If the
   two input revisions exist, then rcs guarantees that the gca will
   exist.  */

char *
gca (rev1, rev2)
    char *rev1;
    char *rev2;
{
    int dots;
    char gca[PATH_MAX];
    char *p[2];
    int j[2];

    if (rev1 == NULL || rev2 == NULL)
    {
	error (0, 0, "sanity failure in gca");
	abort();
    }

    /* walk the strings, reading the common parts. */
    gca[0] = '\0';
    p[0] = rev1;
    p[1] = rev2;
    do
    {
	int i;
	char c[2];
	char *s[2];
	
	for (i = 0; i < 2; ++i)
	{
	    /* swap out the dot */
	    s[i] = strchr (p[i], '.');
	    if (s[i] != NULL) {
		c[i] = *s[i];
	    }
	    
	    /* read an int */
	    j[i] = atoi (p[i]);
	    
	    /* swap back the dot... */
	    if (s[i] != NULL) {
		*s[i] = c[i];
		p[i] = s[i] + 1;
	    }
	    else
	    {
		/* or mark us at the end */
		p[i] = NULL;
	    }
	    
	}
	
	/* use the lowest. */
	(void) sprintf (gca + strlen (gca), "%d.",
			j[0] < j[1] ? j[0] : j[1]);

    } while (j[0] == j[1]
	     && p[0] != NULL
	     && p[1] != NULL);

    /* back up over that last dot. */
    gca[strlen(gca) - 1] = '\0';

    /* numbers differ, or we ran out of strings.  we're done with the
       common parts.  */

    dots = numdots (gca);
    if (dots == 0)
    {
	/* revisions differ in trunk major number.  */

	char *q;
	char *s;

	s = (j[0] < j[1]) ? p[0] : p[1];

	if (s == NULL)
	{
	    /* we only got one number.  this is strange.  */
	    error (0, 0, "bad revisions %s or %s", rev1, rev2);
	    abort();
	}
	else
	{
	    /* we have a minor number.  use it.  */
	    q = gca + strlen (gca);
	    
	    *q++ = '.';
	    for ( ; *s != '.' && *s != '\0'; )
		*q++ = *s++;
	    
	    *q = '\0';
	}
    }
    else if ((dots & 1) == 0)
    {
	/* if we have an even number of dots, then we have a branch.
	   remove the last number in order to make it a revision.  */
	
	char *s;

	s = strrchr(gca, '.');
	*s = '\0';
    }

    return (xstrdup (gca));
}

#ifdef LOSING_TMPNAM_FUNCTION
char *tmpnam(char *s)
{
    static char value[L_tmpnam+1];

    if (s){
       strcpy(s,"/tmp/cvsXXXXXX");
       mktemp(s);
       return s;
    }else{
       strcpy(value,"/tmp/cvsXXXXXX");
       mktemp(s);
       return value;
    }
}
#endif
