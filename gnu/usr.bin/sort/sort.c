/* sort - sort lines of text (with all kinds of options).
   Copyright (C) 1988, 1991 Free Software Foundation

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Written December 1988 by Mike Haertel.
   The author may be reached (Email) at the address mike@gnu.ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation. */

/* Get isblank from GNU libc.  */
#define _GNU_SOURCE

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include "system.h"
#ifdef _POSIX_VERSION
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

void error ();
static void usage ();

#define min(a, b) ((a) < (b) ? (a) : (b))
#define UCHAR_LIM (UCHAR_MAX + 1)
#define UCHAR(c) ((unsigned char) (c))

/* The kind of blanks for '-b' to skip in various options. */
enum blanktype { bl_start, bl_end, bl_both };

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
static struct month
{
  char *name;
  int val;
} const monthtab[] =
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
static int sortalloc =  524288;

/* Initial buffer size for in core merge buffers.  Bear in mind that
   up to NMERGE * mergealloc bytes may be allocated for merge buffers. */
static int mergealloc =  16384;

/* Guess of average line length. */
static int linelength = 30;

/* Maximum number of elements for the array(s) of struct line's, in bytes.  */
#define LINEALLOC 262144

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

/* Lists of key field comparisons to be tried. */
static struct keyfield
{
  int sword;			/* Zero-origin 'word' to start at. */
  int schar;			/* Additional characters to skip. */
  int skipsblanks;		/* Skip leading white space at start. */
  int eword;			/* Zero-origin first word after field. */
  int echar;			/* Additional characters in field. */
  int skipeblanks;		/* Skip trailing white space at finish. */
  int *ignore;			/* Boolean array of characters to ignore. */
  char *translate;		/* Translation applied to characters. */
  int numeric;			/* Flag for numeric comparison. */
  int month;			/* Flag for comparison by month name. */
  int reverse;			/* Reverse the sense of comparison. */
  struct keyfield *next;	/* Next keyfield to try. */
} keyhead;

/* The list of temporary files. */
static struct tempnode
{
  char *name;
  struct tempnode *next;
} temphead;

/* Clean up any remaining temporary files. */

static void
cleanup ()
{
  struct tempnode *node;

  for (node = temphead.next; node; node = node->next)
    unlink (node->name);
}

/* Allocate N bytes of memory dynamically, with error checking.  */

char *
xmalloc (n)
     unsigned n;
{
  char *p;

  p = malloc (n);
  if (p == 0)
    {
      error (0, 0, "virtual memory exhausted");
      cleanup ();
      exit (2);
    }
  return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.
   If P is NULL, run xmalloc.
   If N is 0, run free and return NULL.  */

char *
xrealloc (p, n)
     char *p;
     unsigned n;
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
      error (0, 0, "virtual memory exhausted");
      cleanup ();
      exit (2);
    }
  return p;
}

static FILE *
xfopen (file, how)
     char *file, *how;
{
  FILE *fp = strcmp (file, "-") ? fopen (file, how) : stdin;

  if (fp == 0)
    {
      error (0, errno, "%s", file);
      cleanup ();
      exit (2);
    }
  if (fp == stdin)
    have_read_stdin = 1;
  return fp;
}

static void
xfclose (fp)
     FILE *fp;
{
  fflush (fp);
  if (fp != stdin && fp != stdout)
    {
      if (fclose (fp) != 0)
	{
	  error (0, errno, "error closing file");
	  cleanup ();
	  exit (2);
	}
    }
  else
    /* Allow reading stdin from tty more than once. */
    clearerr (fp);
}

static void
xfwrite (buf, size, nelem, fp)
     char *buf;
     int size, nelem;
     FILE *fp;
{
  if (fwrite (buf, size, nelem, fp) != nelem)
    {
      error (0, errno, "write error");
      cleanup ();
      exit (2);
    }
}

/* Return a name for a temporary file. */

