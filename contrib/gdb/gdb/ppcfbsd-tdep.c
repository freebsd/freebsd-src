/* Target-dependent code for PowerPC systems running FreeBSD.

   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.

   Contributed by Wasabi Systems, Inc.

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
#include "gdbcore.h"
#include "regcache.h"
#include "target.h"
#include "breakpoint.h"
#include "value.h"
#include "osabi.h"

#include "ppc-tdep.h"
#include "ppcfbsd-tdep.h"
#include "trad-frame.h"
#include "gdb_assert.h"
#include "solib-svr4.h"

#define REG_FIXREG_OFFSET(x)	((x) * sizeof(register_t))
#define REG_LR_OFFSET		(32 * sizeof(register_t))
#define REG_CR_OFFSET		(33 * sizeof(register_t))
#define REG_XER_OFFSET		(34 * sizeof(register_t))
#define REG_CTR_OFFSET		(35 * sizeof(register_t))
#define REG_PC_OFFSET		(36 * sizeof(register_t))
#define SIZEOF_STRUCT_REG	(37 * sizeof(register_t))

#define FPREG_FPR_OFFSET(x)	((x) * 8)
#define FPREG_FPSCR_OFFSET	(32 * 8)
#define SIZEOF_STRUCT_FPREG	(33 * 8)

void
ppcfbsd_supply_reg (char *regs, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;

  for (i = tdep->ppc_gp0_regnum; i <= tdep->ppc_gplast_regnum; i++)
    {
      if (regno == i || regno == -1)
	regcache_raw_supply (current_regcache, i, regs +
			     REG_FIXREG_OFFSET (i - tdep->ppc_gp0_regnum));
    }

  if (regno == tdep->ppc_lr_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_lr_regnum,
			 regs + REG_LR_OFFSET);

  if (regno == tdep->ppc_cr_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_cr_regnum,
			 regs + REG_CR_OFFSET);

  if (regno == tdep->ppc_xer_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_xer_regnum,
			 regs + REG_XER_OFFSET);

  if (regno == tdep->ppc_ctr_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_ctr_regnum,
			 regs + REG_CTR_OFFSET);

  if (regno == PC_REGNUM || regno == -1)
    regcache_raw_supply (current_regcache, PC_REGNUM,
			 regs + REG_PC_OFFSET);
}

void
ppcfbsd_fill_reg (char *regs, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;

  for (i = tdep->ppc_gp0_regnum; i <= tdep->ppc_gplast_regnum; i++)
    {
      if (regno == i || regno == -1)
	regcache_raw_collect (current_regcache, i, regs +
			      REG_FIXREG_OFFSET (i - tdep->ppc_gp0_regnum));
    }

  if (regno == tdep->ppc_lr_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_lr_regnum,
			  regs + REG_LR_OFFSET);

  if (regno == tdep->ppc_cr_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_cr_regnum,
			  regs + REG_CR_OFFSET);

  if (regno == tdep->ppc_xer_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_xer_regnum,
			  regs + REG_XER_OFFSET);

  if (regno == tdep->ppc_ctr_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_ctr_regnum,
			  regs + REG_CTR_OFFSET);

  if (regno == PC_REGNUM || regno == -1)
    regcache_raw_collect (current_regcache, PC_REGNUM, regs + REG_PC_OFFSET);
}

void
ppcfbsd_supply_fpreg (char *fpregs, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;

  /* FIXME: jimb/2004-05-05: Some PPC variants don't have floating
     point registers.  Traditionally, GDB's register set has still
     listed the floating point registers for such machines, so this
     code is harmless.  However, the new E500 port actually omits the
     floating point registers entirely from the register set --- they
     don't even have register numbers assigned to them.

     It's not clear to me how best to update this code, so this assert
     will alert the first person to encounter the NetBSD/E500
     combination to the problem.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  for (i = FP0_REGNUM; i <= FPLAST_REGNUM; i++)
    {
      if (regno == i || regno == -1)
	regcache_raw_supply (current_regcache, i, fpregs +
			     FPREG_FPR_OFFSET (i - FP0_REGNUM));
    }

  if (regno == tdep->ppc_fpscr_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_fpscr_regnum,
			 fpregs + FPREG_FPSCR_OFFSET);
}

void
ppcfbsd_fill_fpreg (char *fpregs, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;

  /* FIXME: jimb/2004-05-05: Some PPC variants don't have floating
     point registers.  Traditionally, GDB's register set has still
     listed the floating point registers for such machines, so this
     code is harmless.  However, the new E500 port actually omits the
     floating point registers entirely from the register set --- they
     don't even have register numbers assigned to them.

     It's not clear to me how best to update this code, so this assert
     will alert the first person to encounter the NetBSD/E500
     combination to the problem.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  for (i = FP0_REGNUM; i <= FPLAST_REGNUM; i++)
    {
      if (regno == i || regno == -1)
	regcache_raw_collect (current_regcache, i, fpregs +
			      FPREG_FPR_OFFSET (i - FP0_REGNUM));
    }

  if (regno == tdep->ppc_fpscr_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_fpscr_regnum,
			  fpregs + FPREG_FPSCR_OFFSET);
}

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size, int which,
                      CORE_ADDR ignore)
{
  char *regs, *fpregs;

  /* We get everything from one section.  */
  if (which != 0)
    return;

  regs = core_reg_sect;
  fpregs = core_reg_sect + SIZEOF_STRUCT_REG;

  /* Integer registers.  */
  ppcfbsd_supply_reg (regs, -1);

  /* Floating point registers.  */
  ppcfbsd_supply_fpreg (fpregs, -1);
}

