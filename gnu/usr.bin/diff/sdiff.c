/* SDIFF -- interactive merge front end to diff
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU DIFF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* GNU SDIFF was written by Thomas Lord. */

#include "system.h"
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include "getopt.h"

/* Size of chunks read from files which must be parsed into lines. */
#define SDIFF_BUFSIZE ((size_t) 65536)

/* Default name of the diff program */
#ifndef DIFF_PROGRAM
#define DIFF_PROGRAM "/usr/bin/diff"
#endif

/* Users' editor of nonchoice */
#ifndef DEFAULT_EDITOR
#define DEFAULT_EDITOR "ed"
#endif

extern char version_string[];
static char const *prog;
static char const *diffbin = DIFF_PROGRAM;
static char const *edbin = DEFAULT_EDITOR;

static char *tmpname;
static int volatile tmpmade;
static pid_t volatile diffpid;

struct line_filter;

static FILE *ck_fdopen PARAMS((int, char const *));
static FILE *ck_fopen PARAMS((char const *, char const *));
static RETSIGTYPE catchsig PARAMS((int));
static VOID *xmalloc PARAMS((size_t));
static char const *expand_name PARAMS((char *, int, char const *));
static int edit PARAMS((struct line_filter *, int, struct line_filter *, int, FILE*));
static int interact PARAMS((struct line_filter *, struct line_filter *, struct line_filter *, FILE*));
static int lf_snarf PARAMS((struct line_filter *, char *, size_t));
static int skip_white PARAMS((void));
static size_t ck_fread PARAMS((char *, size_t, FILE *));
static size_t lf_refill PARAMS((struct line_filter *));
static void checksigs PARAMS((void));
static void ck_fclose PARAMS((FILE *));
static void ck_fflush PARAMS((FILE *));
static void ck_fwrite PARAMS((char const *, size_t, FILE *));
static void cleanup PARAMS((void));
static void diffarg PARAMS((char const *));
static void execdiff PARAMS((int, char const *, char const *, char const *));
static void exiterr PARAMS((void));
static void fatal PARAMS((char const *));
static void flush_line PARAMS((void));
static void give_help PARAMS((void));
static void lf_copy PARAMS((struct line_filter *, int, FILE *));
static void lf_init PARAMS((struct line_filter *, FILE *));
static void lf_skip PARAMS((struct line_filter *, int));
static void perror_fatal PARAMS((char const *));
static void trapsigs PARAMS((void));
static void untrapsig PARAMS((int));
static void usage PARAMS((int));

/* this lossage until the gnu libc conquers the universe */
#define PVT_tmpdir "/tmp"
static char *private_tempnam PARAMS((char const *, char const *, int, size_t *));
static int diraccess PARAMS((char const *));
static int exists PARAMS((char const *));

/* Options: */

/* name of output file if -o spec'd */
static char *out_file;

/* do not print common lines if true, set by -s option */
static int suppress_common_flag;

static struct option const longopts[] =
{
  {"ignore-blank-lines", 0, 0, 'B'},
  {"speed-large-files", 0, 0, 'H'},
  {"ignore-matching-lines", 1, 0, 'I'},
  {"ignore-all-space", 0, 0, 'W'}, /* swap W and w for historical reasons */
  {"text", 0, 0, 'a'},
  {"ignore-space-change", 0, 0, 'b'},
  {"minimal", 0, 0, 'd'},
  {"ignore-case", 0, 0, 'i'},
  {"left-column", 0, 0, 'l'},
  {"output", 1, 0, 'o'},
  {"suppress-common-lines", 0, 0, 's'},
  {"expand-tabs", 0, 0, 't'},
  {"width", 1, 0, 'w'},
  {"version", 0, 0, 'v'},
  {"help", 0, 0, 129},
  {0, 0, 0, 0}
};

/* prints usage message and quits */
static void
usage (status)
     int status;
{
  printf ("Usage: %s [options] from-file to-file\n", prog);
  printf ("Options:\n\
	[-abBdHilstv] [-I regexp] [-o outfile] [-w columns]\n\
	[--expand-tabs] [--help] [--ignore-all-space] [--ignore-blank-lines]\n\
	[--ignore-case] [--ignore-matching-lines=regexp]\n\
	[--ignore-space-change] [--left-column] [--minimal]\n\
	[--output=outfile] [--speed-large-files] [--suppress-common-lines]\n\
	[--text] [--version] [--width=columns]\n");
  exit (status);
}

