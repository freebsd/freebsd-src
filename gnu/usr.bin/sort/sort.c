/* sort - sort lines of text (with all kinds of options).
   Copyright (C) 1988, 1991, 1992, 1993, 1994, 1995 Free Software Foundation

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
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

#include <config.h>

/* Get isblank from GNU libc.  */
#define _GNU_SOURCE

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#ifdef __FreeBSD__
#include <locale.h>
#endif
#include "system.h"
#include "version.h"
#include "long-options.h"
#include "error.h"
#include "xstrtod.h"

#ifdef HAVE_LIMITS_H
#include <limits.h>
#else
#ifndef UCHAR_MAX
#define UCHAR_MAX 255
#endif
#endif
#ifndef STDC_HEADERS
char *malloc ();
char *realloc ();
void free ();
#endif

/* Undefine, to avoid warning about redefinition on some systems.  */
#undef min
#define min(a, b) ((a) < (b) ? (a) : (b))

#define UCHAR_LIM (UCHAR_MAX + 1)
#define UCHAR(c) ((unsigned char) (c))

#ifndef DEFAULT_TMPDIR
#define DEFAULT_TMPDIR "/tmp"
#endif

/* The kind of blanks for '-b' to skip in various options. */
enum blanktype { bl_start, bl_end, bl_both };

/* Lines are held in core as counted strings. */
struct line
{
  char *text;			/* Text of the line. */
  int length;			/* Length not including final newline. */
  char *keybeg;			/* Start of first key. */
  char *keylim;			/* Limit of first key. */
};

/* Arrays of lines. */
struct lines
{
  struct line *lines;		/* Dynamically allocated array of lines. */
  int used;			/* Number of slots used. */
  int alloc;			/* Number of slots allocated. */
  int limit;			/* Max number of slots to allocate.  */
};

/* Input buffers. */
struct buffer
{
  char *buf;			/* Dynamically allocated buffer. */
  int used;			/* Number of bytes used. */
  int alloc;			/* Number of bytes allocated. */
  int left;			/* Number of bytes left after line parsing. */
};

struct keyfield
{
  int sword;			/* Zero-origin 'word' to start at. */
  int schar;			/* Additional characters to skip. */
  int skipsblanks;		/* Skip leading white space at start. */
  int eword;			/* Zero-origin first word after field. */
  int echar;			/* Additional characters in field. */
  int skipeblanks;		/* Skip trailing white space at finish. */
  int *ignore;			/* Boolean array of characters to ignore. */
  char *translate;		/* Translation applied to characters. */
  int numeric;			/* Flag for numeric comparison.  Handle
				   strings of digits with optional decimal
				   point, but no exponential notation. */
  int general_numeric;		/* Flag for general, numeric comparison.
				   Handle numbers in exponential notation. */
  int month;			/* Flag for comparison by month name. */
  int reverse;			/* Reverse the sense of comparison. */
  struct keyfield *next;	/* Next keyfield to try. */
};

struct month
{
  char *name;
  int val;
};

/* The name this program was run with. */
char *program_name;

/* Table of digits. */
static int digits[UCHAR_LIM];

/* Table of white space. */
static int blanks[UCHAR_LIM];

/* Table of non-printing characters. */
static int nonprinting[UCHAR_LIM];

/* Table of non-dictionary characters (not letters, digits, or blanks). */
static int nondictionary[UCHAR_LIM];

/* Translation table folding lower case to upper. */
static char fold_toupper[UCHAR_LIM];

/* Table mapping 3-letter month names to integers.
   Alphabetic order allows binary search. */
static struct month const monthtab[] =
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

/* Initial buffer size for in core sorting.  Will not grow unless a
   line longer than this is seen. */
static int sortalloc = 512 * 1024;

/* Initial buffer size for in core merge buffers.  Bear in mind that
   up to NMERGE * mergealloc bytes may be allocated for merge buffers. */
static int mergealloc =  16 * 1024;

/* Guess of average line length. */
static int linelength = 30;

/* Maximum number of elements for the array(s) of struct line's, in bytes.  */
#define LINEALLOC (256 * 1024)

/* Prefix for temporary file names. */
static char *temp_file_prefix;

/* Flag to reverse the order of all comparisons. */
static int reverse;

/* Flag for stable sort.  This turns off the last ditch bytewise
   comparison of lines, and instead leaves lines in the same order
   they were read if all keys compare equal.  */
static int stable;

/* Tab character separating fields.  If NUL, then fields are separated
   by the empty string between a non-whitespace character and a whitespace
   character. */
static char tab;

/* Flag to remove consecutive duplicate lines from the output.
   Only the last of a sequence of equal lines will be output. */
static int unique;

/* Nonzero if any of the input files are the standard input. */
static int have_read_stdin;

/* Lists of key field comparisons to be tried. */
static struct keyfield keyhead;

#ifdef __FreeBSD__
static int collates[UCHAR_LIM];

#define COLLDIFF(A, B) (collates[UCHAR (A)] - collates[UCHAR (B)])

static int
collate_range_cmp (a, b)
	int a, b;
{
	int r;
	static char s[2][2];

	if ((unsigned char)a == (unsigned char)b)
		return 0;
	s[0][0] = a;
	s[1][0] = b;
	if ((r = strcoll(s[0], s[1])) == 0)
		r = (unsigned char)a - (unsigned char)b;
	return r;
}

static int
collcompare (const void *sa, const void *sb)
{
	return collate_range_cmp (*((int *)sa), *((int *)sb));
}

static void
init_collates(void)
{
	register int i, j;
	int reverse[UCHAR_LIM];

	for (i = 0; i < UCHAR_LIM; i++)
		reverse[i] = i;
	qsort(reverse, UCHAR_LIM, sizeof(reverse[0]), collcompare);
	for (i = 0; i < UCHAR_LIM; i++) {
		for (j = 0; j < UCHAR_LIM; j++) {
			if (reverse[j] == i) {
				collates[i] = j;
				break;
			}
		}
	}
}

static int
collcmp (const unsigned char *p1, const unsigned char *p2, size_t n)
{
	int r;

	if (n != 0) {
		do {
			if ((r = COLLDIFF (*p1++, *p2++)) != 0)
				return r;
		} while (--n != 0);
	}
	return (0);
}
#endif

static void
usage (int status)
{
  if (status != 0)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
"),
	      program_name);
      printf (_("\
Write sorted concatenation of all FILE(s) to standard output.\n\
\n\
  +POS1 [-POS2]    start a key at POS1, end it before POS2\n\
  -M               compare (unknown) < `JAN' < ... < `DEC', imply -b\n\
  -T DIRECT        use DIRECT for temporary files, not $TMPDIR or %s\n\
  -b               ignore leading blanks in sort fields or keys\n\
  -c               check if given files already sorted, do not sort\n\
  -d               consider only [a-zA-Z0-9 ] characters in keys\n\
  -f               fold lower case to upper case characters in keys\n\
  -g               compare according to general numerical value, imply -b\n\
  -i               consider only [\\040-\\0176] characters in keys\n\
  -k POS1[,POS2]   same as +POS1 [-POS2], but all positions counted from 1\n\
  -m               merge already sorted files, do not sort\n\
  -n               compare according to string numerical value, imply -b\n\
  -o FILE          write result on FILE instead of standard output\n\
  -r               reverse the result of comparisons\n\
  -s               stabilize sort by disabling last resort comparison\n\
  -t SEP           use SEParator instead of non- to whitespace transition\n\
  -u               with -c, check for strict ordering\n\
  -u               with -m, only output the first of an equal sequence\n\
      --help       display this help and exit\n\
      --version    output version information and exit\n\
\n\
POS is F[.C][OPTS], where F is the field number and C the character\n\
position in the field, both counted from zero.  OPTS is made up of one\n\
or more of Mbdfinr, this effectively disable global -Mbdfinr settings\n\
for that key.  If no key given, use the entire line as key.  With no\n\
FILE, or when FILE is -, read standard input.\n\
")
	      , DEFAULT_TMPDIR);
    }
  exit (status);
}

