/* SDIFF -- interactive merge front end to diff
   Copyright (C) 1992 Free Software Foundation, Inc.

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

#include <stdio.h>
#include <ctype.h>
#include "system.h"
#include <signal.h>
#include "getopt.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

/* Size of chunks read from files which must be parsed into lines. */
#define SDIFF_BUFSIZE 65536

/* Default name of the diff program */
#ifndef DIFF_PROGRAM
#define DIFF_PROGRAM "/usr/bin/diff"
#endif

/* Users' editor of nonchoice */
#ifndef DEFAULT_EDITOR
#define DEFAULT_EDITOR "ed"
#endif

extern char *version_string;
static char const *prog;
static char const *diffbin = DIFF_PROGRAM;
static char const *edbin = DEFAULT_EDITOR;

static char *tmpname;
static int volatile tmpmade;
static pid_t volatile diffpid;

struct line_filter;
static void diffarg (); /* (char *); */
static void execdiff (); /* (int, char const *, char const *, char const *); */
static int edit (); /* (struct line_filter *left, int lenl, struct
		       line_filter *right, int lenr, FILE *outfile); */
static int interact (); /* (struct line_filter *diff,
			  struct line_filter *left,
			  struct line_filter *right, FILE *outfile); */
static void trapsigs (); /* (void); */
/* this lossage until the gnu libc conquers the universe */
#define TMPNAMSIZE 1024
#define PVT_tmpdir "/tmp"
static char *private_tempnam (); /* (const char *, const char *, int, int *); */
static int diraccess ();

/* Options: */

/* name of output file if -o spec'd */
static char *out_file;

/* do not print common lines if true, set by -s option */
static int suppress_common_flag;

static struct option longopts[] =
{
  {"ignore-blank-lines", 0, NULL, 'B'},
  {"speed-large-files", 0, NULL, 'H'},
  {"ignore-matching-lines", 1, NULL, 'I'},
  {"ignore-all-space", 0, NULL, 'W'}, /* swap W and w for historical reasons */
  {"text", 0, NULL, 'a'},
  {"ignore-space-change", 0, NULL, 'b'},
  {"minimal", 0, NULL, 'd'},
  {"ignore-case", 0, NULL, 'i'},
  {"left-column", 0, NULL, 'l'},
  {"output", 1, NULL, 'o'},
  {"suppress-common-lines", 0, NULL, 's'},
  {"expand-tabs", 0, NULL, 't'},
  {"width", 1, NULL, 'w'},
  {"version", 0, NULL, 'v'},
  {NULL, 0, NULL, 0}
};

/* prints usage message and quits */
static void
usage ()
{
  fprintf (stderr, "Usage: %s [options] from-file to-file\n", prog);
  fprintf (stderr, "Options:\n\
       [-abBdHilstv] [-I regexp] [-o outfile] [-w columns]\n\
       [--text] [--minimal] [--speed-large-files] [--expand-tabs]\n\
       [--ignore-case] [--ignore-matching-lines=regexp]\n\
       [--ignore-space-change] [--ignore-blank-lines] [--ignore-all-space]\n\
       [--suppress-common-lines] [--left-column] [--output=outfile]\n\
       [--version] [--width=columns]\n");
  exit (2);
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
  exit (2);
}

static void
fatal (msg)
     char *msg;
{
  fprintf (stderr, "%s: %s\n", prog, msg);
  exiterr ();
}

static void
perror_fatal (msg)
     char *msg;
{
  int e = errno;
  fprintf (stderr, "%s: ", prog);
  errno = e;
  perror (msg);
  exiterr ();
}


/* malloc freely or DIE! */
char *
xmalloc (size)
     size_t size;
{
  char *r = malloc (size);
  if (!r)
    fatal ("virtual memory exhausted");
  return r;
}

static FILE *
ck_fopen (fname, type)
     char *fname, *type;
{
  FILE *r = fopen (fname, type);
  if (!r)
    perror_fatal (fname);
  return r;
}


