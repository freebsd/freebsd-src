/* PPC GNU/Linux native support.
   Copyright 1988, 1989, 1991, 1992, 1994, 1996, 2000, 2001, 2002
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
#include "frame.h"
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>

/* Prototypes for supply_gregset etc. */
#include "gregset.h"
#include "ppc-tdep.h"

#ifndef PT_READ_U
#define PT_READ_U PTRACE_PEEKUSR
#endif
#ifndef PT_WRITE_U
#define PT_WRITE_U PTRACE_POKEUSR
#endif

/* Default the type of the ptrace transfer to int.  */
#ifndef PTRACE_XFER_TYPE
#define PTRACE_XFER_TYPE int
#endif

/* Glibc's headers don't define PTRACE_GETVRREGS so we cannot use a
   configure time check.  Some older glibc's (for instance 2.2.1)
   don't have a specific powerpc version of ptrace.h, and fall back on
   a generic one.  In such cases, sys/ptrace.h defines
   PTRACE_GETFPXREGS and PTRACE_SETFPXREGS to the same numbers that
   ppc kernel's asm/ptrace.h defines PTRACE_GETVRREGS and
   PTRACE_SETVRREGS to be.  This also makes a configury check pretty
   much useless.  */

/* These definitions should really come from the glibc header files,
   but Glibc doesn't know about the vrregs yet.  */
#ifndef PTRACE_GETVRREGS
#define PTRACE_GETVRREGS 18
#define PTRACE_SETVRREGS 19
#endif

/* This oddity is because the Linux kernel defines elf_vrregset_t as
   an array of 33 16 bytes long elements.  I.e. it leaves out vrsave.
   However the PTRACE_GETVRREGS and PTRACE_SETVRREGS requests return
   the vrsave as an extra 4 bytes at the end.  I opted for creating a
   flat array of chars, so that it is easier to manipulate for gdb.

   There are 32 vector registers 16 bytes longs, plus a VSCR register
   which is only 4 bytes long, but is fetched as a 16 bytes
   quantity. Up to here we have the elf_vrregset_t structure.
   Appended to this there is space for the VRSAVE register: 4 bytes.
   Even though this vrsave register is not included in the regset
   typedef, it is handled by the ptrace requests.

   Note that GNU/Linux doesn't support little endian PPC hardware,
   therefore the offset at which the real value of the VSCR register
   is located will be always 12 bytes.

   The layout is like this (where x is the actual value of the vscr reg): */

/* *INDENT-OFF* */
/*
   |.|.|.|.|.....|.|.|.|.||.|.|.|x||.|
   <------->     <-------><-------><->
     VR0           VR31     VSCR    VRSAVE
*/
/* *INDENT-ON* */

#define SIZEOF_VRREGS 33*16+4

typedef char gdb_vrregset_t[SIZEOF_VRREGS];

/* For runtime check of ptrace support for VRREGS.  */
int have_ptrace_getvrregs = 1;

int
kernel_u_size (void)
{
  return (sizeof (struct user));
}

/* *INDENT-OFF* */
/* registers layout, as presented by the ptrace interface:
PT_R0, PT_R1, PT_R2, PT_R3, PT_R4, PT_R5, PT_R6, PT_R7,
PT_R8, PT_R9, PT_R10, PT_R11, PT_R12, PT_R13, PT_R14, PT_R15,
PT_R16, PT_R17, PT_R18, PT_R19, PT_R20, PT_R21, PT_R22, PT_R23,
PT_R24, PT_R25, PT_R26, PT_R27, PT_R28, PT_R29, PT_R30, PT_R31,
PT_FPR0, PT_FPR0 + 2, PT_FPR0 + 4, PT_FPR0 + 6, PT_FPR0 + 8, PT_FPR0 + 10, PT_FPR0 + 12, PT_FPR0 + 14,
PT_FPR0 + 16, PT_FPR0 + 18, PT_FPR0 + 20, PT_FPR0 + 22, PT_FPR0 + 24, PT_FPR0 + 26, PT_FPR0 + 28, PT_FPR0 + 30,
PT_FPR0 + 32, PT_FPR0 + 34, PT_FPR0 + 36, PT_FPR0 + 38, PT_FPR0 + 40, PT_FPR0 + 42, PT_FPR0 + 44, PT_FPR0 + 46,
PT_FPR0 + 48, PT_FPR0 + 50, PT_FPR0 + 52, PT_FPR0 + 54, PT_FPR0 + 56, PT_FPR0 + 58, PT_FPR0 + 60, PT_FPR0 + 62,
PT_NIP, PT_MSR, PT_CCR, PT_LNK, PT_CTR, PT_XER, PT_MQ */
/* *INDENT_ON * */

