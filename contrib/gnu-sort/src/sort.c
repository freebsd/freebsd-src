/* $FreeBSD$ */
/* sort - sort lines of text (with all kinds of options).
   Copyright (C) 88, 1991-2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written December 1988 by Mike Haertel.
   The author may be reached (Email) at the address mike@gnu.ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation.

   Ørn E. Hansen added NLS support in 1997.  */

#include <config.h>

#include <assert.h>
#include <getopt.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>

/* Solaris 2.5 has a bug: <wchar.h> must be included before <wctype.h>.  */
/* Get mbstate_t, mbrtowc(), wcwidth().  */
#if HAVE_WCHAR_H
# include <wchar.h>
#endif

/* Get isw* functions. */
#if HAVE_WCTYPE_H
# include <wctype.h>
#endif

/* Get nl_langinfo(). */
#if HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif 

/* Include this after wctype.h so that we `#undef' ISPRINT
   (from Solaris's euc.h, from widec.h, from wctype.h) before
   redefining and using it. */
#include "system.h"
#include "error.h"
#include "hard-locale.h"
#include "inttostr.h"
#include "long-options.h"
#include "physmem.h"
#include "posixver.h"
#include "quote.h"
#include "stdio-safer.h"
#include "xmemcoll.h"
#include "xstrtol.h"

#if HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#ifndef RLIMIT_DATA
struct rlimit { size_t rlim_cur; };
# define getrlimit(Resource, Rlp) (-1)
#endif

/* MB_LEN_MAX is incorrectly defined to be 1 in at least one GCC
   installation; work around this configuration error.  */
#if !defined MB_LEN_MAX || MB_LEN_MAX == 1
# define MB_LEN_MAX 16
#endif

/* Some systems, like BeOS, have multibyte encodings but lack mbstate_t.  */
#if HAVE_MBRTOWC && defined mbstate_t
# define mbrtowc(pwc, s, n, ps) (mbrtowc) (pwc, s, n, 0)
#endif

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "sort"

#define AUTHORS "Mike Haertel", "Paul Eggert"

#if HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif

#ifndef SA_NOCLDSTOP
# define sigprocmask(How, Set, Oset) /* empty */
# define sigset_t int
#endif

#ifndef STDC_HEADERS
double strtod ();
#endif

#define UCHAR_LIM (UCHAR_MAX + 1)

#ifndef DEFAULT_TMPDIR
# define DEFAULT_TMPDIR "/tmp"
#endif

/* Exit statuses.  */
enum
  {
    /* POSIX says to exit with status 1 if invoked with -c and the
       input is not properly sorted.  */
    SORT_OUT_OF_ORDER = 1,

    /* POSIX says any other irregular exit must exit with a status
       code greater than 1.  */
    SORT_FAILURE = 2
  };

#define C_DECIMAL_POINT '.'
#define NEGATION_SIGN   '-'
#define NUMERIC_ZERO    '0'

#if HAVE_SETLOCALE

static char decimal_point;
static int th_sep; /* if CHAR_MAX + 1, then there is no thousands separator */
static int force_general_numcompare = 0;

/* Nonzero if the corresponding locales are hard.  */
static bool hard_LC_COLLATE;
# if HAVE_NL_LANGINFO
static bool hard_LC_TIME;
# endif

# define IS_THOUSANDS_SEP(x) ((x) == th_sep)

#else

# define decimal_point C_DECIMAL_POINT
# define IS_THOUSANDS_SEP(x) false

#endif

#define NONZERO(x) (x != 0)

/* get a multibyte character's byte length. */
#define GET_BYTELEN_OF_CHAR(LIM, PTR, MBLENGTH, STATE)			\
  do									\
    {									\
      wchar_t wc;							\
      mbstate_t state_bak;						\
									\
      state_bak = STATE;						\
      mblength = mbrtowc (&wc, PTR, LIM - PTR, &STATE);			\
									\
      switch (MBLENGTH)							\
	{								\
	case (size_t)-1:						\
	case (size_t)-2:						\
	  STATE = state_bak;						\
		/* Fall through. */					\
	case 0:								\
	  MBLENGTH = 1;							\
      }									\
    }									\
  while (0)

/* The kind of blanks for '-b' to skip in various options. */
enum blanktype { bl_start, bl_end, bl_both };

/* The character marking end of line. Default to \n. */
static char eolchar = '\n';

/* Lines are held in core as counted strings. */
struct line
{
  char *text;			/* Text of the line. */
  size_t length;		/* Length including final newline. */
  char *keybeg;			/* Start of first key. */
  char *keylim;			/* Limit of first key. */
};

/* Input buffers. */
struct buffer
{
  char *buf;			/* Dynamically allocated buffer,
				   partitioned into 3 regions:
				   - input data;
				   - unused area;
				   - an array of lines, in reverse order.  */
  size_t used;			/* Number of bytes used for input data.  */
  size_t nlines;		/* Number of lines in the line array.  */
  size_t alloc;			/* Number of bytes allocated. */
  size_t left;			/* Number of bytes left from previous reads. */
  size_t line_bytes;		/* Number of bytes to reserve for each line. */
  bool eof;			/* An EOF has been read.  */
};

struct keyfield
{
  size_t sword;			/* Zero-origin 'word' to start at. */
  size_t schar;			/* Additional characters to skip. */
  size_t eword;			/* Zero-origin first word after field. */
  size_t echar;			/* Additional characters in field. */
  bool const *ignore;		/* Boolean array of characters to ignore. */
  char const *translate;	/* Translation applied to characters. */
  bool skipsblanks;		/* Skip leading blanks when finding start.  */
  bool skipeblanks;		/* Skip leading blanks when finding end.  */
  bool numeric;			/* Flag for numeric comparison.  Handle
				   strings of digits with optional decimal
				   point, but no exponential notation. */
  bool general_numeric;		/* Flag for general, numeric comparison.
				   Handle numbers in exponential notation. */
  bool month;			/* Flag for comparison by month name. */
  bool reverse;			/* Reverse the sense of comparison. */
  struct keyfield *next;	/* Next keyfield to try. */
};

struct month
{
  char const *name;
  int val;
};

/* The name this program was run with. */
char *program_name;

/* FIXME: None of these tables work with multibyte character sets.
   Also, there are many other bugs when handling multibyte characters.
   One way to fix this is to rewrite `sort' to use wide characters
   internally, but doing this with good performance is a bit
   tricky.  */

/* Table of blanks.  */
static bool blanks[UCHAR_LIM];

/* Table of non-printing characters. */
static bool nonprinting[UCHAR_LIM];

/* Table of non-dictionary characters (not letters, digits, or blanks). */
static bool nondictionary[UCHAR_LIM];

/* Translation table folding lower case to upper.  */
static char fold_toupper[UCHAR_LIM];

#define MONTHS_PER_YEAR 12

/* Table mapping month names to integers.
   Alphabetic order allows binary search. */
static struct month monthtab[] =
{
  {"APR", 4},
  {"AUG", 8},
  {"DEC", 12},
  {"FEB", 2},
  {"JAN", 1},
  {"JUL", 7},
  {"JUN", 6},
  {"MAR", 3},
  {"MAY", 5},
  {"NOV", 11},
  {"OCT", 10},
  {"SEP", 9}
};

/* During the merge phase, the number of files to merge at once. */
#define NMERGE 16

/* Minimum size for a merge or check buffer.  */
#define MIN_MERGE_BUFFER_SIZE (2 + sizeof (struct line))

/* Minimum sort size; the code might not work with smaller sizes.  */
#define MIN_SORT_SIZE (NMERGE * MIN_MERGE_BUFFER_SIZE)

/* The number of bytes needed for a merge or check buffer, which can
   function relatively efficiently even if it holds only one line.  If
   a longer line is seen, this value is increased.  */
static size_t merge_buffer_size = MAX (MIN_MERGE_BUFFER_SIZE, 256 * 1024);

/* The approximate maximum number of bytes of main memory to use, as
   specified by the user.  Zero if the user has not specified a size.  */
static size_t sort_size;

/* The guessed size for non-regular files.  */
#define INPUT_FILE_SIZE_GUESS (1024 * 1024)

/* Array of directory names in which any temporary files are to be created. */
static char const **temp_dirs;

/* Number of temporary directory names used.  */
static size_t temp_dir_count;

/* Number of allocated slots in temp_dirs.  */
static size_t temp_dir_alloc;

/* Flag to reverse the order of all comparisons. */
static bool reverse;

/* Flag for stable sort.  This turns off the last ditch bytewise
   comparison of lines, and instead leaves lines in the same order
   they were read if all keys compare equal.  */
static bool stable;

/* Tab character separating fields.  If tab_default, then fields are
   separated by the empty string between a non-blank character and a blank
   character. */
static bool tab_default = true;
static unsigned char tab[MB_LEN_MAX + 1];
static size_t tab_length = 1;

/* Flag to remove consecutive duplicate lines from the output.
   Only the last of a sequence of equal lines will be output. */
static bool unique;

/* Nonzero if any of the input files are the standard input. */
static bool have_read_stdin;

/* List of key field comparisons to be tried.  */
static struct keyfield *keylist;