static void
cleanup ()
{
  if (0 < diffpid)
    kill (diffpid, SIGPIPE);
  if (tmpmade)
    unlink (tmpname);
}

static void
exiterr ()
{
  cleanup ();
  untrapsig (0);
  checksigs ();
  exit (2);
}

static void
fatal (msg)
     char const *msg;
{
  fprintf (stderr, "%s: %s\n", prog, msg);
  exiterr ();
}

static void
perror_fatal (msg)
     char const *msg;
{
  int e = errno;
  checksigs ();
  fprintf (stderr, "%s: ", prog);
  errno = e;
  perror (msg);
  exiterr ();
}


/* malloc freely or DIE! */
static VOID *
xmalloc (size)
     size_t size;
{
  VOID *r = (VOID *) malloc (size);
  if (!r)
    fatal ("memory exhausted");
  return r;
}

static FILE *
ck_fopen (fname, type)
     char const *fname, *type;
{
  FILE *r = fopen (fname, type);
  if (!r)
    perror_fatal (fname);
  return r;
}


static FILE *
ck_fdopen (fd, type)
     int fd;
     char const *type;
{
  FILE *r = fdopen (fd, type);
  if (!r)
    perror_fatal ("fdopen");
  return r;
}

static void
ck_fclose (f)
     FILE *f;
{
  if (fclose (f))
    perror_fatal ("input/output error");
}

static size_t
ck_fread (buf, size, f)
     char *buf;
     size_t size;
     FILE *f;
{
  size_t r = fread (buf, sizeof (char), size, f);
  if (r == 0 && ferror (f))
    perror_fatal ("input error");
  return r;
}

static void
ck_fwrite (buf, size, f)
     char const *buf;
     size_t size;
     FILE *f;
{
  if (fwrite (buf, sizeof (char), size, f) != size)
    perror_fatal ("output error");
}

static void
ck_fflush (f)
     FILE *f;
{
  if (fflush (f) != 0)
    perror_fatal ("output error");
}

#if !HAVE_MEMCHR
char *
memchr (s, c, n)
     char const *s;
     int c;
     size_t n;
{
  unsigned char const *p = (unsigned char const *) s, *lim = p + n;
  for (;  p < lim;  p++)
    if (*p == c)
      return (char *) p;
  return 0;
}
#endif

#ifndef HAVE_WAITPID
/* Emulate waitpid well enough for sdiff, which has at most two children.  */
static pid_t
waitpid (pid, stat_loc, options)
     pid_t pid;
     int *stat_loc;
     int options;
{
  static int ostatus;
  static pid_t opid;
  int npid, status;

  if (pid == opid)
    {
      opid = 0;
      status = ostatus;
    }
  else
    while ((npid = wait (&status)) != pid)
      {
	if (npid < 0)
	  return npid;
	opid = npid;
	ostatus = status;
      }
  *stat_loc = status;
  return pid;
}
#endif

static char const *
expand_name (name, isdir, other_name)
     char *name;
     int isdir;
     char const *other_name;
{
  if (strcmp (name, "-") == 0)
    fatal ("cannot interactively merge standard input");
  if (!isdir)
    return name;
  else
    {
      /* Yield NAME/BASE, where BASE is OTHER_NAME's basename.  */
      char const
	*p = strrchr (other_name, '/'),
	*base = p ? p+1 : other_name;
      size_t namelen = strlen (name), baselen = strlen (base);
      char *r = xmalloc (namelen + baselen + 2);
      memcpy (r, name, namelen);
      r[namelen] = '/';
      memcpy (r + namelen + 1, base, baselen + 1);
      return r;
    }
}



struct line_filter {
  FILE *infile;
  char *bufpos;
  char *buffer;
  char *buflim;
};

static void
lf_init (lf, infile)
     struct line_filter *lf;
     FILE *infile;
{
  lf->infile = infile;
  lf->bufpos = lf->buffer = lf->buflim = xmalloc (SDIFF_BUFSIZE + 1);
  lf->buflim[0] = '\n';
}

