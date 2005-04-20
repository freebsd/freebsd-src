/* Function to parse an `unsigned long long int' from text.
   Copyright (C) 1995, 1996, 1997, 1999 Free Software Foundation, Inc.
   NOTE: The canonical source of this file is maintained with the GNU C
   Library.  Bugs can be reported to bug-glibc@gnu.org.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define QUAD 1

#include "strtoul.c"

#ifdef _LIBC
strong_alias (__strtoull_internal, __strtouq_internal)
weak_alias (strtoull, strtouq)
#endif
