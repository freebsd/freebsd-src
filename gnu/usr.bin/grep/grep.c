/* grep.c - main driver file for grep.
   Copyright (C) 1992, 1997, 1998, 1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* Written July 1992 by Mike Haertel.  */
/* Builtin decompression 1997 by Wolfram Schneider <wosch@FreeBSD.org>.  */

/* $FreeBSD$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if defined(HAVE_MMAP)
# include <sys/mman.h>
#endif
#if defined(HAVE_SETRLIMIT)
# include <sys/time.h>
# include <sys/resource.h>
#endif
#include <stdio.h>
#include "system.h"
#include "getopt.h"
#include "getpagesize.h"
#include "grep.h"
#include "savedir.h"

#undef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))

struct stats
{
  struct stats *parent;
  struct stat stat;
};

/* base of chain of stat buffers, used to detect directory loops */
static struct stats stats_base;

/* if non-zero, display usage information and exit */
static int show_help;

/* If non-zero, print the version on standard output and exit.  */
static int show_version;

/* Long options equivalences. */
static struct option long_options[] =
{
  {"after-context", required_argument, NULL, 'A'},
  {"basic-regexp", no_argument, NULL, 'G'},
  {"before-context", required_argument, NULL, 'B'},
  {"byte-offset", no_argument, NULL, 'b'},
  {"context", optional_argument, NULL, 'C'},
  {"count", no_argument, NULL, 'c'},
  {"directories", required_argument, NULL, 'd'},
  {"extended-regexp", no_argument, NULL, 'E'},
  {"file", required_argument, NULL, 'f'},
  {"files-with-matches", no_argument, NULL, 'l'},
  {"files-without-match", no_argument, NULL, 'L'},
  {"fixed-regexp", no_argument, NULL, 'F'},
  {"fixed-strings", no_argument, NULL, 'F'},
  {"help", no_argument, &show_help, 1},
  {"ignore-case", no_argument, NULL, 'i'},
  {"line-number", no_argument, NULL, 'n'},
  {"line-regexp", no_argument, NULL, 'x'},
  {"no-filename", no_argument, NULL, 'h'},
  {"no-messages", no_argument, NULL, 's'},
  {"quiet", no_argument, NULL, 'q'},
  {"recursive", no_argument, NULL, 'r'},
  {"regexp", required_argument, NULL, 'e'},
  {"revert-match", no_argument, NULL, 'v'},
  {"silent", no_argument, NULL, 'q'},
  {"text", no_argument, NULL, 'a'},
#if O_BINARY
  {"binary", no_argument, NULL, 'U'},
  {"unix-byte-offsets", no_argument, NULL, 'u'},
#endif
  {"version", no_argument, NULL, 'V'},
  {"with-filename", no_argument, NULL, 'H'},
  {"word-regexp", no_argument, NULL, 'w'},
#if HAVE_LIBZ > 0
  {"decompress", no_argument, NULL, 'Z'},
#endif
  {0, 0, 0, 0}
};

/* Define flags declared in grep.h. */
char const *matcher;
int match_icase;
int match_words;
int match_lines;

/* For error messages. */
static char *prog;
static char const *filename;
static int errseen;

/* How to handle directories.  */
static enum
  {
    READ_DIRECTORIES,
    RECURSE_DIRECTORIES,
    SKIP_DIRECTORIES
  } directories;

static int  ck_atoi PARAMS ((char const *, int *));
static void usage PARAMS ((int)) __attribute__((noreturn));
static void error PARAMS ((const char *, int));
static int  setmatcher PARAMS ((char const *));
static char *page_alloc PARAMS ((size_t, char **));
static int  reset PARAMS ((int, char const *, struct stats *));
static int  fillbuf PARAMS ((size_t, struct stats *));
static int  grepbuf PARAMS ((char *, char *));
static void prtext PARAMS ((char *, char *, int *));
static void prpending PARAMS ((char *));
static void prline PARAMS ((char *, char *, int));
static void print_offset_sep PARAMS ((off_t, int));
static void nlscan PARAMS ((char *));
static int  grep PARAMS ((int, char const *, struct stats *));
static int  grepdir PARAMS ((char const *, struct stats *));
static int  grepfile PARAMS ((char const *, struct stats *));
#if O_BINARY
static inline int undossify_input PARAMS ((register char *, size_t));
#endif

/* Functions we'll use to search. */
static void (*compile) PARAMS ((char *, size_t));
static char *(*execute) PARAMS ((char *, size_t, char **));

/* Print a message and possibly an error string.  Remember
   that something awful happened. */
static void
error (mesg, errnum)
     const char *mesg;
     int errnum;
{
  if (errnum)
    fprintf (stderr, "%s: %s: %s\n", prog, mesg, strerror (errnum));
  else
    fprintf (stderr, "%s: %s\n", prog, mesg);
  errseen = 1;
}

