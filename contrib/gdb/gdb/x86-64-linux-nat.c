/* Native-dependent code for GNU/Linux x86-64.

   Copyright 2001, 2002 Free Software Foundation, Inc.

   Contributed by Jiri Smid, SuSE Labs.

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
#include "gdbcore.h"
#include "regcache.h"
#include "gdb_assert.h"
#include "x86-64-tdep.h"

#include <sys/ptrace.h>
#include <sys/debugreg.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/reg.h>

/* Mapping between the general-purpose registers in `struct user'
   format and GDB's register array layout.  */

static int x86_64_regmap[] = {
  RAX, RBX, RCX, RDX,
  RSI, RDI, RBP, RSP,
  R8, R9, R10, R11,
  R12, R13, R14, R15,
  RIP, EFLAGS,
  DS, ES, FS, GS
};

static unsigned long
x86_64_linux_dr_get (int regnum)
{
  int tid;
  unsigned long value;

  /* FIXME: kettenis/2001-01-29: It's not clear what we should do with
     multi-threaded processes here.  For now, pretend there is just
     one thread.  */
  tid = PIDGET (inferior_ptid);

  /* FIXME: kettenis/2001-03-27: Calling perror_with_name if the
     ptrace call fails breaks debugging remote targets.  The correct
     way to fix this is to add the hardware breakpoint and watchpoint
     stuff to the target vectore.  For now, just return zero if the
     ptrace call fails.  */
  errno = 0;
  value = ptrace (PT_READ_U, tid,
		  offsetof (struct user, u_debugreg[regnum]), 0);
  if (errno != 0)
#if 0
    perror_with_name ("Couldn't read debug register");
#else
    return 0;
#endif

  return value;
}

static void
x86_64_linux_dr_set (int regnum, unsigned long value)
{
  int tid;

  /* FIXME: kettenis/2001-01-29: It's not clear what we should do with
     multi-threaded processes here.  For now, pretend there is just
     one thread.  */
  tid = PIDGET (inferior_ptid);

  errno = 0;
  ptrace (PT_WRITE_U, tid, offsetof (struct user, u_debugreg[regnum]), value);
  if (errno != 0)
    perror_with_name ("Couldn't write debug register");
}

void
x86_64_linux_dr_set_control (unsigned long control)
{
  x86_64_linux_dr_set (DR_CONTROL, control);
}

void
x86_64_linux_dr_set_addr (int regnum, CORE_ADDR addr)
{
  gdb_assert (regnum >= 0 && regnum <= DR_LASTADDR - DR_FIRSTADDR);

  x86_64_linux_dr_set (DR_FIRSTADDR + regnum, addr);
}

void
x86_64_linux_dr_reset_addr (int regnum)
{
  gdb_assert (regnum >= 0 && regnum <= DR_LASTADDR - DR_FIRSTADDR);

  x86_64_linux_dr_set (DR_FIRSTADDR + regnum, 0L);
}

unsigned long
x86_64_linux_dr_get_status (void)
{
  return x86_64_linux_dr_get (DR_STATUS);
}


/* The register sets used in GNU/Linux ELF core-dumps are identical to
   the register sets used by `ptrace'.  */

#define GETREGS_SUPPLIES(regno) \
  (0 <= (regno) && (regno) < x86_64_num_gregs)
#define GETFPREGS_SUPPLIES(regno) \
  (FP0_REGNUM <= (regno) && (regno) <= MXCSR_REGNUM)


/* Transfering the general-purpose registers between GDB, inferiors
   and core files.  */

/* Fill GDB's register array with the general-purpose register values
   in *GREGSETP.  */

void
supply_gregset (elf_gregset_t * gregsetp)
{
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  int i;

  for (i = 0; i < x86_64_num_gregs; i++)
    supply_register (i, (char *) (regp + x86_64_regmap[i]));
}

/* Fill register REGNO (if it is a general-purpose register) in
   *GREGSETPS with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
fill_gregset (elf_gregset_t * gregsetp, int regno)
{
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  int i;

  for (i = 0; i < x86_64_num_gregs; i++)
    if ((regno == -1 || regno == i))
      read_register_gen (i, (char *) (regp + x86_64_regmap[i]));
}

/* Fetch all general-purpose registers from process/thread TID and
   store their values in GDB's register array.  */

static void
fetch_regs (int tid)
{
  elf_gregset_t regs;

  if (ptrace (PTRACE_GETREGS, tid, 0, (long) &regs) < 0)
    perror_with_name ("Couldn't get registers");

  supply_gregset (&regs);
}

/* Store all valid general-purpose registers in GDB's register array
   into the process/thread specified by TID.  */

