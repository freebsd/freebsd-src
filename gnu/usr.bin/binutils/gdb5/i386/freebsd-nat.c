/* $FreeBSD$ */
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

/* $FreeBSD$ */

#include "defs.h"

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <sys/param.h>
#include <sys/user.h>
#include "gdbcore.h"
#include "value.h"
#include "inferior.h"

#if defined(HAVE_GREGSET_T) || defined(HAVE_FPREGSET_T)
#include <sys/procfs.h>
#endif

/* this table must line up with REGISTER_NAMES in tm-i386v.h */
/* symbols like 'tEAX' come from <machine/reg.h> */
static int tregmap[] =
{
  tEAX, tECX, tEDX, tEBX,
  tESP, tEBP, tESI, tEDI,
  tEIP, tEFLAGS, tCS, tSS,
  tDS, tES, tFS, tGS,
};

static struct save87 pcb_savefpu;

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
   Extract the floating point state out of the core file and store
   it where `float_info' will find it.

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
  int offset;
  struct user *tmp_uaddr;

  /* 
   * First get virtual address of user structure. Then calculate offset.
   */
  memcpy(&tmp_uaddr,
	 &((struct user *) core_reg_sect)->u_kproc.ki_addr,
	 sizeof(tmp_uaddr));
  offset = -reg_addr - (int) tmp_uaddr;
  
  for (regno = 0; regno < NUM_REGS; regno++)
    {
      cregno = tregmap[regno];
      if (cregno == tGS)
        addr = offsetof (struct user, u_pcb) + offsetof (struct pcb, pcb_gs);
      else
        addr = offset + 4 * cregno;
      if (addr < 0 || addr >= core_reg_size)
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
      error ("Register %s not found in core file.", gdb_register_names[bad_reg]);
    }

  addr = offsetof (struct user, u_pcb) + offsetof (struct pcb, pcb_savefpu);
  memcpy (&pcb_savefpu, core_reg_sect + addr, sizeof pcb_savefpu);
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
      int exp;
      int mantissa_or;
      int normal;
      char *sign;
      int st_regno;
      unsigned short *usregs;
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
      
      printf_unfiltered ("  ");

      /*
       * Handle weird cases better than floatformat_to_double () and
       * printf ().
       */
      usregs = (unsigned short *) ep->regs[st_regno];
      sign = usregs[4] & 0x8000 ? "-" : "";
      exp = usregs[4] & 0x7fff;
      normal = usregs[3] & 0x8000;
      mantissa_or = usregs[0] | usregs[1] | usregs[2] | (usregs[3] & 0x7fff);
      if (exp == 0)
	{
	  if (normal)
	    printf_unfiltered ("Pseudo Denormal (0 as a double)");
	  else if (mantissa_or == 0)
	    printf_unfiltered ("%s0", sign);
	  else
	    printf_unfiltered ("Denormal (0 as a double)");
	}
      else if (exp == 0x7fff)
	{
	  if (!normal)
	    printf_unfiltered ("Pseudo ");
	  if (mantissa_or == 0)
	    printf_unfiltered ("%sInf", sign);
	  else
	    printf_unfiltered ("%s NaN",
			       usregs[3] & 0x4000 ? "Quiet" : "Signaling");
	  if (!normal)
	    printf_unfiltered (" (NaN)");
	}
      else if (!normal)
	printf_unfiltered ("Unnormal (NaN)");
      else
	{
#if 0
	  /* Use this we stop trapping on overflow.  */
	  floatformat_to_double(&floatformat_i387_ext,
				(char *) ep->regs[st_regno], &val);
#else
	  i387_to_double((char *) ep->regs[st_regno], (char *) &val);
#endif
	  printf_unfiltered ("%g", val);
	}
      printf_unfiltered ("\n");
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
  if (inferior_pid != 0 && core_bfd == NULL) 
    {
      int pid = inferior_pid & ((1 << 17) - 1);	/* XXX extract pid from tid */
      ptrace(PT_GETFPREGS, pid, &buf[0], sizeof(struct fpreg));
      fpstatep = (struct fpstate *)&buf[0];
    } 
  else 
    fpstatep = &pcb_savefpu;

  print_387_status (fpstatep->sv_ex_sw, (struct env387 *)fpstatep);
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

#ifdef HAVE_GREGSET_T
void
supply_gregset (gp)
  gregset_t *gp;
{
  int regno = 0;

  /* These must be ordered the same as REGISTER_NAMES in
     config/i386/tm-i386.h. */
  supply_register (regno++, (char *)&gp->r_eax);
  supply_register (regno++, (char *)&gp->r_ecx);
  supply_register (regno++, (char *)&gp->r_edx);
  supply_register (regno++, (char *)&gp->r_ebx);
  supply_register (regno++, (char *)&gp->r_esp);
  supply_register (regno++, (char *)&gp->r_ebp);
  supply_register (regno++, (char *)&gp->r_esi);
  supply_register (regno++, (char *)&gp->r_edi);
  supply_register (regno++, (char *)&gp->r_eip);
  supply_register (regno++, (char *)&gp->r_eflags);
  supply_register (regno++, (char *)&gp->r_cs);
  supply_register (regno++, (char *)&gp->r_ss);
  supply_register (regno++, (char *)&gp->r_ds);
  supply_register (regno++, (char *)&gp->r_es);
  supply_register (regno++, (char *)&gp->r_fs);
  supply_register (regno++, (char *)&gp->r_gs);
}
#endif	/* HAVE_GREGSET_T */

#ifdef HAVE_FPREGSET_T
void
supply_fpregset (fp)
  fpregset_t *fp;
{
  memcpy (&pcb_savefpu, fp, sizeof pcb_savefpu);
}
#endif	/* HAVE_FPREGSET_T */

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

#ifdef PT_GETDBREGS

/*
 * 0: no trace output
 * 1: trace watchpoint requests
 * 2: trace `watchpoint hit?' tests, too
 */
