/* Macro definitions for GDB on all SVR4 target systems.
   Copyright 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support (fnf@cygnus.com).

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

/* For SVR4 shared libraries, each call to a library routine goes through
   a small piece of trampoline code in the ".plt" section.
   The horribly ugly wait_for_inferior() routine uses this macro to detect
   when we have stepped into one of these fragments.
   We do not use lookup_solib_trampoline_symbol_by_pc, because
   we cannot always find the shared library trampoline symbols
   (e.g. on Irix5).  */

#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) in_plt_section((pc), (name))
extern int in_plt_section PARAMS ((CORE_ADDR, char *));

/* If PC is in a shared library trampoline code, return the PC
   where the function itself actually starts.  If not, return 0.  */

#define SKIP_TRAMPOLINE_CODE(pc)  find_solib_trampoline_target (pc)

/* It is unknown which, if any, SVR4 assemblers do not accept dollar signs
   in identifiers.  The default in G++ is to use dots instead, for all SVR4
   systems, so we make that our default also.  FIXME: There should be some
   way to get G++ to tell us what CPLUS_MARKER it is using, perhaps by
   stashing it in the debugging information as part of the name of an
   invented symbol ("gcc_cplus_marker$" for example). */

#undef CPLUS_MARKER
#define CPLUS_MARKER '.'