static void sortlines_temp (struct line *, size_t, struct line *);

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
"),
	      program_name);
      fputs (_("\
Write sorted concatenation of all FILE(s) to standard output.\n\
\n\
Ordering options:\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -b, --ignore-leading-blanks ignore leading blanks\n\
  -d, --dictionary-order      consider only blanks and alphanumeric characters\n\
  -f, --ignore-case           fold lower case to upper case characters\n\
"), stdout);
      fputs (_("\
  -g, --general-numeric-sort  compare according to general numerical value\n\
  -i, --ignore-nonprinting    consider only printable characters\n\
  -M, --month-sort            compare (unknown) < `JAN' < ... < `DEC'\n\
  -n, --numeric-sort          compare according to string numerical value\n\
  -r, --reverse               reverse the result of comparisons\n\
\n\
"), stdout);
      fputs (_("\
Other options:\n\
\n\
  -c, --check               check whether input is sorted; do not sort\n\
  -k, --key=POS1[,POS2]     start a key at POS1, end it at POS 2 (origin 1)\n\
  -m, --merge               merge already sorted files; do not sort\n\
  -o, --output=FILE         write result to FILE instead of standard output\n\
  -s, --stable              stabilize sort by disabling last-resort comparison\n\
  -S, --buffer-size=SIZE    use SIZE for main memory buffer\n\
"), stdout);
      printf (_("\
  -t, --field-separator=SEP use SEP instead of non-blank to blank transition\n\
  -T, --temporary-directory=DIR  use DIR for temporaries, not $TMPDIR or %s;\n\
                              multiple options specify multiple directories\n\
  -u, --unique              with -c, check for strict ordering;\n\
                              without -c, output only the first of an equal run\n\
"), DEFAULT_TMPDIR);
      fputs (_("\
  -z, --zero-terminated     end lines with 0 byte, not newline\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
POS is F[.C][OPTS], where F is the field number and C the character position\n\
in the field.  OPTS is one or more single-letter ordering options, which\n\
override global ordering options for that key.  If no key is given, use the\n\
entire line as the key.\n\
\n\
SIZE may be followed by the following multiplicative suffixes:\n\
"), stdout);
      fputs (_("\
% 1% of memory, b 1, K 1024 (default), and so on for M, G, T, P, E, Z, Y.\n\
\n\
With no FILE, or when FILE is -, read standard input.\n\
\n\
*** WARNING ***\n\
The locale specified by the environment affects sort order.\n\
Set LC_ALL=C to get the traditional sort order that uses\n\
native byte values.\n\
"), stdout );
      printf (_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }

  exit (status);
}

#define COMMON_SHORT_OPTIONS "-bcdfgik:mMno:rsS:t:T:uz"

static struct option const long_options[] =
{
  {"ignore-leading-blanks", no_argument, NULL, 'b'},
  {"check", no_argument, NULL, 'c'},
  {"dictionary-order", no_argument, NULL, 'd'},
  {"ignore-case", no_argument, NULL, 'f'},
  {"general-numeric-sort", no_argument, NULL, 'g'},
  {"ignore-nonprinting", no_argument, NULL, 'i'},
  {"key", required_argument, NULL, 'k'},
  {"merge", no_argument, NULL, 'm'},
  {"month-sort", no_argument, NULL, 'M'},
  {"numeric-sort", no_argument, NULL, 'n'},
  {"output", required_argument, NULL, 'o'},
  {"reverse", no_argument, NULL, 'r'},
  {"stable", no_argument, NULL, 's'},
  {"buffer-size", required_argument, NULL, 'S'},
  {"field-separator", required_argument, NULL, 't'},
  {"temporary-directory", required_argument, NULL, 'T'},
  {"unique", no_argument, NULL, 'u'},
  {"zero-terminated", no_argument, NULL, 'z'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {0, 0, 0, 0},
};

/* The set of signals that are caught.  */
static sigset_t caught_signals;

/* The list of temporary files. */
struct tempnode
{
  struct tempnode *volatile next;
  char name[1];  /* Actual size is 1 + file name length.  */
};
static struct tempnode *volatile temphead;

/* Fucntion pointers. */
static void
(*inittables) (void);

static char *
(* begfield) (const struct line *line, const struct keyfield *key);

static char *
(* limfield) (const struct line *line, const struct keyfield *key);

static int
(*getmonth) (const char *s, size_t len);

static int
(* keycompare) (const struct line *a, const struct line *b);

/* Test for white space multibyte character.
   Set LENGTH the byte length of investigated multibyte character. */
#if HAVE_MBRTOWC
static int
ismbblank (const char *str, size_t len, size_t *length)
{
  size_t mblength;
  wchar_t wc;
  mbstate_t state;

  memset (&state, '\0', sizeof(mbstate_t));
  mblength = mbrtowc (&wc, str, len, &state);

  if (mblength == (size_t)-1 || mblength == (size_t)-2)
    {
      *length = 1;
      return 0;
    }

  *length = (mblength < 1) ? 1 : mblength;
  return iswblank (wc);
}
#endif

/* Clean up any remaining temporary files. */

static void
cleanup (void)
{
  struct tempnode const *node;

  for (node = temphead; node; node = node->next)
    unlink (node->name);
}

/* Report MESSAGE for FILE, then clean up and exit.
   If FILE is null, it represents standard output.  */

static void die (char const *, char const *) ATTRIBUTE_NORETURN;
static void
die (char const *message, char const *file)
{
  error (0, errno, "%s: %s", message, file ? file : _("standard output"));
  exit (SORT_FAILURE);
}

/* Create a new temporary file, returning its newly allocated name.
   Store into *PFP a stream open for writing.  */

static char *
create_temp_file (FILE **pfp)
{
  static char const slashbase[] = "/sortXXXXXX";
  static size_t temp_dir_index;
  sigset_t oldset;
  int fd;
  int saved_errno;
  char const *temp_dir = temp_dirs[temp_dir_index];
  size_t len = strlen (temp_dir);
  struct tempnode *node =
    xmalloc (sizeof node->next + len + sizeof slashbase);
  char *file = node->name;

  memcpy (file, temp_dir, len);
  memcpy (file + len, slashbase, sizeof slashbase);
  node->next = temphead;
  if (++temp_dir_index == temp_dir_count)
    temp_dir_index = 0;

  /* Create the temporary file in a critical section, to avoid races.  */
  sigprocmask (SIG_BLOCK, &caught_signals, &oldset);
  fd = mkstemp (file);
  if (0 <= fd)
    temphead = node;
  saved_errno = errno;
  sigprocmask (SIG_SETMASK, &oldset, NULL);
  errno = saved_errno;

  if (fd < 0 || (*pfp = fdopen (fd, "w")) == NULL)
    die (_("cannot create temporary file"), file);

  return file;
}

/* Return a stream for FILE, opened with mode HOW.  A null FILE means
   standard output; HOW should be "w".  When opening for input, "-"
   means standard input.  To avoid confusion, do not return file
   descriptors 0, 1, or 2.  */

static FILE *
xfopen (const char *file, const char *how)
{
  FILE *fp;

  if (!file)
    fp = stdout;
  else if (STREQ (file, "-") && *how == 'r')
    {
      have_read_stdin = true;
      fp = stdin;
    }
  else
    {
      if ((fp = fopen_safer (file, how)) == NULL)
	die (_("open failed"), file);
    }

  return fp;
}

/* Close FP, whose name is FILE, and report any errors.  */

static void
xfclose (FILE *fp, char const *file)
{
  if (fp == stdin)
    {
      /* Allow reading stdin from tty more than once. */
      if (feof (fp))
	clearerr (fp);
    }
  else
    {
      if (fclose (fp) != 0)
	die (_("close failed"), file);
    }
}

static void
write_bytes (const char *buf, size_t n_bytes, FILE *fp, const char *output_file)
{
  if (fwrite (buf, 1, n_bytes, fp) != n_bytes)
    die (_("write failed"), output_file);
}

/* Append DIR to the array of temporary directory names.  */
static void
add_temp_dir (char const *dir)
{
  if (temp_dir_count == temp_dir_alloc)
    temp_dirs = x2nrealloc (temp_dirs, &temp_dir_alloc, sizeof *temp_dirs);

  temp_dirs[temp_dir_count++] = dir;
}

/* Search through the list of temporary files for NAME;
   remove it if it is found on the list. */

static void
zaptemp (const char *name)
{
  struct tempnode *volatile *pnode;
  struct tempnode *node;

  for (pnode = &temphead; (node = *pnode); pnode = &node->next)
    if (node->name == name)
      {
	unlink (name);
	*pnode = node->next;
	free (node);
	break;
      }
}

#if HAVE_LANGINFO_CODESET

static int
struct_month_cmp (const void *m1, const void *m2)
{
  struct month const *month1 = m1;
  struct month const *month2 = m2;
  return strcmp (month1->name, month2->name);
}

#endif

/* Initialize the character class tables. */

static void
inittables_uni (void)
{
  int i;

  for (i = 0; i < UCHAR_LIM; ++i)
    {
      blanks[i] = !!ISBLANK (i);
      nonprinting[i] = !ISPRINT (i);
      nondictionary[i] = !ISALNUM (i) && !ISBLANK (i);
      fold_toupper[i] = (ISLOWER (i) ? toupper (i) : i);
    }

#if HAVE_NL_LANGINFO
  /* If we're not in the "C" locale, read different names for months.  */
  if (hard_LC_TIME)
    {
      for (i = 0; i < MONTHS_PER_YEAR; i++)
	{
	  char const *s;
	  size_t s_len;
	  size_t j;
	  char *name;

	  s = (char *) nl_langinfo (ABMON_1 + i);
	  s_len = strlen (s);
	  monthtab[i].name = name = xmalloc (s_len + 1);
	  monthtab[i].val = i + 1;

	  for (j = 0; j < s_len; j++)
	    name[j] = fold_toupper[to_uchar (s[j])];
	  name[j] = '\0';
	}
      qsort ((void *) monthtab, MONTHS_PER_YEAR,
	     sizeof *monthtab, struct_month_cmp);
    }
#endif
}

#if HAVE_MBRTOWC
static void
inittables_mb (void)
{
  int i, j, k, l;
  char *name, *s;
  size_t s_len, mblength;
  char mbc[MB_LEN_MAX];
  wchar_t wc, pwc;
  mbstate_t state_mb, state_wc;

  for (i = 0; i < MONTHS_PER_YEAR; i++)
    {
      s = (char *) nl_langinfo (ABMON_1 + i);
      s_len = strlen (s);
      monthtab[i].name = name = (char *) xmalloc (s_len + 1);
      monthtab[i].val = i + 1;

      memset (&state_mb, '\0', sizeof (mbstate_t));
      memset (&state_wc, '\0', sizeof (mbstate_t));

      for (j = 0; j < s_len;)
	{
	  if (!ismbblank (s + j, s_len - j, &mblength))
	    break;
	  j += mblength;
	}

      for (k = 0; j < s_len;)
	{
	  mblength = mbrtowc (&wc, (s + j), (s_len - j), &state_mb);
	  assert (mblength != (size_t)-1 && mblength != (size_t)-2);
	  if (mblength == 0)
	    break;

	  pwc = towupper (wc);
	  if (pwc == wc)
	    {
	      memcpy (mbc, s + j, mblength);
	      j += mblength;
	    }
	  else
	    {
	      j += mblength;
	      mblength = wcrtomb (mbc, pwc, &state_wc);
	      assert (mblength != (size_t)0 && mblength != (size_t)-1);
	    }

	  for (l = 0; l < mblength; l++)
	    name[k++] = mbc[l];
	}
      name[k] = '\0';
    }
  qsort ((void *) monthtab, MONTHS_PER_YEAR,
      sizeof (struct month), struct_month_cmp);
}
#endif

/* Specify the amount of main memory to use when sorting.  */
static void
specify_sort_size (char const *s)
{
  uintmax_t n;
  char *suffix;
  enum strtol_error e = xstrtoumax (s, &suffix, 10, &n, "EgGkKmMPtTYZ");

  /* The default unit is KiB.  */
  if (e == LONGINT_OK && ISDIGIT (suffix[-1]))
    {
      if (n <= UINTMAX_MAX / 1024)
	n *= 1024;
      else
	e = LONGINT_OVERFLOW;
    }

  /* A 'b' suffix means bytes; a '%' suffix means percent of memory.  */
  if (e == LONGINT_INVALID_SUFFIX_CHAR && ISDIGIT (suffix[-1]) && ! suffix[1])
    switch (suffix[0])
      {
      case 'b':
	e = LONGINT_OK;
	break;

      case '%':
	{
	  double mem = physmem_total () * n / 100;

	  /* Use "<", not "<=", to avoid problems with rounding.  */
	  if (mem < UINTMAX_MAX)
	    {
	      n = mem;
	      e = LONGINT_OK;
	    }
	  else
	    e = LONGINT_OVERFLOW;
	}
	break;
      }

  if (e == LONGINT_OK)
    {
      /* If multiple sort sizes are specified, take the maximum, so
	 that option order does not matter.  */
      if (n < sort_size)
	return;

      sort_size = n;
      if (sort_size == n)
	{
	  sort_size = MAX (sort_size, MIN_SORT_SIZE);
	  return;
	}

      e = LONGINT_OVERFLOW;
    }

  STRTOL_FATAL_ERROR (s, _("sort size"), e);
}

/* Return the default sort size.  */
static size_t
default_sort_size (void)
{
  /* Let MEM be available memory or 1/8 of total memory, whichever
     is greater.  */
  double avail = physmem_available ();
  double total = physmem_total ();
  double mem = MAX (avail, total / 8);
  struct rlimit rlimit;

  /* Let SIZE be MEM, but no more than the maximum object size or
     system resource limits.  Avoid the MIN macro here, as it is not
     quite right when only one argument is floating point.  Don't
     bother to check for values like RLIM_INFINITY since in practice
     they are not much less than SIZE_MAX.  */
  size_t size = SIZE_MAX;
  if (mem < size)
    size = mem;
  if (getrlimit (RLIMIT_DATA, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#ifdef RLIMIT_AS
  if (getrlimit (RLIMIT_AS, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#endif

  /* Leave a large safety margin for the above limits, as failure can
     occur when they are exceeded.  */
  size /= 2;

#ifdef RLIMIT_RSS
  /* Leave a 1/16 margin for RSS to leave room for code, stack, etc.
     Exceeding RSS is not fatal, but can be quite slow.  */
  if (getrlimit (RLIMIT_RSS, &rlimit) == 0 && rlimit.rlim_cur / 16 * 15 < size)
    size = rlimit.rlim_cur / 16 * 15;
#endif

  /* Use no less than the minimum.  */
  return MAX (size, MIN_SORT_SIZE);
}

/* Return the sort buffer size to use with the input files identified
   by FPS and FILES, which are alternate paths to the same files.
   NFILES gives the number of input files; NFPS may be less.  Assume
   that each input line requires LINE_BYTES extra bytes' worth of line
   information.  Do not exceed a bound on the size: if the bound is
   not specified by the user, use a default.  */

static size_t
sort_buffer_size (FILE *const *fps, int nfps,
		  char *const *files, int nfiles,
		  size_t line_bytes)
{
  /* A bound on the input size.  If zero, the bound hasn't been
     determined yet.  */
  static size_t size_bound;

  /* In the worst case, each input byte is a newline.  */
  size_t worst_case_per_input_byte = line_bytes + 1;

  /* Keep enough room for one extra input line and an extra byte.
     This extra room might be needed when preparing to read EOF.  */
  size_t size = worst_case_per_input_byte + 1;

  int i;

  for (i = 0; i < nfiles; i++)
    {
      struct stat st;
      off_t file_size;
      size_t worst_case;

      if ((i < nfps ? fstat (fileno (fps[i]), &st)
	   : STREQ (files[i], "-") ? fstat (STDIN_FILENO, &st)
	   : stat (files[i], &st))
	  != 0)
	die (_("stat failed"), files[i]);

      if (S_ISREG (st.st_mode))
	file_size = st.st_size;
      else
	{
	  /* The file has unknown size.  If the user specified a sort
	     buffer size, use that; otherwise, guess the size.  */
	  if (sort_size)
	    return sort_size;
	  file_size = INPUT_FILE_SIZE_GUESS;
	}

      if (! size_bound)
	{
	  size_bound = sort_size;
	  if (! size_bound)
	    size_bound = default_sort_size ();
	}

      /* Add the amount of memory needed to represent the worst case
	 where the input consists entirely of newlines followed by a
	 single non-newline.  Check for overflow.  */
      worst_case = file_size * worst_case_per_input_byte + 1;
      if (file_size != worst_case / worst_case_per_input_byte
	  || size_bound - size <= worst_case)
	return size_bound;
      size += worst_case;
    }

  return size;
}

/* Initialize BUF.  Reserve LINE_BYTES bytes for each line; LINE_BYTES
   must be at least sizeof (struct line).  Allocate ALLOC bytes
   initially.  */

static void
initbuf (struct buffer *buf, size_t line_bytes, size_t alloc)
{
  /* Ensure that the line array is properly aligned.  If the desired
     size cannot be allocated, repeatedly halve it until allocation
     succeeds.  The smaller allocation may hurt overall performance,
     but that's better than failing.  */
  for (;;)
    {
      alloc += sizeof (struct line) - alloc % sizeof (struct line);
      buf->buf = malloc (alloc);
      if (buf->buf)
	break;
      alloc /= 2;
      if (alloc <= line_bytes + 1)
	xalloc_die ();
    }

  buf->line_bytes = line_bytes;
  buf->alloc = alloc;
  buf->used = buf->left = buf->nlines = 0;
  buf->eof = false;
}

/* Return one past the limit of the line array.  */

static inline struct line *
buffer_linelim (struct buffer const *buf)
{
  return (struct line *) (buf->buf + buf->alloc);
}

/* Return a pointer to the first character of the field specified
   by KEY in LINE. */

static char *
begfield_uni (const struct line *line, const struct keyfield *key)
{
  register char *ptr = line->text, *lim = ptr + line->length - 1;
  register size_t sword = key->sword;
  register size_t schar = key->schar;
  register size_t remaining_bytes;

  /* The leading field separator itself is included in a field when -t
     is absent.  */

  if (!tab_default)
    while (ptr < lim && sword--)
      {
	while (ptr < lim && *ptr != tab[0])
	  ++ptr;
	if (ptr < lim)
	  ++ptr;
      }
  else
    while (ptr < lim && sword--)
      {
	while (ptr < lim && blanks[to_uchar (*ptr)])
	  ++ptr;
	while (ptr < lim && !blanks[to_uchar (*ptr)])
	  ++ptr;
      }

  if (key->skipsblanks)
    while (ptr < lim && blanks[to_uchar (*ptr)])
      ++ptr;

  /* Advance PTR by SCHAR (if possible), but no further than LIM.  */
  remaining_bytes = lim - ptr;
  if (schar < remaining_bytes)
    ptr += schar;
  else
    ptr = lim;

  return ptr;
}

#if HAVE_MBRTOWC
static char *
begfield_mb (const struct line *line, const struct keyfield *key)
{
  int i;
  char *ptr = line->text, *lim = ptr + line->length - 1;
  size_t sword = key->sword;
  size_t schar = key->schar;
  size_t mblength;
  mbstate_t state;

  memset (&state, '\0', sizeof(mbstate_t));

  if (!tab_default)
    while (ptr < lim && sword--)
      {
	while (ptr < lim && memcmp (ptr, tab, tab_length) != 0)
	  {
	    GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);
	    ptr += mblength;
	  }
	if (ptr < lim)
	  {
	    GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);
	    ptr += mblength;
	  }
      }
  else
    while (ptr < lim && sword--)
      {
	while (ptr < lim && ismbblank (ptr, lim - ptr, &mblength))
	  ptr += mblength;
	if (ptr < lim)
	  {
	    GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);
	    ptr += mblength;
	  }
	while (ptr < lim && !ismbblank (ptr, lim - ptr, &mblength))
	  ptr += mblength;
      }

  if (key->skipsblanks)
    while (ptr < lim && ismbblank (ptr, lim - ptr, &mblength))
      ptr += mblength;

  for (i = 0; i < schar; i++)
    {
      GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);

      if (ptr + mblength > lim)
	break;
      else
	ptr += mblength;
    }

  return ptr;
}
#endif

/* Return the limit of (a pointer to the first character after) the field
   in LINE specified by KEY. */

static char *
limfield_uni (const struct line *line, const struct keyfield *key)
{
  register char *ptr = line->text, *lim = ptr + line->length - 1;
  register size_t eword = key->eword, echar = key->echar;
  register size_t remaining_bytes;

  /* Move PTR past EWORD fields or to one past the last byte on LINE,
     whichever comes first.  If there are more than EWORD fields, leave
     PTR pointing at the beginning of the field having zero-based index,
     EWORD.  If a delimiter character was specified (via -t), then that
     `beginning' is the first character following the delimiting TAB.
     Otherwise, leave PTR pointing at the first `blank' character after
     the preceding field.  */
  if (!tab_default)
    while (ptr < lim && eword--)
      {
	while (ptr < lim && *ptr != tab[0])
	  ++ptr;
	if (ptr < lim && (eword | echar))
	  ++ptr;
      }
  else
    while (ptr < lim && eword--)
      {
	while (ptr < lim && blanks[to_uchar (*ptr)])
	  ++ptr;
	while (ptr < lim && !blanks[to_uchar (*ptr)])
	  ++ptr;
      }

#ifdef POSIX_UNSPECIFIED
  /* The following block of code makes GNU sort incompatible with
     standard Unix sort, so it's ifdef'd out for now.
     The POSIX spec isn't clear on how to interpret this.
     FIXME: request clarification.

     From: kwzh@gnu.ai.mit.edu (Karl Heuer)
     Date: Thu, 30 May 96 12:20:41 -0400
     [Translated to POSIX 1003.1-2001 terminology by Paul Eggert.]

     [...]I believe I've found another bug in `sort'.

     $ cat /tmp/sort.in
     a b c 2 d
     pq rs 1 t
     $ textutils-1.15/src/sort -k1.7,1.7 </tmp/sort.in
     a b c 2 d
     pq rs 1 t
     $ /bin/sort -k1.7,1.7 </tmp/sort.in
     pq rs 1 t
     a b c 2 d

     Unix sort produced the answer I expected: sort on the single character
     in column 7.  GNU sort produced different results, because it disagrees
     on the interpretation of the key-end spec "M.N".  Unix sort reads this
     as "skip M-1 fields, then N-1 characters"; but GNU sort wants it to mean
     "skip M-1 fields, then either N-1 characters or the rest of the current
     field, whichever comes first".  This extra clause applies only to
     key-ends, not key-starts.
     */

  /* Make LIM point to the end of (one byte past) the current field.  */
  if (!tab_default)
    {
      char *newlim;
      newlim = memchr (ptr, tab[0], lim - ptr);
      if (newlim)
	lim = newlim;
    }
  else
    {
      char *newlim;
      newlim = ptr;
      while (newlim < lim && blanks[to_uchar (*newlim)])
	++newlim;
      while (newlim < lim && !blanks[to_uchar (*newlim)])
	++newlim;
      lim = newlim;
    }
#endif

  /* If we're ignoring leading blanks when computing the End
     of the field, don't start counting bytes until after skipping
     past any leading blanks. */
  if (key->skipeblanks)
    while (ptr < lim && blanks[to_uchar (*ptr)])
      ++ptr;

  /* Advance PTR by ECHAR (if possible), but no further than LIM.  */
  remaining_bytes = lim - ptr;
  if (echar < remaining_bytes)
    ptr += echar;
  else
    ptr = lim;

  return ptr;
}

#if HAVE_MBRTOWC
static char *
limfield_mb (const struct line *line, const struct keyfield *key)
{
  char *ptr = line->text, *lim = ptr + line->length - 1;
  size_t eword = key->eword, echar = key->echar;
  int i;
  size_t mblength;
  mbstate_t state;

  memset (&state, '\0', sizeof(mbstate_t));

  if (!tab_default)
    while (ptr < lim && eword--)
      {
	while (ptr < lim && memcmp (ptr, tab, tab_length) != 0)
	  {
	    GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);
	    ptr += mblength;
	  }
	if (ptr < lim && (eword | echar))
	  {
	    GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);
	    ptr += mblength;
	  }
      }
  else
    while (ptr < lim && eword--)
      {
	while (ptr < lim && ismbblank (ptr, lim - ptr, &mblength))
	  ptr += mblength;
	if (ptr < lim)
	  {
	    GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);
	    ptr += mblength;
	  }
	while (ptr < lim && !ismbblank (ptr, lim - ptr, &mblength))
	  ptr += mblength;
      }