static char *
tempname ()
{
  static int seq;
  int len = strlen (temp_file_prefix);
  char *name = xmalloc (len + 16);
  struct tempnode *node =
  (struct tempnode *) xmalloc (sizeof (struct tempnode));

  if (len && temp_file_prefix[len - 1] != '/')
    sprintf (name, "%s/sort%5.5d%5.5d", temp_file_prefix, getpid (), ++seq);
  else
    sprintf (name, "%ssort%5.5d%5.5d", temp_file_prefix, getpid (), ++seq);
  node->name = name;
  node->next = temphead.next;
  temphead.next = node;
  return name;
}

/* Search through the list of temporary files for NAME;
   remove it if it is found on the list. */

static void
zaptemp (name)
     char *name;
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
inittables ()
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
}

/* Initialize BUF, allocating ALLOC bytes initially. */

static void
initbuf (buf, alloc)
     struct buffer *buf;
     int alloc;
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
fillbuf (buf, fp)
     struct buffer *buf;
     FILE *fp;
{
  int cc;

  bcopy (buf->buf + buf->used - buf->left, buf->buf, buf->left);
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
	  error (0, errno, "read error");
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
initlines (lines, alloc, limit)
     struct lines *lines;
     int alloc;
     int limit;
{
  lines->alloc = alloc;
  lines->lines = (struct line *) xmalloc (lines->alloc * sizeof (struct line));
  lines->used = 0;
  lines->limit = limit;
}

/* Return a pointer to the first character of the field specified
   by KEY in LINE. */

static char *
begfield (line, key)
     struct line *line;
     struct keyfield *key;
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

  while (ptr < lim && schar--)
    ++ptr;

  return ptr;
}

/* Return the limit of (a pointer to the first character after) the field
   in LINE specified by KEY. */

static char *
limfield (line, key)
     struct line *line;
     struct keyfield *key;
{
  register char *ptr = line->text, *lim = ptr + line->length;
  register int eword = key->eword, echar = key->echar;

  if (tab)
    while (ptr < lim && eword--)
      {
	while (ptr < lim && *ptr != tab)
	  ++ptr;
	if (ptr < lim && (eword || key->skipeblanks))
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

  if (key->skipeblanks)
    while (ptr < lim && blanks[UCHAR (*ptr)])
      ++ptr;

  while (ptr < lim && echar--)
    ++ptr;

  return ptr;
}

/* Find the lines in BUF, storing pointers and lengths in LINES.
   Also replace newlines with NULs. */

static void
findlines (buf, lines)
     struct buffer *buf;
     struct lines *lines;
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
fraccompare (a, b)
     register char *a, *b;
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
numcompare (a, b)
     register char *a, *b;
{
  register int tmpa, tmpb, loga, logb, tmp;

  tmpa = UCHAR (*a), tmpb = UCHAR (*b);

  while (blanks[tmpa])
    tmpa = UCHAR (*++a);
  while (blanks[tmpb])
    tmpb = UCHAR (*++b);

  if (tmpa == '-')
    {
      tmpa = UCHAR (*++a);
      if (tmpb != '-')
	{
	  if (digits[tmpa] && digits[tmpb])
	    return -1;
	  return 0;
	}
      tmpb = UCHAR (*++b);

      while (tmpa == '0')
	tmpa = UCHAR (*++a);
      while (tmpb == '0')
	tmpb = UCHAR (*++b);

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

      return tmpb - tmpa;
    }
  else if (tmpb == '-')
    {
      if (digits[UCHAR (tmpa)] && digits[UCHAR (*++b)])
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

      return tmpa - tmpb;
    }
}

/* Return an integer <= 12 associated with month name S with length LEN,
   0 if the name in S is not recognized. */

static int
getmonth (s, len)
     char *s;
     int len;
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
keycompare (a, b)
     struct line *a, *b;
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
      else if (key->month)
	{
	  diff = getmonth (texta, lena) - getmonth (textb, lenb);
	  if (diff)
	    return key->reverse ? -diff : diff;
	  continue;
	}
      else if (ignore && translate)
	while (texta < lima && textb < limb)
	  {
	    while (texta < lima && ignore[UCHAR (*texta)])
	      ++texta;
	    while (textb < limb && ignore[UCHAR (*textb)])
	      ++textb;
	    if (texta < lima && textb < limb &&
		translate[UCHAR (*texta++)] != translate[UCHAR (*textb++)])
	      {
		diff = translate[UCHAR (*--texta)] - translate[UCHAR (*--textb)];
		break;
	      }
	  }
      else if (ignore)
	while (texta < lima && textb < limb)
	  {
	    while (texta < lima && ignore[UCHAR (*texta)])
	      ++texta;
	    while (textb < limb && ignore[UCHAR (*textb)])
	      ++textb;
	    if (texta < lima && textb < limb && *texta++ != *textb++)
	      {
		diff = *--texta - *--textb;
		break;
	      }
	  }
      else if (translate)
	while (texta < lima && textb < limb)
	  {
	    if (translate[UCHAR (*texta++)] != translate[UCHAR (*textb++)])
	      {
		diff = translate[UCHAR (*--texta)] - translate[UCHAR (*--textb)];
		break;
	      }
	  }
      else
	diff = memcmp (texta, textb, min (lena, lenb));

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
compare (a, b)
     register struct line *a, *b;
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