/* The list of temporary files. */
static struct tempnode
{
  char *name;
  struct tempnode *next;
} temphead;

/* Clean up any remaining temporary files. */

static void
cleanup (void)
{
  struct tempnode *node;

  for (node = temphead.next; node; node = node->next)
    unlink (node->name);
}

/* Allocate N bytes of memory dynamically, with error checking.  */

static char *
xmalloc (unsigned int n)
{
  char *p;

  p = malloc (n);
  if (p == 0)
    {
      error (0, 0, _("virtual memory exhausted"));
      cleanup ();
      exit (2);
    }
  return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.
   If P is NULL, run xmalloc.
   If N is 0, run free and return NULL.  */

static char *
xrealloc (char *p, unsigned int n)
{
  if (p == 0)
    return xmalloc (n);
  if (n == 0)
    {
      free (p);
      return 0;
    }
  p = realloc (p, n);
  if (p == 0)
    {
      error (0, 0, _("virtual memory exhausted"));
      cleanup ();
      exit (2);
    }
  return p;
}

static FILE *
xtmpfopen (const char *file)
{
  FILE *fp;
  int fd;

  fd = open (file, O_EXCL | O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0 || (fp = fdopen (fd, "w")) == NULL)
    {
      error (0, errno, "%s", file);
      cleanup ();
      exit (2);
    }

  return fp;
}

static FILE *
xfopen (const char *file, const char *how)
{
  FILE *fp;

  if (strcmp (file, "-") == 0)
    {
      fp = stdin;
    }
  else
    {
      if ((fp = fopen (file, how)) == NULL)
	{
	  error (0, errno, "%s", file);
	  cleanup ();
	  exit (2);
	}
    }

  if (fp == stdin)
    have_read_stdin = 1;
  return fp;
}

static void
xfclose (FILE *fp)
{
  if (fp == stdin)
    {
      /* Allow reading stdin from tty more than once. */
      if (feof (fp))
	clearerr (fp);
    }
  else if (fp == stdout)
    {
      if (fflush (fp) != 0)
	{
	  error (0, errno, _("flushing file"));
	  cleanup ();
	  exit (2);
	}
    }
  else
    {
      if (fclose (fp) != 0)
	{
	  error (0, errno, _("error closing file"));
	  cleanup ();
	  exit (2);
	}
    }
}

static void
xfwrite (const char *buf, int size, int nelem, FILE *fp)
{
  if (fwrite (buf, size, nelem, fp) != nelem)
    {
      error (0, errno, _("write error"));
      cleanup ();
      exit (2);
    }
}

/* Return a name for a temporary file. */

static char *
tempname (void)
{
  static unsigned int seq;
  int len = strlen (temp_file_prefix);
  char *name = xmalloc (len + 1 + sizeof ("sort") - 1 + 5 + 5 + 1);
  struct tempnode *node;

  node = (struct tempnode *) xmalloc (sizeof (struct tempnode));
  sprintf (name,
	   "%s%ssort%5.5d%5.5d",
	   temp_file_prefix,
	   (len && temp_file_prefix[len - 1] != '/') ? "/" : "",
	   (unsigned int) getpid () & 0xffff, seq);

  /* Make sure that SEQ's value fits in 5 digits.  */
  ++seq;
  if (seq >= 100000)
    seq = 0;

  node->name = name;
  node->next = temphead.next;
  temphead.next = node;
  return name;
}

/* Search through the list of temporary files for NAME;
   remove it if it is found on the list. */

static void
zaptemp (char *name)
{
  struct tempnode *node, *temp;

  for (node = &temphead; node->next; node = node->next)
    if (!strcmp (name, node->next->name))
      break;
  if (node->next)
    {
      temp = node->next;
      unlink (temp->name);
      free (temp->name);
      node->next = temp->next;
      free ((char *) temp);
    }
}

/* Initialize the character class tables. */

static void
inittables (void)
{
  int i;

  for (i = 0; i < UCHAR_LIM; ++i)
    {
      if (ISBLANK (i))
	blanks[i] = 1;
      if (ISDIGIT (i))
	digits[i] = 1;
      if (!ISPRINT (i))
	nonprinting[i] = 1;
      if (!ISALNUM (i) && !ISBLANK (i))
	nondictionary[i] = 1;
      if (ISLOWER (i))
	fold_toupper[i] = toupper (i);
      else
	fold_toupper[i] = i;
    }
#ifdef __FreeBSD__
    init_collates();
#endif
}

/* Initialize BUF, allocating ALLOC bytes initially. */

static void
initbuf (struct buffer *buf, int alloc)
{
  buf->alloc = alloc;
  buf->buf = xmalloc (buf->alloc);
  buf->used = buf->left = 0;
}

/* Fill BUF reading from FP, moving buf->left bytes from the end
   of buf->buf to the beginning first.	If EOF is reached and the
   file wasn't terminated by a newline, supply one.  Return a count
   of bytes buffered. */

static int
fillbuf (struct buffer *buf, FILE *fp)
{
  int cc;

  memmove (buf->buf, buf->buf + buf->used - buf->left, buf->left);
  buf->used = buf->left;

  while (!feof (fp) && (buf->used == 0 || !memchr (buf->buf, '\n', buf->used)))
    {
      if (buf->used == buf->alloc)
	{
	  buf->alloc *= 2;
	  buf->buf = xrealloc (buf->buf, buf->alloc);
	}
      cc = fread (buf->buf + buf->used, 1, buf->alloc - buf->used, fp);
      if (ferror (fp))
	{
	  error (0, errno, _("read error"));
	  cleanup ();
	  exit (2);
	}
      buf->used += cc;
    }

  if (feof (fp) && buf->used && buf->buf[buf->used - 1] != '\n')
    {
      if (buf->used == buf->alloc)
	{
	  buf->alloc *= 2;
	  buf->buf = xrealloc (buf->buf, buf->alloc);
	}
      buf->buf[buf->used++] = '\n';
    }

  return buf->used;
}

/* Initialize LINES, allocating space for ALLOC lines initially.
   LIMIT is the maximum possible number of lines to allocate space
   for, ever.  */

static void
initlines (struct lines *lines, int alloc, int limit)
{
  lines->alloc = alloc;
  lines->lines = (struct line *) xmalloc (lines->alloc * sizeof (struct line));
  lines->used = 0;
  lines->limit = limit;
}

/* Return a pointer to the first character of the field specified
   by KEY in LINE. */

static char *
begfield (const struct line *line, const struct keyfield *key)
{
  register char *ptr = line->text, *lim = ptr + line->length;
  register int sword = key->sword, schar = key->schar;

  if (tab)
    while (ptr < lim && sword--)
      {
	while (ptr < lim && *ptr != tab)
	  ++ptr;
	if (ptr < lim)
	  ++ptr;
      }
  else
    while (ptr < lim && sword--)
      {
	while (ptr < lim && blanks[UCHAR (*ptr)])
	  ++ptr;
	while (ptr < lim && !blanks[UCHAR (*ptr)])
	  ++ptr;
      }

  if (key->skipsblanks)
    while (ptr < lim && blanks[UCHAR (*ptr)])
      ++ptr;

  if (ptr + schar <= lim)
    ptr += schar;
  else
    ptr = lim;

  return ptr;
}

/* Return the limit of (a pointer to the first character after) the field
   in LINE specified by KEY. */

static char *
limfield (const struct line *line, const struct keyfield *key)
{
  register char *ptr = line->text, *lim = ptr + line->length;
  register int eword = key->eword, echar = key->echar;

  /* Note: from the POSIX spec:
     The leading field separator itself is included in
     a field when -t is not used.  FIXME: move this comment up... */

  /* Move PTR past EWORD fields or to one past the last byte on LINE,
     whichever comes first.  If there are more than EWORD fields, leave
     PTR pointing at the beginning of the field having zero-based index,
     EWORD.  If a delimiter character was specified (via -t), then that
     `beginning' is the first character following the delimiting TAB.
     Otherwise, leave PTR pointing at the first `blank' character after
     the preceding field.  */
  if (tab)
    while (ptr < lim && eword--)
      {
	while (ptr < lim && *ptr != tab)
	  ++ptr;
	if (ptr < lim && (eword || echar > 0))
	  ++ptr;
      }
  else
    while (ptr < lim && eword--)
      {
	while (ptr < lim && blanks[UCHAR (*ptr)])
	  ++ptr;
	while (ptr < lim && !blanks[UCHAR (*ptr)])
	  ++ptr;
      }

  /* Make LIM point to the end of (one byte past) the current field.  */
  if (tab)
    {
      char *newlim;
      newlim = memchr (ptr, tab, lim - ptr);
      if (newlim)
	lim = newlim;
    }
  else
    {
      char *newlim;
      newlim = ptr;
      while (newlim < lim && blanks[UCHAR (*newlim)])
	++newlim;
      while (newlim < lim && !blanks[UCHAR (*newlim)])
	++newlim;
      lim = newlim;
    }

  /* If we're skipping leading blanks, don't start counting characters
     until after skipping past any leading blanks.  */
  if (key->skipsblanks)
    while (ptr < lim && blanks[UCHAR (*ptr)])
      ++ptr;

  /* Advance PTR by ECHAR (if possible), but no further than LIM.  */
  if (ptr + echar <= lim)
    ptr += echar;
  else
    ptr = lim;

  return ptr;
}

/* FIXME */

void
trim_trailing_blanks (const char *a_start, char **a_end)
{
  while (*a_end > a_start && blanks[UCHAR (*(*a_end - 1))])
    --(*a_end);
}

/* Find the lines in BUF, storing pointers and lengths in LINES.
   Also replace newlines in BUF with NULs. */

static void
findlines (struct buffer *buf, struct lines *lines)
{
  register char *beg = buf->buf, *lim = buf->buf + buf->used, *ptr;
  struct keyfield *key = keyhead.next;

  lines->used = 0;

  while (beg < lim && (ptr = memchr (beg, '\n', lim - beg))
	 && lines->used < lines->limit)
    {
      /* There are various places in the code that rely on a NUL
	 being at the end of in-core lines; NULs inside the lines
	 will not cause trouble, though. */
      *ptr = '\0';

      if (lines->used == lines->alloc)
	{
	  lines->alloc *= 2;
	  lines->lines = (struct line *)
	    xrealloc ((char *) lines->lines,
		      lines->alloc * sizeof (struct line));
	}

      lines->lines[lines->used].text = beg;
      lines->lines[lines->used].length = ptr - beg;

      /* Precompute the position of the first key for efficiency. */
      if (key)
	{
	  if (key->eword >= 0)
	    lines->lines[lines->used].keylim =
	      limfield (&lines->lines[lines->used], key);
	  else
	    lines->lines[lines->used].keylim = ptr;

	  if (key->sword >= 0)
	    lines->lines[lines->used].keybeg =
	      begfield (&lines->lines[lines->used], key);
	  else
	    {
	      if (key->skipsblanks)
		while (blanks[UCHAR (*beg)])
		  ++beg;
	      lines->lines[lines->used].keybeg = beg;
	    }
	  if (key->skipeblanks)
	    {
	      trim_trailing_blanks (lines->lines[lines->used].keybeg,
				    &lines->lines[lines->used].keylim);
	    }
	}
      else
	{
	  lines->lines[lines->used].keybeg = 0;
	  lines->lines[lines->used].keylim = 0;
	}

      ++lines->used;
      beg = ptr + 1;
    }

  buf->left = lim - beg;
}

/* Compare strings A and B containing decimal fractions < 1.  Each string
   should begin with a decimal point followed immediately by the digits
   of the fraction.  Strings not of this form are considered to be zero. */

static int
fraccompare (register const char *a, register const char *b)
{
  register tmpa = UCHAR (*a), tmpb = UCHAR (*b);

  if (tmpa == '.' && tmpb == '.')
    {
      do
	tmpa = UCHAR (*++a), tmpb = UCHAR (*++b);
      while (tmpa == tmpb && digits[tmpa]);
      if (digits[tmpa] && digits[tmpb])
	return tmpa - tmpb;
      if (digits[tmpa])
	{
	  while (tmpa == '0')
	    tmpa = UCHAR (*++a);
	  if (digits[tmpa])
	    return 1;
	  return 0;
	}
      if (digits[tmpb])
	{
	  while (tmpb == '0')
	    tmpb = UCHAR (*++b);
	  if (digits[tmpb])
	    return -1;
	  return 0;
	}
      return 0;
    }
  else if (tmpa == '.')
    {
      do
	tmpa = UCHAR (*++a);
      while (tmpa == '0');
      if (digits[tmpa])
	return 1;
      return 0;
    }
  else if (tmpb == '.')
    {
      do
	tmpb = UCHAR (*++b);
      while (tmpb == '0');
      if (digits[tmpb])
	return -1;
      return 0;
    }
  return 0;
}

/* Compare strings A and B as numbers without explicitly converting them to
   machine numbers.  Comparatively slow for short strings, but asymptotically
   hideously fast. */

static int
numcompare (register const char *a, register const char *b)
{
  register int tmpa, tmpb, loga, logb, tmp;

  tmpa = UCHAR (*a);
  tmpb = UCHAR (*b);

  while (blanks[tmpa])
    tmpa = UCHAR (*++a);
  while (blanks[tmpb])
    tmpb = UCHAR (*++b);

  if (tmpa == '-')
    {
      do
	tmpa = UCHAR (*++a);
      while (tmpa == '0');
      if (tmpb != '-')
	{
	  if (tmpa == '.')
	    do
	      tmpa = UCHAR (*++a);
	    while (tmpa == '0');
	  if (digits[tmpa])
	    return -1;
	  while (tmpb == '0')
	    tmpb = UCHAR (*++b);
	  if (tmpb == '.')
	    do
	      tmpb = UCHAR (*++b);
	    while (tmpb == '0');
	  if (digits[tmpb])
	    return -1;
	  return 0;
	}
      do
	tmpb = UCHAR (*++b);
      while (tmpb == '0');

      while (tmpa == tmpb && digits[tmpa])
	tmpa = UCHAR (*++a), tmpb = UCHAR (*++b);

      if ((tmpa == '.' && !digits[tmpb]) || (tmpb == '.' && !digits[tmpa]))
	return -fraccompare (a, b);

      if (digits[tmpa])
	for (loga = 1; digits[UCHAR (*++a)]; ++loga)
	  ;
      else
	loga = 0;

      if (digits[tmpb])
	for (logb = 1; digits[UCHAR (*++b)]; ++logb)
	  ;
      else
	logb = 0;

      if ((tmp = logb - loga) != 0)
	return tmp;

      if (!loga)
	return 0;

#ifdef __FreeBSD__
      return COLLDIFF (tmpb, tmpa);
#else
      return tmpb - tmpa;
#endif
    }
  else if (tmpb == '-')
    {
      do
	tmpb = UCHAR (*++b);
      while (tmpb == '0');
      if (tmpb == '.')
	do
	  tmpb = UCHAR (*++b);
	while (tmpb == '0');
      if (digits[tmpb])
	return 1;
      while (tmpa == '0')
	tmpa = UCHAR (*++a);
      if (tmpa == '.')
	do
	  tmpa = UCHAR (*++a);
	while (tmpa == '0');
      if (digits[tmpa])
	return 1;
      return 0;
    }
  else
    {
      while (tmpa == '0')
	tmpa = UCHAR (*++a);
      while (tmpb == '0')
	tmpb = UCHAR (*++b);

      while (tmpa == tmpb && digits[tmpa])
	tmpa = UCHAR (*++a), tmpb = UCHAR (*++b);

      if ((tmpa == '.' && !digits[tmpb]) || (tmpb == '.' && !digits[tmpa]))
	return fraccompare (a, b);

      if (digits[tmpa])
	for (loga = 1; digits[UCHAR (*++a)]; ++loga)
	  ;
      else
	loga = 0;

      if (digits[tmpb])
	for (logb = 1; digits[UCHAR (*++b)]; ++logb)
	  ;
      else
	logb = 0;

      if ((tmp = loga - logb) != 0)
	return tmp;

      if (!loga)
	return 0;

#ifdef __FreeBSD__
      return COLLDIFF (tmpa, tmpb);
#else
      return tmpa - tmpb;
#endif
    }
}

static int
general_numcompare (const char *sa, const char *sb)
{
  double a, b;
  /* FIXME: add option to warn about failed conversions.  */
  /* FIXME: maybe add option to try expensive FP conversion
     only if A and B can't be compared more cheaply/accurately.  */
  if (xstrtod (sa, NULL, &a))
    {
      a = 0;
    }
  if (xstrtod (sb, NULL, &b))
    {
      b = 0;
    }
  return a == b ? 0 : a < b ? -1 : 1;
}

/* Return an integer <= 12 associated with month name S with length LEN,
   0 if the name in S is not recognized. */

static int
getmonth (const char *s, int len)
{
  char month[4];
  register int i, lo = 0, hi = 12;

  while (len > 0 && blanks[UCHAR(*s)])
    ++s, --len;

  if (len < 3)
    return 0;

  for (i = 0; i < 3; ++i)
    month[i] = fold_toupper[UCHAR (s[i])];
  month[3] = '\0';

  while (hi - lo > 1)
    if (strcmp (month, monthtab[(lo + hi) / 2].name) < 0)
      hi = (lo + hi) / 2;
    else
      lo = (lo + hi) / 2;
  if (!strcmp (month, monthtab[lo].name))
    return monthtab[lo].val;
  return 0;
}

/* Compare two lines A and B trying every key in sequence until there
   are no more keys or a difference is found. */

static int
keycompare (const struct line *a, const struct line *b)
{
  register char *texta, *textb, *lima, *limb, *translate;
  register int *ignore;
  struct keyfield *key;
  int diff = 0, iter = 0, lena, lenb;

  for (key = keyhead.next; key; key = key->next, ++iter)
    {
      ignore = key->ignore;
      translate = key->translate;

      /* Find the beginning and limit of each field. */
      if (iter || a->keybeg == NULL || b->keybeg == NULL)
	{
	  if (key->eword >= 0)
	    lima = limfield (a, key), limb = limfield (b, key);
	  else
	    lima = a->text + a->length, limb = b->text + b->length;

	  if (key->sword >= 0)
	    texta = begfield (a, key), textb = begfield (b, key);
	  else
	    {
	      texta = a->text, textb = b->text;
	      if (key->skipsblanks)
		{
		  while (texta < lima && blanks[UCHAR (*texta)])
		    ++texta;
		  while (textb < limb && blanks[UCHAR (*textb)])
		    ++textb;
		}
	    }
	}
      else
	{
	  /* For the first iteration only, the key positions have
	     been precomputed for us. */
	  texta = a->keybeg, lima = a->keylim;
	  textb = b->keybeg, limb = b->keylim;
	}

      /* Find the lengths. */
      lena = lima - texta, lenb = limb - textb;
      if (lena < 0)
	lena = 0;
      if (lenb < 0)
	lenb = 0;

      if (key->skipeblanks)
        {
	  char *a_end = texta + lena;
	  char *b_end = textb + lenb;
	  trim_trailing_blanks (texta, &a_end);
	  trim_trailing_blanks (textb, &b_end);
	  lena = a_end - texta;
	  lenb = b_end - textb;
	}

      /* Actually compare the fields. */
      if (key->numeric)
	{
	  if (*lima || *limb)
	    {
	      char savea = *lima, saveb = *limb;

	      *lima = *limb = '\0';
	      diff = numcompare (texta, textb);
	      *lima = savea, *limb = saveb;
	    }
	  else
	    diff = numcompare (texta, textb);

	  if (diff)
	    return key->reverse ? -diff : diff;
	  continue;
	}
      else if (key->general_numeric)
	{
	  if (*lima || *limb)
	    {
	      char savea = *lima, saveb = *limb;

	      *lima = *limb = '\0';
	      diff = general_numcompare (texta, textb);
	      *lima = savea, *limb = saveb;
	    }
	  else
	    diff = general_numcompare (texta, textb);

	  if (diff)
	    return key->reverse ? -diff : diff;
	  continue;
	}
      else if (key->month)
	{
	  diff = getmonth (texta, lena) - getmonth (textb, lenb);
	  if (diff)
	    return key->reverse ? -diff : diff;
	  continue;
	}
      else if (ignore && translate)

#ifdef __FreeBSD__
#define CMP_FUNC(A, B) COLLDIFF ((A), (B))
#else
#define CMP_FUNC(A, B) (A) - (B)
#endif
#define CMP_WITH_IGNORE(A, B)						\
  do									\
    {									\
	  while (texta < lima && textb < limb)				\
	    {								\
	      while (texta < lima && ignore[UCHAR (*texta)])		\
		++texta;						\
	      while (textb < limb && ignore[UCHAR (*textb)])		\
		++textb;						\
	      if (texta < lima && textb < limb)				\
		{							\
		  if ((A) != (B))					\
		    {							\
		      diff = CMP_FUNC((A), (B));                        \
		      break;						\
		    }							\
		  ++texta;						\
		  ++textb;						\
		}							\
									\
	      if (texta == lima && textb < limb && !ignore[UCHAR (*textb)]) \
		diff = -1;						\
	      else if (texta < lima && textb == limb			\
		       && !ignore[UCHAR (*texta)])			\
		diff = 1;						\
	    }								\
									\
	  if (diff == 0)						\
	    {								\
	      while (texta < lima && ignore[UCHAR (*texta)])		\
		++texta;						\
	      while (textb < limb && ignore[UCHAR (*textb)])		\
		++textb;						\
									\
	      if (texta == lima && textb < limb)			\
		diff = -1;						\
	      else if (texta < lima && textb == limb)			\
		diff = 1;						\
	    }								\
	  /* Relative lengths are meaningless if characters were ignored.  \
	     Handling this case here avoids what might be an invalid length  \
	     comparison below.  */					\
	  if (diff == 0 && texta == lima && textb == limb)		\
	    return 0;							\
    }									\
  while (0)

	CMP_WITH_IGNORE (translate[UCHAR (*texta)], translate[UCHAR (*textb)]);
      else if (ignore)
	CMP_WITH_IGNORE (*texta, *textb);
      else if (translate)
	while (texta < lima && textb < limb)
	  {
	    if (translate[UCHAR (*texta++)] != translate[UCHAR (*textb++)])
	      {
#ifdef __FreeBSD__
		diff = COLLDIFF (translate[UCHAR (*--texta)],
			  translate[UCHAR (*--textb)]);
#else
		diff = (translate[UCHAR (*--texta)]
			- translate[UCHAR (*--textb)]);
#endif
		break;
	      }
	  }
      else
#ifdef __FreeBSD__
	diff = collcmp (texta, textb, min (lena, lenb));
#else
	diff = memcmp (texta, textb, min (lena, lenb));
#endif

      if (diff)
	return key->reverse ? -diff : diff;
      if ((diff = lena - lenb) != 0)
	return key->reverse ? -diff : diff;
    }

  return 0;
}

/* Compare two lines A and B, returning negative, zero, or positive
   depending on whether A compares less than, equal to, or greater than B. */

static int
compare (register const struct line *a, register const struct line *b)
{
  int diff, tmpa, tmpb, mini;

  /* First try to compare on the specified keys (if any).
     The only two cases with no key at all are unadorned sort,
     and unadorned sort -r. */
  if (keyhead.next)
    {
      diff = keycompare (a, b);
      if (diff != 0)
	return diff;
      if (unique || stable)
	return 0;
    }

  /* If the keys all compare equal (or no keys were specified)
     fall through to the default byte-by-byte comparison. */
  tmpa = a->length, tmpb = b->length;
  mini = min (tmpa, tmpb);
  if (mini == 0)
    diff = tmpa - tmpb;
  else
    {
      char *ap = a->text, *bp = b->text;

#ifdef __FreeBSD__
      diff = COLLDIFF (*ap, *bp);
#else
      diff = UCHAR (*ap) - UCHAR (*bp);
#endif
      if (diff == 0)
	{
#ifdef __FreeBSD__
	  diff = collcmp (ap, bp, mini);
#else
	  diff = memcmp (ap, bp, mini);
#endif
	  if (diff == 0)
	    diff = tmpa - tmpb;
	}
    }

  return reverse ? -diff : diff;
}

/* Check that the lines read from the given FP come in order.  Return
   1 if they do and 0 if there is a disorder.
   FIXME: return number of first out-of-order line if not sorted.  */

static int
checkfp (FILE *fp)
{
  struct buffer buf;		/* Input buffer. */
  struct lines lines;		/* Lines scanned from the buffer. */
  struct line temp;		/* Copy of previous line. */
  int cc;			/* Character count. */
  int alloc, sorted = 1;

  initbuf (&buf, mergealloc);
  initlines (&lines, mergealloc / linelength + 1,
	     LINEALLOC / ((NMERGE + NMERGE) * sizeof (struct line)));
  alloc = linelength;
  temp.text = xmalloc (alloc);

  cc = fillbuf (&buf, fp);
  if (cc == 0)
    goto finish;

  findlines (&buf, &lines);

  while (1)
    {
      struct line *prev_line;	/* Pointer to previous line. */
      int cmp;			/* Result of calling compare. */
      int i;

      /* Compare each line in the buffer with its successor. */
      for (i = 0; i < lines.used - 1; ++i)
	{
	  cmp = compare (&lines.lines[i], &lines.lines[i + 1]);
	  if ((unique && cmp >= 0) || (cmp > 0))
	    {
	      sorted = 0;
	      goto finish;
	    }
	}

      /* Save the last line of the buffer and refill the buffer. */
      prev_line = lines.lines + (lines.used - 1);
      if (prev_line->length > alloc)
	{
	  while (prev_line->length + 1 > alloc)
	    alloc *= 2;
	  temp.text = xrealloc (temp.text, alloc);
	}
      memcpy (temp.text, prev_line->text, prev_line->length + 1);
      temp.length = prev_line->length;
      temp.keybeg = temp.text + (prev_line->keybeg - prev_line->text);
      temp.keylim = temp.text + (prev_line->keylim - prev_line->text);

      cc = fillbuf (&buf, fp);
      if (cc == 0)
        break;

      findlines (&buf, &lines);
      /* Make sure the line saved from the old buffer contents is
	 less than or equal to the first line of the new buffer. */
      cmp = compare (&temp, &lines.lines[0]);
      if ((unique && cmp >= 0) || (cmp > 0))
	{
	  sorted = 0;
	  break;
	}
    }

finish:
  xfclose (fp);
  free (buf.buf);
  free ((char *) lines.lines);
  free (temp.text);
  return sorted;
}

/* Merge lines from FPS onto OFP.  NFPS cannot be greater than NMERGE.
   Close FPS before returning. */

static void
mergefps (FILE **fps, register int nfps, FILE *ofp)
{
  struct buffer buffer[NMERGE];	/* Input buffers for each file. */
  struct lines lines[NMERGE];	/* Line tables for each buffer. */
  struct line saved;		/* Saved line for unique check. */
  int savedflag = 0;		/* True if there is a saved line. */
  int savealloc;		/* Size allocated for the saved line. */
  int cur[NMERGE];		/* Current line in each line table. */
  int ord[NMERGE];		/* Table representing a permutation of fps,
				   such that lines[ord[0]].lines[cur[ord[0]]]
				   is the smallest line and will be next
				   output. */
  register int i, j, t;

#ifdef lint  /* Suppress `used before initialized' warning.  */
  savealloc = 0;
#endif

  /* Allocate space for a saved line if necessary. */
  if (unique)
    {
      savealloc = linelength;
      saved.text = xmalloc (savealloc);
    }

  /* Read initial lines from each input file. */
  for (i = 0; i < nfps; ++i)
    {
      initbuf (&buffer[i], mergealloc);
      /* If a file is empty, eliminate it from future consideration. */
      while (i < nfps && !fillbuf (&buffer[i], fps[i]))
	{
	  xfclose (fps[i]);
	  --nfps;
	  for (j = i; j < nfps; ++j)
	    fps[j] = fps[j + 1];
	}
      if (i == nfps)
	free (buffer[i].buf);
      else
	{
	  initlines (&lines[i], mergealloc / linelength + 1,
		     LINEALLOC / ((NMERGE + NMERGE) * sizeof (struct line)));
	  findlines (&buffer[i], &lines[i]);
	  cur[i] = 0;
	}
    }

  /* Set up the ord table according to comparisons among input lines.
     Since this only reorders two items if one is strictly greater than
     the other, it is stable. */
  for (i = 0; i < nfps; ++i)
    ord[i] = i;
  for (i = 1; i < nfps; ++i)
    if (compare (&lines[ord[i - 1]].lines[cur[ord[i - 1]]],
		 &lines[ord[i]].lines[cur[ord[i]]]) > 0)
      t = ord[i - 1], ord[i - 1] = ord[i], ord[i] = t, i = 0;

  /* Repeatedly output the smallest line until no input remains. */
  while (nfps)
    {
      /* If uniqified output is turned on, output only the first of
	 an identical series of lines. */
      if (unique)
	{
	  if (savedflag && compare (&saved, &lines[ord[0]].lines[cur[ord[0]]]))
	    {
	      xfwrite (saved.text, 1, saved.length, ofp);
	      putc ('\n', ofp);
	      savedflag = 0;
	    }
	  if (!savedflag)
	    {
	      if (savealloc < lines[ord[0]].lines[cur[ord[0]]].length + 1)
		{
		  while (savealloc < lines[ord[0]].lines[cur[ord[0]]].length + 1)
		    savealloc *= 2;
		  saved.text = xrealloc (saved.text, savealloc);
		}
	      saved.length = lines[ord[0]].lines[cur[ord[0]]].length;
	      memcpy (saved.text, lines[ord[0]].lines[cur[ord[0]]].text,
		     saved.length + 1);
	      if (lines[ord[0]].lines[cur[ord[0]]].keybeg != NULL)
		{
		  saved.keybeg = saved.text +
		    (lines[ord[0]].lines[cur[ord[0]]].keybeg
		     - lines[ord[0]].lines[cur[ord[0]]].text);
		}
	      if (lines[ord[0]].lines[cur[ord[0]]].keylim != NULL)
		{
		  saved.keylim = saved.text +
		    (lines[ord[0]].lines[cur[ord[0]]].keylim
		     - lines[ord[0]].lines[cur[ord[0]]].text);
		}
	      savedflag = 1;
	    }
	}
      else
	{
	  xfwrite (lines[ord[0]].lines[cur[ord[0]]].text, 1,
		   lines[ord[0]].lines[cur[ord[0]]].length, ofp);
	  putc ('\n', ofp);
	}

      /* Check if we need to read more lines into core. */
      if (++cur[ord[0]] == lines[ord[0]].used)
	if (fillbuf (&buffer[ord[0]], fps[ord[0]]))
	  {
	    findlines (&buffer[ord[0]], &lines[ord[0]]);
	    cur[ord[0]] = 0;
	  }
	else
	  {
	    /* We reached EOF on fps[ord[0]]. */
	    for (i = 1; i < nfps; ++i)
	      if (ord[i] > ord[0])
		--ord[i];
	    --nfps;
	    xfclose (fps[ord[0]]);
	    free (buffer[ord[0]].buf);
	    free ((char *) lines[ord[0]].lines);
	    for (i = ord[0]; i < nfps; ++i)
	      {
		fps[i] = fps[i + 1];
		buffer[i] = buffer[i + 1];
		lines[i] = lines[i + 1];
		cur[i] = cur[i + 1];
	      }
	    for (i = 0; i < nfps; ++i)
	      ord[i] = ord[i + 1];
	    continue;
	  }

      /* The new line just read in may be larger than other lines
	 already in core; push it back in the queue until we encounter
	 a line larger than it. */
      for (i = 1; i < nfps; ++i)
	{
	  t = compare (&lines[ord[0]].lines[cur[ord[0]]],
		       &lines[ord[i]].lines[cur[ord[i]]]);
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

  if (unique && savedflag)
    {
      xfwrite (saved.text, 1, saved.length, ofp);
      putc ('\n', ofp);
      free (saved.text);
    }
}

/* Sort the array LINES with NLINES members, using TEMP for temporary space. */

static void
sortlines (struct line *lines, int nlines, struct line *temp)
{
  register struct line *lo, *hi, *t;
  register int nlo, nhi;

  if (nlines == 2)
    {
      if (compare (&lines[0], &lines[1]) > 0)
	*temp = lines[0], lines[0] = lines[1], lines[1] = *temp;
      return;
    }

  nlo = nlines / 2;
  lo = lines;
  nhi = nlines - nlo;
  hi = lines + nlo;

  if (nlo > 1)
    sortlines (lo, nlo, temp);

  if (nhi > 1)
    sortlines (hi, nhi, temp);

  t = temp;

  while (nlo && nhi)
    if (compare (lo, hi) <= 0)
      *t++ = *lo++, --nlo;
    else
      *t++ = *hi++, --nhi;
  while (nlo--)
    *t++ = *lo++;

  for (lo = lines, nlo = nlines - nhi, t = temp; nlo; --nlo)
    *lo++ = *t++;
}

/* Check that each of the NFILES FILES is ordered.
   Return a count of disordered files. */

static int
check (char **files, int nfiles)
{
  int i, disorders = 0;
  FILE *fp;

  for (i = 0; i < nfiles; ++i)
    {
      fp = xfopen (files[i], "r");
      if (!checkfp (fp))
	{
	  fprintf (stderr, _("%s: disorder on %s\n"), program_name, files[i]);
	  ++disorders;
	}
    }
  return disorders;
}

/* Merge NFILES FILES onto OFP. */

static void
merge (char **files, int nfiles, FILE *ofp)
{
  int i, j, t;
  char *temp;
  FILE *fps[NMERGE], *tfp;

  while (nfiles > NMERGE)
    {
      t = 0;
      for (i = 0; i < nfiles / NMERGE; ++i)
	{
	  for (j = 0; j < NMERGE; ++j)
	    fps[j] = xfopen (files[i * NMERGE + j], "r");
	  tfp = xtmpfopen (temp = tempname ());
	  mergefps (fps, NMERGE, tfp);
	  xfclose (tfp);
	  for (j = 0; j < NMERGE; ++j)
	    zaptemp (files[i * NMERGE + j]);
	  files[t++] = temp;
	}
      for (j = 0; j < nfiles % NMERGE; ++j)
	fps[j] = xfopen (files[i * NMERGE + j], "r");
      tfp = xtmpfopen (temp = tempname ());
      mergefps (fps, nfiles % NMERGE, tfp);
      xfclose (tfp);
      for (j = 0; j < nfiles % NMERGE; ++j)
	zaptemp (files[i * NMERGE + j]);
      files[t++] = temp;
      nfiles = t;
    }

  for (i = 0; i < nfiles; ++i)
    fps[i] = xfopen (files[i], "r");
  mergefps (fps, i, ofp);
  for (i = 0; i < nfiles; ++i)
    zaptemp (files[i]);
}

/* Sort NFILES FILES onto OFP. */

static void
sort (char **files, int nfiles, FILE *ofp)
{
  struct buffer buf;
  struct lines lines;
  struct line *tmp;
  int i, ntmp;
  FILE *fp, *tfp;
  struct tempnode *node;
  int n_temp_files = 0;
  char **tempfiles;

  initbuf (&buf, sortalloc);
  initlines (&lines, sortalloc / linelength + 1,
	     LINEALLOC / sizeof (struct line));
  ntmp = lines.alloc;
  tmp = (struct line *) xmalloc (ntmp * sizeof (struct line));

  while (nfiles--)
    {
      fp = xfopen (*files++, "r");
      while (fillbuf (&buf, fp))
	{
	  findlines (&buf, &lines);
	  if (lines.used > ntmp)
	    {
	      while (lines.used > ntmp)
		ntmp *= 2;
	      tmp = (struct line *)
		xrealloc ((char *) tmp, ntmp * sizeof (struct line));
	    }
	  sortlines (lines.lines, lines.used, tmp);
	  if (feof (fp) && !nfiles && !n_temp_files && !buf.left)
	    tfp = ofp;
	  else
	    {
	      ++n_temp_files;
	      tfp = xtmpfopen (tempname ());
	    }
	  for (i = 0; i < lines.used; ++i)
	    if (!unique || i == 0
		|| compare (&lines.lines[i], &lines.lines[i - 1]))
	      {
		xfwrite (lines.lines[i].text, 1, lines.lines[i].length, tfp);
		putc ('\n', tfp);
	      }
	  if (tfp != ofp)
	    xfclose (tfp);
	}
      xfclose (fp);
    }

  free (buf.buf);
  free ((char *) lines.lines);
  free ((char *) tmp);

  if (n_temp_files)
    {
      tempfiles = (char **) xmalloc (n_temp_files * sizeof (char *));
      i = n_temp_files;
      for (node = temphead.next; i > 0; node = node->next)
	tempfiles[--i] = node->name;
      merge (tempfiles, n_temp_files, ofp);
      free ((char *) tempfiles);
    }
}

/* Insert key KEY at the end of the list (`keyhead'). */

static void
insertkey (struct keyfield *key)
{
  struct keyfield *k = &keyhead;

  while (k->next)
    k = k->next;
  k->next = key;
  key->next = NULL;
}

static void
badfieldspec (const char *s)
{
  error (2, 0, _("invalid field specification `%s'"), s);
}

/* Handle interrupts and hangups. */

static void
sighandler (int sig)
{
#ifdef SA_INTERRUPT
  struct sigaction sigact;

  sigact.sa_handler = SIG_DFL;
  sigemptyset (&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigaction (sig, &sigact, NULL);
#else				/* !SA_INTERRUPT */
  signal (sig, SIG_DFL);
#endif				/* SA_INTERRUPT */
  cleanup ();
  kill (getpid (), sig);
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
	    key->skipsblanks = 1;
	  if (blanktype == bl_end || blanktype == bl_both)
	    key->skipeblanks = 1;
	  break;
	case 'd':
	  key->ignore = nondictionary;
	  break;
	case 'f':
	  key->translate = fold_toupper;
	  break;
	case 'g':
	  key->general_numeric = 1;
	  break;
	case 'i':
	  key->ignore = nonprinting;
	  break;
	case 'M':
	  key->month = 1;
	  break;
	case 'n':
	  key->numeric = 1;
	  if (blanktype == bl_start || blanktype == bl_both)
	    key->skipsblanks = 1;
	  if (blanktype == bl_end || blanktype == bl_both)
	    key->skipeblanks = 1;
	  break;
	case 'r':
	  key->reverse = 1;
	  break;
	default:
	  return (char *) s;
	}
      ++s;
    }
  return (char *) s;
}

void
main (int argc, char **argv)
{
  struct keyfield *key = NULL, gkey;
  char *s;
  int i, t, t2;
  int checkonly = 0, mergeonly = 0, nfiles = 0;
  char *minus = "-", *outfile = minus, **files, *tmp;
  FILE *ofp;
#ifdef SA_INTERRUPT
  struct sigaction oldact, newact;
#endif				/* SA_INTERRUPT */

#ifdef __FreeBSD__
  (void) setlocale(LC_ALL, "");
#endif
  program_name = argv[0];

  parse_long_options (argc, argv, "sort", version_string, usage);

  have_read_stdin = 0;
  inittables ();

  temp_file_prefix = getenv ("TMPDIR");
  if (temp_file_prefix == NULL)
    temp_file_prefix = DEFAULT_TMPDIR;

#ifdef SA_INTERRUPT
  newact.sa_handler = sighandler;
  sigemptyset (&newact.sa_mask);
  newact.sa_flags = 0;

  sigaction (SIGINT, NULL, &oldact);
  if (oldact.sa_handler != SIG_IGN)
    sigaction (SIGINT, &newact, NULL);
  sigaction (SIGHUP, NULL, &oldact);
  if (oldact.sa_handler != SIG_IGN)
    sigaction (SIGHUP, &newact, NULL);
  sigaction (SIGPIPE, NULL, &oldact);
  if (oldact.sa_handler != SIG_IGN)
    sigaction (SIGPIPE, &newact, NULL);
  sigaction (SIGTERM, NULL, &oldact);
  if (oldact.sa_handler != SIG_IGN)
    sigaction (SIGTERM, &newact, NULL);
#else				/* !SA_INTERRUPT */
  if (signal (SIGINT, SIG_IGN) != SIG_IGN)
    signal (SIGINT, sighandler);
  if (signal (SIGHUP, SIG_IGN) != SIG_IGN)
    signal (SIGHUP, sighandler);
  if (signal (SIGPIPE, SIG_IGN) != SIG_IGN)
    signal (SIGPIPE, sighandler);
  if (signal (SIGTERM, SIG_IGN) != SIG_IGN)
    signal (SIGTERM, sighandler);
#endif				/* !SA_INTERRUPT */

  gkey.sword = gkey.eword = -1;
  gkey.ignore = NULL;
  gkey.translate = NULL;
  gkey.numeric =  gkey.general_numeric = gkey.month = gkey.reverse = 0;
  gkey.skipsblanks = gkey.skipeblanks = 0;

  files = (char **) xmalloc (sizeof (char *) * argc);

  for (i = 1; i < argc; ++i)
    {
      if (argv[i][0] == '+')
	{
	  if (key)
	    insertkey (key);
	  key = (struct keyfield *) xmalloc (sizeof (struct keyfield));
	  key->eword = -1;
	  key->ignore = NULL;
	  key->translate = NULL;
	  key->skipsblanks = key->skipeblanks = 0;
	  key->numeric = key->general_numeric = key->month = key->reverse = 0;
	  s = argv[i] + 1;
	  if (! (digits[UCHAR (*s)] || (*s == '.' && digits[UCHAR (s[1])])))
	    badfieldspec (argv[i]);
	  for (t = 0; digits[UCHAR (*s)]; ++s)
	    t = 10 * t + *s - '0';
	  t2 = 0;
	  if (*s == '.')
	    for (++s; digits[UCHAR (*s)]; ++s)
	      t2 = 10 * t2 + *s - '0';
	  if (t2 || t)
	    {
	      key->sword = t;
	      key->schar = t2;
	    }
	  else
	    key->sword = -1;
	  s = set_ordering (s, key, bl_start);
	  if (*s)
	    badfieldspec (argv[i]);
	}
      else if (argv[i][0] == '-' && argv[i][1])
	{
	  s = argv[i] + 1;
	  if (digits[UCHAR (*s)] || (*s == '.' && digits[UCHAR (s[1])]))
	    {
	      if (!key)
		usage (2);
	      for (t = 0; digits[UCHAR (*s)]; ++s)
		t = t * 10 + *s - '0';
	      t2 = 0;
	      if (*s == '.')
		for (++s; digits[UCHAR (*s)]; ++s)
		  t2 = t2 * 10 + *s - '0';
	      key->eword = t;
	      key->echar = t2;
	      s = set_ordering (s, key, bl_end);
	      if (*s)
		badfieldspec (argv[i]);
	      insertkey (key);
	      key = NULL;
	    }
	  else
	    while (*s)
	      {
		s = set_ordering (s, &gkey, bl_both);
		switch (*s)
		  {
		  case '\0':
		    break;
		  case 'c':
		    checkonly = 1;
		    break;
		  case 'k':
		    if (s[1])
		      ++s;
		    else
		      {
			if (i == argc - 1)
			  error (2, 0, _("option `-k' requires an argument"));
			else
			  s = argv[++i];
		      }
		    if (key)
		      insertkey (key);
		    key = (struct keyfield *)
		      xmalloc (sizeof (struct keyfield));
		    key->eword = -1;
		    key->ignore = NULL;
		    key->translate = NULL;
		    key->skipsblanks = key->skipeblanks = 0;
		    key->numeric = key->month = key->reverse = 0;
		    /* Get POS1. */
		    if (!digits[UCHAR (*s)])
		      badfieldspec (argv[i]);
		    for (t = 0; digits[UCHAR (*s)]; ++s)
		      t = 10 * t + *s - '0';
		    if (t == 0)
		      {
			/* Provoke with `sort -k0' */
			error (0, 0, _("the starting field number argument \
to the `-k' option must be positive"));
			badfieldspec (argv[i]);
		      }
		    --t;
		    t2 = 0;
		    if (*s == '.')
		      {
			if (!digits[UCHAR (s[1])])
			  {
			    /* Provoke with `sort -k1.' */
			    error (0, 0, _("starting field spec has `.' but \
lacks following character offset"));
			    badfieldspec (argv[i]);
			  }
			for (++s; digits[UCHAR (*s)]; ++s)
			  t2 = 10 * t2 + *s - '0';
			if (t2 == 0)
			  {
			    /* Provoke with `sort -k1.0' */
			    error (0, 0, _("starting field character offset \
argument to the `-k' option\nmust be positive"));
			    badfieldspec (argv[i]);
			  }
			--t2;
		      }
		    if (t2 || t)
		      {
			key->sword = t;
			key->schar = t2;
		      }
		    else
		      key->sword = -1;
		    s = set_ordering (s, key, bl_start);
		    if (*s == 0)
		      {
			key->eword = -1;
			key->echar = 0;
		      }
		    else if (*s != ',')
		      badfieldspec (argv[i]);
		    else if (*s == ',')
		      {
			/* Skip over comma.  */
			++s;
			if (*s == 0)
			  {
			    /* Provoke with `sort -k1,' */
			    error (0, 0, _("field specification has `,' but \
lacks following field spec"));
			    badfieldspec (argv[i]);
			  }
			/* Get POS2. */
			for (t = 0; digits[UCHAR (*s)]; ++s)
			  t = t * 10 + *s - '0';
			if (t == 0)
			  {
			    /* Provoke with `sort -k1,0' */
			    error (0, 0, _("ending field number argument \
to the `-k' option must be positive"));
			    badfieldspec (argv[i]);
			  }
			--t;
			t2 = 0;
			if (*s == '.')
			  {
			    if (!digits[UCHAR (s[1])])
			      {
				/* Provoke with `sort -k1,1.' */
				error (0, 0, _("ending field spec has `.' \
but lacks following character offset"));
				badfieldspec (argv[i]);
			      }
			    for (++s; digits[UCHAR (*s)]; ++s)
			      t2 = t2 * 10 + *s - '0';
			  }
			else
			  {
			    /* `-k 2,3' is equivalent to `+1 -3'.  */
			    ++t;
			  }
			key->eword = t;
			key->echar = t2;
			s = set_ordering (s, key, bl_end);
			if (*s)
			  badfieldspec (argv[i]);
		      }
		    insertkey (key);
		    key = NULL;
		    goto outer;
		  case 'm':
		    mergeonly = 1;
		    break;
		  case 'o':
		    if (s[1])
		      outfile = s + 1;
		    else
		      {
			if (i == argc - 1)
			  error (2, 0, _("option `-o' requires an argument"));
			else
			  outfile = argv[++i];
		      }
		    goto outer;
		  case 's':
		    stable = 1;
		    break;
		  case 't':
		    if (s[1])
		      tab = *++s;
		    else if (i < argc - 1)
		      {
			tab = *argv[++i];
			goto outer;
		      }
		    else
		      error (2, 0, _("option `-t' requires an argument"));
		    break;
		  case 'T':
		    if (s[1])
		      temp_file_prefix = ++s;
		    else
		      {
			if (i < argc - 1)
			  temp_file_prefix = argv[++i];
			else
			  error (2, 0, _("option `-T' requires an argument"));
		      }
		    goto outer;
		    /* break; */
		  case 'u':
		    unique = 1;
		    break;
		  case 'y':
		    /* Accept and ignore e.g. -y0 for compatibility with
		       Solaris 2.  */
		    goto outer;
		  default:
		    fprintf (stderr, _("%s: unrecognized option `-%c'\n"),
			     argv[0], *s);
		    usage (2);
		  }
		if (*s)
		  ++s;
	      }
	}
      else			/* Not an option. */
	{
	  files[nfiles++] = argv[i];
	}
    outer:;
    }

  if (key)
    insertkey (key);

  /* Inheritance of global options to individual keys. */
  for (key = keyhead.next; key; key = key->next)
    if (!key->ignore && !key->translate && !key->skipsblanks && !key->reverse
	&& !key->skipeblanks && !key->month && !key->numeric
        && !key->general_numeric)
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

  if (!keyhead.next && (gkey.ignore || gkey.translate || gkey.skipsblanks
			|| gkey.skipeblanks || gkey.month || gkey.numeric
                        || gkey.general_numeric))
    insertkey (&gkey);
  reverse = gkey.reverse;

  if (nfiles == 0)
    {
      nfiles = 1;
      files = &minus;
    }

  if (checkonly)
    exit (check (files, nfiles) != 0);

  if (strcmp (outfile, "-"))
    {
      struct stat outstat;
      if (stat (outfile, &outstat) == 0)
	{
	  /* The following code prevents a race condition when
	     people use the brain dead shell programming idiom:
		  cat file | sort -o file
	     This feature is provided for historical compatibility,
	     but we strongly discourage ever relying on this in
	     new shell programs. */

	  /* Temporarily copy each input file that might be another name
	     for the output file.  When in doubt (e.g. a pipe), copy.  */
	  for (i = 0; i < nfiles; ++i)
	    {
	      char buf[8192];
	      FILE *fp;
	      int cc;

	      if (S_ISREG (outstat.st_mode) && strcmp (outfile, files[i]))
		{
		  struct stat instat;
		  if ((strcmp (files[i], "-")
		       ? stat (files[i], &instat)
		       : fstat (fileno (stdin), &instat)) != 0)
		    {
		      error (0, errno, "%s", files[i]);
		      cleanup ();
		      exit (2);
		    }
		  if (S_ISREG (instat.st_mode)
		      && (instat.st_ino != outstat.st_ino
			  || instat.st_dev != outstat.st_dev))
		    {
		      /* We know the files are distinct.  */
		      continue;
		    }
		}

	      fp = xfopen (files[i], "r");
	      tmp = tempname ();
	      ofp = xtmpfopen (tmp);
	      while ((cc = fread (buf, 1, sizeof buf, fp)) > 0)
		xfwrite (buf, 1, cc, ofp);
	      if (ferror (fp))
		{
		  error (0, errno, "%s", files[i]);
		  cleanup ();
		  exit (2);
		}
	      xfclose (ofp);
	      xfclose (fp);
	      files[i] = tmp;
	    }
	}
      ofp = xfopen (outfile, "w");
    }
  else
    ofp = stdout;

  if (mergeonly)
    merge (files, nfiles, ofp);
  else
    sort (files, nfiles, ofp);
  cleanup ();

  /* If we wait for the implicit flush on exit, and the parent process
     has closed stdout (e.g., exec >&- in a shell), then the output file
     winds up empty.  I don't understand why.  This is under SunOS,
     Solaris, Ultrix, and Irix.  This premature fflush makes the output
     reappear. --karl@cs.umb.edu  */
  if (fflush (ofp) < 0)
    error (1, errno, _("%s: write error"), outfile);

  if (have_read_stdin && fclose (stdin) == EOF)
    error (1, errno, outfile);
  if (ferror (stdout) || fclose (stdout) == EOF)
    error (1, errno, _("%s: write error"), outfile);

  exit (0);
}
