/* quotearg.c - quote arguments for output

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2004, 2005, 2006, 2007 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Paul Eggert <eggert@twinsun.com> */

#include <config.h>

#include "quotearg.h"

#include "xalloc.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)
#define N_(msgid) msgid

#if !HAVE_MBRTOWC
/* Disable multibyte processing entirely.  Since MB_CUR_MAX is 1, the
   other macros are defined only for documentation and to satisfy C
   syntax.  */
# undef MB_CUR_MAX
# define MB_CUR_MAX 1
# undef mbstate_t
# define mbstate_t int
# define mbrtowc(pwc, s, n, ps) ((*(pwc) = *(s)) != 0)
# define iswprint(wc) isprint ((unsigned char) (wc))
# undef HAVE_MBSINIT
#endif

#if !defined mbsinit && !HAVE_MBSINIT
# define mbsinit(ps) 1
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

#define INT_BITS (sizeof (int) * CHAR_BIT)

struct quoting_options
{
  /* Basic quoting style.  */
  enum quoting_style style;

  /* Quote the characters indicated by this bit vector even if the
     quoting style would not normally require them to be quoted.  */
  unsigned int quote_these_too[(UCHAR_MAX / INT_BITS) + 1];
};

/* Names of quoting styles.  */
char const *const quoting_style_args[] =
{
  "literal",
  "shell",
  "shell-always",
  "c",
  "escape",
  "locale",
  "clocale",
  0
};

/* Correspondences to quoting style names.  */
enum quoting_style const quoting_style_vals[] =
{
  literal_quoting_style,
  shell_quoting_style,
  shell_always_quoting_style,
  c_quoting_style,
  escape_quoting_style,
  locale_quoting_style,
  clocale_quoting_style
};

/* The default quoting options.  */
static struct quoting_options default_quoting_options;

/* Allocate a new set of quoting options, with contents initially identical
   to O if O is not null, or to the default if O is null.
   It is the caller's responsibility to free the result.  */
struct quoting_options *
clone_quoting_options (struct quoting_options *o)
{
  int e = errno;
  struct quoting_options *p = xmemdup (o ? o : &default_quoting_options,
				       sizeof *o);
  errno = e;
  return p;
}

/* Get the value of O's quoting style.  If O is null, use the default.  */
enum quoting_style
get_quoting_style (struct quoting_options *o)
{
  return (o ? o : &default_quoting_options)->style;
}

/* In O (or in the default if O is null),
   set the value of the quoting style to S.  */
void
set_quoting_style (struct quoting_options *o, enum quoting_style s)
{
  (o ? o : &default_quoting_options)->style = s;
}

/* In O (or in the default if O is null),
   set the value of the quoting options for character C to I.
   Return the old value.  Currently, the only values defined for I are
   0 (the default) and 1 (which means to quote the character even if
   it would not otherwise be quoted).  */
int
set_char_quoting (struct quoting_options *o, char c, int i)
{
  unsigned char uc = c;
  unsigned int *p =
    (o ? o : &default_quoting_options)->quote_these_too + uc / INT_BITS;
  int shift = uc % INT_BITS;
  int r = (*p >> shift) & 1;
  *p ^= ((i & 1) ^ r) << shift;
  return r;
}

/* MSGID approximates a quotation mark.  Return its translation if it
   has one; otherwise, return either it or "\"", depending on S.  */
static char const *
gettext_quote (char const *msgid, enum quoting_style s)
{
  char const *translation = _(msgid);
  if (translation == msgid && s == clocale_quoting_style)
    translation = "\"";
  return translation;
}

/* Place into buffer BUFFER (of size BUFFERSIZE) a quoted version of
   argument ARG (of size ARGSIZE), using QUOTING_STYLE and the
   non-quoting-style part of O to control quoting.
   Terminate the output with a null character, and return the written
   size of the output, not counting the terminating null.
   If BUFFERSIZE is too small to store the output string, return the
   value that would have been returned had BUFFERSIZE been large enough.
   If ARGSIZE is SIZE_MAX, use the string length of the argument for ARGSIZE.

   This function acts like quotearg_buffer (BUFFER, BUFFERSIZE, ARG,
   ARGSIZE, O), except it uses QUOTING_STYLE instead of the quoting
   style specified by O, and O may not be null.  */

static size_t
quotearg_buffer_restyled (char *buffer, size_t buffersize,
			  char const *arg, size_t argsize,
			  enum quoting_style quoting_style,
			  struct quoting_options const *o)
{
  size_t i;
  size_t len = 0;
  char const *quote_string = 0;
  size_t quote_string_len = 0;
  bool backslash_escapes = false;
  bool unibyte_locale = MB_CUR_MAX == 1;

#define STORE(c) \
    do \
      { \
	if (len < buffersize) \
	  buffer[len] = (c); \
	len++; \
      } \
    while (0)

