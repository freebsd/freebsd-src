/* Low level Pyramid interface to ptrace, for GDB when running under Unix.
   Copyright (C) 1988, 1989, 1991 Free Software Foundation, Inc.

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

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/ioctl.h>
/* #include <fcntl.h>  Can we live without this?  */

#include "gdbcore.h"
#include <sys/user.h>		/* After a.out.h  */
#include <sys/file.h>
#include "gdb_stat.h"


void
fetch_inferior_registers (regno)
     int regno;
{
  register int datum;
  register unsigned int regaddr;
  int reg_buf[NUM_REGS+1];
  struct user u;
  register int skipped_frames = 0;

  registers_fetched ();
  
  for (regno = 0; regno < 64; regno++) {
    reg_buf[regno] = ptrace (3, inferior_pid, (PTRACE_ARG3_TYPE) regno, 0);
    
#if defined(PYRAMID_CONTROL_FRAME_DEBUGGING)
    printf_unfiltered ("Fetching register %s, got %0x\n",
	    reg_names[regno],
	    reg_buf[regno]);
#endif /* PYRAMID_CONTROL_FRAME_DEBUGGING */
    
    if (reg_buf[regno] == -1 && errno == EIO) {
      printf_unfiltered("fetch_interior_registers: fetching register %s\n",
	     reg_names[regno]);
      errno = 0;
    }
    supply_register (regno, reg_buf+regno);
  }
  /* that leaves regs 64, 65, and 66 */
  datum = ptrace (3, inferior_pid,
		  (PTRACE_ARG3_TYPE) (((char *)&u.u_pcb.pcb_csp) -
		  ((char *)&u)), 0);
  
  
  
  /* FIXME: Find the Current Frame Pointer (CFP). CFP is a global
     register (ie, NOT windowed), that gets saved in a frame iff
     the code for that frame has a prologue (ie, "adsf N").  If
     there is a prologue, the adsf insn saves the old cfp in
     pr13, cfp is set to sp, and N bytes of locals are allocated
     (sp is decremented by n).
     This makes finding CFP hard. I guess the right way to do it
     is: 
     - If this is the innermost frame, believe ptrace() or
     the core area.
     - Otherwise:
     Find the first insn of the current frame.
     - find the saved pc;
     - find the call insn that saved it;
     - figure out where the call is to;
     - if the first insn is an adsf, we got a frame
     pointer. */
  
  
  /* Normal processors have separate stack pointers for user and
     kernel mode. Getting the last user mode frame on such
     machines is easy: the kernel context of the ptrace()'d
     process is on the kernel stack, and the USP points to what
     we want. But Pyramids only have a single cfp for both user and
     kernel mode.  And processes being ptrace()'d have some
     kernel-context control frames on their stack.
     To avoid tracing back into the kernel context of an inferior,
     we skip 0 or more contiguous control frames where the pc is
     in the kernel. */ 
  
  while (1) {
    register int inferior_saved_pc;
    inferior_saved_pc = ptrace (1, inferior_pid,
				(PTRACE_ARG3_TYPE) (datum+((32+15)*4)), 0);
    if (inferior_saved_pc > 0) break;
#if defined(PYRAMID_CONTROL_FRAME_DEBUGGING)
    printf_unfiltered("skipping kernel frame %08x, pc=%08x\n", datum,
	   inferior_saved_pc);
#endif /* PYRAMID_CONTROL_FRAME_DEBUGGING */
    skipped_frames++;
    datum -= CONTROL_STACK_FRAME_SIZE;
  }
  
  reg_buf[CSP_REGNUM] = datum;
  supply_register(CSP_REGNUM, reg_buf+CSP_REGNUM);
#ifdef  PYRAMID_CONTROL_FRAME_DEBUGGING
  if (skipped_frames) {
    fprintf_unfiltered (stderr,
	     "skipped %d frames from %x to %x; cfp was %x, now %x\n",
	     skipped_frames, reg_buf[CSP_REGNUM]);
  }
#endif /* PYRAMID_CONTROL_FRAME_DEBUGGING */
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  register unsigned int regaddr;
  char buf[80];

  if (regno >= 0)
    {
      if ((0 <= regno) && (regno < 64)) {
	/*regaddr = register_addr (regno, offset);*/
	regaddr = regno;
	errno = 0;
	ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
		read_register (regno));
	if (errno != 0)
	  {
	    sprintf (buf, "writing register number %d", regno);
	    perror_with_name (buf);
	  }
      }
    }
  else
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	{
	  /*regaddr = register_addr (regno, offset);*/
	  regaddr = regno;
	  errno = 0;
	  ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr,
		  read_register (regno));
	  if (errno != 0)
	    {
	      sprintf (buf, "writing all regs, number %d", regno);
	      perror_with_name (buf);
	    }
	}
}

/*** Extensions to  core and dump files, for GDB. */

extern unsigned int last_frame_offset;

#ifdef PYRAMID_CORE

/* Can't make definitions here static, since corefile.c needs them
   to do bounds checking on the core-file areas. O well. */

/* have two stacks: one for data, one for register windows. */
extern CORE_ADDR reg_stack_start;
extern CORE_ADDR reg_stack_end;

/* need this so we can find the global registers: they never get saved. */
CORE_ADDR global_reg_offset;
static CORE_ADDR last_frame_address;
CORE_ADDR last_frame_offset;


/* Address in core file of start of register window stack area.
   Don't know if is this any of meaningful, useful or necessary.   */
extern int reg_stack_offset;

