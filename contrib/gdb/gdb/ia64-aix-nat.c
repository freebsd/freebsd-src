/* Functions specific to running gdb native on IA-64 running AIX.
   Copyright 2000, 2001 Free Software Foundation, Inc.

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
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"
#include "regcache.h"
#include <sys/procfs.h>

#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"

#include <sys/types.h>
#include <fcntl.h>
#include "gdb_stat.h"

void
supply_gregset (prgregset_t *gregsetp)
{
  int regi;

  for (regi = IA64_GR0_REGNUM; regi <= IA64_GR31_REGNUM; regi++)
    {
      supply_register (regi, 
                       (char *) &(gregsetp->__gpr[regi - IA64_GR0_REGNUM]));
    }

  for (regi = IA64_BR0_REGNUM; regi <= IA64_BR7_REGNUM; regi++)
    {
      supply_register (regi, 
                       (char *) &(gregsetp->__br[regi - IA64_BR0_REGNUM]));
    }

  supply_register (IA64_PSR_REGNUM, (char *) &(gregsetp->__psr));
  supply_register (IA64_IP_REGNUM, (char *) &(gregsetp->__ip));
  supply_register (IA64_CFM_REGNUM, (char *) &(gregsetp->__ifs));
  supply_register (IA64_RSC_REGNUM, (char *) &(gregsetp->__rsc));
  supply_register (IA64_BSP_REGNUM, (char *) &(gregsetp->__bsp));
  supply_register (IA64_BSPSTORE_REGNUM, (char *) &(gregsetp->__bspstore));
  supply_register (IA64_RNAT_REGNUM, (char *) &(gregsetp->__rnat));
  supply_register (IA64_PFS_REGNUM, (char *) &(gregsetp->__pfs));
  supply_register (IA64_UNAT_REGNUM, (char *) &(gregsetp->__unat));
  supply_register (IA64_PR_REGNUM, (char *) &(gregsetp->__preds));
  supply_register (IA64_CCV_REGNUM, (char *) &(gregsetp->__ccv));
  supply_register (IA64_LC_REGNUM, (char *) &(gregsetp->__lc));
  supply_register (IA64_EC_REGNUM, (char *) &(gregsetp->__ec));
  /* FIXME: __nats */
  supply_register (IA64_FPSR_REGNUM, (char *) &(gregsetp->__fpsr));

  /* These (for the most part) are pseudo registers and are obtained
     by other means.  Those that aren't are already handled by the
     code above.  */
  for (regi = IA64_GR32_REGNUM; regi <= IA64_GR127_REGNUM; regi++)
    register_valid[regi] = 1;
  for (regi = IA64_PR0_REGNUM; regi <= IA64_PR63_REGNUM; regi++)
    register_valid[regi] = 1;
  for (regi = IA64_VFP_REGNUM; regi <= NUM_REGS; regi++)
    register_valid[regi] = 1;
}

void
fill_gregset (prgregset_t *gregsetp, int regno)
{
  int regi;

#define COPY_REG(_fld_,_regi_) \
  if ((regno == -1) || regno == _regi_) \
    memcpy (&(gregsetp->_fld_), &registers[REGISTER_BYTE (_regi_)], \
	    REGISTER_RAW_SIZE (_regi_))

  for (regi = IA64_GR0_REGNUM; regi <= IA64_GR31_REGNUM; regi++)
    {
      COPY_REG (__gpr[regi - IA64_GR0_REGNUM], regi);
    }

  for (regi = IA64_BR0_REGNUM; regi <= IA64_BR7_REGNUM; regi++)
    {
      COPY_REG (__br[regi - IA64_BR0_REGNUM], regi);
    }
  COPY_REG (__psr, IA64_PSR_REGNUM);
  COPY_REG (__ip, IA64_IP_REGNUM);
  COPY_REG (__ifs, IA64_CFM_REGNUM);
  COPY_REG (__rsc, IA64_RSC_REGNUM);
  COPY_REG (__bsp, IA64_BSP_REGNUM);

  /* Bad things happen if we don't update both bsp and bspstore at the
     same time.  */
  if (regno == IA64_BSP_REGNUM || regno == -1)
    {
      memcpy (&(gregsetp->__bspstore),
	      &registers[REGISTER_BYTE (IA64_BSP_REGNUM)],
	      REGISTER_RAW_SIZE (IA64_BSP_REGNUM));
      memcpy (&registers[REGISTER_BYTE (IA64_BSPSTORE_REGNUM)],
	      &registers[REGISTER_BYTE (IA64_BSP_REGNUM)],
	      REGISTER_RAW_SIZE (IA64_BSP_REGNUM));
    }

#if 0
  /* We never actually write to bspstore, or we'd have to do the same thing
     here too.  */
  COPY_REG (__bspstore, IA64_BSPSTORE_REGNUM);
#endif
  COPY_REG (__rnat, IA64_RNAT_REGNUM);
  COPY_REG (__pfs, IA64_PFS_REGNUM);
  COPY_REG (__unat, IA64_UNAT_REGNUM);
  COPY_REG (__preds, IA64_PR_REGNUM);
  COPY_REG (__ccv, IA64_CCV_REGNUM);
  COPY_REG (__lc, IA64_LC_REGNUM);
  COPY_REG (__ec, IA64_EC_REGNUM);
  /* FIXME: __nats */
  COPY_REG (__fpsr, IA64_FPSR_REGNUM);
#undef COPY_REG
}

void
supply_fpregset (prfpregset_t *fpregsetp)
{
  register int regi;

  for (regi = IA64_FR0_REGNUM; regi <= IA64_FR127_REGNUM; regi++)
    supply_register (regi, 
                     (char *) &(fpregsetp->__fpr[regi - IA64_FR0_REGNUM]));
}

void
fill_fpregset (prfpregset_t *fpregsetp, int regno)
{
  int regi;
  char *to;
  char *from;

  for (regi = IA64_FR0_REGNUM; regi <= IA64_FR127_REGNUM; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &registers[REGISTER_BYTE (regi)];
	  to = (char *) &(fpregsetp->__fpr[regi - IA64_FR0_REGNUM]);
	  memcpy (to, from, REGISTER_RAW_SIZE (regi));
	}
    }
}