  switch (quoting_style)
    {
    case c_quoting_style:
      STORE ('"');
      backslash_escapes = true;
      quote_string = "\"";
      quote_string_len = 1;
      break;

    case escape_quoting_style:
      backslash_escapes = true;
      break;

    case locale_quoting_style:
    case clocale_quoting_style:
      {
	/* TRANSLATORS:
	   Get translations for open and closing quotation marks.

	   The message catalog should translate "`" to a left
	   quotation mark suitable for the locale, and similarly for
	   "'".  If the catalog has no translation,
	   locale_quoting_style quotes `like this', and
	   clocale_quoting_style quotes "like this".

	   For example, an American English Unicode locale should
	   translate "`" to U+201C (LEFT DOUBLE QUOTATION MARK), and
	   should translate "'" to U+201D (RIGHT DOUBLE QUOTATION
	   MARK).  A British English Unicode locale should instead
	   translate these to U+2018 (LEFT SINGLE QUOTATION MARK) and
	   U+2019 (RIGHT SINGLE QUOTATION MARK), respectively.

	   If you don't know what to put here, please see
	   <http://en.wikipedia.org/wiki/Quotation_mark#Glyphs>
	   and use glyphs suitable for your language.  */

	char const *left = gettext_quote (N_("`"), quoting_style);
	char const *right = gettext_quote (N_("'"), quoting_style);
	for (quote_string = left; *quote_string; quote_string++)
	  STORE (*quote_string);
	backslash_escapes = true;
	quote_string = right;
	quote_string_len = strlen (quote_string);
      }
      break;

    case shell_always_quoting_style:
      STORE ('\'');
      quote_string = "'";
      quote_string_len = 1;
      break;

    default:
      break;
    }

