/* Macro definitions for GDB for a UltraSparc running GNU/Linux.

   Copyright 2001, 2002 Free Software Foundation, Inc.

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

#ifndef TM_SPARC_LIN64_H
#define TM_SPARC_LIN64_H

#define GDB_MULTI_ARCH 0

#include "sparc/tm-sp64.h"

#define SIGCONTEXT_PC_OFFSET 16  /* See asm-sparc64/sigcontext.h */

/* We always want full V9 + Ultra VIS stuff... */
#undef TM_PRINT_INSN_MACH
#define TM_PRINT_INSN_MACH bfd_mach_sparc_v9a

#define GDB_PTRACE_REGS64

#include "tm-sysv4.h"

#endif TM_SPARC_LIN64_H
