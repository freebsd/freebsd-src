/* GNU/Linux/IA64 specific low level interface, for the remote server for GDB.
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

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

int num_regs = 590;

#include <asm/ptrace_offsets.h>

int regmap[] =
  {
    /* general registers */
    -1,		/* gr0 not available; i.e, it's always zero */
    PT_R1,
    PT_R2,
    PT_R3,
    PT_R4,
    PT_R5,
    PT_R6,
    PT_R7,
    PT_R8,
    PT_R9,
    PT_R10,
    PT_R11,
    PT_R12,
    PT_R13,
    PT_R14,
    PT_R15,
    PT_R16,
    PT_R17,
    PT_R18,
    PT_R19,
    PT_R20,
    PT_R21,
    PT_R22,
    PT_R23,
    PT_R24,
    PT_R25,
    PT_R26,
    PT_R27,
    PT_R28,
    PT_R29,
    PT_R30,
    PT_R31,
    /* gr32 through gr127 not directly available via the ptrace interface */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* Floating point registers */
    -1, -1,	/* f0 and f1 not available (f0 is +0.0 and f1 is +1.0) */
    PT_F2,
    PT_F3,
    PT_F4,
    PT_F5,
    PT_F6,
    PT_F7,
    PT_F8,
    PT_F9,
    PT_F10,
    PT_F11,
    PT_F12,
    PT_F13,
    PT_F14,
    PT_F15,
    PT_F16,
    PT_F17,
    PT_F18,
    PT_F19,
    PT_F20,
    PT_F21,
    PT_F22,
    PT_F23,
    PT_F24,
    PT_F25,
    PT_F26,
    PT_F27,
    PT_F28,
    PT_F29,
    PT_F30,
    PT_F31,
    PT_F32,
    PT_F33,
    PT_F34,
    PT_F35,
    PT_F36,
    PT_F37,
    PT_F38,
    PT_F39,
    PT_F40,
    PT_F41,
    PT_F42,
    PT_F43,
    PT_F44,
    PT_F45,
    PT_F46,
    PT_F47,
    PT_F48,
    PT_F49,
    PT_F50,
    PT_F51,
    PT_F52,
    PT_F53,
    PT_F54,
    PT_F55,
    PT_F56,
    PT_F57,
    PT_F58,
    PT_F59,
    PT_F60,
    PT_F61,
    PT_F62,
    PT_F63,
    PT_F64,
    PT_F65,
    PT_F66,
    PT_F67,
    PT_F68,
    PT_F69,
    PT_F70,
    PT_F71,
    PT_F72,
    PT_F73,
    PT_F74,
    PT_F75,
    PT_F76,
    PT_F77,
    PT_F78,
    PT_F79,
    PT_F80,
    PT_F81,
    PT_F82,
    PT_F83,
    PT_F84,
    PT_F85,
    PT_F86,
    PT_F87,
    PT_F88,
    PT_F89,
    PT_F90,
    PT_F91,
    PT_F92,
    PT_F93,
    PT_F94,
    PT_F95,
    PT_F96,
    PT_F97,
    PT_F98,
    PT_F99,
    PT_F100,
    PT_F101,
    PT_F102,
    PT_F103,
    PT_F104,
    PT_F105,
    PT_F106,
    PT_F107,
    PT_F108,
    PT_F109,
    PT_F110,
    PT_F111,
    PT_F112,
    PT_F113,
    PT_F114,
    PT_F115,
    PT_F116,
    PT_F117,
    PT_F118,
    PT_F119,
    PT_F120,
    PT_F121,
    PT_F122,
    PT_F123,
    PT_F124,
    PT_F125,
    PT_F126,
    PT_F127,
    /* predicate registers - we don't fetch these individually */
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    /* branch registers */
    PT_B0,
    PT_B1,
    PT_B2,
    PT_B3,
    PT_B4,
    PT_B5,
    PT_B6,
    PT_B7,
    /* virtual frame pointer and virtual return address pointer */
    -1, -1,
    /* other registers */
    PT_PR,
    PT_CR_IIP,	/* ip */
    PT_CR_IPSR, /* psr */
    PT_CFM,	/* cfm */
    /* kernel registers not visible via ptrace interface (?) */
    -1, -1, -1, -1, -1, -1, -1, -1,
    /* hole */
    -1, -1, -1, -1, -1, -1, -1, -1,
    PT_AR_RSC,
    PT_AR_BSP,
    PT_AR_BSPSTORE,
    PT_AR_RNAT,
    -1,
    -1,		/* Not available: FCR, IA32 floating control register */
    -1, -1,
    -1,		/* Not available: EFLAG */
    -1,		/* Not available: CSD */
    -1,		/* Not available: SSD */
    -1,		/* Not available: CFLG */
    -1,		/* Not available: FSR */
    -1,		/* Not available: FIR */
    -1,		/* Not available: FDR */
    -1,
    PT_AR_CCV,
    -1, -1, -1,
    PT_AR_UNAT,
    -1, -1, -1,
    PT_AR_FPSR,
    -1, -1, -1,
    -1,		/* Not available: ITC */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1,
    PT_AR_PFS,
    PT_AR_LC,
    -1,		/* Not available: EC, the Epilog Count register */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1,
    /* nat bits - not fetched directly; instead we obtain these bits from
       either rnat or unat or from memory. */
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
  };

int
cannot_store_register (int regno)
{
  return 0;
}

int
cannot_fetch_register (int regno)
{
  return 0;
}