# ifdef POSIX_UNSPECIFIED
  /* Make LIM point to the end of (one byte past) the current field.  */
  if (!tab_default)
    {
      char *newlim, *p;

      newlim = NULL;
      for (p = ptr; p < lim;)
 	{
	  if (memcmp (p, tab, tab_length) == 0)
	    {
	      newlim = p;
	      break;
	    }

	  GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);
	  p += mblength;
	}
    }
  else
    {
      char *newlim;
      newlim = ptr;

      while (newlim < lim && ismbblank (newlim, lim - newlim, &mblength))
	newlim += mblength;
      if (ptr < lim)
	{
	  GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);
	  ptr += mblength;
	}
      while (newlim < lim && !ismbblank (newlim, lim - newlim, &mblength))
	newlim += mblength;
      lim = newlim;
    }
# endif

  /* If we're skipping leading blanks, don't start counting characters
   *      until after skipping past any leading blanks.  */
  if (key->skipeblanks)
    while (ptr < lim && ismbblank (ptr, lim - ptr, &mblength))
      ptr += mblength;

  memset (&state, '\0', sizeof(mbstate_t));

  /* Advance PTR by ECHAR (if possible), but no further than LIM.  */
  for (i = 0; i < echar; i++)
    {
      GET_BYTELEN_OF_CHAR (lim, ptr, mblength, state);

      if (ptr + mblength > lim)
	break;
      else
	ptr += mblength;
    }

  return ptr;
}
#endif