/* Fill an exhausted line_filter buffer from its INFILE */
static size_t
lf_refill (lf)
     struct line_filter *lf;
{
  size_t s = ck_fread (lf->buffer, SDIFF_BUFSIZE, lf->infile);
  lf->bufpos = lf->buffer;
  lf->buflim = lf->buffer + s;
  lf->buflim[0] = '\n';
  checksigs ();
  return s;
}

/* Advance LINES on LF's infile, copying lines to OUTFILE */
static void
lf_copy (lf, lines, outfile)
     struct line_filter *lf;
     int lines;
     FILE *outfile;
{
  char *start = lf->bufpos;

  while (lines)
    {
      lf->bufpos = (char *) memchr (lf->bufpos, '\n', lf->buflim - lf->bufpos);
      if (! lf->bufpos)
	{
	  ck_fwrite (start, lf->buflim - start, outfile);
	  if (! lf_refill (lf))
	    return;
	  start = lf->bufpos;
	}
      else
	{
	  --lines;
	  ++lf->bufpos;
	}
    }

  ck_fwrite (start, lf->bufpos - start, outfile);
}

/* Advance LINES on LF's infile without doing output */
static void
lf_skip (lf, lines)
     struct line_filter *lf;
     int lines;
{
  while (lines)
    {
      lf->bufpos = (char *) memchr (lf->bufpos, '\n', lf->buflim - lf->bufpos);
      if (! lf->bufpos)
	{
	  if (! lf_refill (lf))
	    break;
	}
      else
	{
	  --lines;
	  ++lf->bufpos;
	}
    }
}

/* Snarf a line into a buffer.  Return EOF if EOF, 0 if error, 1 if OK.  */
static int
lf_snarf (lf, buffer, bufsize)
     struct line_filter *lf;
     char *buffer;
     size_t bufsize;
{
  char *start = lf->bufpos;

  for (;;)
    {
      char *next = (char *) memchr (start, '\n', lf->buflim + 1 - start);
      size_t s = next - start;
      if (bufsize <= s)
	return 0;
      memcpy (buffer, start, s);
      if (next < lf->buflim)
	{
	  buffer[s] = 0;
	  lf->bufpos = next + 1;
	  return 1;
	}
      if (! lf_refill (lf))
	return s ? 0 : EOF;
      buffer += s;
      bufsize -= s;
      start = next;
    }
}



