/* HP/UX native interface for HP 300's, for GDB when running under Unix.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.
   
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

/* Defining this means some system include files define some extra stuff.  */
#define WOPR
#include <sys/param.h>
#include <signal.h>
#include <sys/user.h>
#include <fcntl.h>

#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/trap.h>

#include <sys/file.h>

/* Get kernel_u_addr using HPUX-style nlist().  */
CORE_ADDR kernel_u_addr;

struct hpnlist {
        char *          n_name;
        long            n_value;
        unsigned char   n_type;
        unsigned char   n_length;
        short           n_almod;
        short           n_unused;
};
static struct hpnlist nl[] = {{ "_u", -1, }, { (char *) 0, }};

/* read the value of the u area from the hp-ux kernel */
void
_initialize_hp300ux_nat ()
{
#ifndef HPUX_VERSION_5
    nlist ("/hp-ux", nl);
    kernel_u_addr = nl[0].n_value;
#else /* HPUX version 5.  */
    kernel_u_addr = (CORE_ADDR) 0x0097900;
#endif
}

#define INFERIOR_AR0(u)							\
  ((ptrace								\
    (PT_RUAREA, inferior_pid,						\
     (PTRACE_ARG3_TYPE) ((char *) &u.u_ar0 - (char *) &u), 0, 0))		\
   - kernel_u_addr)

static void
fetch_inferior_register (regno, regaddr)
     register int regno;
     register unsigned int regaddr;
{
#ifndef HPUX_VERSION_5
  if (regno == PS_REGNUM)
    {
      union { int i; short s[2]; } ps_val;
      int regval;
      
      ps_val.i = (ptrace (PT_RUAREA, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
			  0, 0));
      regval = ps_val.s[0];
      supply_register (regno, (char *)&regval);
    }
  else
#endif /* not HPUX_VERSION_5 */
    {
      char buf[MAX_REGISTER_RAW_SIZE];
      register int i;
      
      for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (int))
	{
	  *(int *) &buf[i] = ptrace (PT_RUAREA, inferior_pid,
				     (PTRACE_ARG3_TYPE) regaddr, 0, 0);
	  regaddr += sizeof (int);
	}
      supply_register (regno, buf);
    }
  return;
}

static void
store_inferior_register_1 (regno, regaddr, val)
     int regno;
     unsigned int regaddr;
     int val;
{
  errno = 0;
  ptrace (PT_WUAREA, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, val, 0);
#if 0
  /* HP-UX randomly sets errno to non-zero for regno == 25.
     However, the value is correctly written, so ignore errno. */
  if (errno != 0)
    {
      char string_buf[64];
      
      sprintf (string_buf, "writing register number %d", regno);
      perror_with_name (string_buf);
    }
#endif
  return;
}

static void
store_inferior_register (regno, regaddr)
     register int regno;
     register unsigned int regaddr;
{
#ifndef HPUX_VERSION_5
  if (regno == PS_REGNUM)
    {
      union { int i; short s[2]; } ps_val;
      
      ps_val.i = (ptrace (PT_RUAREA, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
			  0, 0));
      ps_val.s[0] = (read_register (regno));
      store_inferior_register_1 (regno, regaddr, ps_val.i);
    }
  else
#endif /* not HPUX_VERSION_5 */
    {
      char buf[MAX_REGISTER_RAW_SIZE];
      register int i;
      extern char registers[];
      
      for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (int))
	{
	  store_inferior_register_1
	    (regno, regaddr,
	     (*(int *) &registers[(REGISTER_BYTE (regno)) + i]));
	  regaddr += sizeof (int);
	}
    }
  return;
}

void
fetch_inferior_registers (regno)
     int regno;
{
  struct user u;
  register unsigned int ar0_offset;
  
  ar0_offset = (INFERIOR_AR0 (u));
  if (regno == -1)
    {
      for (regno = 0; (regno < FP0_REGNUM); regno++)
	fetch_inferior_register (regno, (REGISTER_ADDR (ar0_offset, regno)));
      for (; (regno < NUM_REGS); regno++)
	fetch_inferior_register (regno, (FP_REGISTER_ADDR (u, regno)));
    }
  else
    fetch_inferior_register (regno,
			     (regno < FP0_REGNUM
			      ? REGISTER_ADDR (ar0_offset, regno)
			      : FP_REGISTER_ADDR (u, regno)));
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     register int regno;
{
  struct user u;
  register unsigned int ar0_offset;
  extern char registers[];

  if (regno >= FP0_REGNUM)
    {
      store_inferior_register (regno, (FP_REGISTER_ADDR (u, regno)));
      return;
    }
  
  ar0_offset = (INFERIOR_AR0 (u));
  if (regno >= 0)
    {
      store_inferior_register (regno, (REGISTER_ADDR (ar0_offset, regno)));
      return;
    }

  for (regno = 0; (regno < FP0_REGNUM); regno++)
    store_inferior_register (regno, (REGISTER_ADDR (ar0_offset, regno)));
  for (; (regno < NUM_REGS); regno++)
    store_inferior_register (regno, (FP_REGISTER_ADDR (u, regno)));
  return;
}

int
getpagesize ()
{
  return 4096;
}