#define WATCHPOINT_DEBUG 0

#include "breakpoint.h"

int
can_watch(type, cnt, ot)
     int type, cnt, ot;
{
  int rv;
  static int cnt_watch, cnt_awatch;

  switch (type)
    {
    case bp_hardware_watchpoint:
      cnt_watch = cnt;
      break;
      
    case bp_access_watchpoint:
      cnt_awatch = cnt;
      break;
      
    default:
      rv = 0;
      goto overandout;
    }

  rv = cnt_watch + cnt_awatch <= 4? 1: -1;

 overandout:
#if WATCHPOINT_DEBUG
  printf_filtered("can_watch(%d, %d, %d) = %d (counts: w: %d, rw: %d)\n",
		  type, cnt, ot, rv, cnt_watch, cnt_awatch);
#endif

  return rv;
}

int
stopped_by_watchpoint()
{
  struct dbreg dbr;
  extern int inferior_pid;

  if (current_target.to_shortname == 0 ||
      ! (strcmp(current_target.to_shortname, "child") == 0 ||
	 strcmp(current_target.to_shortname, "freebsd-uthreads") == 0))
    return 0;
  
  if (inferior_pid != 0 && core_bfd == NULL) 
    {
      int pid = inferior_pid & ((1 << 17) - 1);	/* XXX extract pid from tid */
  
      if (ptrace(PT_GETDBREGS, pid, (caddr_t)&dbr, 0) == -1)
	{
	  perror("ptrace(PT_GETDBREGS) failed");
	  return 0;
	}
#if WATCHPOINT_DEBUG > 1
      printf_filtered("stopped_by_watchpoint(): DR6 = %#x\n", dbr.dr6);
#endif
      /*
       * If a hardware watchpoint was hit, one of the lower 4 bits in
       * DR6 is set (the actual bit indicates which of DR0...DR3 triggered
       * the trap).
       */
      return dbr.dr6 & 0x0f;
    } 
  else
    {
      warning("Can't set a watchpoint on a core file.");
      return 0;
    }
}

