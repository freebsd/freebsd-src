/* Native-dependent Motorola 88xxx support for GDB, the GNU Debugger.
   Copyright 1988, 1990, 1991, 1992 Free Software Foundation, Inc.

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
#include "frame.h"
#include "inferior.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include "gdbcore.h"
#include <sys/user.h>

#ifndef USER			/* added to support BCS ptrace_user */
#define USER ptrace_user
#endif
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/file.h>
#include "gdb_stat.h"

#include "symtab.h"
#include "setjmp.h"
#include "value.h"

#ifdef DELTA88
#include <sys/ptrace.h>

/* define offsets to the pc instruction offsets in ptrace_user struct */
#define SXIP_OFFSET ((char *)&u.pt_sigframe.sig_sxip - (char *)&u)
#define SNIP_OFFSET ((char *)&u.pt_sigframe.sig_snip - (char *)&u)
#define SFIP_OFFSET ((char *)&u.pt_sigframe.sig_sfip - (char *)&u)
#else
/* define offsets to the pc instruction offsets in ptrace_user struct */
#define SXIP_OFFSET ((char *)&u.pt_sigframe.dg_sigframe.sc_sxip - (char *)&u)
#define SNIP_OFFSET ((char *)&u.pt_sigframe.dg_sigframe.sc_snip - (char *)&u)
#define SFIP_OFFSET ((char *)&u.pt_sigframe.dg_sigframe.sc_sfip - (char *)&u)
#endif

extern int have_symbol_file_p();

extern jmp_buf stack_jmp;

extern int errno;
extern char registers[REGISTER_BYTES];

void
fetch_inferior_registers (regno)
     int regno;		/* Original value discarded */
{
  register unsigned int regaddr;
  char buf[MAX_REGISTER_RAW_SIZE];
  register int i;

  struct USER u;
  unsigned int offset;

  offset = (char *) &u.pt_r0 - (char *) &u; 
  regaddr = offset; /* byte offset to r0;*/

/*  offset = ptrace (3, inferior_pid, (PTRACE_ARG3_TYPE) offset, 0) - KERNEL_U_ADDR; */
  for (regno = 0; regno < NUM_REGS; regno++)
    {
      /*regaddr = register_addr (regno, offset);*/
	/* 88k enhancement  */
        
      for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (int))
 	{
 	  *(int *) &buf[i] = ptrace (3, inferior_pid,
				     (PTRACE_ARG3_TYPE) regaddr, 0);
 	  regaddr += sizeof (int);
 	}
      supply_register (regno, buf);
    }
    /* now load up registers 36 - 38; special pc registers */
    *(int *) &buf[0] = ptrace (3,inferior_pid,
			       (PTRACE_ARG3_TYPE) SXIP_OFFSET ,0);
    supply_register (SXIP_REGNUM, buf);
    *(int *) &buf[0] = ptrace (3, inferior_pid,
			       (PTRACE_ARG3_TYPE) SNIP_OFFSET,0);
    supply_register (SNIP_REGNUM, buf);
    *(int *) &buf[0] = ptrace (3, inferior_pid,
			       (PTRACE_ARG3_TYPE) SFIP_OFFSET,0);
    supply_register (SFIP_REGNUM, buf);
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  register unsigned int regaddr;
  char buf[80];

  struct USER u;

  unsigned int offset = (char *) &u.pt_r0 - (char *) &u;

  regaddr = offset;

  /* Don't try to deal with EXIP_REGNUM or ENIP_REGNUM, because I think either
     svr3 doesn't run on an 88110, or the kernel isolates the different (not
     completely sure this is true, but seems to be.  */
  if (regno >= 0)
    {
      /*      regaddr = register_addr (regno, offset); */
      if (regno < PC_REGNUM)
	{ 
	  regaddr = offset + regno * sizeof (int);
	  errno = 0;
	  ptrace (6, inferior_pid,
		  (PTRACE_ARG3_TYPE) regaddr, read_register (regno));
	  if (errno != 0)
	    {
	      sprintf (buf, "writing register number %d", regno);
	      perror_with_name (buf);
	    }
	}
      else if (regno == SXIP_REGNUM)
	ptrace (6, inferior_pid,
		(PTRACE_ARG3_TYPE) SXIP_OFFSET, read_register(regno));
      else if (regno == SNIP_REGNUM)
	ptrace (6, inferior_pid,
		(PTRACE_ARG3_TYPE) SNIP_OFFSET, read_register(regno));
      else if (regno == SFIP_REGNUM)
	ptrace (6, inferior_pid,
		(PTRACE_ARG3_TYPE) SFIP_OFFSET, read_register(regno));
      else
	printf_unfiltered ("Bad register number for store_inferior routine\n");
    }
  else
    { 
      for (regno = 0; regno < PC_REGNUM; regno++)
	{
	  /*      regaddr = register_addr (regno, offset); */
	  errno = 0;
	  regaddr = offset + regno * sizeof (int);
	  ptrace (6, inferior_pid,
		  (PTRACE_ARG3_TYPE) regaddr, read_register (regno));
	  if (errno != 0)
	    {
	      sprintf (buf, "writing register number %d", regno);
	      perror_with_name (buf);
	    }
	}
      ptrace (6,inferior_pid,
	      (PTRACE_ARG3_TYPE) SXIP_OFFSET,read_register(SXIP_REGNUM));
      ptrace (6,inferior_pid,
	      (PTRACE_ARG3_TYPE) SNIP_OFFSET,read_register(SNIP_REGNUM));
      ptrace (6,inferior_pid,
	      (PTRACE_ARG3_TYPE) SFIP_OFFSET,read_register(SFIP_REGNUM));
    }
}


