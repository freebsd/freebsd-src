/* Allocate and clear storage for bison,
   Copyright (C) 1984, 1989 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


#include <stdio.h>
#include "system.h"

#ifdef NEED_DECLARATION_CALLOC
#if defined (__STDC__) || defined (_MSC_VER)
extern void *calloc ();
#else
extern char *calloc ();
#endif
#endif  /* NEED_DECLARATION_CALLOC */

#ifdef NEED_DECLARATION_REALLOC
#if defined (__STDC__) || defined (_MSC_VER)
extern void *realloc ();
#else
extern char *realloc ();
#endif
#endif  /* NEED_DECLARATION_REALLOC */

char *xmalloc PARAMS((register unsigned));
char *xrealloc PARAMS((register char *, register unsigned));

extern void done PARAMS((int));

extern char *program_name;

char *
xmalloc (register unsigned n)
{
  register char *block;

  /* Avoid uncertainty about what an arg of 0 will do.  */
  if (n == 0)
    n = 1;
  block = calloc (n, 1);
  if (block == NULL)
    {
      fprintf (stderr, _("%s: memory exhausted\n"), program_name);
      done (1);
    }

  return (block);
}

char *
xrealloc (register char *block, register unsigned n)
{
  /* Avoid uncertainty about what an arg of 0 will do.  */
  if (n == 0)
    n = 1;
  block = realloc (block, n);
  if (block == NULL)
    {
      fprintf (stderr, _("%s: memory exhausted\n"), program_name);
      done (1);
    }

  return (block);
}