  for (i = 0;  ! (argsize == SIZE_MAX ? arg[i] == '\0' : i == argsize);  i++)
    {
      unsigned char c;
      unsigned char esc;

      if (backslash_escapes
	  && quote_string_len
	  && i + quote_string_len <= argsize
	  && memcmp (arg + i, quote_string, quote_string_len) == 0)
	STORE ('\\');

      c = arg[i];
      switch (c)
	{
	case '\0':
	  if (backslash_escapes)
	    {
	      STORE ('\\');
	      STORE ('0');
	      STORE ('0');
	      c = '0';
	    }
	  break;

	case '?':
	  switch (quoting_style)
	    {
	    case shell_quoting_style:
	      goto use_shell_always_quoting_style;

	    case c_quoting_style:
	      if (i + 2 < argsize && arg[i + 1] == '?')
		switch (arg[i + 2])
		  {
		  case '!': case '\'':
		  case '(': case ')': case '-': case '/':
		  case '<': case '=': case '>':
		    /* Escape the second '?' in what would otherwise be
		       a trigraph.  */
		    c = arg[i + 2];
		    i += 2;
		    STORE ('?');
		    STORE ('\\');
		    STORE ('?');
		    break;

		  default:
		    break;
		  }
	      break;

	    default:
	      break;
	    }
	  break;

	case '\a': esc = 'a'; goto c_escape;
	case '\b': esc = 'b'; goto c_escape;
	case '\f': esc = 'f'; goto c_escape;
	case '\n': esc = 'n'; goto c_and_shell_escape;
	case '\r': esc = 'r'; goto c_and_shell_escape;
	case '\t': esc = 't'; goto c_and_shell_escape;
	case '\v': esc = 'v'; goto c_escape;
	case '\\': esc = c; goto c_and_shell_escape;

	c_and_shell_escape:
	  if (quoting_style == shell_quoting_style)
	    goto use_shell_always_quoting_style;
	c_escape:
	  if (backslash_escapes)
	    {
	      c = esc;
	      goto store_escape;
	    }
	  break;

	case '{': case '}': /* sometimes special if isolated */
	  if (! (argsize == SIZE_MAX ? arg[1] == '\0' : argsize == 1))
	    break;
	  /* Fall through.  */
	case '#': case '~':
	  if (i != 0)
	    break;
	  /* Fall through.  */
	case ' ':
	case '!': /* special in bash */
	case '"': case '$': case '&':
	case '(': case ')': case '*': case ';':
	case '<':
	case '=': /* sometimes special in 0th or (with "set -k") later args */
	case '>': case '[':
	case '^': /* special in old /bin/sh, e.g. SunOS 4.1.4 */
	case '`': case '|':
	  /* A shell special character.  In theory, '$' and '`' could
	     be the first bytes of multibyte characters, which means
	     we should check them with mbrtowc, but in practice this
	     doesn't happen so it's not worth worrying about.  */
	  if (quoting_style == shell_quoting_style)
	    goto use_shell_always_quoting_style;
	  break;

	case '\'':
	  switch (quoting_style)
	    {
	    case shell_quoting_style:
	      goto use_shell_always_quoting_style;

	    case shell_always_quoting_style:
	      STORE ('\'');
	      STORE ('\\');
	      STORE ('\'');
	      break;

	    default:
	      break;
	    }
	  break;

	case '%': case '+': case ',': case '-': case '.': case '/':
	case '0': case '1': case '2': case '3': case '4': case '5':
	case '6': case '7': case '8': case '9': case ':':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
	case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
	case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
	case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
	case 'Y': case 'Z': case ']': case '_': case 'a': case 'b':
	case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
	case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't':
	case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
	  /* These characters don't cause problems, no matter what the
	     quoting style is.  They cannot start multibyte sequences.  */
	  break;

	default:
	  /* If we have a multibyte sequence, copy it until we reach
	     its end, find an error, or come back to the initial shift
	     state.  For C-like styles, if the sequence has
	     unprintable characters, escape the whole sequence, since
	     we can't easily escape single characters within it.  */
	  {
	    /* Length of multibyte sequence found so far.  */
	    size_t m;

	    bool printable;

	    if (unibyte_locale)
	      {
		m = 1;
		printable = isprint (c) != 0;
	      }
	    else
	      {
		mbstate_t mbstate;
		memset (&mbstate, 0, sizeof mbstate);

		m = 0;
		printable = true;
		if (argsize == SIZE_MAX)
		  argsize = strlen (arg);

		do
		  {
		    wchar_t w;
		    size_t bytes = mbrtowc (&w, &arg[i + m],
					    argsize - (i + m), &mbstate);
		    if (bytes == 0)
		      break;
		    else if (bytes == (size_t) -1)
		      {
			printable = false;
			break;
		      }
		    else if (bytes == (size_t) -2)
		      {
			printable = false;
			while (i + m < argsize && arg[i + m])
			  m++;
			break;
		      }
		    else
		      {
			/* Work around a bug with older shells that "see" a '\'
			   that is really the 2nd byte of a multibyte character.
			   In practice the problem is limited to ASCII
			   chars >= '@' that are shell special chars.  */
			if ('[' == 0x5b && quoting_style == shell_quoting_style)
			  {
			    size_t j;
			    for (j = 1; j < bytes; j++)
			      switch (arg[i + m + j])
				{
				case '[': case '\\': case '^':
				case '`': case '|':
				  goto use_shell_always_quoting_style;

				default:
				  break;
				}
			  }

			if (! iswprint (w))
			  printable = false;
			m += bytes;
		      }
		  }
		while (! mbsinit (&mbstate));
	      }

	    if (1 < m || (backslash_escapes && ! printable))
	      {
		/* Output a multibyte sequence, or an escaped
		   unprintable unibyte character.  */
		size_t ilim = i + m;

		for (;;)
		  {
		    if (backslash_escapes && ! printable)
		      {
			STORE ('\\');
			STORE ('0' + (c >> 6));
			STORE ('0' + ((c >> 3) & 7));
			c = '0' + (c & 7);
		      }
		    if (ilim <= i + 1)
		      break;
		    STORE (c);
		    c = arg[++i];
		  }

		goto store_c;
	      }
	  }
	}

      if (! (backslash_escapes
	     && o->quote_these_too[c / INT_BITS] & (1 << (c % INT_BITS))))
	goto store_c;

    store_escape:
      STORE ('\\');

    store_c:
      STORE (c);
    }

  if (i == 0 && quoting_style == shell_quoting_style)
    goto use_shell_always_quoting_style;

  if (quote_string)
    for (; *quote_string; quote_string++)
      STORE (*quote_string);

  if (len < buffersize)
    buffer[len] = '\0';
  return len;

 use_shell_always_quoting_style:
  return quotearg_buffer_restyled (buffer, buffersize, arg, argsize,
				   shell_always_quoting_style, o);
}

/* Place into buffer BUFFER (of size BUFFERSIZE) a quoted version of
   argument ARG (of size ARGSIZE), using O to control quoting.
   If O is null, use the default.
   Terminate the output with a null character, and return the written
   size of the output, not counting the terminating null.
   If BUFFERSIZE is too small to store the output string, return the
   value that would have been returned had BUFFERSIZE been large enough.
   If ARGSIZE is SIZE_MAX, use the string length of the argument for
   ARGSIZE.  */
size_t
quotearg_buffer (char *buffer, size_t buffersize,
		 char const *arg, size_t argsize,
		 struct quoting_options const *o)
{
  struct quoting_options const *p = o ? o : &default_quoting_options;
  int e = errno;
  size_t r = quotearg_buffer_restyled (buffer, buffersize, arg, argsize,
				       p->style, p);
  errno = e;
  return r;
}