static FILE *
ck_fdopen (fd, type)
     int fd;
     char *type;
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
     char *buf;
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
     char *s;
     int c;
     size_t n;
{
  unsigned char *p = (unsigned char *) s, *lim = p + n;
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
      const char
	*p = rindex (other_name, '/'),
	*base = p ? p+1 : other_name;
      size_t namelen = strlen (name), baselen = strlen (base);
      char *r = xmalloc (namelen + baselen + 2);
      bcopy (name, r, namelen);
      r[namelen] = '/';
      bcopy (base, r + namelen + 1, baselen + 1);
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
      lf->bufpos = memchr (lf->bufpos, '\n', lf->buflim - lf->bufpos);
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
      lf->bufpos = memchr (lf->bufpos, '\n', lf->buflim - lf->bufpos);
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
      char *next = memchr (start, '\n', lf->buflim + 1 - start);
      size_t s = next - start;
      if (bufsize <= s)
	return 0;
      bcopy (start, buffer, s);
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
  int version_requested = 0;
  char *editor = getenv ("EDITOR");
  char *differ = getenv ("DIFF");

  prog = argv[0];
  if (editor)
    edbin = editor;
  if (differ)
    diffbin = differ;

  diffarg ("diff");

  /* parse command line args */
  while ((opt=getopt_long (argc, argv, "abBdHiI:lo:stvw:W", longopts, (int *)0)) != EOF)
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
	  version_requested = 1;
	  fprintf (stderr, "GNU sdiff version %s\n", version_string);
	  ck_fflush (stderr);
	  break;

	case 'w':
	  diffarg ("-W");
	  diffarg (optarg);
	  break;

	case 'W':
	  diffarg ("-w");
	  break;

	default:
	  usage ();
	}
    }

  /* check: did user just want version message? if so exit. */
  if (version_requested && argc - optind == 0)
    exit (0);

  if (argc - optind != 2)
    usage ();

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
	  if (diff_fds[1] != fileno (stdout))
	    {
	      dup2 (diff_fds[1], fileno (stdout));
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

	if (waitpid (pid, &wstatus, 0) < 0)
	  perror_fatal ("wait failed");
	diffpid = 0;

	if (tmpmade)
	  {
	    unlink (tmpname);
	    tmpmade = 0;
	  }

	if (! interact_ok)
	  exit (2);

	if (! (WIFEXITED (wstatus) && WEXITSTATUS (wstatus) < 2))
	  fatal ("Subsidiary diff failed");

	exit (WEXITSTATUS (wstatus));
      }
    }
  return 0;			/* Fool -Wall . . . */
}

static char **diffargv;

static void
diffarg (a)
     char *a;
{
  static unsigned diffargs, diffargsmax;

  if (diffargs == diffargsmax)
    {
      if (! diffargsmax)
	{
	  diffargv = (char **) xmalloc (sizeof (char));
	  diffargsmax = 8;
	}
      diffargsmax *= 2;
      diffargv = (char **) realloc (diffargv, diffargsmax * sizeof (char *));
      if (! diffargv)
	fatal ("out of memory");
    }
  diffargv[diffargs++] = a;
}

static void
execdiff (differences_only, option, file1, file2)
     int differences_only;
     char *option, *file1, *file2;
{
  if (differences_only)
    diffarg ("--suppress-common-lines");
  diffarg (option);
  diffarg ("--");
  diffarg (file1);
  diffarg (file2);
  diffarg (0);

  execvp (diffbin, diffargv);
  write (fileno (stderr), diffbin, strlen (diffbin));
  write (fileno (stderr), ": not found\n", 12);
  _exit (2);
}




/* Signal handling */

static int volatile ignore_signals;

static void
catchsig (s)
     int s;
{
  signal (s, catchsig);
  if (! ignore_signals)
    {
      cleanup ();
      _exit (2);
    }
}