/* Like error (), but die horribly after printing. */
void
fatal (mesg, errnum)
     const char *mesg;
     int errnum;
{
  error (mesg, errnum);
  exit (2);
}

/* Interface to handle errors and fix library lossage. */
char *
xmalloc (size)
     size_t size;
{
  char *result;

  result = malloc (size);
  if (size && !result)
    fatal (_("memory exhausted"), 0);
  return result;
}

/* Interface to handle errors and fix some library lossage. */
char *
xrealloc (ptr, size)
     char *ptr;
     size_t size;
{
  char *result;

  if (ptr)
    result = realloc (ptr, size);
  else
    result = malloc (size);
  if (size && !result)
    fatal (_("memory exhausted"), 0);
  return result;
}

/* Convert STR to a positive integer, storing the result in *OUT.
   If STR is not a valid integer, return -1 (otherwise 0). */
static int
ck_atoi (str, out)
     char const *str;
     int *out;
{
  char const *p;
  for (p = str; *p; p++)
    if (*p < '0' || *p > '9')
      return -1;

  *out = atoi (optarg);
  return 0;
}


/* Hairy buffering mechanism for grep.  The intent is to keep
   all reads aligned on a page boundary and multiples of the
   page size. */

static char *ubuffer;		/* Unaligned base of buffer. */
static char *buffer;		/* Base of buffer. */
static size_t bufsalloc;	/* Allocated size of buffer save region. */
static size_t bufalloc;		/* Total buffer size. */
static int bufdesc;		/* File descriptor. */
static char *bufbeg;		/* Beginning of user-visible stuff. */
static char *buflim;		/* Limit of user-visible stuff. */
static size_t pagesize;		/* alignment of memory pages */

#if defined(HAVE_MMAP)
static int bufmapped;		/* True for ordinary files. */
static off_t bufoffset;		/* What read() normally remembers. */
static off_t initial_bufoffset;	/* Initial value of bufoffset. */
#endif

#if HAVE_LIBZ > 0
#include <zlib.h>
static gzFile gzbufdesc;	/* zlib file descriptor. */
static int Zflag;		/* uncompress before searching. */
#endif

/* Return VAL aligned to the next multiple of ALIGNMENT.  VAL can be
   an integer or a pointer.  Both args must be free of side effects.  */
#define ALIGN_TO(val, alignment) \
  ((size_t) (val) % (alignment) == 0 \
   ? (val) \
   : (val) + ((alignment) - (size_t) (val) % (alignment)))

/* Return the address of a new page-aligned buffer of size SIZE.  Set
   *UP to the newly allocated (but possibly unaligned) buffer used to
   *build the aligned buffer.  To free the buffer, free (*UP).  */
static char *
page_alloc (size, up)
     size_t size;
     char **up;
{
  /* HAVE_WORKING_VALLOC means that valloc is properly declared, and
     you can free the result of valloc.  This symbol is not (yet)
     autoconfigured.  It can be useful to define HAVE_WORKING_VALLOC
     while debugging, since some debugging memory allocators might
     catch more bugs if this symbol is enabled.  */
#if HAVE_WORKING_VALLOC
  *up = valloc (size);
  return *up;
#else
  size_t asize = size + pagesize - 1;
  if (size <= asize)
    {
      *up = malloc (asize);
      if (*up)
	return ALIGN_TO (*up, pagesize);
    }
  return NULL;
#endif
}

/* Reset the buffer for a new file, returning zero if we should skip it.
   Initialize on the first time through. */
static int
reset (fd, file, stats)
     int fd;
     char const *file;
     struct stats *stats;
{
  if (pagesize == 0)
    {
      size_t ubufsalloc;
      pagesize = getpagesize ();
      if (pagesize == 0)
	abort ();
#ifndef BUFSALLOC
      ubufsalloc = MAX (8192, pagesize);
#else
      ubufsalloc = BUFSALLOC;
#endif
      bufsalloc = ALIGN_TO (ubufsalloc, pagesize);
      bufalloc = 5 * bufsalloc;
      /* The 1 byte of overflow is a kludge for dfaexec(), which
	 inserts a sentinel newline at the end of the buffer
	 being searched.  There's gotta be a better way... */
      if (bufsalloc < ubufsalloc
	  || bufalloc / 5 != bufsalloc || bufalloc + 1 < bufalloc
	  || ! (buffer = page_alloc (bufalloc + 1, &ubuffer)))
	fatal (_("memory exhausted"), 0);
      bufbeg = buffer;
      buflim = buffer;
    }
#if HAVE_LIBZ > 0
  if (Zflag) {
    gzbufdesc = gzdopen(fd, "r");
    if (gzbufdesc == NULL)
      fatal(_("memory exhausted"), 0);
  }
#endif
  bufdesc = fd;

