/* Native support for i386 running Solaris 2.
   Copyright 1998 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "nm-sysv4.h"

#ifdef HAVE_THREAD_DB_LIB

#ifdef __STDC__
struct objfile;
#endif

#define target_new_objfile(OBJFILE) sol_thread_new_objfile (OBJFILE)

void sol_thread_new_objfile PARAMS ((struct objfile *objfile));

#define FIND_NEW_THREADS sol_find_new_threads
void sol_find_new_threads PARAMS ((void));

#endif