/* Fill BUF reading from FP, moving buf->left bytes from the end
   of buf->buf to the beginning first.  If EOF is reached and the
   file wasn't terminated by a newline, supply one.  Set up BUF's line
   table too.  FILE is the name of the file corresponding to FP.
   Return true if some input was read.  */

static bool
fillbuf (struct buffer *buf, register FILE *fp, char const *file)
{
  struct keyfield const *key = keylist;
  char eol = eolchar;
  size_t line_bytes = buf->line_bytes;
  size_t mergesize = merge_buffer_size - MIN_MERGE_BUFFER_SIZE;

  if (buf->eof)
    return false;

  if (buf->used != buf->left)
    {
      memmove (buf->buf, buf->buf + buf->used - buf->left, buf->left);
      buf->used = buf->left;
      buf->nlines = 0;
    }

  for (;;)
    {
      char *ptr = buf->buf + buf->used;
      struct line *linelim = buffer_linelim (buf);
      struct line *line = linelim - buf->nlines;
      size_t avail = (char *) linelim - buf->nlines * line_bytes - ptr;
      char *line_start = buf->nlines ? line->text + line->length : buf->buf;

      while (line_bytes + 1 < avail)
	{
	  /* Read as many bytes as possible, but do not read so many
	     bytes that there might not be enough room for the
	     corresponding line array.  The worst case is when the
	     rest of the input file consists entirely of newlines,
	     except that the last byte is not a newline.  */
	  size_t readsize = (avail - 1) / (line_bytes + 1);
	  size_t bytes_read = fread (ptr, 1, readsize, fp);
	  char *ptrlim = ptr + bytes_read;
	  char *p;
	  avail -= bytes_read;

	  if (bytes_read != readsize)
	    {
	      if (ferror (fp))
		die (_("read failed"), file);
	      if (feof (fp))
		{
		  buf->eof = true;
		  if (buf->buf == ptrlim)
		    return false;
		  if (ptrlim[-1] != eol)
		    *ptrlim++ = eol;
		}
	    }

	  /* Find and record each line in the just-read input.  */
	  while ((p = memchr (ptr, eol, ptrlim - ptr)))
	    {
	      ptr = p + 1;
	      line--;
	      line->text = line_start;
	      line->length = ptr - line_start;
	      mergesize = MAX (mergesize, line->length);
	      avail -= line_bytes;

	      if (key)
		{
		  /* Precompute the position of the first key for
                     efficiency. */
		  line->keylim = (key->eword == SIZE_MAX
				  ? p
				  : limfield (line, key));

		  if (key->sword != SIZE_MAX)
		    line->keybeg = begfield (line, key);
		  else
		    {
		      if (key->skipsblanks)
#if HAVE_MBRTOWC
			{
			  if (MB_CUR_MAX > 1)
			    {
			      size_t mblength;

			      while (ismbblank (line_start, ptr - line_start, &mblength))
				line_start += mblength;
			    }
			  else
#endif
			    {
			      while (blanks[to_uchar (*line_start)])
				line_start++;
			    }
			}
		      line->keybeg = line_start;
		    }
		}

	      line_start = ptr;
	    }

	  ptr = ptrlim;
	  if (buf->eof)
	    break;
	}

      buf->used = ptr - buf->buf;
      buf->nlines = buffer_linelim (buf) - line;
      if (buf->nlines != 0)
	{
	  buf->left = ptr - line_start;
	  merge_buffer_size = mergesize + MIN_MERGE_BUFFER_SIZE;
	  return true;
	}

      /* The current input line is too long to fit in the buffer.
	 Double the buffer size and try again.  */
      buf->buf = x2nrealloc (buf->buf, &buf->alloc, sizeof *(buf->buf));
    }
}

/* Compare strings A and B containing decimal fractions < 1.  Each string
   should begin with a decimal point followed immediately by the digits
   of the fraction.  Strings not of this form are considered to be zero. */

/* The goal here, is to take two numbers a and b... compare these
   in parallel.  Instead of converting each, and then comparing the
   outcome.  Most likely stopping the comparison before the conversion
   is complete.  The algorithm used, in the old sort:

   Algorithm: fraccompare
   Action   : compare two decimal fractions
   accepts  : char *a, char *b
   returns  : -1 if a<b, 0 if a=b, 1 if a>b.
   implement:

   if *a == decimal_point AND *b == decimal_point
     find first character different in a and b.
     if both are digits, return the difference *a - *b.
     if *a is a digit
       skip past zeros
       if digit return 1, else 0
     if *b is a digit
       skip past zeros
       if digit return -1, else 0
   if *a is a decimal_point
     skip past decimal_point and zeros
     if digit return 1, else 0
   if *b is a decimal_point
     skip past decimal_point and zeros
     if digit return -1, else 0
   return 0 */

static int
fraccompare (register const char *a, register const char *b)
{
  if (*a == decimal_point && *b == decimal_point)
    {
      while (*++a == *++b)
	if (! ISDIGIT (*a))
	  return 0;
      if (ISDIGIT (*a) && ISDIGIT (*b))
	return *a - *b;
      if (ISDIGIT (*a))
	goto a_trailing_nonzero;
      if (ISDIGIT (*b))
	goto b_trailing_nonzero;
      return 0;
    }
  else if (*a++ == decimal_point)
    {
    a_trailing_nonzero:
      while (*a == NUMERIC_ZERO)
	a++;
      return ISDIGIT (*a);
    }
  else if (*b++ == decimal_point)
    {
    b_trailing_nonzero:
      while (*b == NUMERIC_ZERO)
	b++;
      return - ISDIGIT (*b);
    }
  return 0;
}

/* Compare strings A and B as numbers without explicitly converting them to
   machine numbers.  Comparatively slow for short strings, but asymptotically
   hideously fast. */

static int
numcompare (register const char *a, register const char *b)
{
  char tmpa;
  char tmpb;
  int tmp;
  size_t log_a;
  size_t log_b;

#if HAVE_MBRTOWC
  if (MB_CUR_MAX > 1)
    {
      size_t mblength;
      size_t alen = strnlen (a, MB_LEN_MAX);
      size_t blen = strnlen (b, MB_LEN_MAX);

      while (ismbblank (a, alen, &mblength))
	a += mblength, alen -= mblength;
      while (ismbblank (b, blen, &mblength))
	b += mblength, blen -= mblength;

      tmpa = *a;
      tmpb = *b;
    }
  else
#endif
    {
      tmpa = *a;
      tmpb = *b;

      while (blanks[to_uchar (tmpa)])
	tmpa = *++a;
      while (blanks[to_uchar (tmpb)])
	tmpb = *++b;
    }

  if (tmpa == NEGATION_SIGN)
    {
      do
	tmpa = *++a;
      while (tmpa == NUMERIC_ZERO || IS_THOUSANDS_SEP (tmpa));
      if (tmpb != NEGATION_SIGN)
	{
	  if (tmpa == decimal_point)
	    do
	      tmpa = *++a;
	    while (tmpa == NUMERIC_ZERO);
	  if (ISDIGIT (tmpa))
	    return -1;
	  while (tmpb == NUMERIC_ZERO || IS_THOUSANDS_SEP (tmpb))
	    tmpb = *++b;
	  if (tmpb == decimal_point)
	    do
	      tmpb = *++b;
	    while (tmpb == NUMERIC_ZERO);
	  if (ISDIGIT (tmpb))
	    return -1;
	  return 0;
	}
      do
	tmpb = *++b;
      while (tmpb == NUMERIC_ZERO || IS_THOUSANDS_SEP (tmpb));

      while (tmpa == tmpb && ISDIGIT (tmpa))
	{
	  do
	    tmpa = *++a;
	  while (IS_THOUSANDS_SEP (tmpa));
	  do
	    tmpb = *++b;
	  while (IS_THOUSANDS_SEP (tmpb));
	}

      if ((tmpa == decimal_point && !ISDIGIT (tmpb))
	  || (tmpb == decimal_point && !ISDIGIT (tmpa)))
	return -fraccompare (a, b);

      tmp = tmpb - tmpa;

      for (log_a = 0; ISDIGIT (tmpa); ++log_a)
	do
	  tmpa = *++a;
	while (IS_THOUSANDS_SEP (tmpa));

      for (log_b = 0; ISDIGIT (tmpb); ++log_b)
	do
	  tmpb = *++b;
	while (IS_THOUSANDS_SEP (tmpb));

      if (log_a != log_b)
	return log_a < log_b ? 1 : -1;

      if (!log_a)
	return 0;

      return tmp;
    }
  else if (tmpb == NEGATION_SIGN)
    {
      do
	tmpb = *++b;
      while (tmpb == NUMERIC_ZERO || IS_THOUSANDS_SEP (tmpb));
      if (tmpb == decimal_point)
	do
	  tmpb = *++b;
	while (tmpb == NUMERIC_ZERO);
      if (ISDIGIT (tmpb))
	return 1;
      while (tmpa == NUMERIC_ZERO || IS_THOUSANDS_SEP (tmpa))
	tmpa = *++a;
      if (tmpa == decimal_point)
	do
	  tmpa = *++a;
	while (tmpa == NUMERIC_ZERO);
      if (ISDIGIT (tmpa))
	return 1;
      return 0;
    }
  else
    {
      while (tmpa == NUMERIC_ZERO || IS_THOUSANDS_SEP (tmpa))
	tmpa = *++a;
      while (tmpb == NUMERIC_ZERO || IS_THOUSANDS_SEP (tmpb))
	tmpb = *++b;

      while (tmpa == tmpb && ISDIGIT (tmpa))
	{
	  do
	    tmpa = *++a;
	  while (IS_THOUSANDS_SEP (tmpa));
	  do
	    tmpb = *++b;
	  while (IS_THOUSANDS_SEP (tmpb));
	}

      if ((tmpa == decimal_point && !ISDIGIT (tmpb))
	  || (tmpb == decimal_point && !ISDIGIT (tmpa)))
	return fraccompare (a, b);

      tmp = tmpa - tmpb;

      for (log_a = 0; ISDIGIT (tmpa); ++log_a)
	do
	  tmpa = *++a;
	while (IS_THOUSANDS_SEP (tmpa));

      for (log_b = 0; ISDIGIT (tmpb); ++log_b)
	do
	  tmpb = *++b;
	while (IS_THOUSANDS_SEP (tmpb));

      if (log_a != log_b)
	return log_a < log_b ? -1 : 1;

      if (!log_a)
	return 0;

      return tmp;
    }
}

