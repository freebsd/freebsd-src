/* Copyright (C) 2004
   Free Software Foundation, Inc.
     Written by:  Jeff Conrad    (jeff_conrad@msn.com)
       and        Keith Marshall (keith.d.marshall@ntlworld.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

/* Define the default mechanism, and messages, for error reporting
 * (user may substitute a preferred alternative, by defining his own
 *  implementation of the macros REPORT_ERROR, QUOTE_ARG_MALLOC_FAILED
 *  and QUOTE_ARG_REALLOC_FAILED, in the header file `nonposix.h').
 */

#include "nonposix.h"

#ifndef  REPORT_ERROR
# define REPORT_ERROR(WHY)  fprintf(stderr, "%s:%s\n", program_name, WHY)
#endif
#ifndef  QUOTE_ARG_MALLOC_ERROR
# define QUOTE_ARG_MALLOC_ERROR   "malloc: Buffer allocation failed"
#endif
#ifndef  QUOTE_ARG_REALLOC_ERROR
# define QUOTE_ARG_REALLOC_ERROR  "realloc: Buffer resize failed"
#endif

extern char *program_name;	/* main program must define this */

#undef FALSE
#undef TRUE
#define FALSE 0
#define TRUE  1

static int
needs_quoting(const char *string)
{
  /* Scan `string' to see whether it needs quoting for MSVC `spawn'/`exec'
   * (i.e., whether it contains whitespace or embedded quotes).
   */

  if (string == NULL)		/* ignore NULL strings */
    return FALSE;

  if (*string == '\0')		/* explicit arguments of zero length	  */
    return TRUE;		/* need quoting, so they aren't discarded */
        
  while (*string) {
    /* Scan non-NULL strings, up to '\0' terminator,
     * returning 'TRUE' if quote or white space found.
     */

    if (*string == '"' || isspace(*string))
      return TRUE;

    /* otherwise, continue scanning to end of string */

    ++string;
  }

  /* Fall through, if no quotes or white space found,
   * in which case, return `FALSE'.
   */

  return FALSE;
}
      
char *
quote_arg(char *string)
{
  /* Enclose arguments in double quotes so that the parsing done in the
   * MSVC runtime startup code doesn't split them at whitespace.  Escape
   * embedded double quotes so that they emerge intact from the parsing.
   */

  int backslashes;
  char *quoted, *p, *q;

  if (needs_quoting(string)) {
    /* Need to create a quoted copy of `string';
     * maximum buffer space needed is twice the original length,
     * plus two enclosing quotes and one `\0' terminator.
     */
    
    if ((quoted = (char *)malloc(2 * strlen(string) + 3)) == NULL) {
      /* Couldn't get a buffer for the quoted string,
       * so complain, and bail out gracefully.
       */

      REPORT_ERROR(QUOTE_ARG_MALLOC_ERROR);
      exit(1);
    }

    /* Ok to proceed:
     * insert the opening quote, then copy the source string,
     * adding escapes as required.
     */

    *quoted = '"';
    for (backslashes = 0, p = string, q = quoted; *p; p++) {
      if (*p == '\\') {
	/* Just count backslashes when we find them.
	 * We will copy them out later, when we know if the count
	 * needs to be adjusted, to escape an embedded quote.
	 */
	
	++backslashes;
      }
      else if (*p == '"') {
	/* This embedded quote character must be escaped,
	 * but first double up any immediately preceding backslashes,
	 * with one extra, as the escape character.
	 */

	for (backslashes += backslashes + 1; backslashes; backslashes--)
	  *++q = '\\';

	/* and now, add the quote character itself */

	*++q = '"';
      }
      else {
	/* Any other character is simply copied,
	 * but first, if we have any pending backslashes,
	 * we must now insert them, without any count adjustment.
	 */

	while (backslashes) {
	  *++q = '\\';
	  --backslashes;
	}

	/* and then, copy the current character */

	*++q = *p;
      }
    }

    /* At end of argument:
     * If any backslashes remain to be copied out, append them now,
     * doubling the actual count to protect against reduction by MSVC,
     * as a consequence of the immediately following closing quote.
     */

    for (backslashes += backslashes; backslashes; backslashes--)
      *++q = '\\';

    /* Finally,
     * add the closing quote, terminate the quoted string,
     * and adjust its size to what was actually required,
     * ready for return.
     */

    *++q = '"';
    *++q = '\0';
    if ((string = (char *)realloc(quoted, strlen(quoted) + 1)) == NULL) {
      /* but bail out gracefully, on error */

      REPORT_ERROR(QUOTE_ARG_REALLOC_ERROR);
      exit(1);
    }
  }

  /* `string' now refers to the argument,
   * quoted and escaped, as required.
   */

  return string;
}

void
purge_quoted_args(char **argv)
{
  /* To avoid memory leaks,
   * free all memory previously allocated by `quoted_arg()',
   * within the scope of the referring argument vector, `argv'.
   */

  if (argv)
    while (*argv) {
      /* Any argument beginning with a double quote
       * SHOULD have been allocated by `quoted_arg()'.
       */
      
      if (**argv == '"')
        free( *argv );		/* so free its allocation */
      ++argv;			/* and continue to the next argument */
    }
}

/* quotearg.c: end of file */
