/* GNU/Linux/x86-64 specific low level interface, for the remote server
   for GDB.
   Copyright 2002
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

#include "server.h"
#include "linux-low.h"
#include "i387-fp.h"

#include <sys/reg.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>

static int regmap[] = {
  RAX, RDX, RCX, RBX,
  RSI, RDI, RBP, RSP,
  R8, R9, R10, R11,
  R12, R13, R14, R15,
  RIP, EFLAGS
};

static void
x86_64_fill_gregset (void *buf)
{
  int i;

  for (i = 0; i < 18; i++)
    collect_register (i, ((char *) buf) + regmap[i]);
}

static void
x86_64_store_gregset (void *buf)
{
  int i;

  for (i = 0; i < 18; i++)
    supply_register (i, ((char *) buf) + regmap[i]);
}

static void
x86_64_fill_fpregset (void *buf)
{
  i387_cache_to_fxsave (buf);
}

static void
x86_64_store_fpregset (void *buf)
{
  i387_fxsave_to_cache (buf);
}


struct regset_info target_regsets[] = {
  { PTRACE_GETREGS, PTRACE_SETREGS, sizeof (elf_gregset_t),
    x86_64_fill_gregset, x86_64_store_gregset },
  { PTRACE_GETFPREGS, PTRACE_SETFPREGS, sizeof (elf_fpregset_t),
    x86_64_fill_fpregset, x86_64_store_fpregset },
  { 0, 0, -1, NULL, NULL }
};