  if (
#if defined(HAVE_MMAP)
      1
#else
      directories != READ_DIRECTORIES
#endif
      )
    if (fstat (fd, &stats->stat) != 0)
      {
	error ("fstat", errno);
	return 0;
      }
  if (directories == SKIP_DIRECTORIES && S_ISDIR (stats->stat.st_mode))
    return 0;
#if defined(HAVE_MMAP)
  if (
#if HAVE_LIBZ > 0
      Zflag ||
#endif
      !S_ISREG (stats->stat.st_mode))
    bufmapped = 0;
  else
    {
      bufmapped = 1;
      bufoffset = initial_bufoffset = file ? 0 : lseek (fd, 0, 1);
    }
#endif
  return 1;
}

/* Read new stuff into the buffer, saving the specified
   amount of old stuff.  When we're done, 'bufbeg' points
   to the beginning of the buffer contents, and 'buflim'
   points just after the end.  Return count of new stuff. */
static int
fillbuf (save, stats)
     size_t save;
     struct stats *stats;
{
  int cc;
#if defined(HAVE_MMAP)
  caddr_t maddr;
#endif

  if (save > bufsalloc)
    {
      char *nubuffer;
      char *nbuffer;

      while (save > bufsalloc)
	bufsalloc *= 2;
      bufalloc = 5 * bufsalloc;
      if (bufalloc / 5 != bufsalloc || bufalloc + 1 < bufalloc
	  || ! (nbuffer = page_alloc (bufalloc + 1, &nubuffer)))
	fatal (_("memory exhausted"), 0);

      bufbeg = nbuffer + bufsalloc - save;
      memcpy (bufbeg, buflim - save, save);
      free (ubuffer);
      ubuffer = nubuffer;
      buffer = nbuffer;
    }
  else
    {
      bufbeg = buffer + bufsalloc - save;
      memcpy (bufbeg, buflim - save, save);
    }

#if defined(HAVE_MMAP)
  if (bufmapped && bufoffset % pagesize == 0
      && stats->stat.st_size - bufoffset >= bufalloc - bufsalloc)
    {
      maddr = buffer + bufsalloc;
      maddr = mmap (maddr, bufalloc - bufsalloc, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_FIXED, bufdesc, bufoffset);
      if (maddr == (caddr_t) -1)
	{
          /* This used to issue a warning, but on some hosts
             (e.g. Solaris 2.5) mmap can fail merely because some
             other process has an advisory read lock on the file.
             There's no point alarming the user about this misfeature.  */
#if 0
	  fprintf (stderr, _("%s: warning: %s: %s\n"), prog, filename,
		  strerror (errno));
#endif
	  goto tryread;
	}
#if 0
      /* You might thing this (or MADV_WILLNEED) would help,
	 but it doesn't, at least not on a Sun running 4.1.
	 In fact, it actually slows us down about 30%! */
      madvise (maddr, bufalloc - bufsalloc, MADV_SEQUENTIAL);
#endif
      cc = bufalloc - bufsalloc;
      bufoffset += cc;
    }
  else
    {
    tryread:
      /* We come here when we're not going to use mmap() any more.
	 Note that we need to synchronize the file offset the
	 first time through. */
      if (bufmapped)
	{
	  bufmapped = 0;
	  if (bufoffset != initial_bufoffset)
	    lseek (bufdesc, bufoffset, 0);
	}
#if HAVE_LIBZ > 0
      if (Zflag)
        cc = gzread (gzbufdesc, buffer + bufsalloc, bufalloc - bufsalloc);
      else
#endif
      cc = read (bufdesc, buffer + bufsalloc, bufalloc - bufsalloc);
    }
#else
#if HAVE_LIBZ > 0
  if (Zflag)
    cc = gzread (gzbufdesc, buffer + bufsalloc, bufalloc - bufsalloc);
  else
#endif
  cc = read (bufdesc, buffer + bufsalloc, bufalloc - bufsalloc);
#endif /*HAVE_MMAP*/
#if O_BINARY
  if (cc > 0)
    cc = undossify_input (buffer + bufsalloc, cc);
#endif
  if (cc > 0)
    buflim = buffer + bufsalloc + cc;
  else
    buflim = buffer + bufsalloc;
  return cc;
}

/* Flags controlling the style of output. */
static int always_text;		/* Assume the input is always text. */
static int out_quiet;		/* Suppress all normal output. */
static int out_invert;		/* Print nonmatching stuff. */
static int out_file;		/* Print filenames. */
static int out_line;		/* Print line numbers. */
static int out_byte;		/* Print byte offsets. */
static int out_before;		/* Lines of leading context. */
static int out_after;		/* Lines of trailing context. */
static int count_matches;	/* Count matching lines.  */
static int list_files;		/* List matching files.  */
static int no_filenames;	/* Suppress file names.  */
static int suppress_errors;	/* Suppress diagnostics.  */

/* Internal variables to keep track of byte count, context, etc. */
static off_t totalcc;		/* Total character count before bufbeg. */
static char *lastnl;		/* Pointer after last newline counted. */
static char *lastout;		/* Pointer after last character output;
				   NULL if no character has been output
				   or if it's conceptually before bufbeg. */
