/* Native-dependent code for GNU/Linux SPARC.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "regcache.h"
   
#include <sys/procfs.h>

/* Prototypes for supply_gregset etc.  */
#include "gregset.h"

void
supply_gregset (elf_gregset_t *gregsetp)
{
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  int i;

  for (i = G0_REGNUM; i <= I7_REGNUM; i++)
    supply_register (i, (char *) (regp + (i - G0_REGNUM)));

  supply_register (PS_REGNUM, (char *) (regp + 32));

  supply_register (PC_REGNUM, (char *) (regp + 33));
  supply_register (NPC_REGNUM, (char *) (regp + 34));
  supply_register (Y_REGNUM, (char *) (regp + 35));

  supply_register (WIM_REGNUM, (char *) (regp + 36));
  supply_register (TBR_REGNUM, (char *) (regp + 37));

  /* Fill inaccessible registers with zero.  */
  supply_register (CPS_REGNUM, NULL);
}

void
fill_gregset (elf_gregset_t *gregsetp, int regno)
{
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  int i;

  for (i = G0_REGNUM; i <= I7_REGNUM; i++)
    if (regno == -1 || regno == i)
      regcache_collect (i, regp + (i - G0_REGNUM));

  if (regno == -1 || regno == PS_REGNUM)
    regcache_collect (PS_REGNUM, regp + 32);

  if (regno == -1 || regno == PC_REGNUM)
    regcache_collect (PC_REGNUM, regp + 33);
  if (regno == -1 || regno == NPC_REGNUM)
    regcache_collect (NPC_REGNUM, regp + 34);
  if (regno == -1 || regno == Y_REGNUM)
    regcache_collect (Y_REGNUM, regp + 35);

  if (regno == -1 || regno == WIM_REGNUM)
    regcache_collect (WIM_REGNUM, regp + 36);
  if (regno == -1 || regno == TBR_REGNUM)
    regcache_collect (TBR_REGNUM, regp + 37);
}

void
supply_fpregset (elf_fpregset_t *fpregsetp)
{
  int i;

  for (i = FP0_REGNUM; i < FP0_REGNUM + 32; i++)
    supply_register (i, (char *) &fpregsetp->pr_fr.pr_regs[i - FP0_REGNUM]);

  supply_register (FPS_REGNUM, (char *) &fpregsetp->pr_fsr);
}

void
fill_fpregset (elf_fpregset_t *fpregsetp, int regno)
{
  int i;

  for (i = FP0_REGNUM; i < FP0_REGNUM + 32; i++)
    if (regno == -1 || regno == i)
      regcache_collect (i, &fpregsetp->pr_fr.pr_regs[i - FP0_REGNUM]);

  if (regno == -1 || regno == FPS_REGNUM)
    regcache_collect (FPS_REGNUM, &fpregsetp->pr_fsr);
}