/* blockend is the address of the end of the user structure */
m88k_register_u_addr (blockend, regnum)
{
  struct USER u;
  int ustart = blockend - sizeof (struct USER);
  switch (regnum)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
      return (ustart + ((int) &u.pt_r0 - (int) &u) + REGISTER_SIZE * regnum);
    case PSR_REGNUM:  return (ustart + ((int) &u.pt_psr - (int) &u));
    case FPSR_REGNUM: return (ustart + ((int) &u.pt_fpsr - (int) &u));
    case FPCR_REGNUM: return (ustart + ((int) &u.pt_fpcr - (int) &u));
    case SXIP_REGNUM: return (ustart + SXIP_OFFSET); 
    case SNIP_REGNUM: return (ustart + SNIP_OFFSET);
    case SFIP_REGNUM: return (ustart + SFIP_OFFSET); 
    default: 
	if (regnum < NUM_REGS)
	    /* The register is one of those which is not defined...
	       give it zero */
	    return (ustart + ((int) &u.pt_r0 - (int) &u));
	else
	    return (blockend + REGISTER_SIZE * regnum);
    }
}

#ifdef USE_PROC_FS

#include <sys/procfs.h>

/*  Given a pointer to a general register set in /proc format (gregset_t *),
    unpack the register contents and supply them as gdb's idea of the current
    register values. */

void
supply_gregset (gregsetp)
     gregset_t *gregsetp;
{
    register int regi;
    register greg_t *regp = (greg_t *) gregsetp;

    for (regi=0; regi <= SP_REGNUM; regi++)
	supply_register (regi, (char *) (regp + regi));

    supply_register (SXIP_REGNUM, (char *) (regp + R_XIP));
    supply_register (SNIP_REGNUM, (char *) (regp + R_NIP));
    supply_register (SFIP_REGNUM, (char *) (regp + R_FIP));
    supply_register (PSR_REGNUM, (char *) (regp + R_PSR));
    supply_register (FPSR_REGNUM, (char *) (regp + R_FPSR));
    supply_register (FPCR_REGNUM, (char *) (regp + R_FPCR));
}

void
fill_gregset (gregsetp, regno)
     gregset_t *gregsetp;
     int regno;
{
    int regi;
    register greg_t *regp = (greg_t *) gregsetp;
    extern char registers[];

    for (regi = 0 ; regi <= R_R31 ; regi++)
	if ((regno == -1) || (regno == regi))
	    *(regp + regi) = *(int *) &registers[REGISTER_BYTE(regi)];

    if ((regno == -1) || (regno == SXIP_REGNUM))
	*(regp + R_XIP) = *(int *) &registers[REGISTER_BYTE(SXIP_REGNUM)];
    if ((regno == -1) || (regno == SNIP_REGNUM))
	*(regp + R_NIP) = *(int *) &registers[REGISTER_BYTE(SNIP_REGNUM)];
    if ((regno == -1) || (regno == SFIP_REGNUM))
	*(regp + R_FIP) = *(int *) &registers[REGISTER_BYTE(SFIP_REGNUM)];
    if ((regno == -1) || (regno == PSR_REGNUM))
	*(regp + R_PSR) = *(int *) &registers[REGISTER_BYTE(PSR_REGNUM)];
    if ((regno == -1) || (regno == FPSR_REGNUM))
	*(regp + R_FPSR) = *(int *) &registers[REGISTER_BYTE(FPSR_REGNUM)];
    if ((regno == -1) || (regno == FPCR_REGNUM))
	*(regp + R_FPCR) = *(int *) &registers[REGISTER_BYTE(FPCR_REGNUM)];
}

#endif /* USE_PROC_FS */
