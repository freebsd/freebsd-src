/* Interface between GDB and target environments, including files and processes
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by John Gilmore.

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

#if !defined (TARGET_H)
#define TARGET_H

/* This include file defines the interface between the main part
   of the debugger, and the part which is target-specific, or
   specific to the communications interface between us and the
   target.

   A TARGET is an interface between the debugger and a particular 
   kind of file or process.  Targets can be STACKED in STRATA, 
   so that more than one target can potentially respond to a request.
   In particular, memory accesses will walk down the stack of targets
   until they find a target that is interested in handling that particular
   address.  STRATA are artificial boundaries on the stack, within
   which particular kinds of targets live.  Strata exist so that
   people don't get confused by pushing e.g. a process target and then
   a file target, and wondering why they can't see the current values
   of variables any more (the file target is handling them and they
   never get to the process target).  So when you push a file target,
   it goes into the file stratum, which is always below the process
   stratum.  */

#include "bfd.h"

enum strata {
	dummy_stratum,		/* The lowest of the low */
	file_stratum,		/* Executable files, etc */
	core_stratum,		/* Core dump files */
	process_stratum		/* Executing processes */
};

struct target_ops
{
  char	       *to_shortname;	/* Name this target type */
  char	       *to_longname;	/* Name for printing */
  char 	       *to_doc;	        /* Documentation.  Does not include trailing
				   newline, and starts with a one-line descrip-
				   tion (probably similar to to_longname). */
  void 	      (*to_open) PARAMS ((char *, int));
  void 	      (*to_close) PARAMS ((int));
  void 	      (*to_attach) PARAMS ((char *, int));
  void 	      (*to_detach) PARAMS ((char *, int));
  void 	      (*to_resume) PARAMS ((int, int, int));
  int  	      (*to_wait) PARAMS ((int, int *));
  void 	      (*to_fetch_registers) PARAMS ((int));
  void 	      (*to_store_registers) PARAMS ((int));
  void 	      (*to_prepare_to_store) PARAMS ((void));

  /* Transfer LEN bytes of memory between GDB address MYADDR and
     target address MEMADDR.  If WRITE, transfer them to the target, else
     transfer them from the target.  TARGET is the target from which we
     get this function.

     Return value, N, is one of the following:

     0 means that we can't handle this.  If errno has been set, it is the
     error which prevented us from doing it (FIXME: What about bfd_error?).

     positive (call it N) means that we have transferred N bytes
     starting at MEMADDR.  We might be able to handle more bytes
     beyond this length, but no promises.

     negative (call its absolute value N) means that we cannot
     transfer right at MEMADDR, but we could transfer at least
     something at MEMADDR + N.  */

  int  	      (*to_xfer_memory) PARAMS ((CORE_ADDR memaddr, char *myaddr,
					 int len, int write,
					 struct target_ops * target));

  void 	      (*to_files_info) PARAMS ((struct target_ops *));
  int  	      (*to_insert_breakpoint) PARAMS ((CORE_ADDR, char *));
  int 	      (*to_remove_breakpoint) PARAMS ((CORE_ADDR, char *));
  void 	      (*to_terminal_init) PARAMS ((void));
  void 	      (*to_terminal_inferior) PARAMS ((void));
  void 	      (*to_terminal_ours_for_output) PARAMS ((void));
  void 	      (*to_terminal_ours) PARAMS ((void));
  void 	      (*to_terminal_info) PARAMS ((char *, int));
  void 	      (*to_kill) PARAMS ((void));
  void 	      (*to_load) PARAMS ((char *, int));
  int 	      (*to_lookup_symbol) PARAMS ((char *, CORE_ADDR *));
  void 	      (*to_create_inferior) PARAMS ((char *, char *, char **));
  void 	      (*to_mourn_inferior) PARAMS ((void));
  int	      (*to_can_run) PARAMS ((void));
  void	      (*to_notice_signals) PARAMS ((int pid));
  enum strata   to_stratum;
  struct target_ops
    	       *to_next;
  int		to_has_all_memory;
  int		to_has_memory;
  int		to_has_stack;
  int		to_has_registers;
  int		to_has_execution;
  struct section_table
    	       *to_sections;
  struct section_table
	       *to_sections_end;
  int		to_magic;
  /* Need sub-structure for target machine related rather than comm related? */
};

/* Magic number for checking ops size.  If a struct doesn't end with this
   number, somebody changed the declaration but didn't change all the
   places that initialize one.  */

#define	OPS_MAGIC	3840

/* The ops structure for our "current" target process.  This should
   never be NULL.  If there is no target, it points to the dummy_target.  */

extern struct target_ops	*current_target;

/* Define easy words for doing these operations on our current target.  */

#define	target_shortname	(current_target->to_shortname)
#define	target_longname		(current_target->to_longname)

/* The open routine takes the rest of the parameters from the command,
   and (if successful) pushes a new target onto the stack.
   Targets should supply this routine, if only to provide an error message.  */
#define	target_open(name, from_tty)	\
	(*current_target->to_open) (name, from_tty)

