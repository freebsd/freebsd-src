/* GNU/Linux on ARM native support.
   Copyright 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include "regcache.h"

#include "arm-tdep.h"

#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/utsname.h>
#include <sys/procfs.h>

/* Prototypes for supply_gregset etc. */
#include "gregset.h"

extern int arm_apcs_32;

#define		typeNone		0x00
#define		typeSingle		0x01
#define		typeDouble		0x02
#define		typeExtended		0x03
#define 	FPWORDS			28
#define		ARM_CPSR_REGNUM		16

typedef union tagFPREG
  {
    unsigned int fSingle;
    unsigned int fDouble[2];
    unsigned int fExtended[3];
  }
FPREG;

typedef struct tagFPA11
  {
    FPREG fpreg[8];		/* 8 floating point registers */
    unsigned int fpsr;		/* floating point status register */
    unsigned int fpcr;		/* floating point control register */
    unsigned char fType[8];	/* type of floating point value held in
				   floating point registers.  */
    int initflag;		/* NWFPE initialization flag.  */
  }
FPA11;

/* The following variables are used to determine the version of the
   underlying GNU/Linux operating system.  Examples:

   GNU/Linux 2.0.35             GNU/Linux 2.2.12
   os_version = 0x00020023      os_version = 0x0002020c
   os_major = 2                 os_major = 2
   os_minor = 0                 os_minor = 2
   os_release = 35              os_release = 12

   Note: os_version = (os_major << 16) | (os_minor << 8) | os_release

   These are initialized using get_linux_version() from
   _initialize_arm_linux_nat().  */

static unsigned int os_version, os_major, os_minor, os_release;

/* On GNU/Linux, threads are implemented as pseudo-processes, in which
   case we may be tracing more than one process at a time.  In that
   case, inferior_ptid will contain the main process ID and the
   individual thread (process) ID.  get_thread_id () is used to get
   the thread id if it's available, and the process id otherwise.  */

int
get_thread_id (ptid_t ptid)
{
  int tid = TIDGET (ptid);
  if (0 == tid)
    tid = PIDGET (ptid);
  return tid;
}
#define GET_THREAD_ID(PTID)	get_thread_id ((PTID));

static void
fetch_nwfpe_single (unsigned int fn, FPA11 * fpa11)
{
  unsigned int mem[3];

  mem[0] = fpa11->fpreg[fn].fSingle;
  mem[1] = 0;
  mem[2] = 0;
  supply_register (ARM_F0_REGNUM + fn, (char *) &mem[0]);
}

static void
fetch_nwfpe_double (unsigned int fn, FPA11 * fpa11)
{
  unsigned int mem[3];

  mem[0] = fpa11->fpreg[fn].fDouble[1];
  mem[1] = fpa11->fpreg[fn].fDouble[0];
  mem[2] = 0;
  supply_register (ARM_F0_REGNUM + fn, (char *) &mem[0]);
}

static void
fetch_nwfpe_none (unsigned int fn)
{
  unsigned int mem[3] =
  {0, 0, 0};

  supply_register (ARM_F0_REGNUM + fn, (char *) &mem[0]);
}

static void
fetch_nwfpe_extended (unsigned int fn, FPA11 * fpa11)
{
  unsigned int mem[3];

  mem[0] = fpa11->fpreg[fn].fExtended[0];	/* sign & exponent */
  mem[1] = fpa11->fpreg[fn].fExtended[2];	/* ls bits */
  mem[2] = fpa11->fpreg[fn].fExtended[1];	/* ms bits */
  supply_register (ARM_F0_REGNUM + fn, (char *) &mem[0]);
}

static void
fetch_nwfpe_register (int regno, FPA11 * fpa11)
{
   int fn = regno - ARM_F0_REGNUM;

   switch (fpa11->fType[fn])
     {
     case typeSingle:
       fetch_nwfpe_single (fn, fpa11);
       break;

     case typeDouble:
       fetch_nwfpe_double (fn, fpa11);
       break;

     case typeExtended:
       fetch_nwfpe_extended (fn, fpa11);
       break;

     default:
       fetch_nwfpe_none (fn);
     }
}

static void
store_nwfpe_single (unsigned int fn, FPA11 *fpa11)
{
  unsigned int mem[3];

  regcache_collect (ARM_F0_REGNUM + fn, (char *) &mem[0]);
  fpa11->fpreg[fn].fSingle = mem[0];
  fpa11->fType[fn] = typeSingle;
}

static void
store_nwfpe_double (unsigned int fn, FPA11 *fpa11)
{
  unsigned int mem[3];

  regcache_collect (ARM_F0_REGNUM + fn, (char *) &mem[0]);
  fpa11->fpreg[fn].fDouble[1] = mem[0];
  fpa11->fpreg[fn].fDouble[0] = mem[1];
  fpa11->fType[fn] = typeDouble;
}