int
insert_watchpoint(addr, len, type)
     int addr, len, type;
{
  struct dbreg dbr;
  extern int inferior_pid;
  
  if (current_target.to_shortname == 0 ||
      ! (strcmp(current_target.to_shortname, "child") == 0 ||
	 strcmp(current_target.to_shortname, "freebsd-uthreads") == 0))
    return 0;
  
  if (inferior_pid != 0 && core_bfd == NULL) 
    {
      int pid = inferior_pid & ((1 << 17) - 1);	/* XXX extract pid from tid */
      int i, mask;
      unsigned int sbits;

      if (ptrace(PT_GETDBREGS, pid, (caddr_t)&dbr, 0) == -1)
	{
	  perror("ptrace(PT_GETDBREGS) failed");
	  return 0;
	}

      for (i = 0, mask = 0x03; i < 4; i++, mask <<= 2)
	if ((dbr.dr7 & mask) == 0)
	  break;
      if (i >= 4) {
	warning("no more hardware watchpoints available");
	return -1;
      }

      /* paranoia */
      if (len > 4)
	{
	  warning("watchpoint length %d unsupported, using lenght = 4",
		  len);
	  len = 4;
	}
      else if (len == 3)
	{
	  warning("weird watchpoint length 3, using 2");
	  len = 2;
	}
      else if (len == 0)
	{
	  warning("weird watchpoint length 0, using 1");
	  len = 1;
	}

      switch (len)
	{
	case 1: sbits = 0; break;
	case 2: sbits = 4; break;
	case 4: sbits = 0x0c; break;
	}
      
      /*
       *  The `type' value is 0 for `watch on write', 1 for `watch on
       * read', 2 for `watch on both'.  The i386 debug register
       * breakpoint types are 0 for `execute' (not used in GDB), 1 for
       * `write', and 4 for `read/write'.  Plain `read' trapping is
       * not supported on i386, value 3 is illegal.
       */
      switch (type)
	{
	default:
	  warning("weird watchpoint type %d, using a write watchpoint");
	  /* FALLTHROUGH */
	case 0:
	  sbits |= 1;
	  break;

	case 2:
	  sbits |= 3;
	  break;
	}
      sbits <<= 4 * i + 16;
      sbits |= 1 << 2 * i;

      dbr.dr7 |= sbits;
      *(&dbr.dr0 + i) = (unsigned int)addr;

#if WATCHPOINT_DEBUG
      printf_filtered("insert_watchpoint(), inserting DR7 = %#x, DR%d = %#x\n",
		      dbr.dr7, i, addr);
#endif
      if (ptrace(PT_SETDBREGS, pid, (caddr_t)&dbr, 0) == -1)
	{
	  perror("ptrace(PT_SETDBREGS) failed");
	  return 0;
	}
    }
  else
    {
      warning("Can't set a watchpoint on a core file.");
      return 0;
    }
}

int
remove_watchpoint(addr, len, type)
     int addr, len, type;
{
  struct dbreg dbr;
  extern int inferior_pid;
  
  if (current_target.to_shortname == 0 ||
      ! (strcmp(current_target.to_shortname, "child") == 0 ||
	 strcmp(current_target.to_shortname, "freebsd-uthreads") == 0))
    return 0;

  if (inferior_pid != 0 && core_bfd == NULL) 
    {
      int pid = inferior_pid & ((1 << 17) - 1);	/* XXX extract pid from tid */
      int i;
      unsigned int sbits, *dbregp;
  
      if (ptrace(PT_GETDBREGS, pid, (caddr_t)&dbr, 0) == -1)
	{
	  perror("ptrace(PT_GETDBREGS) failed");
	  return 0;
	}

      for (i = 0, dbregp = &dbr.dr0; i < 4; i++, dbregp++)
	if (*dbregp == (unsigned int)addr)
	  break;
      if (i >= 4)
	{
	  warning("watchpoint for address %#x not found", addr);
	  return -1;
	}

      *dbregp = 0;
      sbits = 0xf << (4 * i + 16);
      sbits |= 3 << 2 * i;
      dbr.dr7 &= ~sbits;

#if WATCHPOINT_DEBUG
      printf_filtered("remove_watchpoint(): removing watchpoint for %#x, DR7 = %#x\n",
		      addr, dbr.dr7);
#endif
      if (ptrace(PT_SETDBREGS, pid, (caddr_t)&dbr, 0) == -1)
	{
	  perror("ptrace(PT_SETDBREGS) failed");
	  return 0;
	}
    }
  else
    {
      warning("Can't set a watchpoint on a core file.");
      return 0;
    }
}

#endif /* PT_GETDBREGS */