int
main (argc, argv)
     int argc;
     char *argv[];
{
  int opt;
  char *editor = getenv ("EDITOR");
  char *differ = getenv ("DIFF");

  prog = argv[0];
  if (editor)
    edbin = editor;
  if (differ)
    diffbin = differ;

  diffarg ("diff");

  /* parse command line args */
  while ((opt = getopt_long (argc, argv, "abBdHiI:lo:stvw:W", longopts, 0))
	 != EOF)
    {
      switch (opt)
	{
	case 'a':
	  diffarg ("-a");
	  break;

	case 'b':
	  diffarg ("-b");
	  break;

	case 'B':
	  diffarg ("-B");
	  break;

	case 'd':
	  diffarg ("-d");
	  break;

	case 'H':
	  diffarg ("-H");
	  break;

	case 'i':
	  diffarg ("-i");
	  break;

	case 'I':
	  diffarg ("-I");
	  diffarg (optarg);
	  break;

	case 'l':
	  diffarg ("--left-column");
	  break;

	case 'o':
	  out_file = optarg;
	  break;

	case 's':
	  suppress_common_flag = 1;
	  break;

	case 't':
	  diffarg ("-t");
	  break;

	case 'v':
	  printf ("GNU sdiff version %s\n", version_string);
	  exit (0);

	case 'w':
	  diffarg ("-W");
	  diffarg (optarg);
	  break;

	case 'W':
	  diffarg ("-w");
	  break;

	case 129:
	  usage (0);

	default:
	  usage (2);
	}
    }

  if (argc - optind != 2)
    usage (2);

  if (! out_file)
    /* easy case: diff does everything for us */
    execdiff (suppress_common_flag, "-y", argv[optind], argv[optind + 1]);
  else
    {
      FILE *left, *right, *out, *diffout;
      int diff_fds[2];
      int interact_ok;
      pid_t pid;
      struct line_filter lfilt;
      struct line_filter rfilt;
      struct line_filter diff_filt;
      int leftdir = diraccess (argv[optind]);
      int rightdir = diraccess (argv[optind + 1]);

      if (leftdir && rightdir)
	fatal ("both files to be compared are directories");

      left = ck_fopen (expand_name (argv[optind], leftdir, argv[optind + 1]), "r");
      ;
      right = ck_fopen (expand_name (argv[optind + 1], rightdir, argv[optind]), "r");
      out = ck_fopen (out_file, "w");

      if (pipe (diff_fds))
	perror_fatal ("pipe");

      trapsigs ();

      diffpid = pid = vfork ();

      if (pid == 0)
	{
	  signal (SIGINT, SIG_IGN);  /* in case user interrupts editor */
	  signal (SIGPIPE, SIG_DFL);

	  close (diff_fds[0]);
	  if (diff_fds[1] != STDOUT_FILENO)
	    {
	      dup2 (diff_fds[1], STDOUT_FILENO);
	      close (diff_fds[1]);
	    }

	  execdiff (0, "--sdiff-merge-assist", argv[optind], argv[optind + 1]);
	}

      if (pid < 0)
	perror_fatal ("fork failed");

      close (diff_fds[1]);
      diffout = ck_fdopen (diff_fds[0], "r");

      lf_init (&diff_filt, diffout);
      lf_init (&lfilt, left);
      lf_init (&rfilt, right);

      interact_ok = interact (&diff_filt, &lfilt, &rfilt, out);

      ck_fclose (diffout);
      ck_fclose (left);
      ck_fclose (right);
      ck_fclose (out);

      {
	int wstatus;

	while (waitpid (pid, &wstatus, 0) < 0)
	  if (errno == EINTR)
	    checksigs ();
	  else
	    perror_fatal ("wait failed");
	diffpid = 0;

	if (tmpmade)
	  {
	    unlink (tmpname);
	    tmpmade = 0;
	  }

	if (! interact_ok)
	  exiterr ();

	if (! (WIFEXITED (wstatus) && WEXITSTATUS (wstatus) < 2))
	  fatal ("Subsidiary diff failed");

	untrapsig (0);
	checksigs ();
	exit (WEXITSTATUS (wstatus));
      }
    }
  return 0;			/* Fool -Wall . . . */
}

static char const **diffargv;

static void
diffarg (a)
     char const *a;
{
  static unsigned diffargs, diffargsmax;

  if (diffargs == diffargsmax)
    {
      if (! diffargsmax)
	{
	  diffargv = (char const **) xmalloc (sizeof (char));
	  diffargsmax = 8;
	}
      diffargsmax *= 2;
      diffargv = (char const **) realloc (diffargv,
					  diffargsmax * sizeof (char const *));
      if (! diffargv)
	fatal ("out of memory");
    }
  diffargv[diffargs++] = a;
}

static void
execdiff (differences_only, option, file1, file2)
     int differences_only;
     char const *option, *file1, *file2;
{
  if (differences_only)
    diffarg ("--suppress-common-lines");
  diffarg (option);
  diffarg ("--");
  diffarg (file1);
  diffarg (file2);
  diffarg (0);

  execvp (diffbin, (char **) diffargv);
  write (STDERR_FILENO, diffbin, strlen (diffbin));
  write (STDERR_FILENO, ": not found\n", 12);
  _exit (2);
}




/* Signal handling */

#define NUM_SIGS (sizeof (sigs) / sizeof (*sigs))
static int const sigs[] = {
#ifdef SIGHUP
       SIGHUP,
#endif
#ifdef SIGQUIT
       SIGQUIT,
#endif
#ifdef SIGTERM
       SIGTERM,
#endif
#ifdef SIGXCPU
       SIGXCPU,
#endif
#ifdef SIGXFSZ
       SIGXFSZ,
#endif
       SIGINT,
       SIGPIPE
};

/* Prefer `sigaction' if it is available, since `signal' can lose signals.  */
#if HAVE_SIGACTION
static struct sigaction initial_action[NUM_SIGS];
#define initial_handler(i) (initial_action[i].sa_handler)
#else
static RETSIGTYPE (*initial_action[NUM_SIGS]) ();
#define initial_handler(i) (initial_action[i])
#endif

static int volatile ignore_SIGINT;
static int volatile signal_received;
static int sigs_trapped;