void
store_nwfpe_extended (unsigned int fn, FPA11 *fpa11)
{
  unsigned int mem[3];

  regcache_collect (ARM_F0_REGNUM + fn, (char *) &mem[0]);
  fpa11->fpreg[fn].fExtended[0] = mem[0];	/* sign & exponent */
  fpa11->fpreg[fn].fExtended[2] = mem[1];	/* ls bits */
  fpa11->fpreg[fn].fExtended[1] = mem[2];	/* ms bits */
  fpa11->fType[fn] = typeDouble;
}

void
store_nwfpe_register (int regno, FPA11 * fpa11)
{
  if (register_cached (regno))
    {
       unsigned int fn = regno - ARM_F0_REGNUM;
       switch (fpa11->fType[fn])
         {
	 case typeSingle:
	   store_nwfpe_single (fn, fpa11);
	   break;

	 case typeDouble:
	   store_nwfpe_double (fn, fpa11);
	   break;

	 case typeExtended:
	   store_nwfpe_extended (fn, fpa11);
	   break;
	 }
    }
}


/* Get the value of a particular register from the floating point
   state of the process and store it into regcache.  */

static void
fetch_fpregister (int regno)
{
  int ret, tid;
  FPA11 fp;
  
  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);

  /* Read the floating point state.  */
  ret = ptrace (PT_GETFPREGS, tid, 0, &fp);
  if (ret < 0)
    {
      warning ("Unable to fetch floating point register.");
      return;
    }

  /* Fetch fpsr.  */
  if (ARM_FPS_REGNUM == regno)
    supply_register (ARM_FPS_REGNUM, (char *) &fp.fpsr);

  /* Fetch the floating point register.  */
  if (regno >= ARM_F0_REGNUM && regno <= ARM_F7_REGNUM)
    {
      int fn = regno - ARM_F0_REGNUM;

      switch (fp.fType[fn])
	{
	case typeSingle:
	  fetch_nwfpe_single (fn, &fp);
	  break;

	case typeDouble:
	    fetch_nwfpe_double (fn, &fp);
	  break;

	case typeExtended:
	    fetch_nwfpe_extended (fn, &fp);
	  break;

	default:
	    fetch_nwfpe_none (fn);
	}
    }
}

/* Get the whole floating point state of the process and store it
   into regcache.  */

static void
fetch_fpregs (void)
{
  int ret, regno, tid;
  FPA11 fp;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Read the floating point state.  */
  ret = ptrace (PT_GETFPREGS, tid, 0, &fp);
  if (ret < 0)
    {
      warning ("Unable to fetch the floating point registers.");
      return;
    }

  /* Fetch fpsr.  */
  supply_register (ARM_FPS_REGNUM, (char *) &fp.fpsr);

  /* Fetch the floating point registers.  */
  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    {
      int fn = regno - ARM_F0_REGNUM;

      switch (fp.fType[fn])
	{
	case typeSingle:
	  fetch_nwfpe_single (fn, &fp);
	  break;

	case typeDouble:
	  fetch_nwfpe_double (fn, &fp);
	  break;

	case typeExtended:
	  fetch_nwfpe_extended (fn, &fp);
	  break;

	default:
	  fetch_nwfpe_none (fn);
	}
    }
}

/* Save a particular register into the floating point state of the
   process using the contents from regcache.  */

static void
store_fpregister (int regno)
{
  int ret, tid;
  FPA11 fp;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Read the floating point state.  */
  ret = ptrace (PT_GETFPREGS, tid, 0, &fp);
  if (ret < 0)
    {
      warning ("Unable to fetch the floating point registers.");
      return;
    }

  /* Store fpsr.  */
  if (ARM_FPS_REGNUM == regno && register_cached (ARM_FPS_REGNUM))
    regcache_collect (ARM_FPS_REGNUM, (char *) &fp.fpsr);

  /* Store the floating point register.  */
  if (regno >= ARM_F0_REGNUM && regno <= ARM_F7_REGNUM)
    {
      store_nwfpe_register (regno, &fp);
    }

  ret = ptrace (PTRACE_SETFPREGS, tid, 0, &fp);
  if (ret < 0)
    {
      warning ("Unable to store floating point register.");
      return;
    }
}

/* Save the whole floating point state of the process using
   the contents from regcache.  */