static off_t totalnl;		/* Total newline count before lastnl. */
static int pending;		/* Pending lines of output. */
static int done_on_match;		/* Stop scanning file on first match */

#if O_BINARY
# include "dosbuf.c"
#endif

static void
nlscan (lim)
     char *lim;
{
  char *beg;

  for (beg = lastnl; beg < lim; ++beg)
    if (*beg == '\n')
      ++totalnl;
  lastnl = beg;
}

static void
print_offset_sep (pos, sep)
     off_t pos;
     int sep;
{
  /* Do not rely on printf to print pos, since off_t may be longer than long,
     and long long is not portable.  */

  char buf[sizeof pos * CHAR_BIT];
  char *p = buf + sizeof buf - 1;
  *p = sep;

  do
    *--p = '0' + pos % 10;
  while ((pos /= 10) != 0);

  fwrite (p, 1, buf + sizeof buf - p, stdout);
}

static void
prline (beg, lim, sep)
     char *beg;
     char *lim;
     int sep;
{
  if (out_file)
    printf ("%s%c", filename, sep);
  if (out_line)
    {
      nlscan (beg);
      print_offset_sep (++totalnl, sep);
      lastnl = lim;
    }
  if (out_byte)
    {
      off_t pos = totalcc + (beg - bufbeg);
#if O_BINARY
      pos = dossified_pos (pos);
#endif
      print_offset_sep (pos, sep);
    }
  fwrite (beg, 1, lim - beg, stdout);
  if (ferror (stdout))
    error (_("writing output"), errno);
  lastout = lim;
}

/* Print pending lines of trailing context prior to LIM. */
static void
prpending (lim)
     char *lim;
{
  char *nl;

  if (!lastout)
    lastout = bufbeg;
  while (pending > 0 && lastout < lim)
    {
      --pending;
      if ((nl = memchr (lastout, '\n', lim - lastout)) != 0)
	++nl;
      else
	nl = lim;
      prline (lastout, nl, '-');
    }
}

/* Print the lines between BEG and LIM.  Deal with context crap.
   If NLINESP is non-null, store a count of lines between BEG and LIM. */
static void
prtext (beg, lim, nlinesp)
     char *beg;
     char *lim;
     int *nlinesp;
{
  static int used;		/* avoid printing "--" before any output */
  char *bp, *p, *nl;
  int i, n;

  if (!out_quiet && pending > 0)
    prpending (beg);

  p = beg;

  if (!out_quiet)
    {
      /* Deal with leading context crap. */

      bp = lastout ? lastout : bufbeg;
      for (i = 0; i < out_before; ++i)
	if (p > bp)
	  do
	    --p;
	  while (p > bp && p[-1] != '\n');

      /* We only print the "--" separator if our output is
	 discontiguous from the last output in the file. */
      if ((out_before || out_after) && used && p != lastout)
	puts ("--");

      while (p < beg)
	{
	  nl = memchr (p, '\n', beg - p);
	  prline (p, nl + 1, '-');
	  p = nl + 1;
	}
    }

  if (nlinesp)
    {
      /* Caller wants a line count. */
      for (n = 0; p < lim; ++n)
	{
	  if ((nl = memchr (p, '\n', lim - p)) != 0)
	    ++nl;
	  else
	    nl = lim;
	  if (!out_quiet)
	    prline (p, nl, ':');
	  p = nl;
	}
      *nlinesp = n;
    }
  else
    if (!out_quiet)
      prline (beg, lim, ':');

  pending = out_after;
  used = 1;
}

/* Scan the specified portion of the buffer, matching lines (or
   between matching lines if OUT_INVERT is true).  Return a count of
   lines printed. */
static int
grepbuf (beg, lim)
     char *beg;
     char *lim;
{
  int nlines, n;
  register char *p, *b;
  char *endp;

  nlines = 0;
  p = beg;
  while ((b = (*execute)(p, lim - p, &endp)) != 0)
    {
      /* Avoid matching the empty line at the end of the buffer. */
      if (b == lim && ((b > beg && b[-1] == '\n') || b == beg))
	break;
      if (!out_invert)
	{
	  prtext (b, endp, (int *) 0);
	  nlines += 1;
	  if (done_on_match)
	    return nlines;
	}
      else if (p < b)
	{
	  prtext (p, b, &n);
	  nlines += n;
	}
      p = endp;
    }
  if (out_invert && p < lim)
    {
      prtext (p, lim, &n);
      nlines += n;
    }
  return nlines;
}

/* Search a given file.  Normally, return a count of lines printed;
   but if the file is a directory and we search it recursively, then
   return -2 if there was a match, and -1 otherwise.  */
static int
grep (fd, file, stats)
     int fd;
     char const *file;
     struct stats *stats;
{
  int nlines, i;
  int not_text;
  size_t residue, save;
  char *beg, *lim;