static RETSIGTYPE
catchsig (s)
     int s;
{
#if ! HAVE_SIGACTION
  signal (s, SIG_IGN);
#endif
  if (! (s == SIGINT && ignore_SIGINT))
    signal_received = s;
}

static void
trapsigs ()
{
  int i;

#if HAVE_SIGACTION
  struct sigaction catchaction;
  bzero (&catchaction, sizeof (catchaction));
  catchaction.sa_handler = catchsig;
#ifdef SA_INTERRUPT
  /* Non-Posix BSD-style systems like SunOS 4.1.x need this
     so that `read' calls are interrupted properly.  */
  catchaction.sa_flags = SA_INTERRUPT;
#endif
  sigemptyset (&catchaction.sa_mask);
  for (i = 0;  i < NUM_SIGS;  i++)
    sigaddset (&catchaction.sa_mask, sigs[i]);
  for (i = 0;  i < NUM_SIGS;  i++)
    {
      sigaction (sigs[i], 0, &initial_action[i]);
      if (initial_handler (i) != SIG_IGN
	  && sigaction (sigs[i], &catchaction, 0) != 0)
	fatal ("signal error");
    }
#else /* ! HAVE_SIGACTION */
  for (i = 0;  i < NUM_SIGS;  i++)
    {
      initial_action[i] = signal (sigs[i], SIG_IGN);
      if (initial_handler (i) != SIG_IGN
	  && signal (sigs[i], catchsig) != SIG_IGN)
	fatal ("signal error");
    }
#endif /* ! HAVE_SIGACTION */
  sigs_trapped = 1;
}

/* Untrap signal S, or all trapped signals if S is zero.  */
static void
untrapsig (s)
     int s;
{
  int i;

  if (sigs_trapped)
    for (i = 0;  i < NUM_SIGS;  i++)
      if ((!s || sigs[i] == s)  &&  initial_handler (i) != SIG_IGN)
#if HAVE_SIGACTION
	  sigaction (sigs[i], &initial_action[i], 0);
#else
	  signal (sigs[i], initial_action[i]);
#endif
}

/* Exit if a signal has been received.  */
static void
checksigs ()
{
  int s = signal_received;
  if (s)
    {
      cleanup ();

      /* Yield an exit status indicating that a signal was received.  */
      untrapsig (s);
      kill (getpid (), s);

      /* That didn't work, so exit with error status.  */
      exit (2);
    }
}



static void
give_help ()
{
  fprintf (stderr,"l:\tuse the left version\n");
  fprintf (stderr,"r:\tuse the right version\n");
  fprintf (stderr,"e l:\tedit then use the left version\n");
  fprintf (stderr,"e r:\tedit then use the right version\n");
  fprintf (stderr,"e b:\tedit then use the left and right versions concatenated\n");
  fprintf (stderr,"e:\tedit a new version\n");
  fprintf (stderr,"s:\tsilently include common lines\n");
  fprintf (stderr,"v:\tverbosely include common lines\n");
  fprintf (stderr,"q:\tquit\n");
}

static int
skip_white ()
{
  int c;
  while (isspace (c = getchar ()) && c != '\n')
    checksigs ();
  if (ferror (stdin))
    perror_fatal ("input error");
  return c;
}

static void
flush_line ()
{
  int c;
  while ((c = getchar ()) != '\n' && c != EOF)
    ;
  if (ferror (stdin))
    perror_fatal ("input error");
}