static int
general_numcompare (const char *sa, const char *sb)
{
  /* FIXME: add option to warn about failed conversions.  */
  /* FIXME: maybe add option to try expensive FP conversion
     only if A and B can't be compared more cheaply/accurately.  */

  char *bufa, *ea;
  char *bufb, *eb;
  double a;
  double b;

  char *p;
  struct lconv *lconvp = localeconv ();
  size_t thousands_sep_len = strlen (lconvp->thousands_sep);

  bufa = (char *) xmalloc (strlen (sa) + 1);
  bufb = (char *) xmalloc (strlen (sb) + 1);
  strcpy (bufa, sa);
  strcpy (bufb, sb);

  if (force_general_numcompare)
    {
      while (1)
	{
	  a = strtod (bufa, &ea);
	  if (memcmp (ea, lconvp->thousands_sep, thousands_sep_len) == 0)
	    {
	      for (p = ea; *(p + thousands_sep_len) != '\0'; p++)
		*p = *(p + thousands_sep_len);
	      *p = '\0';
	      continue;
	    }
	  break;
	}

      while (1)
	{
	  b = strtod (bufb, &eb);
	  if (memcmp (eb, lconvp->thousands_sep, thousands_sep_len) == 0)
	    {
	      for (p = eb; *(p + thousands_sep_len) != '\0'; p++)
		*p = *(p + thousands_sep_len);
	      *p = '\0';
	      continue;
	    }
	  break;
	}
    }
  else
    {
      a = strtod (bufa, &ea);
      b = strtod (bufb, &eb);
    }

  /* Put conversion errors at the start of the collating sequence.  */
  free (bufa);
  free (bufb);
  if (bufa == ea)
    return bufb == eb ? 0 : -1;
  if (bufb == eb)
    return 1;

  /* Sort numbers in the usual way, where -0 == +0.  Put NaNs after
     conversion errors but before numbers; sort them by internal
     bit-pattern, for lack of a more portable alternative.  */
  return (a < b ? -1
	  : a > b ? 1
	  : a == b ? 0
	  : b == b ? -1
	  : a == a ? 1
	  : memcmp ((char *) &a, (char *) &b, sizeof a));
}

/* Return an integer in 1..12 of the month name S with length LEN.
   Return 0 if the name in S is not recognized.  */

static int
getmonth_uni (const char *s, size_t len)
{
  char *month;
  register size_t i;
  register int lo = 0, hi = MONTHS_PER_YEAR, result;

  while (len > 0 && blanks[to_uchar (*s)])
    {
      ++s;
      --len;
    }

  if (len == 0)
    return 0;

  month = alloca (len + 1);
  for (i = 0; i < len; ++i)
    month[i] = fold_toupper[to_uchar (s[i])];
  month[len] = '\0';

  do
    {
      int ix = (lo + hi) / 2;

      if (strncmp (month, monthtab[ix].name, strlen (monthtab[ix].name)) < 0)
	hi = ix;
      else
	lo = ix;
    }
  while (hi - lo > 1);

  result = (!strncmp (month, monthtab[lo].name, strlen (monthtab[lo].name))
	    ? monthtab[lo].val : 0);

  return result;
}

#if HAVE_MBRTOWC
static int
getmonth_mb (const char *s, size_t len)
{
  char *month;
  register size_t i;
  register int lo = 0, hi = MONTHS_PER_YEAR, result;
  char *tmp;
  size_t wclength, mblength;
  const char **pp;
  const wchar_t **wpp;
  wchar_t *month_wcs;
  mbstate_t state;

  while (len > 0 && ismbblank (s, len, &mblength))
    {
      s += mblength;
      len -= mblength;
    }

  if (len == 0)
    return 0;

  month = (char *) alloca (len + 1);

  tmp = (char *) alloca (len + 1);
  memcpy (tmp, s, len);
  tmp[len] = '\0';
  pp = (const char **)&tmp;
  month_wcs = (wchar_t *) alloca ((len + 1) * sizeof (wchar_t));
  memset (&state, '\0', sizeof(mbstate_t));

  wclength = mbsrtowcs (month_wcs, pp, len + 1, &state);
  assert (wclength != (size_t)-1 && *pp == NULL);

  for (i = 0; i < wclength; i++)
      month_wcs[i] = towupper(month_wcs[i]);
  month_wcs[i] = L'\0';

  wpp = (const wchar_t **)&month_wcs;

  mblength = wcsrtombs (month, wpp, len + 1, &state);
  assert (mblength != (-1) && *wpp == NULL);

  do
    {
      int ix = (lo + hi) / 2;

      if (strncmp (month, monthtab[ix].name, strlen (monthtab[ix].name)) < 0)
	hi = ix;
      else
	lo = ix;
    }
  while (hi - lo > 1);

  result = (!strncmp (month, monthtab[lo].name, strlen (monthtab[lo].name))
      ? monthtab[lo].val : 0);

  return result;
}
#endif

/* Compare two lines A and B trying every key in sequence until there
   are no more keys or a difference is found. */

static int
keycompare_uni (const struct line *a, const struct line *b)
{
  struct keyfield const *key = keylist;

  /* For the first iteration only, the key positions have been
     precomputed for us. */
  register char *texta = a->keybeg;
  register char *textb = b->keybeg;
  register char *lima = a->keylim;
  register char *limb = b->keylim;

  int diff;

  for (;;)
    {
      register char const *translate = key->translate;
      register bool const *ignore = key->ignore;

      /* Find the lengths. */
      size_t lena = lima <= texta ? 0 : lima - texta;
      size_t lenb = limb <= textb ? 0 : limb - textb;

      /* Actually compare the fields. */
      if (key->numeric | key->general_numeric)
	{
	  char savea = *lima, saveb = *limb;

	  *lima = *limb = '\0';
	  diff = ((key->numeric ? numcompare : general_numcompare)
		  (texta, textb));
	  *lima = savea, *limb = saveb;
	}
      else if (key->month)
	diff = getmonth (texta, lena) - getmonth (textb, lenb);
      /* Sorting like this may become slow, so in a simple locale the user
         can select a faster sort that is similar to ascii sort  */
      else if (HAVE_SETLOCALE && hard_LC_COLLATE)
	{
	  if (ignore || translate)
	    {
	      char *copy_a = alloca (lena + 1 + lenb + 1);
	      char *copy_b = copy_a + lena + 1;
	      size_t new_len_a, new_len_b, i;

	      /* Ignore and/or translate chars before comparing.  */
	      for (new_len_a = new_len_b = i = 0; i < MAX (lena, lenb); i++)
		{
		  if (i < lena)
		    {
		      copy_a[new_len_a] = (translate
					   ? translate[to_uchar (texta[i])]
					   : texta[i]);
		      if (!ignore || !ignore[to_uchar (texta[i])])
			++new_len_a;
		    }
		  if (i < lenb)
		    {
		      copy_b[new_len_b] = (translate
					   ? translate[to_uchar (textb[i])]
					   : textb [i]);
		      if (!ignore || !ignore[to_uchar (textb[i])])
			++new_len_b;
		    }
		}

	      diff = xmemcoll (copy_a, new_len_a, copy_b, new_len_b);
	    }
	  else if (lena == 0)
	    diff = - NONZERO (lenb);
	  else if (lenb == 0)
	    goto greater;
	  else
	    diff = xmemcoll (texta, lena, textb, lenb);
	}
      else if (ignore)
	{
#define CMP_WITH_IGNORE(A, B)						\
  do									\
    {									\
	  for (;;)							\
	    {								\
	      while (texta < lima && ignore[to_uchar (*texta)])		\
		++texta;						\
	      while (textb < limb && ignore[to_uchar (*textb)])		\
		++textb;						\
	      if (! (texta < lima && textb < limb))			\
		break;							\
	      diff = to_uchar (A) - to_uchar (B);			\
	      if (diff)							\
		goto not_equal;						\
	      ++texta;							\
	      ++textb;							\
	    }								\
									\
	  diff = (texta < lima) - (textb < limb);			\
    }									\
  while (0)

	  if (translate)
	    CMP_WITH_IGNORE (translate[to_uchar (*texta)],
			     translate[to_uchar (*textb)]);
	  else
	    CMP_WITH_IGNORE (*texta, *textb);
	}
      else if (lena == 0)
	diff = - NONZERO (lenb);
      else if (lenb == 0)
	goto greater;
      else
	{
	  if (translate)
	    {
	      while (texta < lima && textb < limb)
		{
		  diff = (to_uchar (translate[to_uchar (*texta++)])
			  - to_uchar (translate[to_uchar (*textb++)]));
		  if (diff)
		    goto not_equal;
		}
	    }
	  else
	    {
	      diff = memcmp (texta, textb, MIN (lena, lenb));
	      if (diff)
		goto not_equal;
	    }
	  diff = lena < lenb ? -1 : lena != lenb;
	}

      if (diff)
	goto not_equal;

      key = key->next;
      if (! key)
	break;

      /* Find the beginning and limit of the next field.  */
      if (key->eword != SIZE_MAX)
	lima = limfield (a, key), limb = limfield (b, key);
      else
	lima = a->text + a->length - 1, limb = b->text + b->length - 1;

      if (key->sword != SIZE_MAX)
	texta = begfield (a, key), textb = begfield (b, key);
      else
	{
	  texta = a->text, textb = b->text;
	  if (key->skipsblanks)
	    {
	      while (texta < lima && blanks[to_uchar (*texta)])
		++texta;
	      while (textb < limb && blanks[to_uchar (*textb)])
		++textb;
	    }
	}
    }

  return 0;

 greater:
  diff = 1;
 not_equal:
  return key->reverse ? -diff : diff;
}

#if HAVE_MBRTOWC
static int
keycompare_mb (const struct line *a, const struct line *b)
{
  struct keyfield *key = keylist;

  /* For the first iteration only, the key positions have been
     precomputed for us. */
  char *texta = a->keybeg;
  char *textb = b->keybeg;
  char *lima = a->keylim;
  char *limb = b->keylim;

  size_t mblength_a, mblength_b;
  wchar_t wc_a, wc_b;
  mbstate_t state_a, state_b;

  int diff;

  memset (&state_a, '\0', sizeof(mbstate_t));
  memset (&state_b, '\0', sizeof(mbstate_t));

  for (;;)
    {
      unsigned char *translate = (unsigned char *) key->translate;
      bool const *ignore = key->ignore;

      /* Find the lengths. */
      size_t lena = lima <= texta ? 0 : lima - texta;
      size_t lenb = limb <= textb ? 0 : limb - textb;

      /* Actually compare the fields. */
      if (key->numeric | key->general_numeric)
	{
	  char savea = *lima, saveb = *limb;

	  *lima = *limb = '\0';
	  if (force_general_numcompare)
	    diff = general_numcompare (texta, textb);
	  else
	    diff = ((key->numeric ? numcompare : general_numcompare)
		(texta, textb));
	  *lima = savea, *limb = saveb;
	}
      else if (key->month)
	diff = getmonth (texta, lena) - getmonth (textb, lenb);
      else
	{
	  if (ignore || translate)
	    {
	      char *copy_a = (char *) alloca (lena + 1 + lenb + 1);
	      char *copy_b = copy_a + lena + 1;
	      size_t new_len_a, new_len_b;
	      size_t i, j;

	      /* Ignore and/or translate chars before comparing.  */
# define IGNORE_CHARS(NEW_LEN, LEN, TEXT, COPY, WC, MBLENGTH, STATE)	\
  do									\
    {									\
      wchar_t uwc;							\
      char mbc[MB_LEN_MAX];						\
      mbstate_t state_wc;						\
									\
      for (NEW_LEN = i = 0; i < LEN;)					\
	{								\
	  mbstate_t state_bak;						\
									\
	  state_bak = STATE;						\
	  MBLENGTH = mbrtowc (&WC, TEXT + i, LEN - i, &STATE);		\
									\
	  if (MBLENGTH == (size_t)-2 || MBLENGTH == (size_t)-1		\
	      || MBLENGTH == 0)						\
	    {								\
	      if (MBLENGTH == (size_t)-2 || MBLENGTH == (size_t)-1)	\
		STATE = state_bak;					\
	      if (!ignore)						\
		COPY[NEW_LEN++] = TEXT[i++];				\
	      continue;							\
	    }								\
									\
	  if (ignore)							\
	    {								\
	      if ((ignore == nonprinting && !iswprint (WC))		\
		   || (ignore == nondictionary				\
		       && !iswalnum (WC) && !iswblank (WC)))		\
		{							\
		  i += MBLENGTH;					\
		  continue;						\
		}							\
	    }								\
									\
	  if (translate)						\
	    {								\
									\
	      uwc = toupper(WC);					\
	      if (WC == uwc)						\
		{							\
		  memcpy (mbc, TEXT + i, MBLENGTH);			\
		  i += MBLENGTH;					\
		}							\
	      else							\
		{							\
		  i += MBLENGTH;					\
		  WC = uwc;						\
		  memset (&state_wc, '\0', sizeof (mbstate_t));		\
									\
		  MBLENGTH = wcrtomb (mbc, WC, &state_wc);		\
		  assert (MBLENGTH != (size_t)-1 && MBLENGTH != 0);	\
		}							\
									\
	      for (j = 0; j < MBLENGTH; j++)				\
		COPY[NEW_LEN++] = mbc[j];				\
	    }								\
	  else								\
	    for (j = 0; j < MBLENGTH; j++)				\
	      COPY[NEW_LEN++] = TEXT[i++];				\
	}								\
      COPY[NEW_LEN] = '\0';						\
    }									\
  while (0)
	      IGNORE_CHARS (new_len_a, lena, texta, copy_a,
			    wc_a, mblength_a, state_a);
	      IGNORE_CHARS (new_len_b, lenb, textb, copy_b,
			    wc_b, mblength_b, state_b);
	      diff = xmemcoll (copy_a, new_len_a, copy_b, new_len_b);
	    }
	  else if (lena == 0)
	    diff = - NONZERO (lenb);
	  else if (lenb == 0)
	    goto greater;
	  else
	    diff = xmemcoll (texta, lena, textb, lenb);
	}

      if (diff)
	goto not_equal;

      key = key->next;
      if (! key)
	break;

      /* Find the beginning and limit of the next field.  */
      if (key->eword != -1)
	lima = limfield (a, key), limb = limfield (b, key);
      else
	lima = a->text + a->length - 1, limb = b->text + b->length - 1;

      if (key->sword != -1)
	texta = begfield (a, key), textb = begfield (b, key);
      else
	{
	  texta = a->text, textb = b->text;
	  if (key->skipsblanks)
	    {
	      while (texta < lima && ismbblank (texta, lima - texta, &mblength_a))
		texta += mblength_a;
	      while (textb < limb && ismbblank (textb, limb - textb, &mblength_b))
		textb += mblength_b;
	    }
	}
    }

  return 0;

greater:
  diff = 1;
not_equal:
  return key->reverse ? -diff : diff;
}
#endif