static void
fetch_elfcore_registers (char *core_reg_sect, unsigned core_reg_size, int which,
                         CORE_ADDR ignore)
{
  switch (which)
    {
    case 0:  /* Integer registers.  */
      if (core_reg_size != SIZEOF_STRUCT_REG)
	warning (_("Wrong size register set in core file."));
      else
	ppcfbsd_supply_reg (core_reg_sect, -1);
      break;

    case 2:  /* Floating point registers.  */
      if (core_reg_size != SIZEOF_STRUCT_FPREG)
	warning (_("Wrong size FP register set in core file."));
      else
	ppcfbsd_supply_fpreg (core_reg_sect, -1);
      break;

    default:
      /* Don't know what kind of register request this is; just ignore it.  */
      break;
    }
}

static struct core_fns ppcfbsd_core_fns =
{
  bfd_target_unknown_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

static struct core_fns ppcfbsd_elfcore_fns =
{
  bfd_target_elf_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_elfcore_registers,		/* core_read_registers */
  NULL					/* next */
};

static int
ppcfbsd_pc_in_sigtramp (CORE_ADDR pc, char *func_name)
{
  return (pc >= 0x7fffef00U) ? 1 : 0;
}

/* NetBSD is confused.  It appears that 1.5 was using the correct SVr4
   convention but, 1.6 switched to the below broken convention.  For
   the moment use the broken convention.  Ulgh!.  */

static enum return_value_convention
ppcfbsd_return_value (struct gdbarch *gdbarch, struct type *valtype,
		      struct regcache *regcache, void *readbuf,
		      const void *writebuf)
{
  if ((TYPE_CODE (valtype) == TYPE_CODE_STRUCT
       || TYPE_CODE (valtype) == TYPE_CODE_UNION)
      && !((TYPE_LENGTH (valtype) == 16 || TYPE_LENGTH (valtype) == 8))
      && !(TYPE_LENGTH (valtype) == 1
	   || TYPE_LENGTH (valtype) == 2
	   || TYPE_LENGTH (valtype) == 4
	   || TYPE_LENGTH (valtype) == 8)) 
    return RETURN_VALUE_STRUCT_CONVENTION; 
  else 
    return ppc_sysv_abi_broken_return_value (gdbarch, valtype, regcache,
					     readbuf, writebuf); 
}

static void
ppcfbsd_init_abi (struct gdbarch_info info,
                  struct gdbarch *gdbarch)
{
  set_gdbarch_pc_in_sigtramp (gdbarch, ppcfbsd_pc_in_sigtramp);
  /* For NetBSD, this is an on again, off again thing.  Some systems
     do use the broken struct convention, and some don't.  */
  set_gdbarch_return_value (gdbarch, ppcfbsd_return_value);
#ifdef __powerpc64__
  set_solib_svr4_fetch_link_map_offsets (gdbarch,
                                svr4_lp64_fetch_link_map_offsets);
#else
  set_solib_svr4_fetch_link_map_offsets (gdbarch,
                                svr4_ilp32_fetch_link_map_offsets);
#endif
}

void
_initialize_ppcfbsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_rs6000, 0, GDB_OSABI_FREEBSD_ELF,
			  ppcfbsd_init_abi);
  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_FREEBSD_ELF,
			  ppcfbsd_init_abi);

  add_core_fns (&ppcfbsd_core_fns);
  add_core_fns (&ppcfbsd_elfcore_fns);
}