  if (!reset (fd, file, stats))
    return 0;

  if (file && directories == RECURSE_DIRECTORIES
      && S_ISDIR (stats->stat.st_mode))
    {
      /* Close fd now, so that we don't open a lot of file descriptors
	 when we recurse deeply.  */
#if HAVE_LIBZ > 0
      if (Zflag)
	gzclose(gzbufdesc);
      else
#endif
      if (close (fd) != 0)
	error (file, errno);
      return grepdir (file, stats) - 2;
    }

  totalcc = 0;
  lastout = 0;
  totalnl = 0;
  pending = 0;

  nlines = 0;
  residue = 0;
  save = 0;

  if (fillbuf (save, stats) < 0)
    {
      if (! (is_EISDIR (errno, file) && suppress_errors))
	error (filename, errno);
      return nlines;
    }

  not_text = (! (always_text | out_quiet)
	      && memchr (bufbeg, '\0', buflim - bufbeg));
  done_on_match += not_text;
  out_quiet += not_text;

  for (;;)
    {
      lastnl = bufbeg;
      if (lastout)
	lastout = bufbeg;
      if (buflim - bufbeg == save)
	break;
      beg = bufbeg + save - residue;
      for (lim = buflim; lim > beg && lim[-1] != '\n'; --lim)
	;
      residue = buflim - lim;
      if (beg < lim)
	{
	  nlines += grepbuf (beg, lim);
	  if (pending)
	    prpending (lim);
	  if (nlines && done_on_match && !out_invert)
	    goto finish_grep;
	}
      i = 0;
      beg = lim;
      while (i < out_before && beg > bufbeg && beg != lastout)
	{
	  ++i;
	  do
	    --beg;
	  while (beg > bufbeg && beg[-1] != '\n');
	}
      if (beg != lastout)
	lastout = 0;
      save = residue + lim - beg;
      totalcc += buflim - bufbeg - save;
      if (out_line)
	nlscan (beg);
      if (fillbuf (save, stats) < 0)
	{
	  if (! (is_EISDIR (errno, file) && suppress_errors))
	    error (filename, errno);
	  goto finish_grep;
	}
    }
  if (residue)
    {
      nlines += grepbuf (bufbeg + save - residue, buflim);
      if (pending)
	prpending (buflim);
    }

 finish_grep:
  done_on_match -= not_text;
  out_quiet -= not_text;
  if ((not_text & ~out_quiet) && nlines != 0)
    printf (_("Binary file %s matches\n"), filename);
  return nlines;
}

static int
grepfile (file, stats)
     char const *file;
     struct stats *stats;
{
  int desc;
  int count;
  int status;

  if (! file)
    {
      desc = 0;
      filename = _("(standard input)");
    }
  else
    {
      desc = open (file, O_RDONLY);

      if (desc < 0)
	{
	  int e = errno;
	    
	  if (is_EISDIR (e, file) && directories == RECURSE_DIRECTORIES)
	    {
	      if (stat (file, &stats->stat) != 0)
		{
		  error (file, errno);
		  return 1;
		}

	      return grepdir (file, stats);
	    }
	      
	  if (!suppress_errors)
	    {
	      if (directories == SKIP_DIRECTORIES)
		switch (e)
		  {
#ifdef EISDIR
		  case EISDIR:
		    return 1;
#endif
		  case EACCES:
		    /* When skipping directories, don't worry about
		       directories that can't be opened.  */
		    if (stat (file, &stats->stat) == 0
			&& S_ISDIR (stats->stat.st_mode))
		      return 1;
		    break;
		  }

	      error (file, e);
	    }

	  return 1;
	}

      filename = file;
    }

#if O_BINARY
  /* Set input to binary mode.  Pipes are simulated with files
     on DOS, so this includes the case of "foo | grep bar".  */
  if (!isatty (desc))
    SET_BINARY (desc);
#endif

  count = grep (desc, file, stats);
  if (count < 0)
    status = count + 2;
  else
    {
      if (count_matches)
	{
	  if (out_file)
	    printf ("%s:", filename);
	  printf ("%d\n", count);
	}

      if (count)
	{
	  status = 0;
	  if (list_files == 1)
	    printf ("%s\n", filename);
	}
      else
	{
	  status = 1;
	  if (list_files == -1)
	    printf ("%s\n", filename);
	}

#if HAVE_LIBZ > 0
      if (Zflag)
	gzclose(gzbufdesc);
      else
#endif
      if (file && close (desc) != 0)
	error (file, errno);
    }

  return status;
}

static int
grepdir (dir, stats)
     char const *dir;
     struct stats *stats;
{
  int status = 1;
  struct stats *ancestor;
  char *name_space;

  for (ancestor = stats;  (ancestor = ancestor->parent) != 0;  )
    if (! ((ancestor->stat.st_ino ^ stats->stat.st_ino)
	   | (ancestor->stat.st_dev ^ stats->stat.st_dev)))
      {
	if (!suppress_errors)
	  fprintf (stderr, _("%s: warning: %s: %s\n"), prog, dir,
		   _("recursive directory loop"));
	return 1;
      }