/* Compare two lines A and B, returning negative, zero, or positive
   depending on whether A compares less than, equal to, or greater than B. */

static int
compare (register const struct line *a, register const struct line *b)
{
  int diff;
  size_t alen, blen;

  /* First try to compare on the specified keys (if any).
     The only two cases with no key at all are unadorned sort,
     and unadorned sort -r. */
  if (keylist)
    {
      diff = keycompare (a, b);
      alloca (0);
      if (diff | unique | stable)
	return diff;
    }

  /* If the keys all compare equal (or no keys were specified)
     fall through to the default comparison.  */
  alen = a->length - 1, blen = b->length - 1;

  if (alen == 0)
    diff = - NONZERO (blen);
  else if (blen == 0)
    diff = 1;
  else if (HAVE_SETLOCALE && hard_LC_COLLATE)
    diff = xmemcoll (a->text, alen, b->text, blen);
  else if (! (diff = memcmp (a->text, b->text, MIN (alen, blen))))
    diff = alen < blen ? -1 : alen != blen;

  return reverse ? -diff : diff;
}

/* Check that the lines read from FILE_NAME come in order.  Print a
   diagnostic (FILE_NAME, line number, contents of line) to stderr and return
   false if they are not in order.  Otherwise, print no diagnostic
   and return true.  */

static bool
check (char const *file_name)
{
  FILE *fp = xfopen (file_name, "r");
  struct buffer buf;		/* Input buffer. */
  struct line temp;		/* Copy of previous line. */
  size_t alloc = 0;
  uintmax_t line_number = 0;
  struct keyfield const *key = keylist;
  bool nonunique = ! unique;
  bool ordered = true;

  initbuf (&buf, sizeof (struct line),
	   MAX (merge_buffer_size, sort_size));
  temp.text = NULL;

  while (fillbuf (&buf, fp, file_name))
    {
      struct line const *line = buffer_linelim (&buf);
      struct line const *linebase = line - buf.nlines;

      /* Make sure the line saved from the old buffer contents is
	 less than or equal to the first line of the new buffer. */
      if (alloc && nonunique <= compare (&temp, line - 1))
	{
	found_disorder:
	  {
	    struct line const *disorder_line = line - 1;
	    uintmax_t disorder_line_number =
	      buffer_linelim (&buf) - disorder_line + line_number;
	    char hr_buf[INT_BUFSIZE_BOUND (uintmax_t)];
	    fprintf (stderr, _("%s: %s:%s: disorder: "),
		     program_name, file_name,
		     umaxtostr (disorder_line_number, hr_buf));
	    write_bytes (disorder_line->text, disorder_line->length, stderr,
			 _("standard error"));
	    ordered = false;
	    break;
	  }
	}

      /* Compare each line in the buffer with its successor.  */
      while (linebase < --line)
	if (nonunique <= compare (line, line - 1))
	  goto found_disorder;

      line_number += buf.nlines;

      /* Save the last line of the buffer.  */
      if (alloc < line->length)
	{
	  do
	    {
	      alloc *= 2;
	      if (! alloc)
		{
		  alloc = line->length;
		  break;
		}
	    }
	  while (alloc < line->length);

	  temp.text = xrealloc (temp.text, alloc);
	}
      memcpy (temp.text, line->text, line->length);
      temp.length = line->length;
      if (key)
	{
	  temp.keybeg = temp.text + (line->keybeg - line->text);
	  temp.keylim = temp.text + (line->keylim - line->text);
	}
    }

  xfclose (fp, file_name);
  free (buf.buf);
  if (temp.text)
    free (temp.text);
  return ordered;
}

/* Merge lines from FILES onto OFP.  NFILES cannot be greater than
   NMERGE.  Close input and output files before returning.
   OUTPUT_FILE gives the name of the output file.  If it is NULL,
   the output file is standard output.  If OFP is NULL, the output
   file has not been opened yet (or written to, if standard output).  */

static void
mergefps (char **files, register int nfiles,
	  FILE *ofp, const char *output_file)
{
  FILE *fps[NMERGE];		/* Input streams for each file.  */
  struct buffer buffer[NMERGE];	/* Input buffers for each file. */
  struct line saved;		/* Saved line storage for unique check. */
  struct line const *savedline = NULL;
				/* &saved if there is a saved line. */
  size_t savealloc = 0;		/* Size allocated for the saved line. */
  struct line const *cur[NMERGE]; /* Current line in each line table. */
  struct line const *base[NMERGE]; /* Base of each line table.  */
  int ord[NMERGE];		/* Table representing a permutation of fps,
				   such that cur[ord[0]] is the smallest line
				   and will be next output. */
  register int i, j, t;
  struct keyfield const *key = keylist;
  saved.text = NULL;

  /* Read initial lines from each input file. */
  for (i = 0; i < nfiles; )
    {
      fps[i] = xfopen (files[i], "r");
      initbuf (&buffer[i], sizeof (struct line),
	       MAX (merge_buffer_size, sort_size / nfiles));
      if (fillbuf (&buffer[i], fps[i], files[i]))
	{
	  struct line const *linelim = buffer_linelim (&buffer[i]);
	  cur[i] = linelim - 1;
	  base[i] = linelim - buffer[i].nlines;
	  i++;
	}
      else
	{
	  /* fps[i] is empty; eliminate it from future consideration.  */
	  xfclose (fps[i], files[i]);
	  zaptemp (files[i]);
	  free (buffer[i].buf);
	  --nfiles;
	  for (j = i; j < nfiles; ++j)
	    files[j] = files[j + 1];
	}
    }

  if (! ofp)
    ofp = xfopen (output_file, "w");

  /* Set up the ord table according to comparisons among input lines.
     Since this only reorders two items if one is strictly greater than
     the other, it is stable. */
  for (i = 0; i < nfiles; ++i)
    ord[i] = i;
  for (i = 1; i < nfiles; ++i)
    if (0 < compare (cur[ord[i - 1]], cur[ord[i]]))
      t = ord[i - 1], ord[i - 1] = ord[i], ord[i] = t, i = 0;

  /* Repeatedly output the smallest line until no input remains. */
  while (nfiles)
    {
      struct line const *smallest = cur[ord[0]];

      /* If uniquified output is turned on, output only the first of
	 an identical series of lines. */
      if (unique)
	{
	  if (savedline && compare (savedline, smallest))
	    {
	      savedline = 0;
	      write_bytes (saved.text, saved.length, ofp, output_file);
	    }
	  if (!savedline)
	    {
	      savedline = &saved;
	      if (savealloc < smallest->length)
		{
		  do
		    if (! savealloc)
		      {
			savealloc = smallest->length;
			break;
		      }
		  while ((savealloc *= 2) < smallest->length);

		  saved.text = xrealloc (saved.text, savealloc);
		}
	      saved.length = smallest->length;
	      memcpy (saved.text, smallest->text, saved.length);
	      if (key)
		{
		  saved.keybeg =
		    saved.text + (smallest->keybeg - smallest->text);
		  saved.keylim =
		    saved.text + (smallest->keylim - smallest->text);
		}
	    }
	}
      else
	write_bytes (smallest->text, smallest->length, ofp, output_file);

      /* Check if we need to read more lines into core. */
      if (base[ord[0]] < smallest)
	cur[ord[0]] = smallest - 1;
      else
	{
	  if (fillbuf (&buffer[ord[0]], fps[ord[0]], files[ord[0]]))
	    {
	      struct line const *linelim = buffer_linelim (&buffer[ord[0]]);
	      cur[ord[0]] = linelim - 1;
	      base[ord[0]] = linelim - buffer[ord[0]].nlines;
	    }
	  else
	    {
	      /* We reached EOF on fps[ord[0]]. */
	      for (i = 1; i < nfiles; ++i)
		if (ord[i] > ord[0])
		  --ord[i];
	      --nfiles;
	      xfclose (fps[ord[0]], files[ord[0]]);
	      zaptemp (files[ord[0]]);
	      free (buffer[ord[0]].buf);
	      for (i = ord[0]; i < nfiles; ++i)
		{
		  fps[i] = fps[i + 1];
		  files[i] = files[i + 1];
		  buffer[i] = buffer[i + 1];
		  cur[i] = cur[i + 1];
		  base[i] = base[i + 1];
		}
	      for (i = 0; i < nfiles; ++i)
		ord[i] = ord[i + 1];
	      continue;
	    }
	}

      /* The new line just read in may be larger than other lines
	 already in core; push it back in the queue until we encounter
	 a line larger than it. */
      for (i = 1; i < nfiles; ++i)
	{
	  t = compare (cur[ord[0]], cur[ord[i]]);
	  if (!t)
	    t = ord[0] - ord[i];
	  if (t < 0)
	    break;
	}
      t = ord[0];
      for (j = 1; j < i; ++j)
	ord[j - 1] = ord[j];
      ord[i - 1] = t;
    }

  if (unique && savedline)
    {
      write_bytes (saved.text, saved.length, ofp, output_file);
      free (saved.text);
    }

  xfclose (ofp, output_file);
}

