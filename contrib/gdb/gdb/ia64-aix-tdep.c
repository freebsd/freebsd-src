/* Target-dependent code for the IA-64 for GDB, the GNU debugger.
   Copyright 2000, 2001
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

#include "defs.h"

/* External hook for finding gate addresses on AIX.  */
void (*aix5_find_gate_addresses_hook) (CORE_ADDR *, CORE_ADDR *) = 0;

/* Offset to sc_context member of sigcontext structure from frame of handler */
#define IA64_AIX_SIGCONTEXT_OFFSET 64

/* Return a non-zero value iff PC is in a signal trampoline.  On
   AIX, we determine this by seeing if the pc is in a special
   execute only page called the ``gate page''.  The addresses in
   question are determined by letting the AIX native portion of
   the code determine these addresses via its own nefarious
   means.  */

int
ia64_aix_in_sigtramp (CORE_ADDR pc, char *func_name)
{
  CORE_ADDR gate_area_start, gate_area_end;

  if (aix5_find_gate_addresses_hook == 0)
    return 0;

  (*aix5_find_gate_addresses_hook) (&gate_area_start, &gate_area_end);

  return (pc >=  gate_area_start && pc < gate_area_end);
}


/* IA-64 AIX specific function which, given a frame address and
   a register number, returns the address at which that register may be
   found.  0 is returned for registers which aren't stored in the
   sigcontext structure.  */

CORE_ADDR
ia64_aix_sigcontext_register_address (CORE_ADDR sp, int regno)
{
  /* The hardcoded offsets that follow are actually offsets to the
     corresponding members in struct __context in
     /usr/include/sys/context.h (on an IA-64 AIX5 box).  */
  if (IA64_GR0_REGNUM <= regno && regno <= IA64_GR31_REGNUM)
    return sp + IA64_AIX_SIGCONTEXT_OFFSET + 152 + 8 * (regno - IA64_GR0_REGNUM);
  else if (IA64_BR0_REGNUM <= regno && regno <= IA64_BR7_REGNUM)
    return sp + IA64_AIX_SIGCONTEXT_OFFSET + 408 + 8 * (regno - IA64_BR0_REGNUM);
  else if (IA64_FR0_REGNUM <= regno && regno <= IA64_FR127_REGNUM)
    return sp + IA64_AIX_SIGCONTEXT_OFFSET + 480 + 16 * (regno - IA64_FR0_REGNUM);
  else
    switch (regno)
      {
      case IA64_PSR_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 0;
      case IA64_IP_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 8;
      /* iipa is at 16.
         isr is at 24.
	 ifa is at 32.
	 iim is at 40. */
      case IA64_CFM_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 48;  /* ifs, actually */
      case IA64_RSC_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 56;
      case IA64_BSP_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 64;
      case IA64_BSPSTORE_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 72;
      case IA64_RNAT_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 80;
      case IA64_PFS_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 88;
      case IA64_UNAT_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 96;
      case IA64_PR_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 104;
      case IA64_CCV_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 112;
      case IA64_LC_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 120;
      case IA64_EC_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 128;
      /* nats is at 136; this is an address independent NaT bitmask */
      case IA64_FPSR_REGNUM :
	return sp + IA64_AIX_SIGCONTEXT_OFFSET + 144;
      default :
	return 0;
      }
}
