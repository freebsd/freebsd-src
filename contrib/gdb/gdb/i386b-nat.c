/* Native-dependent code for BSD Unix running on i386's, for GDB.
   Copyright 1988, 1989, 1991, 1992, 1994, 1996 Free Software Foundation, Inc.

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

#ifdef FETCH_INFERIOR_REGISTERS
#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include "inferior.h"

void
fetch_inferior_registers(regno)
     int regno;
{
  struct reg inferior_registers;

  ptrace (PT_GETREGS, inferior_pid, (PTRACE_ARG3_TYPE) &inferior_registers, 0);
  memcpy (&registers[REGISTER_BYTE (0)], &inferior_registers, 4*NUM_REGS);
  registers_fetched ();
}

void
store_inferior_registers(regno)
     int regno;
{
  struct reg inferior_registers;

  memcpy (&inferior_registers, &registers[REGISTER_BYTE (0)], 4*NUM_REGS);
  ptrace (PT_SETREGS, inferior_pid, (PTRACE_ARG3_TYPE) &inferior_registers, 0);
}

struct md_core {
  struct reg intreg;
  struct fpreg freg;
};

void
fetch_core_registers (core_reg_sect, core_reg_size, which, ignore)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int ignore;
{
  struct md_core *core_reg = (struct md_core *)core_reg_sect;

  /* integer registers */
  memcpy(&registers[REGISTER_BYTE (0)], &core_reg->intreg,
	 sizeof(struct reg));
  /* floating point registers */
  /* XXX */
}

#else

#include <machine/reg.h>

/* Some systems don't provide all the registers on a trap.  Use SS as a
   default if so.  */

#ifndef tDS
#define tDS tSS
#endif
#ifndef tES
#define tES tSS
#endif
#ifndef tFS
#define tFS tSS
#endif
#ifndef tGS
#define tGS tSS
#endif

/* These tables map between the registers on a trap frame, and the register
   order used by the rest of GDB.  */
/* this table must line up with REGISTER_NAMES in tm-i386.h */
/* symbols like 'tEAX' come from <machine/reg.h> */
static int tregmap[] = 
{
  tEAX, tECX, tEDX, tEBX,
  tESP, tEBP, tESI, tEDI,
  tEIP, tEFLAGS, tCS, tSS,
  tDS, tES, tFS, tGS
};

#ifdef sEAX
static int sregmap[] = 
{
  sEAX, sECX, sEDX, sEBX,
  sESP, sEBP, sESI, sEDI,
  sEIP, sEFLAGS, sCS, sSS
};
#else /* No sEAX */

/* FreeBSD has decided to collapse the s* and t* symbols.  So if the s*
   ones aren't around, use the t* ones for sregmap too.  */

static int sregmap[] = 
{
  tEAX, tECX, tEDX, tEBX,
  tESP, tEBP, tESI, tEDI,
  tEIP, tEFLAGS, tCS, tSS,
  tDS, tES, tFS, tGS
};
#endif /* No sEAX */

/* blockend is the value of u.u_ar0, and points to the
   place where ES is stored.  */

int
i386_register_u_addr (blockend, regnum)
     int blockend;
     int regnum;
{
  /* The following condition is a kludge to get at the proper register map
     depending upon the state of pcb_flag.
     The proper condition would be
     if (u.u_pcb.pcb_flag & FM_TRAP)
     but that would require a ptrace call here and wouldn't work
     for corefiles.  */

  if (blockend < 0x1fcc)
    return (blockend + 4 * tregmap[regnum]);
  else
    return (blockend + 4 * sregmap[regnum]);
}

#endif /* !FETCH_INFERIOR_REGISTERS */

#ifdef FLOAT_INFO
#include "expression.h"
#include "language.h"			/* for local_hex_string */
#include "floatformat.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <a.out.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>
#define curpcb Xcurpcb	/* XXX avoid leaking declaration from pcb.h */
#include <sys/user.h>
#undef curpcb
#include <sys/file.h>
#include "gdb_stat.h"
#include <sys/ptrace.h>

extern void print_387_control_word ();		/* i387-tdep.h */
extern void print_387_status_word ();

#define	fpstate		save87
#define	U_FPSTATE(u)	u.u_pcb.pcb_savefpu

struct env387 
{
  unsigned short control;
  unsigned short r0;
  unsigned short status;
  unsigned short r1;
  unsigned short tag;
  unsigned short r2;
  unsigned long eip;
  unsigned short code_seg;
  unsigned short opcode;
  unsigned long operand;
  unsigned short operand_seg;
  unsigned short r3;
  unsigned char regs[8][10];
};

