/* sysdep.h -- handle host dependencies for the GNU linker
   Copyright 1995, 1996, 1997, 1999 Free Software Foundation, Inc.

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
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef LD_SYSDEP_H
#define LD_SYSDEP_H

#include "ansidecl.h"

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#else
extern char *strchr ();
extern char *strrchr ();
#endif
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef USE_BINARY_FOPEN
#include "fopen-bin.h"
#else
#include "fopen-same.h"
#endif

#ifdef NEED_DECLARATION_STRSTR
extern char *strstr ();
#endif

#ifdef NEED_DECLARATION_FREE
extern void free ();
#endif

#ifdef NEED_DECLARATION_GETENV
extern char *getenv ();
#endif

#ifdef NEED_DECLARATION_ENVIRON
extern char **environ;
#endif

#endif /* ! defined (LD_SYSDEP_H) */
