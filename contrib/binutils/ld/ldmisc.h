/* ldmisc.h -
   Copyright 1991, 1992, 1993, 1994, 1996, 1997, 2001
   Free Software Foundation, Inc.

   This file is part of GLD, the Gnu Linker.

   GLD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GLD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GLD; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

#ifndef LDMISC_H
#define LDMISC_H

extern void einfo PARAMS ((const char *, ...));
extern void minfo PARAMS ((const char *, ...));
extern void info_msg PARAMS ((const char *, ...));
extern void lfinfo PARAMS ((FILE *, const char *, ...));
extern void info_assert PARAMS ((const char *, unsigned int));
extern void yyerror PARAMS ((const char *));
extern PTR xmalloc PARAMS ((size_t));
extern PTR xrealloc PARAMS ((PTR, size_t));
extern void xexit PARAMS ((int));

#define ASSERT(x) \
do { if (!(x)) info_assert(__FILE__,__LINE__); } while (0)

#define FAIL() \
do { info_assert(__FILE__,__LINE__); } while (0)

extern void print_space PARAMS ((void));
extern void print_nl PARAMS ((void));
extern char *demangle PARAMS ((const char *));

#endif
