/* COFF (SVR3) Shared library declarations for GDB, the GNU Debugger.
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef __STDC__		/* Forward decl's for prototypes */
struct target_ops;
#endif

/* Called when we free all symtabs, to free the shared library information
   as well. */

#if 0
#define CLEAR_SOLIB			coff_clear_solib

extern void
coff_clear_solib PARAMS ((void));
#endif

/* Called to add symbols from a shared library to gdb's symbol table. */

#define SOLIB_ADD(filename, from_tty, targ) \
    coff_solib_add (filename, from_tty, targ)

extern void
coff_solib_add PARAMS ((char *, int, struct target_ops *));

/* Function to be called when the inferior starts up, to discover the names
   of shared libraries that are dynamically linked, the base addresses to
   which they are linked, and sufficient information to read in their symbols
   at a later time. */

#define SOLIB_CREATE_INFERIOR_HOOK(PID)	coff_solib_create_inferior_hook()

extern void
coff_solib_create_inferior_hook PARAMS((void));	/* solib.c */

/* If we can't set a breakpoint, and it's in a shared library, just
   disable it.  */

#if 0
#define DISABLE_UNSETTABLE_BREAK(addr)	coff_solib_address(addr)

extern int
solib_address PARAMS ((CORE_ADDR));		/* solib.c */
#endif
