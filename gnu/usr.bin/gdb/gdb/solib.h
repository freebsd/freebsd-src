/* Shared library declarations for GDB, the GNU Debugger.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef __STDC__		/* Forward decl's for prototypes */
struct target_ops;
#endif

/* Called when we free all symtabs, to free the shared library information
   as well. */

#define CLEAR_SOLIB			clear_solib

extern void
clear_solib PARAMS ((void));

/* Called to add symbols from a shared library to gdb's symbol table. */

#define SOLIB_ADD(filename, from_tty, targ) \
    solib_add (filename, from_tty, targ)

extern void
solib_add PARAMS ((char *, int, struct target_ops *));

/* Function to be called when the inferior starts up, to discover the names
   of shared libraries that are dynamically linked, the base addresses to
   which they are linked, and sufficient information to read in their symbols
   at a later time. */

#define SOLIB_CREATE_INFERIOR_HOOK(PID)	solib_create_inferior_hook()

extern void
solib_create_inferior_hook PARAMS((void));	/* solib.c */

/* If we can't set a breakpoint, and it's in a shared library, just
   disable it.  */

#define DISABLE_UNSETTABLE_BREAK(addr)	solib_address(addr)

extern int
solib_address PARAMS ((CORE_ADDR));		/* solib.c */