static int
ppc_register_u_addr (int regno)
{
  int u_addr = -1;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  /* General purpose registers occupy 1 slot each in the buffer */
  if (regno >= tdep->ppc_gp0_regnum && regno <= tdep->ppc_gplast_regnum )
    u_addr =  ((PT_R0 + regno) * 4);

  /* Floating point regs: 2 slots each */
  if (regno >= FP0_REGNUM && regno <= FPLAST_REGNUM)
    u_addr = ((PT_FPR0 + (regno - FP0_REGNUM) * 2) * 4);

  /* UISA special purpose registers: 1 slot each */
  if (regno == PC_REGNUM)
    u_addr = PT_NIP * 4;
  if (regno == tdep->ppc_lr_regnum)
    u_addr = PT_LNK * 4;
  if (regno == tdep->ppc_cr_regnum)
    u_addr = PT_CCR * 4;
  if (regno == tdep->ppc_xer_regnum)
    u_addr = PT_XER * 4;
  if (regno == tdep->ppc_ctr_regnum)
    u_addr = PT_CTR * 4;
  if (regno == tdep->ppc_mq_regnum)
    u_addr = PT_MQ * 4;
  if (regno == tdep->ppc_ps_regnum)
    u_addr = PT_MSR * 4;

  return u_addr;
}

static int
ppc_ptrace_cannot_fetch_store_register (int regno)
{
  return (ppc_register_u_addr (regno) == -1);
}

/* The Linux kernel ptrace interface for AltiVec registers uses the
   registers set mechanism, as opposed to the interface for all the
   other registers, that stores/fetches each register individually.  */
static void
fetch_altivec_register (int tid, int regno)
{
  int ret;
  int offset = 0;
  gdb_vrregset_t regs;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int vrregsize = REGISTER_RAW_SIZE (tdep->ppc_vr0_regnum);

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name ("Unable to fetch AltiVec register");
    }
 
  /* VSCR is fetched as a 16 bytes quantity, but it is really 4 bytes
     long on the hardware.  We deal only with the lower 4 bytes of the
     vector.  VRSAVE is at the end of the array in a 4 bytes slot, so
     there is no need to define an offset for it.  */
  if (regno == (tdep->ppc_vrsave_regnum - 1))
    offset = vrregsize - REGISTER_RAW_SIZE (tdep->ppc_vrsave_regnum);
  
  supply_register (regno,
                   regs + (regno - tdep->ppc_vr0_regnum) * vrregsize + offset);
}

static void
fetch_register (int tid, int regno)
{
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  char mess[128];              /* For messages */
  register int i;
  unsigned int offset;         /* Offset of registers within the u area. */
  char *buf = alloca (MAX_REGISTER_RAW_SIZE);
  CORE_ADDR regaddr = ppc_register_u_addr (regno);

  if (altivec_register_p (regno))
    {
      /* If this is the first time through, or if it is not the first
         time through, and we have comfirmed that there is kernel
         support for such a ptrace request, then go and fetch the
         register.  */
      if (have_ptrace_getvrregs)
       {
         fetch_altivec_register (tid, regno);
         return;
       }
     /* If we have discovered that there is no ptrace support for
        AltiVec registers, fall through and return zeroes, because
        regaddr will be -1 in this case.  */
    }

  if (regaddr == -1)
    {
      memset (buf, '\0', REGISTER_RAW_SIZE (regno));   /* Supply zeroes */
      supply_register (regno, buf);
      return;
    }

  for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      *(PTRACE_XFER_TYPE *) & buf[i] = ptrace (PT_READ_U, tid,
					       (PTRACE_ARG3_TYPE) regaddr, 0);
      regaddr += sizeof (PTRACE_XFER_TYPE);
      if (errno != 0)
	{
	  sprintf (mess, "reading register %s (#%d)", 
		   REGISTER_NAME (regno), regno);
	  perror_with_name (mess);
	}
    }
  supply_register (regno, buf);
}

