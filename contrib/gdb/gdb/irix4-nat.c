/* Native support for the SGI Iris running IRIX version 4, for GDB.
   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1995
   Free Software Foundation, Inc.
   Contributed by Alessandro Forin(af@cs.cmu.edu) at CMU
   and by Per Bothner(bothner@cs.wisc.edu) at U.Wisconsin.
   Implemented for Irix 4.x by Garrett A. Wollman.

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

#include "defs.h"
#include "inferior.h"
#include "gdbcore.h"

#include <sys/time.h>
#include <sys/procfs.h>
#include <setjmp.h>		/* For JB_XXX.  */

/* Size of elements in jmpbuf */

#define JB_ELEMENT_SIZE 4

typedef unsigned int greg_t;	/* why isn't this defined? */

/*
 * See the comment in m68k-tdep.c regarding the utility of these functions.
 */

void 
supply_gregset (gregsetp)
     gregset_t *gregsetp;
{
  register int regi;
  register greg_t *regp = (greg_t *)(gregsetp->gp_regs);
  static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};

  /* FIXME: somewhere, there should be a #define for the meaning
     of this magic number 32; we should use that. */
  for(regi = 0; regi < 32; regi++)
    supply_register (regi, (char *)(regp + regi));

  supply_register (PC_REGNUM, (char *)&(gregsetp->gp_pc));
  supply_register (HI_REGNUM, (char *)&(gregsetp->gp_mdhi));
  supply_register (LO_REGNUM, (char *)&(gregsetp->gp_mdlo));
  supply_register (CAUSE_REGNUM, (char *)&(gregsetp->gp_cause));

  /* Fill inaccessible registers with zero.  */
  supply_register (BADVADDR_REGNUM, zerobuf);
}

void
fill_gregset (gregsetp, regno)
     gregset_t *gregsetp;
     int regno;
{
  int regi;
  register greg_t *regp = (greg_t *)(gregsetp->gp_regs);

  /* same FIXME as above wrt 32*/
  for (regi = 0; regi < 32; regi++)
    if ((regno == -1) || (regno == regi))
      *(regp + regi) = *(greg_t *) &registers[REGISTER_BYTE (regi)];

  if ((regno == -1) || (regno == PC_REGNUM))
    gregsetp->gp_pc = *(greg_t *) &registers[REGISTER_BYTE (PC_REGNUM)];

  if ((regno == -1) || (regno == CAUSE_REGNUM))
    gregsetp->gp_cause = *(greg_t *) &registers[REGISTER_BYTE (CAUSE_REGNUM)];

  if ((regno == -1) || (regno == HI_REGNUM))
    gregsetp->gp_mdhi = *(greg_t *) &registers[REGISTER_BYTE (HI_REGNUM)];

  if ((regno == -1) || (regno == LO_REGNUM))
    gregsetp->gp_mdlo = *(greg_t *) &registers[REGISTER_BYTE (LO_REGNUM)];
}

/*
 * Now we do the same thing for floating-point registers.
 * We don't bother to condition on FP0_REGNUM since any
 * reasonable MIPS configuration has an R3010 in it.
 *
 * Again, see the comments in m68k-tdep.c.
 */

void
supply_fpregset (fpregsetp)
     fpregset_t *fpregsetp;
{
  register int regi;
  static char zerobuf[MAX_REGISTER_RAW_SIZE] = {0};

  for (regi = 0; regi < 32; regi++)
    supply_register (FP0_REGNUM + regi,
		     (char *)&fpregsetp->fp_r.fp_regs[regi]);

  supply_register (FCRCS_REGNUM, (char *)&fpregsetp->fp_csr);

  /* FIXME: how can we supply FCRIR_REGNUM?  SGI doesn't tell us. */
  supply_register (FCRIR_REGNUM, zerobuf);
}

void
fill_fpregset (fpregsetp, regno)
     fpregset_t *fpregsetp;
     int regno;
{
  int regi;
  char *from, *to;

  for (regi = FP0_REGNUM; regi < FP0_REGNUM + 32; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &registers[REGISTER_BYTE (regi)];
	  to = (char *) &(fpregsetp->fp_r.fp_regs[regi - FP0_REGNUM]);
	  memcpy(to, from, REGISTER_RAW_SIZE (regi));
	}
    }

  if ((regno == -1) || (regno == FCRCS_REGNUM))
    fpregsetp->fp_csr = *(unsigned *) &registers[REGISTER_BYTE(FCRCS_REGNUM)];
}


/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target (pc)
     CORE_ADDR *pc;
{
  char buf[TARGET_PTR_BIT / TARGET_CHAR_BIT];
  CORE_ADDR jb_addr;

  jb_addr = read_register (A0_REGNUM);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;			/* Unused */
     unsigned int reg_addr;	/* Unused */
{
  if (core_reg_size != REGISTER_BYTES)
    {
      warning ("wrong size gregset struct in core file");
      return;
    }

  memcpy ((char *)registers, core_reg_sect, core_reg_size);
}


/* Register that we are able to handle irix4 core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns irix4_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_irix4 ()
{
  add_core_fns (&irix4_core_fns);
}
