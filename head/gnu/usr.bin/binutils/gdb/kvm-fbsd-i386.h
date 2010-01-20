/* Kernel core dump functions below target vector, for GDB on FreeBSD/i386.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

__FBSDID("$FreeBSD$");

#include <machine/frame.h>

static CORE_ADDR
ksym_maxuseraddr (void)
{
  static CORE_ADDR maxuseraddr;
  struct minimal_symbol *sym;

  if (maxuseraddr == 0)
    {
      sym = lookup_minimal_symbol ("PTmap", NULL, NULL);
      if (sym == NULL) {
	maxuseraddr = VM_MAXUSER_ADDRESS;
      } else {
	maxuseraddr = SYMBOL_VALUE_ADDRESS (sym);
      }
    }
  return maxuseraddr;
}


/* Symbol names of kernel entry points.  Use special frames.  */
#define	KSYM_TRAP	"calltrap"
#define	KSYM_INTR	"Xintr"
#define	KSYM_FASTINTR	"Xfastintr"
#define	KSYM_OLDSYSCALL	"Xlcall_syscall"
#define	KSYM_SYSCALL	"Xint0x80_syscall"

/* The following is FreeBSD-specific hackery to decode special frames
   and elide the assembly-language stub.  This could be made faster by
   defining a frame_type field in the machine-dependent frame information,
   but we don't think that's too important right now.  */
enum frametype { tf_normal, tf_trap, tf_interrupt, tf_syscall };

CORE_ADDR
fbsd_kern_frame_saved_pc (struct frame_info *fi)
{
  struct minimal_symbol *sym;
  CORE_ADDR this_saved_pc;
  enum frametype frametype;

  this_saved_pc = read_memory_integer (fi->frame + 4, 4);
  sym = lookup_minimal_symbol_by_pc (this_saved_pc);
  frametype = tf_normal;
  if (sym != NULL)
    {
      if (strcmp (SYMBOL_NAME (sym), KSYM_TRAP) == 0)
	frametype = tf_trap;
      else
	if (strncmp (SYMBOL_NAME (sym), KSYM_INTR,
	    strlen (KSYM_INTR)) == 0 || strncmp (SYMBOL_NAME(sym),
	    KSYM_FASTINTR, strlen (KSYM_FASTINTR)) == 0)
	  frametype = tf_interrupt;
      else
	if (strcmp (SYMBOL_NAME (sym), KSYM_SYSCALL) == 0 ||
	    strcmp (SYMBOL_NAME (sym), KSYM_OLDSYSCALL) == 0)
	  frametype = tf_syscall;
    }

  switch (frametype)
    {
      case tf_normal:
        return (this_saved_pc);
#define oEIP   offsetof (struct trapframe, tf_eip)

      case tf_trap:
	return (read_memory_integer (fi->frame + 8 + oEIP, 4));

      case tf_interrupt:
	return (read_memory_integer (fi->frame + 12 + oEIP, 4));

      case tf_syscall:
	return (read_memory_integer (fi->frame + 8 + oEIP, 4));
#undef oEIP
    }
}

static void
fetch_kcore_registers (struct pcb *pcb)
{
  int i;
  int noreg;

  /* Get the register values out of the sys pcb and store them where
     `read_register' will find them.  */
  /*
   * XXX many registers aren't available.
   * XXX for the non-core case, the registers are stale - they are for
   *     the last context switch to the debugger.
   * XXX gcc's register numbers aren't all #defined in tm-i386.h.
   */
  noreg = 0;
  for (i = 0; i < 3; ++i)		/* eax,ecx,edx */
    supply_register (i, (char *)&noreg);

  supply_register (3, (char *) &pcb->pcb_ebx);
  supply_register (SP_REGNUM, (char *) &pcb->pcb_esp);
  supply_register (FP_REGNUM, (char *) &pcb->pcb_ebp);
  supply_register (6, (char *) &pcb->pcb_esi);
  supply_register (7, (char *) &pcb->pcb_edi);
  supply_register (PC_REGNUM, (char *) &pcb->pcb_eip);

  for (i = 9; i < 14; ++i)		/* eflags, cs, ss, ds, es, fs */
    supply_register (i, (char *) &noreg);
  supply_register (15, (char *) &pcb->pcb_gs);

  /* XXX 80387 registers?  */
}