static void
supply_vrregset (gdb_vrregset_t *vrregsetp)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int num_of_vrregs = tdep->ppc_vrsave_regnum - tdep->ppc_vr0_regnum + 1;
  int vrregsize = REGISTER_RAW_SIZE (tdep->ppc_vr0_regnum);
  int offset = vrregsize - REGISTER_RAW_SIZE (tdep->ppc_vrsave_regnum);

  for (i = 0; i < num_of_vrregs; i++)
    {
      /* The last 2 registers of this set are only 32 bit long, not
         128.  However an offset is necessary only for VSCR because it
         occupies a whole vector, while VRSAVE occupies a full 4 bytes
         slot.  */
      if (i == (num_of_vrregs - 2))
        supply_register (tdep->ppc_vr0_regnum + i,
                         *vrregsetp + i * vrregsize + offset);
      else
        supply_register (tdep->ppc_vr0_regnum + i, *vrregsetp + i * vrregsize);
    }
}

static void
fetch_altivec_registers (int tid)
{
  int ret;
  gdb_vrregset_t regs;
  
  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
	{
          have_ptrace_getvrregs = 0;
	  return;
	}
      perror_with_name ("Unable to fetch AltiVec registers");
    }
  supply_vrregset (&regs);
}

static void 
fetch_ppc_registers (int tid)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  for (i = 0; i <= tdep->ppc_mq_regnum; i++)
    fetch_register (tid, i);
  if (have_ptrace_getvrregs)
    if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
      fetch_altivec_registers (tid);
}

/* Fetch registers from the child process.  Fetch all registers if
   regno == -1, otherwise fetch all general registers or all floating
   point registers depending upon the value of regno.  */
void
fetch_inferior_registers (int regno)
{
  /* Overload thread id onto process id */
  int tid = TIDGET (inferior_ptid);

  /* No thread id, just use process id */
  if (tid == 0)
    tid = PIDGET (inferior_ptid);

  if (regno == -1)
    fetch_ppc_registers (tid);
  else 
    fetch_register (tid, regno);
}

/* Store one register. */
static void
store_altivec_register (int tid, int regno)
{
  int ret;
  int offset = 0;
  gdb_vrregset_t regs;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int vrregsize = REGISTER_RAW_SIZE (tdep->ppc_vr0_regnum);

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name ("Unable to fetch AltiVec register");
    }

  /* VSCR is fetched as a 16 bytes quantity, but it is really 4 bytes
     long on the hardware.  */
  if (regno == (tdep->ppc_vrsave_regnum - 1))
    offset = vrregsize - REGISTER_RAW_SIZE (tdep->ppc_vrsave_regnum);

  regcache_collect (regno,
                    regs + (regno - tdep->ppc_vr0_regnum) * vrregsize + offset);

  ret = ptrace (PTRACE_SETVRREGS, tid, 0, &regs);
  if (ret < 0)
    perror_with_name ("Unable to store AltiVec register");
}

static void
store_register (int tid, int regno)
{
  /* This isn't really an address.  But ptrace thinks of it as one.  */
  CORE_ADDR regaddr = ppc_register_u_addr (regno);
  char mess[128];              /* For messages */
  register int i;
  unsigned int offset;         /* Offset of registers within the u area.  */
  char *buf = alloca (MAX_REGISTER_RAW_SIZE);

  if (altivec_register_p (regno))
    {
      store_altivec_register (tid, regno);
      return;
    }

  if (regaddr == -1)
    return;

  regcache_collect (regno, buf);
  for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (PTRACE_XFER_TYPE))
    {
      errno = 0;
      ptrace (PT_WRITE_U, tid, (PTRACE_ARG3_TYPE) regaddr,
	      *(PTRACE_XFER_TYPE *) & buf[i]);
      regaddr += sizeof (PTRACE_XFER_TYPE);
      if (errno != 0)
	{
	  sprintf (mess, "writing register %s (#%d)", 
		   REGISTER_NAME (regno), regno);
	  perror_with_name (mess);
	}
    }
}

static void
fill_vrregset (gdb_vrregset_t *vrregsetp)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int num_of_vrregs = tdep->ppc_vrsave_regnum - tdep->ppc_vr0_regnum + 1;
  int vrregsize = REGISTER_RAW_SIZE (tdep->ppc_vr0_regnum);
  int offset = vrregsize - REGISTER_RAW_SIZE (tdep->ppc_vrsave_regnum);

  for (i = 0; i < num_of_vrregs; i++)
    {
      /* The last 2 registers of this set are only 32 bit long, not
         128, but only VSCR is fetched as a 16 bytes quantity.  */
      if (i == (num_of_vrregs - 2))
        regcache_collect (tdep->ppc_vr0_regnum + i,
                          *vrregsetp + i * vrregsize + offset);
      else
        regcache_collect (tdep->ppc_vr0_regnum + i, *vrregsetp + i * vrregsize);
    }
}