/* Merge into T the two sorted arrays of lines LO (with NLO members)
   and HI (with NHI members).  T, LO, and HI point just past their
   respective arrays, and the arrays are in reverse order.  NLO and
   NHI must be positive, and HI - NHI must equal T - (NLO + NHI).  */

static inline void
mergelines (struct line *t,
	    struct line const *lo, size_t nlo,
	    struct line const *hi, size_t nhi)
{
  for (;;)
    if (compare (lo - 1, hi - 1) <= 0)
      {
	*--t = *--lo;
	if (! --nlo)
	  {
	    /* HI - NHI equalled T - (NLO + NHI) when this function
	       began.  Therefore HI must equal T now, and there is no
	       need to copy from HI to T.  */
	    return;
	  }
      }
    else
      {
	*--t = *--hi;
	if (! --nhi)
	  {
	    do
	      *--t = *--lo;
	    while (--nlo);

	    return;
	  }
      }
}

/* Sort the array LINES with NLINES members, using TEMP for temporary space.
   NLINES must be at least 2.
   The input and output arrays are in reverse order, and LINES and
   TEMP point just past the end of their respective arrays.

   Use a recursive divide-and-conquer algorithm, in the style
   suggested by Knuth volume 3 (2nd edition), exercise 5.2.4-23.  Use
   the optimization suggested by exercise 5.2.4-10; this requires room
   for only 1.5*N lines, rather than the usual 2*N lines.  Knuth
   writes that this memory optimization was originally published by
   D. A. Bell, Comp J. 1 (1958), 75.  */

static void
sortlines (struct line *lines, size_t nlines, struct line *temp)
{
  if (nlines == 2)
    {
      if (0 < compare (&lines[-1], &lines[-2]))
	{
	  struct line tmp = lines[-1];
	  lines[-1] = lines[-2];
	  lines[-2] = tmp;
	}
    }
  else
    {
      size_t nlo = nlines / 2;
      size_t nhi = nlines - nlo;
      struct line *lo = lines;
      struct line *hi = lines - nlo;
      struct line *sorted_lo = temp;

      sortlines (hi, nhi, temp);
      if (1 < nlo)
	sortlines_temp (lo, nlo, sorted_lo);
      else
	sorted_lo[-1] = lo[-1];

      mergelines (lines, sorted_lo, nlo, hi, nhi);
    }
}

/* Like sortlines (LINES, NLINES, TEMP), except output into TEMP
   rather than sorting in place.  */

static void
sortlines_temp (struct line *lines, size_t nlines, struct line *temp)
{
  if (nlines == 2)
    {
      bool swap = (0 < compare (&lines[-1], &lines[-2]));
      temp[-1] = lines[-1 - swap];
      temp[-2] = lines[-2 + swap];
    }
  else
    {
      size_t nlo = nlines / 2;
      size_t nhi = nlines - nlo;
      struct line *lo = lines;
      struct line *hi = lines - nlo;
      struct line *sorted_hi = temp - nlo;

      sortlines_temp (hi, nhi, sorted_hi);
      if (1 < nlo)
	sortlines (lo, nlo, temp);

      mergelines (temp, lo, nlo, sorted_hi, nhi);
    }
}

/* Return the index of the first of NFILES FILES that is the same file
   as OUTFILE.  If none can be the same, return NFILES.

   This test ensures that an otherwise-erroneous use like
   "sort -m -o FILE ... FILE ..." copies FILE before writing to it.
   It's not clear that POSIX requires this nicety.
   Detect common error cases, but don't try to catch obscure cases like
   "cat ... FILE ... | sort -m -o FILE"
   where traditional "sort" doesn't copy the input and where
   people should know that they're getting into trouble anyway.
   Catching these obscure cases would slow down performance in
   common cases.  */

static int
first_same_file (char * const *files, int nfiles, char const *outfile)
{
  int i;
  bool got_outstat = false;
  struct stat instat, outstat;

  for (i = 0; i < nfiles; i++)
    {
      bool standard_input = STREQ (files[i], "-");

      if (outfile && STREQ (outfile, files[i]) && ! standard_input)
	return i;

      if (! got_outstat)
	{
	  got_outstat = true;
	  if ((outfile
	       ? stat (outfile, &outstat)
	       : fstat (STDOUT_FILENO, &outstat))
	      != 0)
	    return nfiles;
	}

      if (((standard_input
	    ? fstat (STDIN_FILENO, &instat)
	    : stat (files[i], &instat))
	   == 0)
	  && SAME_INODE (instat, outstat))
	return i;
    }

  return nfiles;
}

/* Merge NFILES FILES onto OUTPUT_FILE.  However, merge at most
   MAX_MERGE input files directly onto OUTPUT_FILE.  MAX_MERGE cannot
   exceed NMERGE.  A null OUTPUT_FILE stands for standard output.  */

static void
merge (char **files, int nfiles, int max_merge, char const *output_file)
{
  while (max_merge < nfiles)
    {
      FILE *tfp;
      int i, t = 0;
      char *temp;
      for (i = 0; i < nfiles / NMERGE; ++i)
	{
	  temp = create_temp_file (&tfp);
	  mergefps (&files[i * NMERGE], NMERGE, tfp, temp);
	  files[t++] = temp;
	}
      temp = create_temp_file (&tfp);
      mergefps (&files[i * NMERGE], nfiles % NMERGE, tfp, temp);
      files[t++] = temp;
      nfiles = t;
      if (nfiles == 1)
	break;
    }

  mergefps (files, nfiles, NULL, output_file);
}

/* Sort NFILES FILES onto OUTPUT_FILE. */

static void
sort (char * const *files, int nfiles, char const *output_file)
{
  struct buffer buf;
  int n_temp_files = 0;
  bool output_file_created = false;

  buf.alloc = 0;

  while (nfiles)
    {
      char const *temp_output;
      char const *file = *files;
      FILE *fp = xfopen (file, "r");
      FILE *tfp;
      size_t bytes_per_line = (2 * sizeof (struct line)
			       - sizeof (struct line) / 2);

      if (! buf.alloc)
	initbuf (&buf, bytes_per_line,
		 sort_buffer_size (&fp, 1, files, nfiles, bytes_per_line));
      buf.eof = false;
      files++;
      nfiles--;

      while (fillbuf (&buf, fp, file))
	{
	  struct line *line;
	  struct line *linebase;

	  if (buf.eof && nfiles
	      && (bytes_per_line + 1
		  < (buf.alloc - buf.used - bytes_per_line * buf.nlines)))
	    {
	      /* End of file, but there is more input and buffer room.
		 Concatenate the next input file; this is faster in
		 the usual case.  */
	      buf.left = buf.used;
	      break;
	    }

	  line = buffer_linelim (&buf);
	  linebase = line - buf.nlines;
	  if (1 < buf.nlines)
	    sortlines (line, buf.nlines, linebase);
	  if (buf.eof && !nfiles && !n_temp_files && !buf.left)
	    {
	      xfclose (fp, file);
	      tfp = xfopen (output_file, "w");
	      temp_output = output_file;
	      output_file_created = true;
	    }
	  else
	    {
	      ++n_temp_files;
	      temp_output = create_temp_file (&tfp);
	    }

	  do
	    {
	      line--;
	      write_bytes (line->text, line->length, tfp, temp_output);
	      if (unique)
		while (linebase < line && compare (line, line - 1) == 0)
		  line--;
	    }
	  while (linebase < line);

	  xfclose (tfp, temp_output);

	  if (output_file_created)
	    goto finish;
	}
      xfclose (fp, file);
    }

 finish:
  free (buf.buf);

  if (! output_file_created)
    {
      int i = n_temp_files;
      struct tempnode *node;
      char **tempfiles = xnmalloc (n_temp_files, sizeof *tempfiles);
      for (node = temphead; i > 0; node = node->next)
	tempfiles[--i] = node->name;
      merge (tempfiles, n_temp_files, NMERGE, output_file);
      free (tempfiles);
    }
}

/* Insert key KEY at the end of the key list.  */

static void
insertkey (struct keyfield *key)
{
  struct keyfield **p;

  for (p = &keylist; *p; p = &(*p)->next)
    continue;
  *p = key;
  key->next = NULL;
}

/* Report a bad field specification SPEC, with extra info MSGID.  */

static void badfieldspec (char const *, char const *)
     ATTRIBUTE_NORETURN;
static void
badfieldspec (char const *spec, char const *msgid)
{
  error (SORT_FAILURE, 0, _("%s: invalid field specification `%s'"),
	 _(msgid), spec);
  abort ();
}

/* Parse the leading integer in STRING and store the resulting value
   (which must fit into size_t) into *VAL.  Return the address of the
   suffix after the integer.  If MSGID is NULL, return NULL after
   failure; otherwise, report MSGID and exit on failure.  */

static char const *
parse_field_count (char const *string, size_t *val, char const *msgid)
{
  char *suffix;
  uintmax_t n;

  switch (xstrtoumax (string, &suffix, 10, &n, ""))
    {
    case LONGINT_OK:
    case LONGINT_INVALID_SUFFIX_CHAR:
      *val = n;
      if (*val == n)
	break;
      /* Fall through.  */
    case LONGINT_OVERFLOW:
    case LONGINT_OVERFLOW | LONGINT_INVALID_SUFFIX_CHAR:
      if (msgid)
	error (SORT_FAILURE, 0, _("%s: count `%.*s' too large"),
	       _(msgid), (int) (suffix - string), string);
      return NULL;

    case LONGINT_INVALID:
      if (msgid)
	error (SORT_FAILURE, 0, _("%s: invalid count at start of `%s'"),
	       _(msgid), string);
      return NULL;
    }

  return suffix;
}

/* Handle interrupts and hangups. */

static void
sighandler (int sig)
{
#ifndef SA_NOCLDSTOP
  signal (sig, SIG_IGN);
#endif

  cleanup ();

  signal (sig, SIG_DFL);
  raise (sig);
}

/* Set the ordering options for KEY specified in S.
   Return the address of the first character in S that
   is not a valid ordering option.
   BLANKTYPE is the kind of blanks that 'b' should skip. */

static char *
set_ordering (register const char *s, struct keyfield *key,
	      enum blanktype blanktype)
{
  while (*s)
    {
      switch (*s)
	{
	case 'b':
	  if (blanktype == bl_start || blanktype == bl_both)
	    key->skipsblanks = true;
	  if (blanktype == bl_end || blanktype == bl_both)
	    key->skipeblanks = true;
	  break;
	case 'd':
	  key->ignore = nondictionary;
	  break;
	case 'f':
	  key->translate = fold_toupper;
	  break;
	case 'g':
	  key->general_numeric = true;
	  break;
	case 'i':
	  /* Option order should not matter, so don't let -i override
	     -d.  -d implies -i, but -i does not imply -d.  */
	  if (! key->ignore)
	    key->ignore = nonprinting;
	  break;
	case 'M':
	  key->month = true;
	  break;
	case 'n':
	  key->numeric = true;
	  break;
	case 'r':
	  key->reverse = true;
	  break;
	default:
	  return (char *) s;
	}
      ++s;
    }
  return (char *) s;
}

static struct keyfield *
new_key (void)
{
  struct keyfield *key = xzalloc (sizeof *key);
  key->eword = SIZE_MAX;
  return key;
}