  name_space = savedir (dir, (unsigned) stats->stat.st_size);

  if (! name_space)
    {
      if (errno)
	{
	  if (!suppress_errors)
	    error (dir, errno);
	}
      else
	fatal (_("Memory exhausted"), 0);
    }
  else
    {
      size_t dirlen = strlen (dir);
      int needs_slash = ! (dirlen == FILESYSTEM_PREFIX_LEN (dir)
			   || IS_SLASH (dir[dirlen - 1]));
      char *file = NULL;
      char *namep = name_space;
      struct stats child;
      child.parent = stats;
      out_file += !no_filenames;
      while (*namep)
	{
	  size_t namelen = strlen (namep);
	  file = xrealloc (file, dirlen + 1 + namelen + 1);
	  strcpy (file, dir);
	  file[dirlen] = '/';
	  strcpy (file + dirlen + needs_slash, namep);
	  namep += namelen + 1;
	  status &= grepfile (file, &child);
	}
      out_file -= !no_filenames;
      if (file)
        free (file);
      free (name_space);
    }

  return status;
}

static void
usage(status)
int status;
{
  if (status != 0)
    {
      fprintf (stderr, _("Usage: %s [OPTION]... PATTERN [FILE]...\n"), prog);
      fprintf (stderr, _("Try `%s --help' for more information.\n"), prog);
    }
  else
    {
      printf (_("Usage: %s [OPTION]... PATTERN [FILE] ...\n"), prog);
      printf (_("\
Search for PATTERN in each FILE or standard input.\n\
\n\
Regexp selection and interpretation:\n\
  -E, --extended-regexp     PATTERN is an extended regular expression\n\
  -F, --fixed-regexp        PATTERN is a fixed string separated by newlines\n\
  -G, --basic-regexp        PATTERN is a basic regular expression\n\
  -e, --regexp=PATTERN      use PATTERN as a regular expression\n\
  -f, --file=FILE           obtain PATTERN from FILE\n\
  -i, --ignore-case         ignore case distinctions\n\
  -w, --word-regexp         force PATTERN to match only whole words\n\
  -x, --line-regexp         force PATTERN to match only whole lines\n"));
      printf (_("\
\n\
Miscellaneous:\n\
  -s, --no-messages         suppress error messages\n\
  -v, --revert-match        select non-matching lines\n\
  -V, --version             print version information and exit\n\
  -Z, --decompress          decompress input before searching (HAVE_LIBZ=1)\n\
      --help                display this help and exit\n"));
      printf (_("\
\n\
Output control:\n\
  -b, --byte-offset         print the byte offset with output lines\n\
  -n, --line-number         print line number with output lines\n\
  -H, --with-filename       print the filename for each match\n\
  -h, --no-filename         suppress the prefixing filename on output\n\
  -q, --quiet, --silent     suppress all normal output\n\
  -a, --text                do not suppress binary output\n\
  -d, --directories=ACTION  how to handle directories\n\
                            ACTION is 'read', 'recurse', or 'skip'.\n\
  -r, --recursive           equivalent to --directories=recurse.\n\
  -L, --files-without-match only print FILE names containing no match\n\
  -l, --files-with-matches  only print FILE names containing matches\n\
  -c, --count               only print a count of matching lines per FILE\n"));
      printf (_("\
\n\
Context control:\n\
  -B, --before-context=NUM  print NUM lines of leading context\n\
  -A, --after-context=NUM   print NUM lines of trailing context\n\
  -C, --context[=NUM]       print NUM (default 2) lines of output context\n\
                            unless overriden by -A or -B\n\
  -NUM                      same as --context=NUM\n\
  -U, --binary              do not strip CR characters at EOL (MSDOS)\n\
  -u, --unix-byte-offsets   report offsets as if CRs were not there (MSDOS)\n\
\n\
If no -[GEF], then `egrep' assumes -E, `fgrep' -F, else -G.\n\
With no FILE, or when FILE is -, read standard input. If less than\n\
two FILEs given, assume -h. Exit with 0 if matches, with 1 if none.\n\
Exit with 2 if syntax errors or system errors.\n"));
      printf (_("\nReport bugs to <bug-gnu-utils@gnu.org>.\n"));
    }
  exit (status);
}

/* Go through the matchers vector and look for the specified matcher.
   If we find it, install it in compile and execute, and return 1.  */
