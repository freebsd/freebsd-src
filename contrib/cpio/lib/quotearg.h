/* quotearg.h - quote arguments for output

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2004, 2006 Free
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

#ifndef QUOTEARG_H_
# define QUOTEARG_H_ 1

# include <stddef.h>

/* Basic quoting styles.  */
enum quoting_style
  {
    /* Output names as-is (ls --quoting-style=literal).  */
    literal_quoting_style,

    /* Quote names for the shell if they contain shell metacharacters
       or would cause ambiguous output (ls --quoting-style=shell).  */
    shell_quoting_style,

    /* Quote names for the shell, even if they would normally not
       require quoting (ls --quoting-style=shell-always).  */
    shell_always_quoting_style,

    /* Quote names as for a C language string (ls --quoting-style=c).  */
    c_quoting_style,

    /* Like c_quoting_style except omit the surrounding double-quote
       characters (ls --quoting-style=escape).  */
    escape_quoting_style,

    /* Like clocale_quoting_style, but quote `like this' instead of
       "like this" in the default C locale (ls --quoting-style=locale).  */
    locale_quoting_style,

    /* Like c_quoting_style except use quotation marks appropriate for
       the locale (ls --quoting-style=clocale).  */
    clocale_quoting_style
  };

/* For now, --quoting-style=literal is the default, but this may change.  */
# ifndef DEFAULT_QUOTING_STYLE
#  define DEFAULT_QUOTING_STYLE literal_quoting_style
# endif

/* Names of quoting styles and their corresponding values.  */
extern char const *const quoting_style_args[];
extern enum quoting_style const quoting_style_vals[];

struct quoting_options;

/* The functions listed below set and use a hidden variable
   that contains the default quoting style options.  */

/* Allocate a new set of quoting options, with contents initially identical
   to O if O is not null, or to the default if O is null.
   It is the caller's responsibility to free the result.  */
struct quoting_options *clone_quoting_options (struct quoting_options *o);

/* Get the value of O's quoting style.  If O is null, use the default.  */
enum quoting_style get_quoting_style (struct quoting_options *o);

/* In O (or in the default if O is null),
   set the value of the quoting style to S.  */
void set_quoting_style (struct quoting_options *o, enum quoting_style s);

/* In O (or in the default if O is null),
   set the value of the quoting options for character C to I.
   Return the old value.  Currently, the only values defined for I are
   0 (the default) and 1 (which means to quote the character even if
   it would not otherwise be quoted).  */
int set_char_quoting (struct quoting_options *o, char c, int i);

/* Place into buffer BUFFER (of size BUFFERSIZE) a quoted version of
   argument ARG (of size ARGSIZE), using O to control quoting.
   If O is null, use the default.
   Terminate the output with a null character, and return the written
   size of the output, not counting the terminating null.
   If BUFFERSIZE is too small to store the output string, return the
   value that would have been returned had BUFFERSIZE been large enough.
   If ARGSIZE is -1, use the string length of the argument for ARGSIZE.  */
size_t quotearg_buffer (char *buffer, size_t buffersize,
			char const *arg, size_t argsize,
			struct quoting_options const *o);

/* Like quotearg_buffer, except return the result in a newly allocated
   buffer.  It is the caller's responsibility to free the result.  */
char *quotearg_alloc (char const *arg, size_t argsize,
		      struct quoting_options const *o);

/* Use storage slot N to return a quoted version of the string ARG.
   Use the default quoting options.
   The returned value points to static storage that can be
   reused by the next call to this function with the same value of N.
   N must be nonnegative.  */
char *quotearg_n (int n, char const *arg);

/* Equivalent to quotearg_n (0, ARG).  */
char *quotearg (char const *arg);

/* Use style S and storage slot N to return a quoted version of the string ARG.
   This is like quotearg_n (N, ARG), except that it uses S with no other
   options to specify the quoting method.  */
char *quotearg_n_style (int n, enum quoting_style s, char const *arg);

/* Use style S and storage slot N to return a quoted version of the
   argument ARG of size ARGSIZE.  This is like quotearg_n_style
   (N, S, ARG), except it can quote null bytes.  */
char *quotearg_n_style_mem (int n, enum quoting_style s,
			    char const *arg, size_t argsize);

/* Equivalent to quotearg_n_style (0, S, ARG).  */
char *quotearg_style (enum quoting_style s, char const *arg);

/* Like quotearg (ARG), except also quote any instances of CH.  */
char *quotearg_char (char const *arg, char ch);

/* Equivalent to quotearg_char (ARG, ':').  */
char *quotearg_colon (char const *arg);

/* Free any dynamically allocated memory.  */
void quotearg_free (void);

#endif /* !QUOTEARG_H_ */
