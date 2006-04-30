/* Kernel core dump functions below target vector, for GDB on FreeBSD/sparc64.
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

#define	SPARC_INTREG_SIZE	8

static void
fetch_kcore_registers (struct pcb *pcbp)
{
  static struct frame top;
  CORE_ADDR f_addr;
  int i;

  /* Get the register values out of the sys pcb and store them where
     `read_register' will find them.  */
  /*
   * XXX many registers aren't available.
   * XXX for the non-core case, the registers are stale - they are for
   *     the last context switch to the debugger.
   * XXX do something with the floating-point registers?
   */
  supply_register (SP_REGNUM, (char *)&pcbp->pcb_ufp);
  supply_register (PC_REGNUM, (char *)&pcbp->pcb_pc);
  f_addr = extract_address (&pcbp->pcb_ufp, SPARC_INTREG_SIZE);
  /* Load the previous frame by hand (XXX) and supply it. */
  read_memory (f_addr + SPOFF, (char *)&top, sizeof (top));
  for (i = 0; i < 8; i++)
    supply_register (i + L0_REGNUM, (char *)&top.fr_local[i]);
  for (i = 0; i < 8; i++)
    supply_register (i + I0_REGNUM, (char *)&top.fr_in[i]);
}

CORE_ADDR
fbsd_kern_frame_saved_pc (struct frame_info *fi)
{
  struct minimal_symbol *sym;
  CORE_ADDR frame, pc_addr, pc;
  char *buf;

  buf = alloca (MAX_REGISTER_RAW_SIZE);
  /* XXX: duplicates fi->extra_info->bottom. */
  frame = (fi->next != NULL) ? fi->next->frame : read_sp ();
  pc_addr = frame + offsetof (struct frame, fr_in[7]);

#define	READ_PC(pc, a, b) do { \
  read_memory (a, b, SPARC_INTREG_SIZE); \
  pc = extract_address (b, SPARC_INTREG_SIZE); \
} while (0)

  READ_PC (pc, pc_addr, buf);

  sym = lookup_minimal_symbol_by_pc (pc);
  if (sym != NULL)
    {
      if (strncmp (SYMBOL_NAME (sym), "tl0_", 4) == 0 ||
	  strcmp (SYMBOL_NAME (sym), "btext") == 0 ||
	  strcmp (SYMBOL_NAME (sym), "mp_startup") == 0 ||
	  strcmp (SYMBOL_NAME (sym), "fork_trampoline") == 0)
        {
	  /*
	   * Ugly kluge: user space addresses aren't separated from kernel
	   * ones by range; if encountering a trap from user space, just
	   * return a 0 to stop the trace.
	   * Do the same for entry points of kernel processes to avoid
	   * printing garbage.
	   */
	  pc = 0;
        }
      if (strncmp (SYMBOL_NAME (sym), "tl1_", 4) == 0)
        {
          pc_addr = fi->frame + sizeof (struct frame) +
	    offsetof (struct trapframe, tf_tpc);
          READ_PC (pc, pc_addr, buf);
	}
    }
  return (pc);
}
