/* SDIFF -- interactive merge front end to diff
   Copyright (C) 1992, 1993, 1994 Free Software Foundation, Inc.

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

#ifndef lint
static const char rcsid[] =
"$FreeBSD$";
#endif

#include "system.h"
#include <stdio.h>
#include <signal.h>
#include "getopt.h"

/* Size of chunks read from files which must be parsed into lines. */
#define SDIFF_BUFSIZE ((size_t) 65536)

/* Default name of the diff program */
#ifndef DIFF_PROGRAM
#define DIFF_PROGRAM "/usr/bin/diff"
#endif

/* Users' editor of nonchoice */
#ifndef DEFAULT_EDITOR_PROGRAM
#define DEFAULT_EDITOR_PROGRAM "ed"
#endif

extern char version_string[];
static char const *program_name;
static char const *diffbin = DIFF_PROGRAM;
static char const *edbin = DEFAULT_EDITOR_PROGRAM;
static char const **diffargv;

static char *tmpname;
static int volatile tmpmade;

#if HAVE_FORK
static pid_t volatile diffpid;
#endif

struct line_filter;

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
static void execdiff PARAMS((void));
static void exiterr PARAMS((void));
static void fatal PARAMS((char const *));
static void flush_line PARAMS((void));
static void give_help PARAMS((void));
static void lf_copy PARAMS((struct line_filter *, int, FILE *));
static void lf_init PARAMS((struct line_filter *, FILE *));
static void lf_skip PARAMS((struct line_filter *, int));
static void perror_fatal PARAMS((char const *));
static void trapsigs PARAMS((void));
static void try_help PARAMS((char const *));
static void untrapsig PARAMS((int));
static void usage PARAMS((void));

static int diraccess PARAMS((char const *));

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

static void
try_help (reason)
     char const *reason;
{
  if (reason)
    fprintf (stderr, "%s: %s\n", program_name, reason);
  fprintf (stderr, "%s: Try `%s --help' for more information.\n",
	   program_name, program_name);
  exit (2);
}

static void
usage ()
{
  printf ("Usage: %s [OPTIONS]... FILE1 FILE2\n\n", program_name);
  printf ("%s", "\
  -o FILE  --output=FILE  Operate interactively, sending output to FILE.\n\n");
  printf ("%s", "\
  -i  --ignore-case  Consider upper- and lower-case to be the same.\n\
  -W  --ignore-all-space  Ignore all white space.\n\
  -b  --ignore-space-change  Ignore changes in the amount of white space.\n\
  -B  --ignore-blank-lines  Ignore changes whose lines are all blank.\n\
  -I RE  --ignore-matching-lines=RE  Ignore changes whose lines all match RE.\n\
  -a  --text  Treat all files as text.\n\n");
  printf ("%s", "\
  -w NUM  --width=NUM  Output at most NUM (default 130) characters per line.\n\
  -l  --left-column  Output only the left column of common lines.\n\
  -s  --suppress-common-lines  Do not output common lines.\n\n");
  printf ("\
  -t  --expand-tabs  Expand tabs to spaces in output.\n\n");
  printf ("%s", "\
  -d  --minimal  Try hard to find a smaller set of changes.\n\
  -H  --speed-large-files  Assume large files and many scattered small changes.\n\n");
 printf ("%s", "\
  -v  --version  Output version info.\n\
  --help  Output this help.\n\n\
If FILE1 or FILE2 is `-', read standard input.\n");
}

static void
cleanup ()
{
#if HAVE_FORK
  if (0 < diffpid)
    kill (diffpid, SIGPIPE);
#endif
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
  fprintf (stderr, "%s: %s\n", program_name, msg);
  exiterr ();
}