      diff = UCHAR (*ap) - UCHAR (*bp);
      if (diff == 0)
	{
	  diff = memcmp (ap, bp, mini);
	  if (diff == 0)
	    diff = tmpa - tmpb;
	}
    }

  return reverse ? -diff : diff;
}

/* Check that the lines read from the given FP come in order.  Return
   1 if they do and 0 if there is a disorder. */

static int
checkfp (fp)
     FILE *fp;
{
  struct buffer buf;		/* Input buffer. */
  struct lines lines;		/* Lines scanned from the buffer. */
  struct line temp;		/* Copy of previous line. */
  int cc;			/* Character count. */
  int cmp;			/* Result of calling compare. */
  int alloc, i, success = 1;

  initbuf (&buf, mergealloc);
  initlines (&lines, mergealloc / linelength + 1,
	     LINEALLOC / ((NMERGE + NMERGE) * sizeof (struct line)));
  alloc = linelength;
  temp.text = xmalloc (alloc);

  cc = fillbuf (&buf, fp);
  findlines (&buf, &lines);

  if (cc)
    do
      {
	/* Compare each line in the buffer with its successor. */
	for (i = 0; i < lines.used - 1; ++i)
	  {
	    cmp = compare (&lines.lines[i], &lines.lines[i + 1]);
	    if ((unique && cmp >= 0) || (cmp > 0))
	      {
		success = 0;
		goto finish;
	      }
	  }

	/* Save the last line of the buffer and refill the buffer. */
	if (lines.lines[lines.used - 1].length > alloc)
	  {
	    while (lines.lines[lines.used - 1].length + 1 > alloc)
	      alloc *= 2;
	    temp.text = xrealloc (temp.text, alloc);
	  }
	bcopy (lines.lines[lines.used - 1].text, temp.text,
	       lines.lines[lines.used - 1].length + 1);
	temp.length = lines.lines[lines.used - 1].length;

	cc = fillbuf (&buf, fp);
	if (cc)
	  {
	    findlines (&buf, &lines);
	    /* Make sure the line saved from the old buffer contents is
	       less than or equal to the first line of the new buffer. */
	    cmp = compare (&temp, &lines.lines[0]);
	    if ((unique && cmp >= 0) || (cmp > 0))
	      {
		success = 0;
		break;
	      }
	  }
      }
    while (cc);

finish:
  xfclose (fp);
  free (buf.buf);
  free ((char *) lines.lines);
  free (temp.text);
  return success;
}

/* Merge lines from FPS onto OFP.  NFPS cannot be greater than NMERGE.
   Close FPS before returning. */

static void
mergefps (fps, nfps, ofp)
     FILE *fps[], *ofp;
     register int nfps;
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
      /* If uniqified output is turned out, output only the first of
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
	      bcopy (lines[ord[0]].lines[cur[ord[0]]].text, saved.text,
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
sortlines (lines, nlines, temp)
     struct line *lines, *temp;
     int nlines;
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
check (files, nfiles)
     char *files[];
     int nfiles;
{
  int i, disorders = 0;
  FILE *fp;

  for (i = 0; i < nfiles; ++i)
    {
      fp = xfopen (files[i], "r");
      if (!checkfp (fp))
	{
	  printf ("%s: disorder on %s\n", program_name, files[i]);
	  ++disorders;
	}
    }
  return disorders;
}

/* Merge NFILES FILES onto OFP. */