static void
print_387_status (status, ep)
     unsigned short status;
     struct env387 *ep;
{
  int i;
  int bothstatus;
  int top;
  int fpreg;
  
  bothstatus = ((status != 0) && (ep->status != 0));
  if (status != 0) 
    {
      if (bothstatus)
	printf_unfiltered ("u: ");
      print_387_status_word ((unsigned int)status);
    }
  
  if (ep->status != 0) 
    {
      if (bothstatus)
	printf_unfiltered ("e: ");
      print_387_status_word ((unsigned int)ep->status);
    }
  
  print_387_control_word ((unsigned int)ep->control);
  printf_unfiltered ("last instruction: ");
  printf_unfiltered ("opcode %s; ", local_hex_string(ep->opcode));
  printf_unfiltered ("pc %s:", local_hex_string(ep->code_seg));
  printf_unfiltered ("%s; ", local_hex_string(ep->eip));
  printf_unfiltered ("operand %s", local_hex_string(ep->operand_seg));
  printf_unfiltered (":%s\n", local_hex_string(ep->operand));

  top = (ep->status >> 11) & 7;
  
  printf_unfiltered ("regno     tag  msb              lsb  value\n");
  for (fpreg = 7; fpreg >= 0; fpreg--) 
    {
      double val;
      
      printf_unfiltered ("%s %d: ", fpreg == top ? "=>" : "  ", fpreg); 

      switch ((ep->tag >> (fpreg * 2)) & 3) 
	{
	case 0: printf_unfiltered ("valid "); break;
	case 1: printf_unfiltered ("zero  "); break;
	case 2: printf_unfiltered ("trap  "); break;
	case 3: printf_unfiltered ("empty "); break;
	}
      for (i = 9; i >= 0; i--)
	printf_unfiltered ("%02x", ep->regs[fpreg][i]);
      
      floatformat_to_double(&floatformat_i387_ext, (char *) ep->regs[fpreg], 
			      &val);
      printf_unfiltered ("  %g\n", val);
    }
}

void
i386_float_info ()
{
  struct user u; /* just for address computations */
  int i;
  /* fpstate defined in <sys/user.h> */
  struct fpstate *fpstatep;
  char buf[sizeof (struct fpstate) + 2 * sizeof (int)];
  unsigned int uaddr;
  char fpvalid;
  unsigned int rounded_addr;
  unsigned int rounded_size;
  /*extern int corechan;*/
  int skip;
  extern int inferior_pid;
  
  uaddr = (char *)&U_FPSTATE(u) - (char *)&u;
  if (inferior_pid) 
    {
      int *ip;
      
      rounded_addr = uaddr & -sizeof (int);
      rounded_size = (((uaddr + sizeof (struct fpstate)) - uaddr) +
		      sizeof (int) - 1) / sizeof (int);
      skip = uaddr - rounded_addr;
      
      ip = (int *)buf;
      for (i = 0; i < rounded_size; i++) 
	{
	  *ip++ = ptrace (PT_READ_U, inferior_pid, (caddr_t)rounded_addr, 0);
	  rounded_addr += sizeof (int);
	}
    } 
  else 
    {
      printf("float info: can't do a core file (yet)\n");
      return;
#if 0
      if (lseek (corechan, uaddr, 0) < 0)
	perror_with_name ("seek on core file");
      if (myread (corechan, buf, sizeof (struct fpstate)) < 0) 
	perror_with_name ("read from core file");
      skip = 0;
#endif
    }

#ifdef	__FreeBSD__
  fpstatep = (struct fpstate *)(buf + skip);
  print_387_status (fpstatep->sv_ex_sw, (struct env387 *)fpstatep);
#else
  print_387_status (0, (struct env387 *)buf);
#endif
}
#endif /* FLOAT_INFO */

int
kernel_u_size ()
{
  return (sizeof (struct user));
}

#ifdef	SETUP_ARBITRARY_FRAME
#include "frame.h"
struct frame_info *
setup_arbitrary_frame (argc, argv)
	int argc;
	CORE_ADDR *argv;
{
    if (argc != 2)
	error ("i386 frame specifications require two arguments: sp and pc");

    return create_new_frame (argv[0], argv[1]);
}
#endif	/* SETUP_ARBITRARY_FRAME */