static void
perror_fatal (msg)
     char const *msg;
{
  int e = errno;
  checksigs ();
  fprintf (stderr, "%s: ", program_name);
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

static char const *
expand_name (name, is_dir, other_name)
     char *name;
     int is_dir;
     char const *other_name;
{
  if (strcmp (name, "-") == 0)
    fatal ("cannot interactively merge standard input");
  if (!is_dir)
    return name;
  else
    {
      /* Yield NAME/BASE, where BASE is OTHER_NAME's basename.  */
      char const *p = filename_lastdirchar (other_name);
      char const *base = p ? p+1 : other_name;
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
  char *editor;
  char *differ;

  initialize_main (&argc, &argv);
  program_name = argv[0];

  editor = getenv ("EDITOR");
  if (editor)
    edbin = editor;
  differ = getenv ("DIFF");
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
	  printf ("sdiff - GNU diffutils version %s\n", version_string);
	  exit (0);

	case 'w':
	  diffarg ("-W");
	  diffarg (optarg);
	  break;

	case 'W':
	  diffarg ("-w");
	  break;

	case 129:
	  usage ();
	  if (ferror (stdout) || fclose (stdout) != 0)
	    fatal ("write error");
	  exit (0);

	default:
	  try_help (0);
	}
    }

  if (argc - optind != 2)
    try_help (argc - optind < 2 ? "missing operand" : "extra operand");

  if (! out_file)
    {
      /* easy case: diff does everything for us */
      if (suppress_common_flag)
	diffarg ("--suppress-common-lines");
      diffarg ("-y");
      diffarg ("--");
      diffarg (argv[optind]);
      diffarg (argv[optind + 1]);
      diffarg (0);
      execdiff ();
    }
  else
    {
      FILE *left, *right, *out, *diffout;
      int interact_ok;
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

      diffarg ("--sdiff-merge-assist");
      diffarg ("--");
      diffarg (argv[optind]);
      diffarg (argv[optind + 1]);
      diffarg (0);

      trapsigs ();

#if ! HAVE_FORK
      {
	size_t cmdsize = 1;
	char *p, *command;
	int i;

	for (i = 0;  diffargv[i];  i++)
	  cmdsize += 4 * strlen (diffargv[i]) + 3;
	command = p = xmalloc (cmdsize);
	for (i = 0;  diffargv[i];  i++)
	  {
	    char const *a = diffargv[i];
	    SYSTEM_QUOTE_ARG (p, a);
	    *p++ = ' ';
	  }
	p[-1] = '\0';
	diffout = popen (command, "r");
	if (!diffout)
	  perror_fatal (command);
	free (command);
      }
#else /* HAVE_FORK */
      {
	int diff_fds[2];

	if (pipe (diff_fds) != 0)
	  perror_fatal ("pipe");

	diffpid = vfork ();
	if (diffpid < 0)
	  perror_fatal ("fork failed");
	if (!diffpid)
	  {
	    signal (SIGINT, SIG_IGN);  /* in case user interrupts editor */
	    signal (SIGPIPE, SIG_DFL);

	    close (diff_fds[0]);
	    if (diff_fds[1] != STDOUT_FILENO)
	      {
		dup2 (diff_fds[1], STDOUT_FILENO);
		close (diff_fds[1]);
	      }

	    execdiff ();
	  }

	close (diff_fds[1]);
	diffout = fdopen (diff_fds[0], "r");
	if (!diffout)
	  perror_fatal ("fdopen");
      }
#endif /* HAVE_FORK */

      lf_init (&diff_filt, diffout);
      lf_init (&lfilt, left);
      lf_init (&rfilt, right);

      interact_ok = interact (&diff_filt, &lfilt, &rfilt, out);

      ck_fclose (left);
      ck_fclose (right);
      ck_fclose (out);

      {
	int wstatus;

#if ! HAVE_FORK
	wstatus = pclose (diffout);
#else
	ck_fclose (diffout);
	while (waitpid (diffpid, &wstatus, 0) < 0)
	  if (errno == EINTR)
	    checksigs ();
	  else
	    perror_fatal ("wait failed");
	diffpid = 0;
#endif

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
execdiff ()
{
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

#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD SIGCLD
#endif
#ifdef SIGCHLD
  /* System V fork+wait does not work if SIGCHLD is ignored.  */
  signal (SIGCHLD, SIG_DFL);
#endif

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
  for (;;)
    {
      c = getchar ();
      if (!ISSPACE (c) || c == '\n')
	break;
      checksigs ();
    }
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
	  {
	    int tfd;
	    FILE *tmp;

	    if (tmpmade)
	      {
	        unlink (tmpname);
	        tmpmade = 0;
		free (tmpname);
	      }

	    asprintf (&tmpname, "%s/sdiff.XXXXXX",
	      getenv("TMPDIR") ?: P_tmpdir);
	    if (tmpname == NULL)
	      perror_fatal ("temporary file name");
	    tfd = mkstemp(tmpname);
	    if (tfd == -1)
	      perror_fatal ("temporary file name");
	    tmp = fdopen (tfd, "w+");
	    if (tmp == NULL)
	      perror_fatal ("temporary file name");
 
	    tmpmade = 1;

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
	      int wstatus;
#if ! HAVE_FORK
	      char *command = xmalloc (strlen (edbin) + strlen (tmpname) + 2);
	      sprintf (command, "%s %s", edbin, tmpname);
	      wstatus = system (command);
	      free (command);
#else /* HAVE_FORK */
	      pid_t pid;

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
#endif /* HAVE_FORK */

	      if (wstatus != 0)
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
