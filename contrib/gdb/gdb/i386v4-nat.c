/* Native-dependent code for SVR4 Unix running on i386's, for GDB.
   Copyright 1988, 1989, 1991, 1992, 1996 Free Software Foundation, Inc.

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

#ifdef HAVE_SYS_PROCFS_H

#include <sys/procfs.h>

/*  The /proc interface divides the target machine's register set up into
    two different sets, the general register set (gregset) and the floating
    point register set (fpregset).  For each set, there is an ioctl to get
    the current register set and another ioctl to set the current values.

    The actual structure passed through the ioctl interface is, of course,
    naturally machine dependent, and is different for each set of registers.
    For the i386 for example, the general register set is typically defined
    by:

	typedef int gregset_t[19];		(in <sys/regset.h>)

	#define GS	0			(in <sys/reg.h>)
	#define FS	1
	...
	#define UESP	17
	#define SS	18

    and the floating point set by:

	typedef struct fpregset
	  {
	    union
	      {
		struct fpchip_state	// fp extension state //
		{
		  int state[27];	// 287/387 saved state //
		  int status;		// status word saved at exception //
		} fpchip_state;
		struct fp_emul_space	// for emulators //
		{
		  char fp_emul[246];
		  char fp_epad[2];
		} fp_emul_space;
		int f_fpregs[62];	// union of the above //
	      } fp_reg_set;
	    long f_wregs[33];		// saved weitek state //
	} fpregset_t;

    These routines provide the packing and unpacking of gregset_t and
    fpregset_t formatted data.

 */

#ifdef HAVE_GREGSET_T

/* This is a duplicate of the table in i386-xdep.c. */

static int regmap[] = 
{
  EAX, ECX, EDX, EBX,
  UESP, EBP, ESI, EDI,
  EIP, EFL, CS, SS,
  DS, ES, FS, GS,
};


/*  FIXME:  These routine absolutely depends upon (NUM_REGS - NUM_FREGS)
    being less than or equal to the number of registers that can be stored
    in a gregset_t.  Note that with the current scheme there will typically
    be more registers actually stored in a gregset_t that what we know
    about.  This is bogus and should be fixed. */

/*  Given a pointer to a general register set in /proc format (gregset_t *),
    unpack the register contents and supply them as gdb's idea of the current
    register values. */

void
supply_gregset (gregsetp)
     gregset_t *gregsetp;
{
  register int regi;
  register greg_t *regp = (greg_t *) gregsetp;
  extern int regmap[];

  for (regi = 0 ; regi < (NUM_REGS - NUM_FREGS) ; regi++)
    {
      supply_register (regi, (char *) (regp + regmap[regi]));
    }
}

void
fill_gregset (gregsetp, regno)
     gregset_t *gregsetp;
     int regno;
{
  int regi;
  register greg_t *regp = (greg_t *) gregsetp;
  extern char registers[];
  extern int regmap[];

  for (regi = 0 ; regi < (NUM_REGS - NUM_FREGS) ; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  *(regp + regmap[regi]) = *(int *) &registers[REGISTER_BYTE (regi)];
	}
    }
}

#endif	/* HAVE_GREGSET_T */

#if defined (FP0_REGNUM) && defined (HAVE_FPREGSET_T)

/*  Given a pointer to a floating point register set in /proc format
    (fpregset_t *), unpack the register contents and supply them as gdb's
    idea of the current floating point register values. */

void 
supply_fpregset (fpregsetp)
     fpregset_t *fpregsetp;
{
  register int regi;
  
  /* FIXME: see m68k-tdep.c for an example, for the m68k. */
}

/*  Given a pointer to a floating point register set in /proc format
    (fpregset_t *), update the register specified by REGNO from gdb's idea
    of the current floating point register set.  If REGNO is -1, update
    them all. */

void
fill_fpregset (fpregsetp, regno)
     fpregset_t *fpregsetp;
     int regno;
{
  int regi;
  char *to;
  char *from;
  extern char registers[];

  /* FIXME: see m68k-tdep.c for an example, for the m68k. */
}

#endif	/* defined (FP0_REGNUM) && defined (HAVE_FPREGSET_T) */

#endif	/* HAVE_SYS_PROCFS_H */