static void
trapsigs ()
{
  static int const sigs[] = {
#   ifdef SIGHUP
	  SIGHUP,
#   endif
#   ifdef SIGQUIT
	  SIGQUIT,
#   endif
#   ifdef SIGTERM
	  SIGTERM,
#   endif
#   ifdef SIGXCPU
	  SIGXCPU,
#   endif
#   ifdef SIGXFSZ
	  SIGXFSZ,
#   endif
	  SIGINT,
	  SIGPIPE
  };
  int const *p;

  for (p = sigs;  p < sigs + sizeof (sigs) / sizeof (*sigs);  p++)
    if (signal (*p, SIG_IGN) != SIG_IGN  &&  signal (*p, catchsig) != SIG_IGN)
      fatal ("signal error");
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
    ;
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
	      give_help ();
	      flush_line ();
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

	      ignore_signals = 1;

	      pid = vfork ();
	      if (pid == 0)
		{
		  char const *argv[3];
		  int i = 0;

		  argv[i++] = edbin;
		  argv[i++] = tmpname;
		  argv[i++] = 0;

		  execvp (edbin, (char **) argv);
		  write (fileno (stderr), edbin, strlen (edbin));
		  write (fileno (stderr), ": not found\n", 12);
		  _exit (1);
		}

	      if (pid < 0)
		perror_fatal ("fork failed");

	      while (waitpid (pid, &wstatus, 0) < 0)
		if (errno != EINTR)
		  perror_fatal ("wait failed");

	      ignore_signals = 0;

	      if (! (WIFEXITED (wstatus) && WEXITSTATUS (wstatus) < 1))
		fatal ("Subsidiary editor failed");
	    }

	    if (fseek (tmp, 0L, SEEK_SET) != 0)
	      perror_fatal ("fseek");
	    {
	      /* SDIFF_BUFSIZE is too big for a local var
		 in some compilers, so we allocate it dynamically.  */
	      char *buf = (char *) xmalloc (SDIFF_BUFSIZE);
	      size_t size;

	      while ((size = ck_fread (buf, SDIFF_BUFSIZE, tmp)) != 0)
		ck_fwrite (buf, size, outfile);
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



/* Alternately reveal bursts of diff output and handle user editing comands.  */
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

      switch (diff_help[0])
	{
	case ' ':
	  puts (diff_help + 1);
	  break;
	case 'i':
	  {
	    int lenl = atoi (diff_help + 1), lenr, lenmax;
	    char *p = index (diff_help, ',');

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
	    char *p = index (diff_help, ',');

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
/* Return nonzero if DIR is an existent directory.  */
static int
diraccess (dir)
     const char *dir;
{
  struct stat buf;
  return stat (dir, &buf) == 0 && S_ISDIR (buf.st_mode);
}

/* Return nonzero if FILE exists.  */
static int
exists (file)
     const char *file;
{
  struct stat buf;
  return stat (file, &buf) == 0;
}

/* These are the characters used in temporary filenames.  */
static const char letters[] =
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* Generate a temporary filename.
   If DIR_SEARCH is nonzero, DIR and PFX are used as
   described for tempnam.  If not, a temporary filename
   in P_tmpdir with no special prefix is generated.  If LENPTR
   is not NULL, *LENPTR is set the to length (including the
   terminating '\0') of the resultant filename, which is returned.
   This goes through a cyclic pattern of all possible filenames
   consisting of five decimal digits of the current pid and three
   of the characters in `letters'.  Data for tempnam and tmpnam
   is kept separate, but when tempnam is using P_tmpdir and no
   prefix (i.e, it is identical to tmpnam), the same data is used.
   Each potential filename is tested for an already-existing file of
   the same name, and no name of an existing file will be returned.
   When the cycle reaches its end (12345ZZZ), NULL is returned.  */


static char *
private_tempnam (dir, pfx, dir_search, lenptr)
     const char *dir;
     const char *pfx;
     int dir_search;
     size_t *lenptr;
{
  static const char tmpdir[] = PVT_tmpdir;
  static struct
    {
      char buf[3];
      char *s;
      size_t i;
    } infos[2], *info;
  static char buf[TMPNAMSIZE];
  static pid_t oldpid = 0;
  pid_t pid = getpid ();
  register size_t len, plen;

  if (dir_search)
    {
      register const char *d = getenv ("TMPDIR");
      if (d != NULL && !diraccess (d))
	d = NULL;
      if (d == NULL && dir != NULL && diraccess (dir))
	d = dir;
      if (d == NULL && diraccess (tmpdir))
	d = tmpdir;
      if (d == NULL && diraccess ("/tmp"))
	d = "/tmp";
      if (d == NULL)
	{
	  errno = ENOENT;
	  return NULL;
	}
      dir = d;
    }
  else
    dir = tmpdir;

  if (pfx != NULL && *pfx != '\0')
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
  for (;;)
    {
      *info->s = letters[info->i];
      sprintf (buf, "%s/%.*s%.5d%.3s", dir, (int) plen, pfx,
	      pid % 100000, info->buf);
      if (!exists (buf))
	break;
      ++info->i;
      if (info->i > sizeof (letters) - 1)
	{
	  info->i = 0;
	  if (info->s == &info->buf[2])
	    {
	      errno = EEXIST;
	      return NULL;
	    }
	  ++info->s;
	}
    }

  if (lenptr != NULL)
    *lenptr = len;
  return buf;
}