/* Does whatever cleanup is required for a target that we are no longer
   going to be calling.  Argument says whether we are quitting gdb and
   should not get hung in case of errors, or whether we want a clean
   termination even if it takes a while.  This routine is automatically
   always called just before a routine is popped off the target stack.
   Closing file descriptors and freeing memory are typical things it should
   do.  */

#define	target_close(quitting)	\
	(*current_target->to_close) (quitting)

/* Attaches to a process on the target side.  Arguments are as passed
   to the `attach' command by the user.  This routine can be called
   when the target is not on the target-stack, if the target_can_run
   routine returns 1; in that case, it must push itself onto the stack.  
   Upon exit, the target should be ready for normal operations, and
   should be ready to deliver the status of the process immediately 
   (without waiting) to an upcoming target_wait call.  */

#define	target_attach(args, from_tty)	\
	(*current_target->to_attach) (args, from_tty)

/* Takes a program previously attached to and detaches it.
   The program may resume execution (some targets do, some don't) and will
   no longer stop on signals, etc.  We better not have left any breakpoints
   in the program or it'll die when it hits one.  ARGS is arguments
   typed by the user (e.g. a signal to send the process).  FROM_TTY
   says whether to be verbose or not.  */

extern void
target_detach PARAMS ((char *, int));

/* Resume execution of the target process PID.  STEP says whether to
   single-step or to run free; SIGGNAL is the signal value (e.g. SIGINT) to be
   given to the target, or zero for no signal.  */

#define	target_resume(pid, step, siggnal)	\
	(*current_target->to_resume) (pid, step, siggnal)

/* Wait for process pid to do something.  Pid = -1 to wait for any pid to do
   something.  Return pid of child, or -1 in case of error; store status
   through argument pointer STATUS.  */

#define	target_wait(pid, status)		\
	(*current_target->to_wait) (pid, status)

/* Fetch register REGNO, or all regs if regno == -1.  No result.  */

#define	target_fetch_registers(regno)	\
	(*current_target->to_fetch_registers) (regno)

/* Store at least register REGNO, or all regs if REGNO == -1.
   It can store as many registers as it wants to, so target_prepare_to_store
   must have been previously called.  Calls error() if there are problems.  */

#define	target_store_registers(regs)	\
	(*current_target->to_store_registers) (regs)

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that REGISTERS contains all the registers from the program being
   debugged.  */

#define	target_prepare_to_store()	\
	(*current_target->to_prepare_to_store) ()

extern int
target_read_string PARAMS ((CORE_ADDR, char *, int));

extern int
target_read_memory PARAMS ((CORE_ADDR, char *, int));

extern int
target_read_memory_partial PARAMS ((CORE_ADDR, char *, int, int *));

extern int
target_write_memory PARAMS ((CORE_ADDR, char *, int));

extern int
xfer_memory PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

extern int
child_xfer_memory PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

/* Transfer LEN bytes between target address MEMADDR and GDB address MYADDR.
   Returns 0 for success, errno code for failure (which includes partial
   transfers--if you want a more useful response to partial transfers, try
   target_read_memory_partial).  */

extern int target_xfer_memory PARAMS ((CORE_ADDR memaddr, char *myaddr,
				       int len, int write));

/* From exec.c */

extern void
print_section_info PARAMS ((struct target_ops *, bfd *));

/* Print a line about the current target.  */

#define	target_files_info()	\
	(*current_target->to_files_info) (current_target)

/* Insert a breakpoint at address ADDR in the target machine.
   SAVE is a pointer to memory allocated for saving the
   target contents.  It is guaranteed by the caller to be long enough
   to save "sizeof BREAKPOINT" bytes.  Result is 0 for success, or
   an errno value.  */

#define	target_insert_breakpoint(addr, save)	\
	(*current_target->to_insert_breakpoint) (addr, save)

/* Remove a breakpoint at address ADDR in the target machine.
   SAVE is a pointer to the same save area 
   that was previously passed to target_insert_breakpoint.  
   Result is 0 for success, or an errno value.  */

#define	target_remove_breakpoint(addr, save)	\
	(*current_target->to_remove_breakpoint) (addr, save)

/* Initialize the terminal settings we record for the inferior,
   before we actually run the inferior.  */

#define target_terminal_init() \
	(*current_target->to_terminal_init) ()

/* Put the inferior's terminal settings into effect.
   This is preparation for starting or resuming the inferior.  */

#define target_terminal_inferior() \
	(*current_target->to_terminal_inferior) ()

/* Put some of our terminal settings into effect,
   enough to get proper results from our output,
   but do not change into or out of RAW mode
   so that no input is discarded.

   After doing this, either terminal_ours or terminal_inferior
   should be called to get back to a normal state of affairs.  */

#define target_terminal_ours_for_output() \
	(*current_target->to_terminal_ours_for_output) ()

/* Put our terminal settings into effect.
   First record the inferior's terminal settings
   so they can be restored properly later.  */

#define target_terminal_ours() \
	(*current_target->to_terminal_ours) ()

/* Print useful information about our terminal status, if such a thing
   exists.  */