static void
store_regs (int tid, int regno)
{
  elf_gregset_t regs;

  if (ptrace (PTRACE_GETREGS, tid, 0, (long) &regs) < 0)
    perror_with_name ("Couldn't get registers");

  fill_gregset (&regs, regno);

  if (ptrace (PTRACE_SETREGS, tid, 0, (long) &regs) < 0)
    perror_with_name ("Couldn't write registers");
}


/* Transfering floating-point registers between GDB, inferiors and cores.  */

static void *
x86_64_fxsave_offset (elf_fpregset_t * fxsave, int regnum)
{
  const char *reg_name;
  int reg_index;

  gdb_assert (x86_64_num_gregs - 1 < regnum && regnum < x86_64_num_regs);

  reg_name = x86_64_register_name (regnum);

  if (reg_name[0] == 's' && reg_name[1] == 't')
    {
      reg_index = reg_name[2] - '0';
      return &fxsave->st_space[reg_index * 2];
    }

  if (reg_name[0] == 'x' && reg_name[1] == 'm' && reg_name[2] == 'm')
    {
      reg_index = reg_name[3] - '0';
      return &fxsave->xmm_space[reg_index * 4];
    }

  if (strcmp (reg_name, "mxcsr") == 0)
    return &fxsave->mxcsr;

  return NULL;
}

/* Fill GDB's register array with the floating-point and SSE register
   values in *FXSAVE.  This function masks off any of the reserved
   bits in *FXSAVE.  */

void
supply_fpregset (elf_fpregset_t * fxsave)
{
  int i, reg_st0, reg_mxcsr;

  reg_st0 = x86_64_register_number ("st0");
  reg_mxcsr = x86_64_register_number ("mxcsr");

  gdb_assert (reg_st0 > 0 && reg_mxcsr > reg_st0);

  for (i = reg_st0; i <= reg_mxcsr; i++)
    supply_register (i, x86_64_fxsave_offset (fxsave, i));
}

/* Fill register REGNUM (if it is a floating-point or SSE register) in
   *FXSAVE with the value in GDB's register array.  If REGNUM is -1, do
   this for all registers.  This function doesn't touch any of the
   reserved bits in *FXSAVE.  */

void
fill_fpregset (elf_fpregset_t * fxsave, int regnum)
{
  int i, last_regnum = MXCSR_REGNUM;
  void *ptr;

  if (gdbarch_tdep (current_gdbarch)->num_xmm_regs == 0)
    last_regnum = FOP_REGNUM;

  for (i = FP0_REGNUM; i <= last_regnum; i++)
    if (regnum == -1 || regnum == i)
      {
	ptr = x86_64_fxsave_offset (fxsave, i);
	if (ptr)
	  regcache_collect (i, ptr);
      }
}

/* Fetch all floating-point registers from process/thread TID and store
   thier values in GDB's register array.  */

static void
fetch_fpregs (int tid)
{
  elf_fpregset_t fpregs;

  if (ptrace (PTRACE_GETFPREGS, tid, 0, (long) &fpregs) < 0)
    perror_with_name ("Couldn't get floating point status");

  supply_fpregset (&fpregs);
}

/* Store all valid floating-point registers in GDB's register array
   into the process/thread specified by TID.  */

static void
store_fpregs (int tid, int regno)
{
  elf_fpregset_t fpregs;

  if (ptrace (PTRACE_GETFPREGS, tid, 0, (long) &fpregs) < 0)
    perror_with_name ("Couldn't get floating point status");

  fill_fpregset (&fpregs, regno);

  if (ptrace (PTRACE_SETFPREGS, tid, 0, (long) &fpregs) < 0)
    perror_with_name ("Couldn't write floating point status");
}


/* Transferring arbitrary registers between GDB and inferior.  */

/* Fetch register REGNO from the child process.  If REGNO is -1, do
   this for all registers (including the floating point and SSE
   registers).  */

void
fetch_inferior_registers (int regno)
{
  int tid;

  /* GNU/Linux LWP ID's are process ID's.  */
  if ((tid = TIDGET (inferior_ptid)) == 0)
    tid = PIDGET (inferior_ptid);	/* Not a threaded program.  */

  if (regno == -1)
    {
      fetch_regs (tid);
      fetch_fpregs (tid);
      return;
    }

  if (GETREGS_SUPPLIES (regno))
    {
      fetch_regs (tid);
      return;
    }

  if (GETFPREGS_SUPPLIES (regno))
    {
      fetch_fpregs (tid);
      return;
    }

  internal_error (__FILE__, __LINE__,
		  "Got request for bad register number %d.", regno);
}