#endif /* PYRAMID_CORE */  


/* Work with core dump and executable files, for GDB. 
   This code would be in corefile.c if it weren't machine-dependent. */

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

#ifdef PYRAMID_CORE
  reg_stack_start = CONTROL_STACK_ADDR;
  reg_stack_end = CONTROL_STACK_ADDR;	/* this isn't strictly true...*/
#endif /* PYRAMID_CORE */

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
      /* 4.2-style (and perhaps also sysV-style) core dump file.  */
      {
	struct user u;

	unsigned int reg_offset;

	val = myread (corechan, &u, sizeof u);
	if (val < 0)
	  perror_with_name ("Not a core file: reading upage");
	if (val != sizeof u)
	  error ("Not a core file: could only read %d bytes", val);
	data_start = exec_data_start;

	data_end = data_start + NBPG * u.u_dsize;
	data_offset = NBPG * UPAGES;
	stack_offset = NBPG * (UPAGES + u.u_dsize);

	/* find registers in core file */
#ifdef PYRAMID_PTRACE
	stack_start = stack_end - NBPG * u.u_ussize;
	reg_stack_offset = stack_offset + (NBPG *u.u_ussize);
	reg_stack_end = reg_stack_start + NBPG * u.u_cssize;

	last_frame_address = ((int) u.u_pcb.pcb_csp);
	last_frame_offset = reg_stack_offset + last_frame_address
		- CONTROL_STACK_ADDR ;
	global_reg_offset = (char *)&u - (char *)&u.u_pcb.pcb_gr0 ;

	/* skip any control-stack frames that were executed in the
	   kernel. */

	while (1) {
	    char buf[4];
	    val = lseek (corechan, last_frame_offset+(47*4), 0);
	    if (val < 0)
		    perror_with_name (filename);
	    val = myread (corechan, buf, sizeof buf);
	    if (val < 0)
		    perror_with_name (filename);

	    if (*(int *)buf >= 0)
		    break;
	    printf_unfiltered ("skipping frame %s\n", local_hex_string (last_frame_address));
	    last_frame_offset -= CONTROL_STACK_FRAME_SIZE;
	    last_frame_address -= CONTROL_STACK_FRAME_SIZE;
	}
	reg_offset = last_frame_offset;

#if 1 || defined(PYRAMID_CONTROL_FRAME_DEBUGGING)
	printf_unfiltered ("Control stack pointer = %s\n",
		local_hex_string (u.u_pcb.pcb_csp));
	printf_unfiltered ("offset to control stack %d outermost frame %d (%s)\n",
	      reg_stack_offset, reg_offset, local_hex_string (last_frame_address));
#endif /* PYRAMID_CONTROL_FRAME_DEBUGGING */

#else /* not PYRAMID_CORE */
	stack_start = stack_end - NBPG * u.u_ssize;
        reg_offset = (int) u.u_ar0 - KERNEL_U_ADDR;
#endif /* not PYRAMID_CORE */

#ifdef __not_on_pyr_yet
	/* Some machines put an absolute address in here and some put
	   the offset in the upage of the regs.  */
	reg_offset = (int) u.u_ar0;
	if (reg_offset > NBPG * UPAGES)
	  reg_offset -= KERNEL_U_ADDR;
#endif

	/* I don't know where to find this info.
	   So, for now, mark it as not available.  */
	N_SET_MAGIC (core_aouthdr, 0);

	/* Read the register values out of the core file and store
	   them where `read_register' will find them.  */

	{
	  register int regno;

	  for (regno = 0; regno < 64; regno++)
	    {
	      char buf[MAX_REGISTER_RAW_SIZE];

	      val = lseek (corechan, register_addr (regno, reg_offset), 0);
	      if (val < 0
		  || (val = myread (corechan, buf, sizeof buf)) < 0)
		{
		  char * buffer = (char *) alloca (strlen (reg_names[regno])
						   + 30);
		  strcpy (buffer, "Reading register ");
		  strcat (buffer, reg_names[regno]);
						   
		  perror_with_name (buffer);
		}

	      if (val < 0)
		perror_with_name (filename);
#ifdef PYRAMID_CONTROL_FRAME_DEBUGGING
      printf_unfiltered ("[reg %s(%d), offset in file %s=0x%0x, addr =0x%0x, =%0x]\n",
	      reg_names[regno], regno, filename,
	      register_addr(regno, reg_offset),
	      regno * 4 + last_frame_address,
	      *((int *)buf));
#endif /* PYRAMID_CONTROL_FRAME_DEBUGGING */
	      supply_register (regno, buf);
	    }
	}
      }
      if (filename[0] == '/')
	corefile = savestring (filename, strlen (filename));
      else
	{
	  corefile = concat (current_directory, "/", filename, NULL);
	}

#if 1 || defined(PYRAMID_CONTROL_FRAME_DEBUGGING)
      printf_unfiltered ("Providing CSP (%s) as nominal address of current frame.\n",
	      local_hex_string(last_frame_address));
#endif PYRAMID_CONTROL_FRAME_DEBUGGING
      /* FIXME: Which of the following is correct? */
#if 0
      set_current_frame ( create_new_frame (read_register (FP_REGNUM),
					    read_pc ()));
#else
      set_current_frame ( create_new_frame (last_frame_address,
					    read_pc ()));
#endif

      select_frame (get_current_frame (), 0);
      validate_files ();
    }
  else if (from_tty)
    printf_unfiltered ("No core file now.\n");
}