static void
store_altivec_registers (int tid)
{
  int ret;
  gdb_vrregset_t regs;

  ret = ptrace (PTRACE_GETVRREGS, tid, 0, (int) &regs);
  if (ret < 0)
    {
      if (errno == EIO)
        {
          have_ptrace_getvrregs = 0;
          return;
        }
      perror_with_name ("Couldn't get AltiVec registers");
    }

  fill_vrregset (&regs);
  
  if (ptrace (PTRACE_SETVRREGS, tid, 0, (int) &regs) < 0)
    perror_with_name ("Couldn't write AltiVec registers");
}

static void
store_ppc_registers (int tid)
{
  int i;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  
  for (i = 0; i <= tdep->ppc_mq_regnum; i++)
    store_register (tid, i);
  if (have_ptrace_getvrregs)
    if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
      store_altivec_registers (tid);
}

void
store_inferior_registers (int regno)
{
  /* Overload thread id onto process id */
  int tid = TIDGET (inferior_ptid);

  /* No thread id, just use process id */
  if (tid == 0)
    tid = PIDGET (inferior_ptid);

  if (regno >= 0)
    store_register (tid, regno);
  else
    store_ppc_registers (tid);
}

void
supply_gregset (gdb_gregset_t *gregsetp)
{
  int regi;
  register elf_greg_t *regp = (elf_greg_t *) gregsetp;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch); 

  for (regi = 0; regi < 32; regi++)
    supply_register (regi, (char *) (regp + regi));

  supply_register (PC_REGNUM, (char *) (regp + PT_NIP));
  supply_register (tdep->ppc_lr_regnum, (char *) (regp + PT_LNK));
  supply_register (tdep->ppc_cr_regnum, (char *) (regp + PT_CCR));
  supply_register (tdep->ppc_xer_regnum, (char *) (regp + PT_XER));
  supply_register (tdep->ppc_ctr_regnum, (char *) (regp + PT_CTR));
  supply_register (tdep->ppc_mq_regnum, (char *) (regp + PT_MQ));
  supply_register (tdep->ppc_ps_regnum, (char *) (regp + PT_MSR));
}

void
fill_gregset (gdb_gregset_t *gregsetp, int regno)
{
  int regi;
  elf_greg_t *regp = (elf_greg_t *) gregsetp;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch); 

  for (regi = 0; regi < 32; regi++)
    {
      if ((regno == -1) || regno == regi)
        regcache_collect (regi, regp + PT_R0 + regi);
    }

  if ((regno == -1) || regno == PC_REGNUM)
    regcache_collect (PC_REGNUM, regp + PT_NIP);
  if ((regno == -1) || regno == tdep->ppc_lr_regnum)
    regcache_collect (tdep->ppc_lr_regnum, regp + PT_LNK);
  if ((regno == -1) || regno == tdep->ppc_cr_regnum)
    regcache_collect (tdep->ppc_cr_regnum, regp + PT_CCR);
  if ((regno == -1) || regno == tdep->ppc_xer_regnum)
    regcache_collect (tdep->ppc_xer_regnum, regp + PT_XER);
  if ((regno == -1) || regno == tdep->ppc_ctr_regnum)
    regcache_collect (tdep->ppc_ctr_regnum, regp + PT_CTR);
  if ((regno == -1) || regno == tdep->ppc_mq_regnum)
    regcache_collect (tdep->ppc_mq_regnum, regp + PT_MQ);
  if ((regno == -1) || regno == tdep->ppc_ps_regnum)
    regcache_collect (tdep->ppc_ps_regnum, regp + PT_MSR);
}

void
supply_fpregset (gdb_fpregset_t * fpregsetp)
{
  int regi;

  for (regi = 0; regi < 32; regi++)
    supply_register (FP0_REGNUM + regi, (char *) (*fpregsetp + regi));
}

/* Given a pointer to a floating point register set in /proc format
   (fpregset_t *), update the register specified by REGNO from gdb's
   idea of the current floating point register set.  If REGNO is -1,
   update them all.  */
void
fill_fpregset (gdb_fpregset_t *fpregsetp, int regno)
{
  int regi;
  
  for (regi = 0; regi < 32; regi++)
    {
      if ((regno == -1) || (regno == FP0_REGNUM + regi))
	regcache_collect (FP0_REGNUM + regi, (char *) (*fpregsetp + regi));
    }
}
