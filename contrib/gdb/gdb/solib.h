/* Shared library declarations for GDB, the GNU Debugger.
   Copyright (C) 1992, 1998 Free Software Foundation, Inc.

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

/* Function to be called to remove the connection between debugger and
   dynamic linker that was established by SOLIB_CREATE_INFERIOR_HOOK.
   (This operation does not remove shared library information from
   the debugger, as CLEAR_SOLIB does.)

   This functionality is presently not implemented for this target.
   */
#define SOLIB_REMOVE_INFERIOR_HOOK(PID) (0)

extern void
solib_create_inferior_hook PARAMS((void));	/* solib.c */

/* This function is called by the "catch load" command.  It allows
   the debugger to be notified by the dynamic linker when a specified
   library file (or any library file, if filename is NULL) is loaded.

   Presently, this functionality is not implemented.
   */
#define SOLIB_CREATE_CATCH_LOAD_HOOK(pid,tempflag,filename,cond_string) \
   error("catch of library loads/unloads not yet implemented on this platform")

/* This function is called by the "catch unload" command.  It allows
   the debugger to be notified by the dynamic linker when a specified
   library file (or any library file, if filename is NULL) is unloaded.

   Presently, this functionality is not implemented.
   */
#define SOLIB_CREATE_CATCH_UNLOAD_HOOK(pid,tempflag,filename,cond_string) \
   error("catch of library loads/unloads not yet implemented on this platform")

/* This function returns TRUE if the dynamic linker has just reported
   a load of a library.

   This function must be used only when the inferior has stopped in
   the dynamic linker hook, or undefined results are guaranteed.

   Presently, this functionality is not implemented.
   */

/*
#define SOLIB_HAVE_LOAD_EVENT(pid) \
    error("catch of library loads/unloads not yet implemented on this platform")
*/

#define SOLIB_HAVE_LOAD_EVENT(pid) \
(0)

/* This function returns a pointer to the string representation of the
   pathname of the dynamically-linked library that has just been loaded.

   This function must be used only when SOLIB_HAVE_LOAD_EVENT is TRUE,
   or undefined results are guaranteed.

   This string's contents are only valid immediately after the inferior
   has stopped in the dynamic linker hook, and becomes invalid as soon
   as the inferior is continued.  Clients should make a copy of this
   string if they wish to continue the inferior and then access the string.

   Presently, this functionality is not implemented.
   */

/*
#define SOLIB_LOADED_LIBRARY_PATHNAME(pid) \
   error("catch of library loads/unloads not yet implemented on this platform")
*/

#define SOLIB_LOADED_LIBRARY_PATHNAME(pid) \
(0)

/* This function returns TRUE if the dynamic linker has just reported
   an unload of a library.

   This function must be used only when the inferior has stopped in
   the dynamic linker hook, or undefined results are guaranteed.

   Presently, this functionality is not implemented.
   */
/*
#define SOLIB_HAVE_UNLOAD_EVENT(pid) \
   error("catch of library loads/unloads not yet implemented on this platform")
*/

#define SOLIB_HAVE_UNLOAD_EVENT(pid) \
(0)

/* This function returns a pointer to the string representation of the
   pathname of the dynamically-linked library that has just been unloaded.

   This function must be used only when SOLIB_HAVE_UNLOAD_EVENT is TRUE,
   or undefined results are guaranteed.

   This string's contents are only valid immediately after the inferior
   has stopped in the dynamic linker hook, and becomes invalid as soon
   as the inferior is continued.  Clients should make a copy of this
   string if they wish to continue the inferior and then access the string.

   Presently, this functionality is not implemented.
   */
/*
#define SOLIB_UNLOADED_LIBRARY_PATHNAME(pid) \
   error("catch of library loads/unloads not yet implemented on this platform")
*/

#define SOLIB_UNLOADED_LIBRARY_PATHNAME(pid) \
(0)

/* This function returns TRUE if pc is the address of an instruction that
   lies within the dynamic linker (such as the event hook, or the dld
   itself).

   This function must be used only when a dynamic linker event has been
   caught, and the inferior is being stepped out of the hook, or undefined
   results are guaranteed.

   Presently, this functionality is not implemented.
   */

/*
#define SOLIB_IN_DYNAMIC_LINKER(pid,pc) \
   error("catch of library loads/unloads not yet implemented on this platform")
*/

#define SOLIB_IN_DYNAMIC_LINKER(pid,pc) \
(0)

/* This function must be called when the inferior is killed, and the program
   restarted.  This is not the same as CLEAR_SOLIB, in that it doesn't discard
   any symbol tables.

   Presently, this functionality is not implemented.
   */
#define SOLIB_RESTART() \
  (0)

/* If we can't set a breakpoint, and it's in a shared library, just
   disable it.  */

#define DISABLE_UNSETTABLE_BREAK(addr)	(solib_address(addr) != NULL)

extern char *
solib_address PARAMS ((CORE_ADDR));		/* solib.c */

/* If ADDR lies in a shared library, return its name.  */

#define PC_SOLIB(addr)	solib_address (addr)

#ifdef SVR4_SHARED_LIBS

/* Return 1 if PC lies in the dynamic symbol resolution code of the
   SVR4 run time loader.  */

#define IN_SOLIB_DYNSYM_RESOLVE_CODE(pc) in_svr4_dynsym_resolve_code (pc)

extern int
in_svr4_dynsym_resolve_code PARAMS ((CORE_ADDR));

#endif