static void
store_fpregs (void)
{
  int ret, regno, tid;
  FPA11 fp;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Read the floating point state.  */
  ret = ptrace (PT_GETFPREGS, tid, 0, &fp);
  if (ret < 0)
    {
      warning ("Unable to fetch the floating point registers.");
      return;
    }

  /* Store fpsr.  */
  if (register_cached (ARM_FPS_REGNUM))
    regcache_collect (ARM_FPS_REGNUM, (char *) &fp.fpsr);

  /* Store the floating point registers.  */
  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    {
      fetch_nwfpe_register (regno, &fp);
    }

  ret = ptrace (PTRACE_SETFPREGS, tid, 0, &fp);
  if (ret < 0)
    {
      warning ("Unable to store floating point registers.");
      return;
    }
}

/* Fetch a general register of the process and store into
   regcache.  */

static void
fetch_register (int regno)
{
  int ret, tid;
  elf_gregset_t regs;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  ret = ptrace (PTRACE_GETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning ("Unable to fetch general register.");
      return;
    }

  if (regno >= ARM_A1_REGNUM && regno < ARM_PC_REGNUM)
    supply_register (regno, (char *) &regs[regno]);

  if (ARM_PS_REGNUM == regno)
    {
      if (arm_apcs_32)
        supply_register (ARM_PS_REGNUM, (char *) &regs[ARM_CPSR_REGNUM]);
      else
        supply_register (ARM_PS_REGNUM, (char *) &regs[ARM_PC_REGNUM]);
    }
    
  if (ARM_PC_REGNUM == regno)
    { 
      regs[ARM_PC_REGNUM] = ADDR_BITS_REMOVE (regs[ARM_PC_REGNUM]);
      supply_register (ARM_PC_REGNUM, (char *) &regs[ARM_PC_REGNUM]);
    }
}

/* Fetch all general registers of the process and store into
   regcache.  */

static void
fetch_regs (void)
{
  int ret, regno, tid;
  elf_gregset_t regs;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  ret = ptrace (PTRACE_GETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning ("Unable to fetch general registers.");
      return;
    }

  for (regno = ARM_A1_REGNUM; regno < ARM_PC_REGNUM; regno++)
    supply_register (regno, (char *) &regs[regno]);

  if (arm_apcs_32)
    supply_register (ARM_PS_REGNUM, (char *) &regs[ARM_CPSR_REGNUM]);
  else
    supply_register (ARM_PS_REGNUM, (char *) &regs[ARM_PC_REGNUM]);

  regs[ARM_PC_REGNUM] = ADDR_BITS_REMOVE (regs[ARM_PC_REGNUM]);
  supply_register (ARM_PC_REGNUM, (char *) &regs[ARM_PC_REGNUM]);
}

/* Store all general registers of the process from the values in
   regcache.  */

static void
store_register (int regno)
{
  int ret, tid;
  elf_gregset_t regs;
  
  if (!register_cached (regno))
    return;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Get the general registers from the process.  */
  ret = ptrace (PTRACE_GETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning ("Unable to fetch general registers.");
      return;
    }

  if (regno >= ARM_A1_REGNUM && regno <= ARM_PC_REGNUM)
    regcache_collect (regno, (char *) &regs[regno]);

  ret = ptrace (PTRACE_SETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning ("Unable to store general register.");
      return;
    }
}

static void
store_regs (void)
{
  int ret, regno, tid;
  elf_gregset_t regs;

  /* Get the thread id for the ptrace call.  */
  tid = GET_THREAD_ID (inferior_ptid);
  
  /* Fetch the general registers.  */
  ret = ptrace (PTRACE_GETREGS, tid, 0, &regs);
  if (ret < 0)
    {
      warning ("Unable to fetch general registers.");
      return;
    }

  for (regno = ARM_A1_REGNUM; regno <= ARM_PC_REGNUM; regno++)
    {
      if (register_cached (regno))
	regcache_collect (regno, (char *) &regs[regno]);
    }

  ret = ptrace (PTRACE_SETREGS, tid, 0, &regs);

  if (ret < 0)
    {
      warning ("Unable to store general registers.");
      return;
    }
}

/* Fetch registers from the child process.  Fetch all registers if
   regno == -1, otherwise fetch all general registers or all floating
   point registers depending upon the value of regno.  */

void
fetch_inferior_registers (int regno)
{
  if (-1 == regno)
    {
      fetch_regs ();
      fetch_fpregs ();
    }
  else 
    {
      if (regno < ARM_F0_REGNUM || regno > ARM_FPS_REGNUM)
        fetch_register (regno);

      if (regno >= ARM_F0_REGNUM && regno <= ARM_FPS_REGNUM)
        fetch_fpregister (regno);
    }
}

/* Store registers back into the inferior.  Store all registers if
   regno == -1, otherwise store all general registers or all floating
   point registers depending upon the value of regno.  */

