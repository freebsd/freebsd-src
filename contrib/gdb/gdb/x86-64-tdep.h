/* Target-dependent code for GDB, the GNU debugger.
   Copyright 2001
   Free Software Foundation, Inc.
   Contributed by Jiri Smid, SuSE Labs.

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

#ifndef X86_64_TDEP_H
#define X86_64_TDEP_H

#include "i386-tdep.h"

extern int x86_64_num_regs;
extern int x86_64_num_gregs;

int x86_64_register_number (const char *name);
char *x86_64_register_name (int reg_nr);
	

gdbarch_frame_saved_pc_ftype x86_64_linux_frame_saved_pc;
gdbarch_saved_pc_after_call_ftype x86_64_linux_saved_pc_after_call;

#endif
