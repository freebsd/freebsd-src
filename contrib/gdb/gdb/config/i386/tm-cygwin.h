/* Macro definitions for i386 running under the win32 API Unix.
   Copyright 1995, 1996, 1997, 1998, 1999, 2000
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Use SSE registers if winnt.h contains information about them.  */
#ifdef HAVE_CONTEXT_EXTENDED_REGISTERS
#define HAVE_SSE_REGS
#else
#undef HAVE_SSE_REGS
#endif /* CONTEXT_EXTENDED_REGISTERS */
#define HAVE_I387_REGS

#include "i386/tm-i386.h"

#if 0
#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) skip_trampoline_code (pc, name)
#define SKIP_TRAMPOLINE_CODE(pc)           skip_trampoline_code (pc, 0)
extern CORE_ADDR skip_trampoline_code (CORE_ADDR pc, char *name);
#endif

#define ATTACH_NO_WAIT
#define SOLIB_ADD(filename, from_tty, targ, readsyms) child_solib_add(filename, from_tty, targ, readsyms)
#define SOLIB_LOADED_LIBRARY_PATHNAME(pid) child_solib_loaded_library_pathname(pid)
#define CLEAR_SOLIB child_clear_solibs
#define ADD_SHARED_SYMBOL_FILES dll_symbol_command

struct target_ops;
char *cygwin_pid_to_str (ptid_t ptid);
void child_solib_add (char *, int, struct target_ops *, int);
char *child_solib_loaded_library_pathname(int);
void child_clear_solibs (void);
void dll_symbol_command (char *, int);