static int
setmatcher (name)
     char const *name;
{
  int i;
#ifdef HAVE_SETRLIMIT
  struct rlimit rlim;
#endif

  for (i = 0; matchers[i].name; ++i)
    if (strcmp (name, matchers[i].name) == 0)
      {
	compile = matchers[i].compile;
	execute = matchers[i].execute;
#if HAVE_SETRLIMIT && defined(RLIMIT_STACK)
	/* I think every platform needs to do this, so that regex.c
	   doesn't oveflow the stack.  The default value of
	   `re_max_failures' is too large for some platforms: it needs
	   more than 3MB-large stack.

	   The test for HAVE_SETRLIMIT should go into `configure'.  */
	if (!getrlimit (RLIMIT_STACK, &rlim))
	  {
	    long newlim;
	    extern long int re_max_failures; /* from regex.c */

	    /* Approximate the amount regex.c needs, plus some more.  */
	    newlim = re_max_failures * 2 * 20 * sizeof (char *);
	    if (newlim > rlim.rlim_max)
	      {
		newlim = rlim.rlim_max;
		re_max_failures = newlim / (2 * 20 * sizeof (char *));
	      }
	    if (rlim.rlim_cur < newlim)
	      rlim.rlim_cur = newlim;

	    setrlimit (RLIMIT_STACK, &rlim);
	  }
#endif
	return 1;
      }
  return 0;
}

