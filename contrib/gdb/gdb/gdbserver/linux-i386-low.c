/* GNU/Linux/i386 specific low level interface, for the remote server for GDB.
   Copyright 1995, 1996, 1998, 1999, 2000, 2001, 2002
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

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

/* This module only supports access to the general purpose registers.  */

int num_regs = 16;

/* This stuff comes from i386-linux-nat.c.  */

/* Mapping between the general-purpose registers in `struct user'
   format and GDB's register array layout.  */
int regmap[] = 
{
  EAX * 4, ECX * 4, EDX * 4, EBX * 4,
  UESP * 4, EBP * 4, ESI * 4, EDI * 4,
  EIP * 4, EFL * 4, CS * 4, SS * 4,
  DS * 4, ES * 4, FS * 4, GS * 4
};

int
cannot_store_register (int regno)
{
  return (regno >= num_regs);
}

int
cannot_fetch_register (int regno)
{
  return (regno >= num_regs);
}


#ifdef HAVE_LINUX_REGSETS
#include <sys/procfs.h>
#include <sys/ptrace.h>

static void
i386_fill_gregset (void *buf)
{
  int i;

  for (i = 0; i < num_regs; i++)
    collect_register (i, ((char *) buf) + regmap[i]);

  collect_register_by_name ("orig_eax", ((char *) buf) + ORIG_EAX * 4);
}

static void
i386_store_gregset (void *buf)
{
  int i;

  for (i = 0; i < num_regs; i++)
    supply_register (i, ((char *) buf) + regmap[i]);

  supply_register_by_name ("orig_eax", ((char *) buf) + ORIG_EAX * 4);
}

static void
i386_fill_fpregset (void *buf)
{
  i387_cache_to_fsave (buf);
}

static void
i386_store_fpregset (void *buf)
{
  i387_fsave_to_cache (buf);
}

static void
i386_fill_fpxregset (void *buf)
{
  i387_cache_to_fxsave (buf);
}

static void
i386_store_fpxregset (void *buf)
{
  i387_fxsave_to_cache (buf);
}


struct regset_info target_regsets[] = {
  { PTRACE_GETREGS, PTRACE_SETREGS, sizeof (elf_gregset_t),
    i386_fill_gregset, i386_store_gregset },
#ifdef HAVE_PTRACE_GETFPXREGS
  { PTRACE_GETFPXREGS, PTRACE_SETFPXREGS, sizeof (elf_fpxregset_t),
    i386_fill_fpxregset, i386_store_fpxregset },
#endif
  { PTRACE_GETFPREGS, PTRACE_SETFPREGS, sizeof (elf_fpregset_t),
    i386_fill_fpregset, i386_store_fpregset },
  { 0, 0, -1, NULL, NULL }
};

#endif /* HAVE_LINUX_REGSETS */