/* Like quotearg_buffer (..., ARG, ARGSIZE, O), except return newly
   allocated storage containing the quoted string.  */
char *
quotearg_alloc (char const *arg, size_t argsize,
		struct quoting_options const *o)
{
  int e = errno;
  size_t bufsize = quotearg_buffer (0, 0, arg, argsize, o) + 1;
  char *buf = xcharalloc (bufsize);
  quotearg_buffer (buf, bufsize, arg, argsize, o);
  errno = e;
  return buf;
}

/* A storage slot with size and pointer to a value.  */
struct slotvec
{
  size_t size;
  char *val;
};

/* Preallocate a slot 0 buffer, so that the caller can always quote
   one small component of a "memory exhausted" message in slot 0.  */
static char slot0[256];
static unsigned int nslots = 1;
static struct slotvec slotvec0 = {sizeof slot0, slot0};
static struct slotvec *slotvec = &slotvec0;

void
quotearg_free (void)
{
  struct slotvec *sv = slotvec;
  unsigned int i;
  for (i = 1; i < nslots; i++)
    free (sv[i].val);
  if (sv[0].val != slot0)
    {
      free (sv[0].val);
      slotvec0.size = sizeof slot0;
      slotvec0.val = slot0;
    }
  if (sv != &slotvec0)
    {
      free (sv);
      slotvec = &slotvec0;
    }
  nslots = 1;
}

/* Use storage slot N to return a quoted version of argument ARG.
   ARG is of size ARGSIZE, but if that is SIZE_MAX, ARG is a
   null-terminated string.
   OPTIONS specifies the quoting options.
   The returned value points to static storage that can be
   reused by the next call to this function with the same value of N.
   N must be nonnegative.  N is deliberately declared with type "int"
   to allow for future extensions (using negative values).  */
static char *
quotearg_n_options (int n, char const *arg, size_t argsize,
		    struct quoting_options const *options)
{
  int e = errno;

  unsigned int n0 = n;
  struct slotvec *sv = slotvec;

  if (n < 0)
    abort ();

  if (nslots <= n0)
    {
      /* FIXME: technically, the type of n1 should be `unsigned int',
	 but that evokes an unsuppressible warning from gcc-4.0.1 and
	 older.  If gcc ever provides an option to suppress that warning,
	 revert to the original type, so that the test in xalloc_oversized
	 is once again performed only at compile time.  */
      size_t n1 = n0 + 1;
      bool preallocated = (sv == &slotvec0);

      if (xalloc_oversized (n1, sizeof *sv))
	xalloc_die ();

      slotvec = sv = xrealloc (preallocated ? NULL : sv, n1 * sizeof *sv);
      if (preallocated)
	*sv = slotvec0;
      memset (sv + nslots, 0, (n1 - nslots) * sizeof *sv);
      nslots = n1;
    }

  {
    size_t size = sv[n].size;
    char *val = sv[n].val;
    size_t qsize = quotearg_buffer (val, size, arg, argsize, options);

    if (size <= qsize)
      {
	sv[n].size = size = qsize + 1;
	if (val != slot0)
	  free (val);
	sv[n].val = val = xcharalloc (size);
	quotearg_buffer (val, size, arg, argsize, options);
      }

    errno = e;
    return val;
  }
}

char *
quotearg_n (int n, char const *arg)
{
  return quotearg_n_options (n, arg, SIZE_MAX, &default_quoting_options);
}

char *
quotearg (char const *arg)
{
  return quotearg_n (0, arg);
}

/* Return quoting options for STYLE, with no extra quoting.  */
static struct quoting_options
quoting_options_from_style (enum quoting_style style)
{
  struct quoting_options o;
  o.style = style;
  memset (o.quote_these_too, 0, sizeof o.quote_these_too);
  return o;
}

char *
quotearg_n_style (int n, enum quoting_style s, char const *arg)
{
  struct quoting_options const o = quoting_options_from_style (s);
  return quotearg_n_options (n, arg, SIZE_MAX, &o);
}

char *
quotearg_n_style_mem (int n, enum quoting_style s,
		      char const *arg, size_t argsize)
{
  struct quoting_options const o = quoting_options_from_style (s);
  return quotearg_n_options (n, arg, argsize, &o);
}

char *
quotearg_style (enum quoting_style s, char const *arg)
{
  return quotearg_n_style (0, s, arg);
}

char *
quotearg_char (char const *arg, char ch)
{
  struct quoting_options options;
  options = default_quoting_options;
  set_char_quoting (&options, ch, 1);
  return quotearg_n_options (0, arg, SIZE_MAX, &options);
}

char *
quotearg_colon (char const *arg)
{
  return quotearg_char (arg, ':');
}