/* interpret an edit command */
static int
edit (left, lenl, right, lenr, outfile)
     struct line_filter *left;
     int lenl;
     struct line_filter *right;
     int lenr;
     FILE *outfile;
{
  for (;;)
    {
      int cmd0, cmd1;
      int gotcmd = 0;

      cmd1 = 0; /* Pacify `gcc -W'.  */

      while (!gotcmd)
	{
	  if (putchar ('%') != '%')
	    perror_fatal ("output error");
	  ck_fflush (stdout);

	  cmd0 = skip_white ();
	  switch (cmd0)
	    {
	    case 'l': case 'r': case 's': case 'v': case 'q':
	      if (skip_white () != '\n')
		{
		  give_help ();
		  flush_line ();
		  continue;
		}
	      gotcmd = 1;
	      break;

	    case 'e':
	      cmd1 = skip_white ();
	      switch (cmd1)
		{
		case 'l': case 'r': case 'b':
		  if (skip_white () != '\n')
		    {
		      give_help ();
		      flush_line ();
		      continue;
		    }
		  gotcmd = 1;
		  break;
		case '\n':
		  gotcmd = 1;
		  break;
		default:
		  give_help ();
		  flush_line ();
		  continue;
		}
	      break;
	    case EOF:
	      if (feof (stdin))
		{
		  gotcmd = 1;
		  cmd0 = 'q';
		  break;
		}
	      /* falls through */
	    default:
	      flush_line ();
	      /* falls through */
	    case '\n':
	      give_help ();
	      continue;
	    }
	}

      switch (cmd0)
	{
	case 'l':
	  lf_copy (left, lenl, outfile);
	  lf_skip (right, lenr);
	  return 1;
	case 'r':
	  lf_copy (right, lenr, outfile);
	  lf_skip (left, lenl);
	  return 1;
	case 's':
	  suppress_common_flag = 1;
	  break;
	case 'v':
	  suppress_common_flag = 0;
	  break;
	case 'q':
	  return 0;
	case 'e':
	  if (! tmpname && ! (tmpname = private_tempnam (0, "sdiff", 1, 0)))
	    perror_fatal ("temporary file name");

	  tmpmade = 1;

	  {
	    FILE *tmp = ck_fopen (tmpname, "w+");

	    if (cmd1 == 'l' || cmd1 == 'b')
	      lf_copy (left, lenl, tmp);
	    else
	      lf_skip (left, lenl);

	    if (cmd1 == 'r' || cmd1 == 'b')
	      lf_copy (right, lenr, tmp);
	    else
	      lf_skip (right, lenr);

	    ck_fflush (tmp);

	    {
	      pid_t pid;
	      int wstatus;

	      ignore_SIGINT = 1;
	      checksigs ();

	      pid = vfork ();
	      if (pid == 0)
		{
		  char const *argv[3];
		  int i = 0;

		  argv[i++] = edbin;
		  argv[i++] = tmpname;
		  argv[i++] = 0;

		  execvp (edbin, (char **) argv);
		  write (STDERR_FILENO, edbin, strlen (edbin));
		  write (STDERR_FILENO, ": not found\n", 12);
		  _exit (1);
		}

	      if (pid < 0)
		perror_fatal ("fork failed");

	      while (waitpid (pid, &wstatus, 0) < 0)
		if (errno == EINTR)
		  checksigs ();
		else
		  perror_fatal ("wait failed");

	      ignore_SIGINT = 0;

	      if (! (WIFEXITED (wstatus) && WEXITSTATUS (wstatus) < 1))
		fatal ("Subsidiary editor failed");
	    }

	    if (fseek (tmp, 0L, SEEK_SET) != 0)
	      perror_fatal ("fseek");
	    {
	      /* SDIFF_BUFSIZE is too big for a local var
		 in some compilers, so we allocate it dynamically.  */
	      char *buf = xmalloc (SDIFF_BUFSIZE);
	      size_t size;

	      while ((size = ck_fread (buf, SDIFF_BUFSIZE, tmp)) != 0)
		{
		  checksigs ();
		  ck_fwrite (buf, size, outfile);
		}
	      ck_fclose (tmp);

	      free (buf);
	    }
	    return 1;
	  }
	default:
	  give_help ();
	  break;
	}
    }
}



/* Alternately reveal bursts of diff output and handle user commands.  */
static int
interact (diff, left, right, outfile)
     struct line_filter *diff;
     struct line_filter *left;
     struct line_filter *right;
     FILE *outfile;
{
  for (;;)
    {
      char diff_help[256];
      int snarfed = lf_snarf (diff, diff_help, sizeof (diff_help));

      if (snarfed <= 0)
	return snarfed;

      checksigs ();

      switch (diff_help[0])
	{
	case ' ':
	  puts (diff_help + 1);
	  break;
	case 'i':
	  {
	    int lenl = atoi (diff_help + 1), lenr, lenmax;
	    char *p = strchr (diff_help, ',');

	    if (!p)
	      fatal (diff_help);
	    lenr = atoi (p + 1);
	    lenmax = max (lenl, lenr);

	    if (suppress_common_flag)
	      lf_skip (diff, lenmax);
	    else
	      lf_copy (diff, lenmax, stdout);

	    lf_copy (left, lenl, outfile);
	    lf_skip (right, lenr);
	    break;
	  }
	case 'c':
	  {
	    int lenl = atoi (diff_help + 1), lenr;
	    char *p = strchr (diff_help, ',');

	    if (!p)
	      fatal (diff_help);
	    lenr = atoi (p + 1);
	    lf_copy (diff, max (lenl, lenr), stdout);
	    if (! edit (left, lenl, right, lenr, outfile))
	      return 0;
	    break;
	  }
	default:
	  fatal (diff_help);
	  break;
	}
    }
}



