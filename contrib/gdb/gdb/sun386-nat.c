/* Native support for Sun 386i's for GDB, the GNU debugger.
   Copyright (C) 1986, 1987, 1989, 1991, 1992 Free Software Foundation, Inc.
   Changes for sun386i by Jean Daniel Fekete (jdf@litp.univ-p6-7.fr),
   C2V Paris, April 89.

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
#include "gdbcore.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/ptrace.h>
#include <machine/reg.h>

#include <sys/file.h>
#include "gdb_stat.h"
#include <sys/core.h>


/* Machine-dependent code which would otherwise be in corefile.c */
/* Work with core files, for GDB. */


void
core_file_command (filename, from_tty)
     char *filename;
     int from_tty;
{
  int val;
  extern char registers[];

  /* Discard all vestiges of any previous core file
     and mark data and stack spaces as empty.  */

  if (corefile)
    free (corefile);
  corefile = 0;

  if (corechan >= 0)
    close (corechan);
  corechan = -1;

  data_start = 0;
  data_end = 0;
  stack_start = STACK_END_ADDR;
  stack_end = STACK_END_ADDR;

  /* Now, if a new core file was specified, open it and digest it.  */

  if (filename)
    {
      filename = tilde_expand (filename);
      make_cleanup (free, filename);
      
      if (have_inferior_p ())
	error ("To look at a core file, you must kill the program with \"kill\".");
      corechan = open (filename, O_RDONLY, 0);
      if (corechan < 0)
	perror_with_name (filename);

      {
	struct core corestr;

	val = myread (corechan, &corestr, sizeof corestr);
	if (val < 0)
	  perror_with_name (filename);
	if (corestr.c_magic != CORE_MAGIC)
	  error ("\"%s\" does not appear to be a core dump file (magic 0x%x, expected 0x%x)",
		 filename, corestr.c_magic, (int) CORE_MAGIC);
	else if (sizeof (struct core) != corestr.c_len)
	  error ("\"%s\" has an invalid struct core length (%d, expected %d)",
		 filename, corestr.c_len, (int) sizeof (struct core));

	data_start = exec_data_start;
	data_end = data_start + corestr.c_dsize;
	stack_start = stack_end - corestr.c_ssize;
	data_offset = sizeof corestr;
	stack_offset = sizeof corestr + corestr.c_dsize;

	memcpy (registers, &corestr.c_regs, sizeof corestr.c_regs);

	memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	       corestr.c_fpu.f_fpstatus.f_st,
	       sizeof corestr.c_fpu.f_fpstatus.f_st);
	memcpy (&registers[REGISTER_BYTE (FPC_REGNUM)],
	       &corestr.c_fpu.f_fpstatus.f_ctrl,
	       sizeof corestr.c_fpu.f_fpstatus -
	       sizeof corestr.c_fpu.f_fpstatus.f_st);

	/* the struct aouthdr of sun coff is not the struct exec stored
	   in the core file. */
	memcpy (&core_aouthdr, &corestr.c_aouthdr, sizeof (struct exec));
#ifndef COFF_ENCAPSULATE
	core_aouthdr.magic = corestr.c_aouthdr.a_info;
	core_aouthdr.vstamp = /*SUNVERSION*/ 31252;
#endif
	printf_unfiltered ("Core file is from \"%s\".\n", corestr.c_cmdname);
	if (corestr.c_signo > 0)
	  printf_unfiltered ("Program terminated with signal %d, %s.\n",
		  corestr.c_signo, safe_strsignal (corestr.c_signo));
      }
      if (filename[0] == '/')
	corefile = savestring (filename, strlen (filename));
      else
	{
	  corefile = concat (current_directory, "/", filename, NULL);
	}

      flush_cached_frames ();
      select_frame (get_current_frame (), 0);

      validate_files ();
    }
  else if (from_tty)
    printf_unfiltered ("No core file now.\n");
}

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

void
fetch_inferior_registers (regno)
     int regno;
{
  struct regs inferior_registers;
  struct fp_state inferior_fp_registers;
  extern char registers[];

  registers_fetched ();

  ptrace (PTRACE_GETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers);
  ptrace (PTRACE_GETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers);

  memcpy (registers, &inferior_registers, sizeof inferior_registers);

  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],inferior_fp_registers.f_st,
	 sizeof inferior_fp_registers.f_st);
  memcpy (&registers[REGISTER_BYTE (FPC_REGNUM)],
	 &inferior_fp_registers.f_ctrl,
	 sizeof inferior_fp_registers - sizeof inferior_fp_registers.f_st);
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  struct regs inferior_registers;
  struct fp_state inferior_fp_registers;
  extern char registers[];

  memcpy (&inferior_registers, registers, 20 * 4);

  memcpy (inferior_fp_registers.f_st,
	 &registers[REGISTER_BYTE (FP0_REGNUM)],
	 sizeof inferior_fp_registers.f_st);
  memcpy (&inferior_fp_registers.f_ctrl,
	 &registers[REGISTER_BYTE (FPC_REGNUM)],
	 sizeof inferior_fp_registers - sizeof inferior_fp_registers.f_st);
  
#ifdef PTRACE_FP_BUG
  if (regno == FP_REGNUM || regno == -1)
    /* Storing the frame pointer requires a gross hack, in which an
       instruction that moves eax into ebp gets single-stepped.  */
    {
      int stack = inferior_registers.r_reg[SP_REGNUM];
      int stuff = ptrace (PTRACE_PEEKDATA, inferior_pid,
			  (PTRACE_ARG3_TYPE) stack);
      int reg = inferior_registers.r_reg[EAX];
      inferior_registers.r_reg[EAX] =
	inferior_registers.r_reg[FP_REGNUM];
      ptrace (PTRACE_SETREGS, inferior_pid, 
	      (PTRACE_ARG3_TYPE) &inferior_registers);
      ptrace (PTRACE_POKEDATA, inferior_pid, (PTRACE_ARG3_TYPE) stack,
	      0xc589);
      ptrace (PTRACE_SINGLESTEP, inferior_pid, (PTRACE_ARG3_TYPE) stack,
	      0);
      wait (0);
      ptrace (PTRACE_POKEDATA, inferior_pid, (PTRACE_ARG3_TYPE) stack,
	      stuff);
      inferior_registers.r_reg[EAX] = reg;
    }
#endif
  ptrace (PTRACE_SETREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_registers);
  ptrace (PTRACE_SETFPREGS, inferior_pid,
	  (PTRACE_ARG3_TYPE) &inferior_fp_registers);
}
