/* Storage allocation interface for bison,
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define	NEW(t)		((t *) xmalloc((unsigned) sizeof(t)))
#define	NEW2(n, t)	((t *) xmalloc((unsigned) ((n) * sizeof(t))))

#ifdef __STDC__
#define	FREE(x)		(x ? (void) free((char *) (x)) : (void)0)
#else
#define FREE(x) 	((x) != 0 && (free ((char *) (x)), 0))
#endif

extern char *xmalloc  PARAMS((register unsigned));
extern char *xrealloc PARAMS((register char *, register unsigned));