/* Store register REGNO back into the child process.  If REGNO is -1,
   do this for all registers (including the floating point and SSE
   registers).  */
void
store_inferior_registers (int regno)
{
  int tid;

  /* GNU/Linux LWP ID's are process ID's.  */
  if ((tid = TIDGET (inferior_ptid)) == 0)
    tid = PIDGET (inferior_ptid);	/* Not a threaded program.  */

  if (regno == -1)
    {
      store_regs (tid, regno);
      store_fpregs (tid, regno);
      return;
    }

  if (GETREGS_SUPPLIES (regno))
    {
      store_regs (tid, regno);
      return;
    }

  if (GETFPREGS_SUPPLIES (regno))
    {
      store_fpregs (tid, regno);
      return;
    }

  internal_error (__FILE__, __LINE__,
		  "Got request to store bad register number %d.", regno);
}


static const unsigned char linux_syscall[] = { 0x0f, 0x05 };

#define LINUX_SYSCALL_LEN (sizeof linux_syscall)

/* The system call number is stored in the %rax register.  */
#define LINUX_SYSCALL_REGNUM 0	/* %rax */

/* We are specifically interested in the sigreturn and rt_sigreturn
   system calls.  */

#ifndef SYS_sigreturn
#define SYS_sigreturn		__NR_sigreturn
#endif
#ifndef SYS_rt_sigreturn
#define SYS_rt_sigreturn	__NR_rt_sigreturn
#endif

/* Offset to saved processor flags, from <asm/sigcontext.h>.  */
#define LINUX_SIGCONTEXT_EFLAGS_OFFSET (152)
/* Offset to saved processor registers from <asm/ucontext.h> */
#define LINUX_UCONTEXT_SIGCONTEXT_OFFSET (36)

/* Interpreting register set info found in core files.  */
/* Provide registers to GDB from a core file.

   CORE_REG_SECT points to an array of bytes, which are the contents
   of a `note' from a core file which BFD thinks might contain
   register contents.  CORE_REG_SIZE is its size.

   WHICH says which register set corelow suspects this is:
     0 --- the general-purpose register set, in elf_gregset_t format
     2 --- the floating-point register set, in elf_fpregset_t format

   REG_ADDR isn't used on GNU/Linux.  */

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size,
		      int which, CORE_ADDR reg_addr)
{
  elf_gregset_t gregset;
  elf_fpregset_t fpregset;
  switch (which)
    {
    case 0:
      if (core_reg_size != sizeof (gregset))
	warning ("Wrong size gregset in core file.");
      else
	{
	  memcpy (&gregset, core_reg_sect, sizeof (gregset));
	  supply_gregset (&gregset);
	}
      break;

    case 2:
      if (core_reg_size != sizeof (fpregset))
	warning ("Wrong size fpregset in core file.");
      else
	{
	  memcpy (&fpregset, core_reg_sect, sizeof (fpregset));
	  supply_fpregset (&fpregset);
	}
      break;

    default:
      /* We've covered all the kinds of registers we know about here,
         so this must be something we wouldn't know what to do with
         anyway.  Just ignore it.  */
      break;
    }
}

/* Register that we are able to handle GNU/Linux ELF core file formats.  */

static struct core_fns linux_elf_core_fns = {
  bfd_target_elf_flavour,	/* core_flavour */
  default_check_format,		/* check_format */
  default_core_sniffer,		/* core_sniffer */
  fetch_core_registers,		/* core_read_registers */
  NULL				/* next */
};


#if !defined (offsetof)
#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)
#endif

/* Return the address of register REGNUM.  BLOCKEND is the value of
   u.u_ar0, which should point to the registers.  */
CORE_ADDR
x86_64_register_u_addr (CORE_ADDR blockend, int regnum)
{
  struct user u;
  CORE_ADDR fpstate;
  CORE_ADDR ubase;
  ubase = blockend;
  if (IS_FP_REGNUM (regnum))
    {
      fpstate = ubase + ((char *) &u.i387.st_space - (char *) &u);
      return (fpstate + 16 * (regnum - FP0_REGNUM));
    }
  else if (IS_SSE_REGNUM (regnum))
    {
      fpstate = ubase + ((char *) &u.i387.xmm_space - (char *) &u);
      return (fpstate + 16 * (regnum - XMM0_REGNUM));
    }
  else
    return (ubase + 8 * x86_64_regmap[regnum]);
}

void
_initialize_x86_64_linux_nat (void)
{
  add_core_fns (&linux_elf_core_fns);
}

int
kernel_u_size (void)
{
  return (sizeof (struct user));
}
