/* HP SOM Shared library declarations for GDB, the GNU Debugger.
   Copyright (C) 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Written by the Center for Software Science at the Univerity of Utah
and by Cygnus Support.  */

#ifdef __STDC__		/* Forward decl's for prototypes */
struct target_ops;
struct objfile;
struct section_offsets;
#endif

/* Called to add symbols from a shared library to gdb's symbol table. */

#define SOLIB_ADD(filename, from_tty, targ) \
    som_solib_add (filename, from_tty, targ)

extern void
som_solib_add PARAMS ((char *, int, struct target_ops *));

extern CORE_ADDR
som_solib_get_got_by_pc PARAMS ((CORE_ADDR));

extern int
som_solib_section_offsets PARAMS ((struct objfile *, struct section_offsets *));

/* Function to be called when the inferior starts up, to discover the names
   of shared libraries that are dynamically linked, the base addresses to
   which they are linked, and sufficient information to read in their symbols
   at a later time. */

#define SOLIB_CREATE_INFERIOR_HOOK(PID)	som_solib_create_inferior_hook()

extern void
som_solib_create_inferior_hook PARAMS((void));