static void
merge (files, nfiles, ofp)
     char *files[];
     int nfiles;
     FILE *ofp;
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
	  tfp = xfopen (temp = tempname (), "w");
	  mergefps (fps, NMERGE, tfp);
	  xfclose (tfp);
	  for (j = 0; j < NMERGE; ++j)
	    zaptemp (files[i * NMERGE + j]);
	  files[t++] = temp;
	}
      for (j = 0; j < nfiles % NMERGE; ++j)
	fps[j] = xfopen (files[i * NMERGE + j], "r");
      tfp = xfopen (temp = tempname (), "w");
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
sort (files, nfiles, ofp)
     char **files;
     int nfiles;
     FILE *ofp;
{
  struct buffer buf;
  struct lines lines;
  struct line *tmp;
  int i, ntmp;
  FILE *fp, *tfp;
  struct tempnode *node;
  int ntemp = 0;
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
	  if (feof (fp) && !nfiles && !ntemp && !buf.left)
	    tfp = ofp;
	  else
	    {
	      ++ntemp;
	      tfp = xfopen (tempname (), "w");
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

  if (ntemp)
    {
      tempfiles = (char **) xmalloc (ntemp * sizeof (char *));
      i = ntemp;
      for (node = temphead.next; i > 0; node = node->next)
	tempfiles[--i] = node->name;
      merge (tempfiles, ntemp, ofp);
      free ((char *) tempfiles);
    }
}

/* Insert key KEY at the end of the list (`keyhead'). */

static void
insertkey (key)
     struct keyfield *key;
{
  struct keyfield *k = &keyhead;

  while (k->next)
    k = k->next;
  k->next = key;
  key->next = NULL;
}

static void
badfieldspec (s)
     char *s;
{
  error (2, 0, "invalid field specification `%s'", s);
}

/* Handle interrupts and hangups. */

static void
sighandler (sig)
     int sig;
{
#ifdef _POSIX_VERSION
  struct sigaction sigact;

  sigact.sa_handler = SIG_DFL;
  sigemptyset (&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigaction (sig, &sigact, NULL);
#else				/* !_POSIX_VERSION */
  signal (sig, SIG_DFL);
#endif				/* _POSIX_VERSION */
  cleanup ();
  kill (getpid (), sig);
}

/* Set the ordering options for KEY specified in S.
   Return the address of the first character in S that
   is not a valid ordering option.
   BLANKTYPE is the kind of blanks that 'b' should skip. */

static char *
set_ordering (s, key, blanktype)
     register char *s;
     struct keyfield *key;
     enum blanktype blanktype;
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
#if 0
	case 'g':
	  /* Reserved for comparing floating-point numbers. */
	  break;
#endif
	case 'i':
	  key->ignore = nonprinting;
	  break;
	case 'M':
	  key->month = 1;
	  break;
	case 'n':
	  key->numeric = 1;
	  break;
	case 'r':
	  key->reverse = 1;
	  break;
	default:
	  return s;
	}
      ++s;
    }
  return s;
}