/* temporary lossage: this is torn from gnu libc */
/* Return nonzero if DIR is an existing directory.  */
static int
diraccess (dir)
     char const *dir;
{
  struct stat buf;
  return stat (dir, &buf) == 0 && S_ISDIR (buf.st_mode);
}

/* Return nonzero if FILE exists.  */
static int
exists (file)
     char const *file;
{
  struct stat buf;
  return stat (file, &buf) == 0;
}

/* These are the characters used in temporary filenames.  */
static char const letters[] =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* Generate a temporary filename.
   If DIR_SEARCH is nonzero, DIR and PFX are used as
   described for tempnam.  If not, a temporary filename
   in P_tmpdir with no special prefix is generated.  If LENPTR
   is not 0, *LENPTR is set the to length (including the
   terminating '\0') of the resultant filename, which is returned.
   This goes through a cyclic pattern of all possible filenames
   consisting of five decimal digits of the current pid and three
   of the characters in `letters'.  Data for tempnam and tmpnam
   is kept separate, but when tempnam is using P_tmpdir and no
   prefix (i.e, it is identical to tmpnam), the same data is used.
   Each potential filename is tested for an already-existing file of
   the same name, and no name of an existing file will be returned.
   When the cycle reaches its end (12345ZZZ), 0 is returned.  */


static char *
private_tempnam (dir, pfx, dir_search, lenptr)
     char const *dir;
     char const *pfx;
     int dir_search;
     size_t *lenptr;
{
  static char const tmpdir[] = PVT_tmpdir;
  static struct
    {
      char buf[3];
      char *s;
      size_t i;
    } infos[2], *info;
  static char *buf;
  static size_t bufsize = 1;
  static pid_t oldpid = 0;
  pid_t pid = getpid ();
  register size_t len, plen;

  if (dir_search)
    {
      register char const *d = getenv ("TMPDIR");
      if (d && !diraccess (d))
	d = 0;
      if (!d && dir && diraccess (dir))
	d = dir;
      if (!d && diraccess (tmpdir))
	d = tmpdir;
      if (!d && diraccess ("/tmp"))
	d = "/tmp";
      if (!d)
	{
	  errno = ENOENT;
	  return 0;
	}
      dir = d;
    }
  else
    dir = tmpdir;

  if (pfx && *pfx)
    {
      plen = strlen (pfx);
      if (plen > 5)
	plen = 5;
    }
  else
    plen = 0;

  if (dir != tmpdir && !strcmp (dir, tmpdir))
    dir = tmpdir;
  info = &infos[(plen == 0 && dir == tmpdir) ? 1 : 0];

  if (pid != oldpid)
    {
      oldpid = pid;
      info->buf[0] = info->buf[1] = info->buf[2] = '0';
      info->s = &info->buf[0];
      info->i = 0;
    }

  len = strlen (dir) + 1 + plen + 8;
  if (bufsize <= len)
    {
      do
	{
	  bufsize *= 2;
	}
      while (bufsize <= len);

      if (buf)
	free (buf);
      buf = xmalloc (bufsize);
    }
  for (;;)
    {
      *info->s = letters[info->i];
      sprintf (buf, "%s/%.*s%.5lu%.3s", dir, (int) plen, pfx,
	       (unsigned long) pid % 100000, info->buf);
      if (!exists (buf))
	break;
      ++info->i;
      if (info->i > sizeof (letters) - 1)
	{
	  info->i = 0;
	  if (info->s == &info->buf[2])
	    {
	      errno = EEXIST;
	      return 0;
	    }
	  ++info->s;
	}
    }

  if (lenptr)
    *lenptr = len;
  return buf;
}