int
main (argc, argv)
     int argc;
     char *argv[];
{
  char *keys;
  size_t keycc, oldcc, keyalloc;
  int with_filenames;
  int opt, cc, status;
  unsigned digit_args_val, default_context;
  FILE *fp;
  extern char *optarg;
  extern int optind;

  initialize_main (&argc, &argv);
  prog = argv[0];
  if (prog && strrchr (prog, '/'))
    prog = strrchr (prog, '/') + 1;

#if HAVE_LIBZ > 0
  if (prog[0] == 'z') {
    Zflag = 1;
    ++prog;
  }
#endif

#if defined(__MSDOS__) || defined(_WIN32)
  /* DOS and MS-Windows use backslashes as directory separators, and usually
     have an .exe suffix.  They also have case-insensitive filesystems.  */
  if (prog)
    {
      char *p = prog;
      char *bslash = strrchr (argv[0], '\\');

      if (bslash && bslash >= prog) /* for mixed forward/backslash case */
	prog = bslash + 1;
      else if (prog == argv[0]
	       && argv[0][0] && argv[0][1] == ':') /* "c:progname" */
	prog = argv[0] + 2;

      /* Collapse the letter-case, so `strcmp' could be used hence.  */
      for ( ; *p; p++)
	if (*p >= 'A' && *p <= 'Z')
	  *p += 'a' - 'A';

      /* Remove the .exe extension, if any.  */
      if ((p = strrchr (prog, '.')) && strcmp (p, ".exe") == 0)
	*p = '\0';
    }
#endif

  keys = NULL;
  keycc = 0;
  with_filenames = 0;
  matcher = NULL;

  /* The value -1 means to use DEFAULT_CONTEXT. */
  out_after = out_before = -1;
  /* Default before/after context: chaged by -C/-NUM options */
  default_context = 0;
  /* Accumulated value of individual digits in a -NUM option */
  digit_args_val = 0;


/* Internationalization. */
#if HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
#if ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  while ((opt = getopt_long (argc, argv,
#if O_BINARY
         "0123456789A:B:C::EFGHVX:abcd:e:f:hiLlnqrsvwxyUu",
#elif HAVE_LIBZ > 0
         "0123456789A:B:C::EFGHRVX:Zabcd:e:f:hiLlnqrsvwxy",
#else
         "0123456789A:B:C::EFGHRVX:abcd:e:f:hiLlnqrsvwxy",
#endif
         long_options, NULL)) != EOF)
    switch (opt)
      {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
	digit_args_val = 10 * digit_args_val + opt - '0';
	default_context = digit_args_val;
	break;
      case 'A':
	if (optarg)
	  {
	    if (ck_atoi (optarg, &out_after))
	      fatal (_("invalid context length argument"), 0);
	  }
	break;
      case 'B':
	if (optarg)
	  {
	    if (ck_atoi (optarg, &out_before))
	      fatal (_("invalid context length argument"), 0);
	  }
	break;
      case 'C':
	/* Set output match context, but let any explicit leading or
	   trailing amount specified with -A or -B stand. */
	if (optarg)
	  {
	    if (ck_atoi (optarg, &default_context))
	      fatal (_("invalid context length argument"), 0);
	  }
	else
	  default_context = 2;
	break;
      case 'E':
	if (matcher && strcmp (matcher, "posix-egrep") != 0)
	  fatal (_("you may specify only one of -E, -F, or -G"), 0);
	matcher = "posix-egrep";
	break;
      case 'F':
	if (matcher && strcmp(matcher, "fgrep") != 0)
	  fatal(_("you may specify only one of -E, -F, or -G"), 0);;
	matcher = "fgrep";
	break;
      case 'G':
	if (matcher && strcmp (matcher, "grep") != 0)
	  fatal (_("you may specify only one of -E, -F, or -G"), 0);
	matcher = "grep";
	break;
      case 'H':
	with_filenames = 1;
	break;
#if O_BINARY
      case 'U':
	dos_use_file_type = DOS_BINARY;
	break;
      case 'u':
	dos_report_unix_offset = 1;
	break;
#endif
      case 'V':
	show_version = 1;
	break;
      case 'X':
	if (matcher)
	  fatal (_("matcher already specified"), 0);
	matcher = optarg;
	break;
#if HAVE_LIBZ > 0
      case 'Z':
	Zflag = 1;
	break;
#endif
      case 'a':
	always_text = 1;
	break;
      case 'b':
	out_byte = 1;
	break;
      case 'c':
	out_quiet = 1;
	count_matches = 1;
	break;
      case 'd':
	if (strcmp (optarg, "read") == 0)
	  directories = READ_DIRECTORIES;
	else if (strcmp (optarg, "skip") == 0)
	  directories = SKIP_DIRECTORIES;
	else if (strcmp (optarg, "recurse") == 0)
	  directories = RECURSE_DIRECTORIES;
	else
	  fatal (_("unknown directories method"), 0);
	break;
      case 'e':
	cc = strlen (optarg);
	keys = xrealloc (keys, keycc + cc + 1);
	strcpy (&keys[keycc], optarg);
	keycc += cc;
	keys[keycc++] = '\n';
	break;
      case 'f':
	fp = strcmp (optarg, "-") != 0 ? fopen (optarg, "r") : stdin;
	if (!fp)
	  fatal (optarg, errno);
	for (keyalloc = 1; keyalloc <= keycc + 1; keyalloc *= 2)
	  ;
	keys = xrealloc (keys, keyalloc);
	oldcc = keycc;
	while (!feof (fp)
	       && (cc = fread (keys + keycc, 1, keyalloc - 1 - keycc, fp)) > 0)
	  {
	    keycc += cc;
	    if (keycc == keyalloc - 1)
	      keys = xrealloc (keys, keyalloc *= 2);
	  }
	if (fp != stdin)
	  fclose(fp);
	/* Append final newline if file ended in non-newline. */
	if (oldcc != keycc && keys[keycc - 1] != '\n')
	  keys[keycc++] = '\n';
	break;
      case 'h':
	no_filenames = 1;
	break;
      case 'i':
      case 'y':			/* For old-timers . . . */
	match_icase = 1;
	break;
      case 'L':
	/* Like -l, except list files that don't contain matches.
	   Inspired by the same option in Hume's gre. */
	out_quiet = 1;
	list_files = -1;
	done_on_match = 1;
	break;
      case 'l':
	out_quiet = 1;
	list_files = 1;
	done_on_match = 1;
	break;
      case 'n':
	out_line = 1;
	break;
      case 'q':
	done_on_match = 1;
	out_quiet = 1;
	break;
      case 'R':
      case 'r':
	directories = RECURSE_DIRECTORIES;
	break;
      case 's':
	suppress_errors = 1;
	break;
      case 'v':
	out_invert = 1;
	break;
      case 'w':
	match_words = 1;
	break;
      case 'x':
	match_lines = 1;
	break;
      case 0:
	/* long options */
	break;
      default:
	usage (2);
	break;
      }

  if (out_after < 0)
    out_after = default_context;
  if (out_before < 0)
    out_before = default_context;

  if (show_version)
    {
      printf (_("grep (GNU grep) %s\n"), VERSION);
      printf ("\n");
      printf (_("\
Copyright (C) 1988, 1992-1998, 1999 Free Software Foundation, Inc.\n"));
      printf (_("\
This is free software; see the source for copying conditions. There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"));
      printf ("\n");
      exit (0);
    }

  if (show_help)
    usage (0);

  if (keys)
    {
      if (keycc == 0)
	/* No keys were specified (e.g. -f /dev/null).  Match nothing.  */
        out_invert ^= 1;
      else
	/* Strip trailing newline. */
        --keycc;
    }
  else
    if (optind < argc)
      {
	keys = argv[optind++];
	keycc = strlen (keys);
      }
    else
      usage (2);

  if (! matcher)
    matcher = prog;

  if (!setmatcher (matcher) && !setmatcher ("default"))
    abort ();

  (*compile)(keys, keycc);

  if ((argc - optind > 1 && !no_filenames) || with_filenames)
    out_file = 1;

#if O_BINARY
  /* Output is set to binary mode because we shouldn't convert
     NL to CR-LF pairs, especially when grepping binary files.  */
  if (!isatty (1))
    SET_BINARY (1);
#endif


  if (optind < argc)
    {
	status = 1;
	do
	{
	  char *file = argv[optind];
	  status &= grepfile (strcmp (file, "-") == 0 ? (char *) NULL : file,
			      &stats_base);
	}
	while ( ++optind < argc);
    }
  else
    status = grepfile ((char *) NULL, &stats_base);

  if (fclose (stdout) == EOF)
    error (_("writing output"), errno);

  exit (errseen ? 2 : status);
}
