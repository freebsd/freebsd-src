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

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include "gdbcore.h"
#include "value.h"
#include "inferior.h"

/* this table must line up with REGISTER_NAMES in tm-i386v.h */
/* symbols like 'tEAX' come from <machine/reg.h> */
static int tregmap[] =
{
  tEAX, tECX, tEDX, tEBX,
  tESP, tEBP, tESI, tEDI,
  tEIP, tEFLAGS, tCS, tSS,
  tDS, tES, tFS, tGS,
};

void
fetch_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;	/* ptrace order, not gcc/gdb order */
  int r;

  ptrace (PT_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);

  for (r = 0; r < NUM_REGS; r++)
    memcpy (&registers[REGISTER_BYTE (r)], ((int *)&inferior_registers) + tregmap[r], 4);

  registers_fetched ();
}

void
store_inferior_registers (regno)
     int regno;
{
  struct reg inferior_registers;	/* ptrace order, not gcc/gdb order */
  int r;

  for (r = 0; r < NUM_REGS; r++)
    memcpy (((int *)&inferior_registers) + tregmap[r], &registers[REGISTER_BYTE (r)], 4);

  ptrace (PT_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers, 0);
}

/* Extract the register values out of the core file and store
   them where `read_register' will find them.

   CORE_REG_SECT points to the register values themselves, read into memory.
   CORE_REG_SIZE is the size of that area.
   WHICH says which set of registers we are handling (0 = int, 2 = float
         on machines where they are discontiguous).
   REG_ADDR is the offset from u.u_ar0 to the register values relative to
            core_reg_sect.  This is used with old-fashioned core files to
	    locate the registers in a large upage-plus-stack ".reg" section.
	    Original upage address X is at location core_reg_sect+x+reg_addr.
 */

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     CORE_ADDR reg_addr;
{
  register int regno;
  register int cregno;
  register int addr;
  int bad_reg = -1;
  int offset = -reg_addr & (core_reg_size - 1);

  for (regno = 0; regno < NUM_REGS; regno++)
    {
      cregno = tregmap[regno];
#if 1
      if (cregno == tFS)
	cregno = tCS;
      else if (cregno == tGS)
	cregno = tDS;
#endif
      addr = offset + 4 * cregno;
      if (cregno == tFS)
	{
#if 0
	  supply_register (15, (char *)&u.u_pcb.pcb_fs);
#endif
	}
      else if (cregno == tGS)
	{
#if 0
	  supply_register (16, (char *)&u.u_pcb.pcb_gs);
#endif
	}
      else if (addr < 0 || addr >= core_reg_size)
	{
	  if (bad_reg < 0)
	    bad_reg = regno;
	}
      else
	{
	  supply_register (regno, core_reg_sect + addr);
	}
    }
  if (bad_reg >= 0)
    {
      error ("Register %s not found in core file.", reg_names[bad_reg]);
    }
}

#ifdef FLOAT_INFO
#include "expression.h"
#include "language.h"			/* for local_hex_string */
#include "floatformat.h"

#include <sys/param.h>
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

static void
i387_to_double (from, to)
     char *from;
     char *to;
{
  long *lp;
  /* push extended mode on 387 stack, then pop in double mode
   *
   * first, set exception masks so no error is generated -
   * number will be rounded to inf or 0, if necessary
   */
  asm ("pushl %eax"); 		/* grab a stack slot */
  asm ("fstcw (%esp)");		/* get 387 control word */
  asm ("movl (%esp),%eax");	/* save old value */
  asm ("orl $0x3f,%eax");		/* mask all exceptions */
  asm ("pushl %eax");
  asm ("fldcw (%esp)");		/* load new value into 387 */

  asm ("movl 8(%ebp),%eax");
  asm ("fldt (%eax)");		/* push extended number on 387 stack */
  asm ("fwait");
  asm ("movl 12(%ebp),%eax");
  asm ("fstpl (%eax)");		/* pop double */
  asm ("fwait");

  asm ("popl %eax");		/* flush modified control word */
  asm ("fnclex");			/* clear exceptions */
  asm ("fldcw (%esp)");		/* restore original control word */
  asm ("popl %eax");		/* flush saved copy */
}

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
  
  printf_unfiltered (" regno     tag  msb              lsb  value\n");
  for (fpreg = 7; fpreg >= 0; fpreg--) 
    {
      int st_regno;
      double val;
      
      /* The physical regno `fpreg' is only relevant as an index into the
       * tag word.  Logical `%st' numbers are required for indexing ep->regs.
       */
      st_regno = (fpreg + 8 - top) & 7;

      printf_unfiltered ("%%st(%d) %s ", st_regno, fpreg == top ? "=>" : "  ");

      switch ((ep->tag >> (fpreg * 2)) & 3) 
	{
	case 0: printf_unfiltered ("valid "); break;
	case 1: printf_unfiltered ("zero  "); break;
	case 2: printf_unfiltered ("trap  "); break;
	case 3: printf_unfiltered ("empty "); break;
	}
      for (i = 9; i >= 0; i--)
	printf_unfiltered ("%02x", ep->regs[st_regno][i]);
      
      i387_to_double((char *) ep->regs[st_regno], (char *) &val);
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

/* Register that we are able to handle aout (trad-core) file formats.  */

static struct core_fns aout_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_aout ()
{
  add_core_fns (&aout_core_fns);
}
