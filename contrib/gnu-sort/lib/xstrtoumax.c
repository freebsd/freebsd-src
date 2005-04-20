/* xstrtoumax.c -- A more useful interface to strtoumax.
   Copyright (C) 1999, 2003, 2004 Free Software Foundation, Inc.

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
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "xstrtol.h"

#define __strtol strtoumax
#define __strtol_t uintmax_t
#define __xstrtol xstrtoumax
#ifdef UINTMAX_MAX
# define STRTOL_T_MINIMUM 0
# define STRTOL_T_MAXIMUM UINTMAX_MAX
#endif
#include "xstrtol.c"