void
main (argc, argv)
     int argc;
     char *argv[];
{
  struct keyfield *key = NULL, gkey;
  char *s;
  int i, t, t2;
  int checkonly = 0, mergeonly = 0, nfiles = 0;
  char *minus = "-", *outfile = minus, **files, *tmp;
  FILE *ofp;
#ifdef _POSIX_VERSION
  struct sigaction oldact, newact;
#endif				/* _POSIX_VERSION */

  program_name = argv[0];
  have_read_stdin = 0;
  inittables ();

  temp_file_prefix = getenv ("TMPDIR");
  if (temp_file_prefix == NULL)
    temp_file_prefix = "/tmp";

#ifdef _POSIX_VERSION
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
#else				/* !_POSIX_VERSION */
  if (signal (SIGINT, SIG_IGN) != SIG_IGN)
    signal (SIGINT, sighandler);
  if (signal (SIGHUP, SIG_IGN) != SIG_IGN)
    signal (SIGHUP, sighandler);
  if (signal (SIGPIPE, SIG_IGN) != SIG_IGN)
    signal (SIGPIPE, sighandler);
  if (signal (SIGTERM, SIG_IGN) != SIG_IGN)
    signal (SIGTERM, sighandler);
#endif				/* !_POSIX_VERSION */

  gkey.sword = gkey.eword = -1;
  gkey.ignore = NULL;
  gkey.translate = NULL;
  gkey.numeric = gkey.month = gkey.reverse = 0;
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
	  key->numeric = key->month = key->reverse = 0;
	  s = argv[i] + 1;
	  if (!digits[UCHAR (*s)])
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
	  if (digits[UCHAR (*s)])
	    {
	      if (!key)
		usage ();
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
			  error (2, 0, "option `-k' requires an argument");
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
		    if (t)
		      t--;
		    t2 = 0;
		    if (*s == '.')
		      {
			for (++s; digits[UCHAR (*s)]; ++s)
			  t2 = 10 * t2 + *s - '0';
			if (t2)
			  t2--;
		      }
		    if (t2 || t)
		      {
			key->sword = t;
			key->schar = t2;
		      }
		    else
		      key->sword = -1;
		    s = set_ordering (s, key, bl_start);
		    if (*s && *s != ',')
		      badfieldspec (argv[i]);
		    else if (*s++)
		      {
			/* Get POS2. */
			for (t = 0; digits[UCHAR (*s)]; ++s)
			  t = t * 10 + *s - '0';
			t2 = 0;
			if (*s == '.')
			  {
			    for (++s; digits[UCHAR (*s)]; ++s)
			      t2 = t2 * 10 + *s - '0';
			    if (t2)
			      t--;
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
			  error (2, 0, "option `-o' requires an argument");
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
		      error (2, 0, "option `-t' requires an argument");
		    break;
		  case 'T':
		    if (s[1])
		      temp_file_prefix = ++s;
		    else if (i < argc - 1)
		      {
			temp_file_prefix = argv[++i];
			goto outer;
		      }
		    else
		      error (2, 0, "option `-T' requires an argument");
		    break;
		  case 'u':
		    unique = 1;
		    break;
		  default:
		    fprintf (stderr, "%s: unrecognized option `-%c'\n",
			     argv[0], *s);
		    usage ();
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
	&& !key->skipeblanks && !key->month && !key->numeric)
      {
	key->ignore = gkey.ignore;
	key->translate = gkey.translate;
	key->skipsblanks = gkey.skipsblanks;
	key->skipeblanks = gkey.skipeblanks;
	key->month = gkey.month;
	key->numeric = gkey.numeric;
	key->reverse = gkey.reverse;
      }

  if (!keyhead.next && (gkey.ignore || gkey.translate || gkey.skipsblanks
			|| gkey.skipeblanks || gkey.month || gkey.numeric))
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
      for (i = 0; i < nfiles; ++i)
	if (!strcmp (outfile, files[i]))
	  break;
      if (i == nfiles)
	ofp = xfopen (outfile, "w");
      else
	{
	  char buf[8192];
	  FILE *fp = xfopen (outfile, "r");
	  int cc;

	  tmp = tempname ();
	  ofp = xfopen (tmp, "w");
	  while ((cc = fread (buf, 1, sizeof buf, fp)) > 0)
	    xfwrite (buf, 1, cc, ofp);
	  if (ferror (fp))
	    {
	      error (0, errno, "%s", outfile);
	      cleanup ();
	      exit (2);
	    }
	  xfclose (ofp);
	  xfclose (fp);
	  files[i] = tmp;
	  ofp = xfopen (outfile, "w");
	}
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
    error (1, errno, "fflush", outfile);

  if (have_read_stdin && fclose (stdin) == EOF)
    error (1, errno, "-");
  if (ferror (stdout) || fclose (stdout) == EOF)
    error (1, errno, "write error");

  exit (0);
}

static void
usage ()
{
  fprintf (stderr, "\
Usage: %s [-cmus] [-t separator] [-o output-file] [-T tempdir] [-bdfiMnr]\n\
       [+POS1 [-POS2]] [-k POS1[,POS2]] [file...]\n",
	   program_name);
  exit (2);
}