#define target_terminal_info(arg, from_tty) \
	(*current_target->to_terminal_info) (arg, from_tty)

/* Kill the inferior process.   Make it go away.  */

#define target_kill() \
	(*current_target->to_kill) ()

/* Load an executable file into the target process.  This is expected to
   not only bring new code into the target process, but also to update
   GDB's symbol tables to match.  */

#define target_load(arg, from_tty) \
	(*current_target->to_load) (arg, from_tty)

/* Look up a symbol in the target's symbol table.  NAME is the symbol
   name.  ADDRP is a CORE_ADDR * pointing to where the value of the symbol
   should be returned.  The result is 0 if successful, nonzero if the
   symbol does not exist in the target environment.  This function should
   not call error() if communication with the target is interrupted, since
   it is called from symbol reading, but should return nonzero, possibly
   doing a complain().  */

#define target_lookup_symbol(name, addrp) 	\
  (*current_target->to_lookup_symbol) (name, addrp)

/* Start an inferior process and set inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().
   On VxWorks and various standalone systems, we ignore exec_file.  */
 
#define	target_create_inferior(exec_file, args, env)	\
	(*current_target->to_create_inferior) (exec_file, args, env)

/* The inferior process has died.  Do what is right.  */

#define	target_mourn_inferior()	\
	(*current_target->to_mourn_inferior) ()

/* Does target have enough data to do a run or attach command? */

#define target_can_run(t) \
  	((t)->to_can_run) ()

/* post process changes to signal handling in the inferior.  */

#define target_notice_signals(pid) \
  	(*current_target->to_notice_signals) (pid)

/* Pointer to next target in the chain, e.g. a core file and an exec file.  */

#define	target_next \
	(current_target->to_next)

/* Does the target include all of memory, or only part of it?  This
   determines whether we look up the target chain for other parts of
   memory if this target can't satisfy a request.  */

#define	target_has_all_memory	\
	(current_target->to_has_all_memory)

/* Does the target include memory?  (Dummy targets don't.)  */

#define	target_has_memory	\
	(current_target->to_has_memory)

/* Does the target have a stack?  (Exec files don't, VxWorks doesn't, until
   we start a process.)  */
   
#define	target_has_stack	\
	(current_target->to_has_stack)

/* Does the target have registers?  (Exec files don't.)  */

#define	target_has_registers	\
	(current_target->to_has_registers)

/* Does the target have execution?  Can we make it jump (through
   hoops), or pop its stack a few times?  FIXME: If this is to work that
   way, it needs to check whether an inferior actually exists.
   remote-udi.c and probably other targets can be the current target
   when the inferior doesn't actually exist at the moment.  Right now
   this just tells us whether this target is *capable* of execution.  */

#define	target_has_execution	\
	(current_target->to_has_execution)

/* Converts a process id to a string.  Usually, the string just contains
   `process xyz', but on some systems it may contain
   `process xyz thread abc'.  */

#ifndef target_pid_to_str
#define target_pid_to_str(PID) \
	normal_pid_to_str (PID)
extern char *normal_pid_to_str PARAMS ((int pid));
#endif

/* Routines for maintenance of the target structures...

   add_target:   Add a target to the list of all possible targets.

   push_target:  Make this target the top of the stack of currently used
		 targets, within its particular stratum of the stack.  Result
		 is 0 if now atop the stack, nonzero if not on top (maybe
		 should warn user).

   unpush_target: Remove this from the stack of currently used targets,
		 no matter where it is on the list.  Returns 0 if no
		 change, 1 if removed from stack.

   pop_target:	 Remove the top thing on the stack of current targets.  */

extern void
add_target PARAMS ((struct target_ops *));

extern int
push_target PARAMS ((struct target_ops *));

extern int
unpush_target PARAMS ((struct target_ops *));

extern void
target_preopen PARAMS ((int));

extern void
pop_target PARAMS ((void));

/* Struct section_table maps address ranges to file sections.  It is
   mostly used with BFD files, but can be used without (e.g. for handling
   raw disks, or files not in formats handled by BFD).  */

struct section_table {
  CORE_ADDR addr;		/* Lowest address in section */
  CORE_ADDR endaddr;		/* 1+highest address in section */
  sec_ptr   sec_ptr;		/* BFD section pointer */
  bfd	   *bfd;		/* BFD file pointer */
};

/* Builds a section table, given args BFD, SECTABLE_PTR, SECEND_PTR.
   Returns 0 if OK, 1 on error.  */

extern int
build_section_table PARAMS ((bfd *, struct section_table **,
			     struct section_table **));

/* From mem-break.c */

extern int
memory_remove_breakpoint PARAMS ((CORE_ADDR, char *));

extern int
memory_insert_breakpoint PARAMS ((CORE_ADDR, char *));

/* From target.c */

void
noprocess PARAMS ((void));

void
find_default_attach PARAMS ((char *, int));

void
find_default_create_inferior PARAMS ((char *, char *, char **));

struct target_ops *
find_core_target PARAMS ((void));

#endif	/* !defined (TARGET_H) */
