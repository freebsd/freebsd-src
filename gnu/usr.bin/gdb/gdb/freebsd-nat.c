/* Native-dependent code for BSD Unix running on i386's, for GDB.
   Copyright 1988, 1989, 1991, 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include <machine/reg.h>

/* this table must line up with REGISTER_NAMES in tm-i386.h */
/* symbols like 'tEAX' come from <machine/reg.h> */
static int tregmap[] = 
{
  tEAX, tECX, tEDX, tEBX,
  tESP, tEBP, tESI, tEDI,
  tEIP, tEFLAGS, tCS, tSS
};
#ifdef sEAX
static int sregmap[] = 
{
  sEAX, sECX, sEDX, sEBX,
  sESP, sEBP, sESI, sEDI,
  sEIP, sEFLAGS, sCS, sSS
};
#endif
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

#ifdef sEAX
  if (blockend < 0x1fcc)
    return (blockend + 4 * tregmap[regnum]);
  else
    return (blockend + 4 * sregmap[regnum]);
#else
    return (blockend + 4 * tregmap[regnum]);
#endif
}

#ifdef FLOAT_INFO
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
#include <sys/stat.h>
#include <sys/ptrace.h>

#define	fpstate		save87
#define	U_FPSTATE(u)	u.u_pcb.pcb_savefpu

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

double_to_i387 (from, to)
     char *from;
     char *to;
{
  /* push double mode on 387 stack, then pop in extended mode
   * no errors are possible because every 64-bit pattern
   * can be converted to an extended
   */
  asm ("movl 8(%ebp),%eax");
  asm ("fldl (%eax)");
  asm ("fwait");
  asm ("movl 12(%ebp),%eax");
  asm ("fstpt (%eax)");
  asm ("fwait");
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

void
print_387_control_word (control)
unsigned int control;
{
  printf ("control 0x%04x: ", control);
  printf ("compute to ");
  switch ((control >> 8) & 3) 
    {
    case 0: printf ("24 bits; "); break;
    case 1: printf ("(bad); "); break;
    case 2: printf ("53 bits; "); break;
    case 3: printf ("64 bits; "); break;
    }
  printf ("round ");
  switch ((control >> 10) & 3) 
    {
    case 0: printf ("NEAREST; "); break;
    case 1: printf ("DOWN; "); break;
    case 2: printf ("UP; "); break;
    case 3: printf ("CHOP; "); break;
    }
  if (control & 0x3f) 
    {
      printf ("mask:");
      if (control & 0x0001) printf (" INVALID");
      if (control & 0x0002) printf (" DENORM");
      if (control & 0x0004) printf (" DIVZ");
      if (control & 0x0008) printf (" OVERF");
      if (control & 0x0010) printf (" UNDERF");
      if (control & 0x0020) printf (" LOS");
      printf (";");
    }
  printf ("\n");
  if (control & 0xe080) printf ("warning: reserved bits on 0x%x\n",
				control & 0xe080);
}

void
print_387_status_word (status)
     unsigned int status;
{
  printf ("status 0x%04x: ", status);
  if (status & 0xff) 
    {
      printf ("exceptions:");
      if (status & 0x0001) printf (" INVALID");
      if (status & 0x0002) printf (" DENORM");
      if (status & 0x0004) printf (" DIVZ");
      if (status & 0x0008) printf (" OVERF");
      if (status & 0x0010) printf (" UNDERF");
      if (status & 0x0020) printf (" LOS");
      if (status & 0x0040) printf (" FPSTACK");
      printf ("; ");
    }
  printf ("flags: %d%d%d%d; ",
	  (status & 0x4000) != 0,
	  (status & 0x0400) != 0,
	  (status & 0x0200) != 0,
	  (status & 0x0100) != 0);
  
  printf ("top %d\n", (status >> 11) & 7);
}

static
print_387_status (status, ep)
     unsigned short status;
     struct env387 *ep;
{
  int i;
  int bothstatus;
  int top;
  int fpreg;
  unsigned char *p;
  
  bothstatus = ((status != 0) && (ep->status != 0));
  if (status != 0) 
    {
      if (bothstatus)
	printf ("u: ");
      print_387_status_word ((unsigned int)status);
    }
  
  if (ep->status != 0) 
    {
      if (bothstatus)
	printf ("e: ");
      print_387_status_word ((unsigned int)ep->status);
    }
  
  print_387_control_word ((unsigned int)ep->control);
  printf ("last exception: ");
  printf ("opcode 0x%x; ", ep->opcode);
  printf ("pc 0x%x:0x%x; ", ep->code_seg, ep->eip);
  printf ("operand 0x%x:0x%x\n", ep->operand_seg, ep->operand);
  
  top = (ep->status >> 11) & 7;
  
  printf (" regno     tag  msb              lsb  value\n");
  for (fpreg = 7; fpreg >= 0; fpreg--) 
    {
      int st_regno;
      double val;
      
      /* The physical regno `fpreg' is only relevant as an index into the
       * tag word.  Logical `%st' numbers are required for indexing `p->regs.
       */
      st_regno = (fpreg + 8 - top) & 0x7;

      printf ("%%st(%d) %s ", st_regno, fpreg == top ? "=>" : "  ");
      
      switch ((ep->tag >> (fpreg * 2)) & 3) 
	{
	case 0: printf ("valid "); break;
	case 1: printf ("zero  "); break;
	case 2: printf ("trap  "); break;
	case 3: printf ("empty "); break;
	}
      for (i = 9; i >= 0; i--)
	printf ("%02x", ep->regs[st_regno][i]);
      
      i387_to_double (ep->regs[st_regno], (char *)&val);
      printf ("  %g\n", val);
    }
}

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
  
  print_387_status (0, (struct env387 *)buf);
}

#endif