int
main (int argc, char **argv)
{
  struct keyfield *key;
  struct keyfield gkey;
  char const *s;
  int c = 0;
  bool checkonly = false;
  bool mergeonly = false;
  int nfiles = 0;
  bool posixly_correct = (getenv ("POSIXLY_CORRECT") != NULL);
  bool obsolete_usage = (posix2_version () < 200112);
  char const *short_options = (obsolete_usage
			       ? COMMON_SHORT_OPTIONS "y::"
			       : COMMON_SHORT_OPTIONS "y:");
  char *minus = "-", **files;
  char const *outfile = NULL;

  initialize_main (&argc, &argv);
  program_name = argv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (cleanup);

  initialize_exit_failure (SORT_FAILURE);
  atexit (close_stdout);

  hard_LC_COLLATE = hard_locale (LC_COLLATE);
#if HAVE_NL_LANGINFO
  hard_LC_TIME = hard_locale (LC_TIME);
#endif

#if HAVE_SETLOCALE
  /* Let's get locale's representation of the decimal point */
  {
    struct lconv const *lconvp = localeconv ();

    decimal_point = *lconvp->decimal_point;
    if (! decimal_point || lconvp->decimal_point[1])
      {
	decimal_point = C_DECIMAL_POINT;
	if (lconvp->decimal_point[0] && lconvp->decimal_point[1])
	  force_general_numcompare = 1;
      }

    /* We don't support multibyte thousands separators yet.  */
    th_sep = *lconvp->thousands_sep;
    if (! th_sep || lconvp->thousands_sep[1])
      {
	th_sep = CHAR_MAX + 1;
	if (lconvp->thousands_sep[0] && lconvp->thousands_sep[1])
	  force_general_numcompare = 1;
      }
  }
#endif

#if HAVE_MBRTOWC
  if (MB_CUR_MAX > 1)
    {
      inittables = inittables_mb;
      begfield = begfield_mb;
      limfield = limfield_mb;
      getmonth = getmonth_mb;
      keycompare = keycompare_mb;
    }
  else
#endif
    {
      inittables = inittables_uni;
      begfield = begfield_uni;
      limfield = limfield_uni;
      keycompare = keycompare_uni;
      getmonth = getmonth_uni;
    }

  have_read_stdin = false;
  inittables ();

  {
    int i;
    static int const sig[] = { SIGHUP, SIGINT, SIGPIPE, SIGTERM };
    enum { nsigs = sizeof sig / sizeof sig[0] };

#ifdef SA_NOCLDSTOP
    struct sigaction act;

    sigemptyset (&caught_signals);
    for (i = 0; i < nsigs; i++)
      {
	sigaction (sig[i], NULL, &act);
	if (act.sa_handler != SIG_IGN)
	  sigaddset (&caught_signals, sig[i]);
      }

    act.sa_handler = sighandler;
    act.sa_mask = caught_signals;
    act.sa_flags = 0;

    for (i = 0; i < nsigs; i++)
      if (sigismember (&caught_signals, sig[i]))
	sigaction (sig[i], &act, NULL);
#else
    for (i = 0; i < nsigs; i++)
      if (signal (sig[i], SIG_IGN) != SIG_IGN)
	signal (sig[i], sighandler);
#endif
  }

  gkey.sword = gkey.eword = SIZE_MAX;
  gkey.ignore = NULL;
  gkey.translate = NULL;
  gkey.numeric = gkey.general_numeric = gkey.month = gkey.reverse = false;
  gkey.skipsblanks = gkey.skipeblanks = false;

  files = xnmalloc (argc, sizeof *files);

  for (;;)
    {
      /* Parse an operand as a file after "--" was seen; or if
         pedantic and a file was seen, unless the POSIX version
         predates 1003.1-2001 and -c was not seen and the operand is
         "-o FILE" or "-oFILE".  */

      if (c == -1
	  || (posixly_correct && nfiles != 0
	      && ! (obsolete_usage
		    && ! checkonly
		    && optind != argc
		    && argv[optind][0] == '-' && argv[optind][1] == 'o'
		    && (argv[optind][2] || optind + 1 != argc)))
	  || ((c = getopt_long (argc, argv, short_options,
				long_options, NULL))
	      == -1))
	{
	  if (argc <= optind)
	    break;
	  files[nfiles++] = argv[optind++];
	}
      else switch (c)
	{
	case 1:
	  key = NULL;
	  if (obsolete_usage && optarg[0] == '+')
	    {
	      /* Treat +POS1 [-POS2] as a key if possible; but silently
		 treat an operand as a file if it is not a valid +POS1.  */
	      key = new_key ();
	      s = parse_field_count (optarg + 1, &key->sword, NULL);
	      if (s && *s == '.')
		s = parse_field_count (s + 1, &key->schar, NULL);
	      if (! (key->sword | key->schar))
		key->sword = SIZE_MAX;
	      if (! s || *set_ordering (s, key, bl_start))
		{
		  free (key);
		  key = NULL;
		}
	      else
		{
		  if (optind != argc && argv[optind][0] == '-'
		      && ISDIGIT (argv[optind][1]))
		    {
		      char const *optarg1 = argv[optind++];
		      s = parse_field_count (optarg1 + 1, &key->eword,
					     N_("invalid number after `-'"));
		      if (*s == '.')
			s = parse_field_count (s + 1, &key->echar,
					       N_("invalid number after `.'"));
		      if (*set_ordering (s, key, bl_end))
			badfieldspec (optarg1,
				      N_("stray character in field spec"));
		    }
		  insertkey (key);
		}
	    }
	  if (! key)
	    files[nfiles++] = optarg;
	  break;

	case 'b':
	case 'd':
	case 'f':
	case 'g':
	case 'i':
	case 'M':
	case 'n':
	case 'r':
	  {
	    char str[2];
	    str[0] = c;
	    str[1] = '\0';
	    set_ordering (str, &gkey, bl_both);
	  }
	  break;

	case 'c':
	  checkonly = true;
	  break;

	case 'k':
	  key = new_key ();

	  /* Get POS1. */
	  s = parse_field_count (optarg, &key->sword,
				 N_("invalid number at field start"));
	  if (! key->sword--)
	    {
	      /* Provoke with `sort -k0' */
	      badfieldspec (optarg, N_("field number is zero"));
	    }
	  if (*s == '.')
	    {
	      s = parse_field_count (s + 1, &key->schar,
				     N_("invalid number after `.'"));
	      if (! key->schar--)
		{
		  /* Provoke with `sort -k1.0' */
		  badfieldspec (optarg, N_("character offset is zero"));
		}
	    }
	  if (! (key->sword | key->schar))
	    key->sword = SIZE_MAX;
	  s = set_ordering (s, key, bl_start);
	  if (*s != ',')
	    {
	      key->eword = SIZE_MAX;
	      key->echar = 0;
	    }
	  else
	    {
	      /* Get POS2. */
	      s = parse_field_count (s + 1, &key->eword,
				     N_("invalid number after `,'"));
	      if (! key->eword--)
		{
		  /* Provoke with `sort -k1,0' */
		  badfieldspec (optarg, N_("field number is zero"));
		}
	      if (*s == '.')
		s = parse_field_count (s + 1, &key->echar,
				       N_("invalid number after `.'"));
	      else
		{
		  /* `-k 2,3' is equivalent to `+1 -3'.  */
		  key->eword++;
		}
	      s = set_ordering (s, key, bl_end);
	    }
	  if (*s)
	    badfieldspec (optarg, N_("stray character in field spec"));
	  insertkey (key);
	  break;

	case 'm':
	  mergeonly = true;
	  break;

	case 'o':
	  if (outfile && !STREQ (outfile, optarg))
	    error (SORT_FAILURE, 0, _("multiple output files specified"));
	  outfile = optarg;
	  break;

	case 's':
	  stable = true;
	  break;

	case 'S':
	  specify_sort_size (optarg);
	  break;

	case 't':
	  {
	    char newtab[MB_LEN_MAX + 1];
	    size_t newtab_length = 1;
	    strncpy (newtab, optarg, MB_LEN_MAX);
	    if (! newtab[0])
	      error (SORT_FAILURE, 0, _("empty tab"));
#if HAVE_MBRTOWC
	    if (MB_CUR_MAX > 1)
	      {
		wchar_t wc;
		mbstate_t state;
		size_t i;

		memset (&state, '\0', sizeof (mbstate_t));
		newtab_length = mbrtowc (&wc, newtab, strnlen (newtab, MB_LEN_MAX), &state);
		switch (newtab_length)
                  {
                  case (size_t) -1:
                  case (size_t) -2:
                  case 0:
                    newtab_length = 1;
                  }

                if (optarg[newtab_length])
		  {
		    /* Provoke with `sort -txx'.  Complain about
		       "multi-character tab" instead of "multibyte tab", so
		       that the diagnostic's wording does not need to be
		       changed once multibyte characters are supported.  */
		    error (SORT_FAILURE, 0, _("multi-character tab `%s'"),
			   optarg);
		  }
	      }
            else
#endif

	    if (optarg[1])
	      {
		if (STREQ (optarg, "\\0"))
		  newtab[0] = '\0';
		else
		  {
		    /* Provoke with `sort -txx'.  Complain about
		       "multi-character tab" instead of "multibyte tab", so
		       that the diagnostic's wording does not need to be
		       changed once multibyte characters are supported.  */
		    error (SORT_FAILURE, 0, _("multi-character tab `%s'"),
			   optarg);
		  }
	      }
	    if (!tab_default && (tab_length != newtab_length
		|| memcmp(tab, newtab, tab_length) != 0))
	      error (SORT_FAILURE, 0, _("incompatible tabs"));
	    memcpy(tab, newtab, newtab_length);
	    tab_length = newtab_length;
	    tab_default = false;
	  }
	  break;

	case 'T':
	  add_temp_dir (optarg);
	  break;

	case 'u':
	  unique = true;
	  break;

	case 'y':
	  /* Accept and ignore e.g. -y0 for compatibility with Solaris
	     2.x through Solaris 7.  -y is marked as obsolete starting
	     with Solaris 8.  */
	  break;

	case 'z':
	  eolchar = 0;
	  break;

	case_GETOPT_HELP_CHAR;

	case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

	default:
	  usage (SORT_FAILURE);
	}
    }

  /* Inheritance of global options to individual keys. */
  for (key = keylist; key; key = key->next)
    if (! (key->ignore || key->translate
	   || (key->skipsblanks | key->reverse
	       | key->skipeblanks | key->month | key->numeric
	       | key->general_numeric)))
      {
	key->ignore = gkey.ignore;
	key->translate = gkey.translate;
	key->skipsblanks = gkey.skipsblanks;
	key->skipeblanks = gkey.skipeblanks;
	key->month = gkey.month;
	key->numeric = gkey.numeric;
	key->general_numeric = gkey.general_numeric;
	key->reverse = gkey.reverse;
      }

  if (!keylist && (gkey.ignore || gkey.translate
		   || (gkey.skipsblanks | gkey.skipeblanks | gkey.month
		       | gkey.numeric | gkey.general_numeric)))
    insertkey (&gkey);
  reverse = gkey.reverse;

  if (temp_dir_count == 0)
    {
      char const *tmp_dir = getenv ("TMPDIR");
      add_temp_dir (tmp_dir ? tmp_dir : DEFAULT_TMPDIR);
    }

  if (nfiles == 0)
    {
      nfiles = 1;
      files = &minus;
    }

  if (checkonly)
    {
      if (nfiles > 1)
	{
	  error (0, 0, _("extra operand %s not allowed with -c"),
		 quote (files[1]));
	  usage (SORT_FAILURE);
	}

      /* POSIX requires that sort return 1 IFF invoked with -c and the
	 input is not properly sorted.  */
      exit (check (files[0]) ? EXIT_SUCCESS : SORT_OUT_OF_ORDER);
    }

  if (mergeonly)
    {
      int max_merge = first_same_file (files, MIN (nfiles, NMERGE), outfile);
      merge (files, nfiles, max_merge, outfile);
    }
  else
    sort (files, nfiles, outfile);

  if (have_read_stdin && fclose (stdin) == EOF)
    die (_("close failed"), "-");

  exit (EXIT_SUCCESS);
}