void
store_inferior_registers (int regno)
{
  if (-1 == regno)
    {
      store_regs ();
      store_fpregs ();
    }
  else
    {
      if ((regno < ARM_F0_REGNUM) || (regno > ARM_FPS_REGNUM))
        store_register (regno);

      if ((regno >= ARM_F0_REGNUM) && (regno <= ARM_FPS_REGNUM))
        store_fpregister (regno);
    }
}

/* Fill register regno (if it is a general-purpose register) in
   *gregsetp with the appropriate value from GDB's register array.
   If regno is -1, do this for all registers.  */

void
fill_gregset (gdb_gregset_t *gregsetp, int regno)
{
  if (-1 == regno)
    {
      int regnum;
      for (regnum = ARM_A1_REGNUM; regnum <= ARM_PC_REGNUM; regnum++) 
	regcache_collect (regnum, (char *) &(*gregsetp)[regnum]);
    }
  else if (regno >= ARM_A1_REGNUM && regno <= ARM_PC_REGNUM)
    regcache_collect (regno, (char *) &(*gregsetp)[regno]);

  if (ARM_PS_REGNUM == regno || -1 == regno)
    {
      if (arm_apcs_32)
	regcache_collect (ARM_PS_REGNUM,
			  (char *) &(*gregsetp)[ARM_CPSR_REGNUM]);
      else
	regcache_collect (ARM_PC_REGNUM,
			  (char *) &(*gregsetp)[ARM_PC_REGNUM]);
    }
}

/* Fill GDB's register array with the general-purpose register values
   in *gregsetp.  */

void
supply_gregset (gdb_gregset_t *gregsetp)
{
  int regno, reg_pc;

  for (regno = ARM_A1_REGNUM; regno < ARM_PC_REGNUM; regno++)
    supply_register (regno, (char *) &(*gregsetp)[regno]);

  if (arm_apcs_32)
    supply_register (ARM_PS_REGNUM, (char *) &(*gregsetp)[ARM_CPSR_REGNUM]);
  else
    supply_register (ARM_PS_REGNUM, (char *) &(*gregsetp)[ARM_PC_REGNUM]);

  reg_pc = ADDR_BITS_REMOVE ((CORE_ADDR)(*gregsetp)[ARM_PC_REGNUM]);
  supply_register (ARM_PC_REGNUM, (char *) &reg_pc);
}

/* Fill register regno (if it is a floating-point register) in
   *fpregsetp with the appropriate value from GDB's register array.
   If regno is -1, do this for all registers.  */

void
fill_fpregset (gdb_fpregset_t *fpregsetp, int regno)
{
  FPA11 *fp = (FPA11 *) fpregsetp;
  
  if (-1 == regno)
    {
       int regnum;
       for (regnum = ARM_F0_REGNUM; regnum <= ARM_F7_REGNUM; regnum++)
         store_nwfpe_register (regnum, fp);
    }
  else if (regno >= ARM_F0_REGNUM && regno <= ARM_F7_REGNUM)
    {
      store_nwfpe_register (regno, fp);
      return;
    }

  /* Store fpsr.  */
  if (ARM_FPS_REGNUM == regno || -1 == regno)
    regcache_collect (ARM_FPS_REGNUM, (char *) &fp->fpsr);
}

/* Fill GDB's register array with the floating-point register values
   in *fpregsetp.  */

void
supply_fpregset (gdb_fpregset_t *fpregsetp)
{
  int regno;
  FPA11 *fp = (FPA11 *) fpregsetp;

  /* Fetch fpsr.  */
  supply_register (ARM_FPS_REGNUM, (char *) &fp->fpsr);

  /* Fetch the floating point registers.  */
  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    {
      fetch_nwfpe_register (regno, fp);
    }
}

int
arm_linux_kernel_u_size (void)
{
  return (sizeof (struct user));
}

static unsigned int
get_linux_version (unsigned int *vmajor,
		   unsigned int *vminor,
		   unsigned int *vrelease)
{
  struct utsname info;
  char *pmajor, *pminor, *prelease, *tail;

  if (-1 == uname (&info))
    {
      warning ("Unable to determine GNU/Linux version.");
      return -1;
    }

  pmajor = strtok (info.release, ".");
  pminor = strtok (NULL, ".");
  prelease = strtok (NULL, ".");

  *vmajor = (unsigned int) strtoul (pmajor, &tail, 0);
  *vminor = (unsigned int) strtoul (pminor, &tail, 0);
  *vrelease = (unsigned int) strtoul (prelease, &tail, 0);

  return ((*vmajor << 16) | (*vminor << 8) | *vrelease);
}

void
_initialize_arm_linux_nat (void)
{
  os_version = get_linux_version (&os_major, &os_minor, &os_release);
}
