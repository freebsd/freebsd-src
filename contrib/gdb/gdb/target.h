/* Interface between GDB and target environments, including files and processes
   Copyright 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

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
	download_stratum,	/* Downloading of remote targets */
	process_stratum		/* Executing processes */
};

/* Stuff for target_wait.  */

/* Generally, what has the program done?  */
enum target_waitkind {
  /* The program has exited.  The exit status is in value.integer.  */
  TARGET_WAITKIND_EXITED,

  /* The program has stopped with a signal.  Which signal is in value.sig.  */
  TARGET_WAITKIND_STOPPED,

  /* The program has terminated with a signal.  Which signal is in
     value.sig.  */
  TARGET_WAITKIND_SIGNALLED,

  /* The program is letting us know that it dynamically loaded something
     (e.g. it called load(2) on AIX).  */
  TARGET_WAITKIND_LOADED,

  /* Nothing happened, but we stopped anyway.  This perhaps should be handled
     within target_wait, but I'm not sure target_wait should be resuming the
     inferior.  */
  TARGET_WAITKIND_SPURIOUS
  };

/* The numbering of these signals is chosen to match traditional unix
   signals (insofar as various unices use the same numbers, anyway).
   It is also the numbering of the GDB remote protocol.  Other remote
   protocols, if they use a different numbering, should make sure to
   translate appropriately.  */

/* This is based strongly on Unix/POSIX signals for several reasons:
   (1) This set of signals represents a widely-accepted attempt to
   represent events of this sort in a portable fashion, (2) we want a
   signal to make it from wait to child_wait to the user intact, (3) many
   remote protocols use a similar encoding.  However, it is
   recognized that this set of signals has limitations (such as not
   distinguishing between various kinds of SIGSEGV, or not
   distinguishing hitting a breakpoint from finishing a single step).
   So in the future we may get around this either by adding additional
   signals for breakpoint, single-step, etc., or by adding signal
   codes; the latter seems more in the spirit of what BSD, System V,
   etc. are doing to address these issues.  */

/* For an explanation of what each signal means, see
   target_signal_to_string.  */

enum target_signal {
  /* Used some places (e.g. stop_signal) to record the concept that
     there is no signal.  */
  TARGET_SIGNAL_0 = 0,
  TARGET_SIGNAL_FIRST = 0,
  TARGET_SIGNAL_HUP = 1,
  TARGET_SIGNAL_INT = 2,
  TARGET_SIGNAL_QUIT = 3,
  TARGET_SIGNAL_ILL = 4,
  TARGET_SIGNAL_TRAP = 5,
  TARGET_SIGNAL_ABRT = 6,
  TARGET_SIGNAL_EMT = 7,
  TARGET_SIGNAL_FPE = 8,
  TARGET_SIGNAL_KILL = 9,
  TARGET_SIGNAL_BUS = 10,
  TARGET_SIGNAL_SEGV = 11,
  TARGET_SIGNAL_SYS = 12,
  TARGET_SIGNAL_PIPE = 13,
  TARGET_SIGNAL_ALRM = 14,
  TARGET_SIGNAL_TERM = 15,
  TARGET_SIGNAL_URG = 16,
  TARGET_SIGNAL_STOP = 17,
  TARGET_SIGNAL_TSTP = 18,
  TARGET_SIGNAL_CONT = 19,
  TARGET_SIGNAL_CHLD = 20,
  TARGET_SIGNAL_TTIN = 21,
  TARGET_SIGNAL_TTOU = 22,
  TARGET_SIGNAL_IO = 23,
  TARGET_SIGNAL_XCPU = 24,
  TARGET_SIGNAL_XFSZ = 25,
  TARGET_SIGNAL_VTALRM = 26,
  TARGET_SIGNAL_PROF = 27,
  TARGET_SIGNAL_WINCH = 28,
  TARGET_SIGNAL_LOST = 29,
  TARGET_SIGNAL_USR1 = 30,
  TARGET_SIGNAL_USR2 = 31,
  TARGET_SIGNAL_PWR = 32,
  /* Similar to SIGIO.  Perhaps they should have the same number.  */
  TARGET_SIGNAL_POLL = 33,
  TARGET_SIGNAL_WIND = 34,
  TARGET_SIGNAL_PHONE = 35,
  TARGET_SIGNAL_WAITING = 36,
  TARGET_SIGNAL_LWP = 37,
  TARGET_SIGNAL_DANGER = 38,
  TARGET_SIGNAL_GRANT = 39,
  TARGET_SIGNAL_RETRACT = 40,
  TARGET_SIGNAL_MSG = 41,
  TARGET_SIGNAL_SOUND = 42,
  TARGET_SIGNAL_SAK = 43,
  TARGET_SIGNAL_PRIO = 44,
  TARGET_SIGNAL_REALTIME_33 = 45,
  TARGET_SIGNAL_REALTIME_34 = 46,
  TARGET_SIGNAL_REALTIME_35 = 47,
  TARGET_SIGNAL_REALTIME_36 = 48,
  TARGET_SIGNAL_REALTIME_37 = 49,
  TARGET_SIGNAL_REALTIME_38 = 50,
  TARGET_SIGNAL_REALTIME_39 = 51,
  TARGET_SIGNAL_REALTIME_40 = 52,
  TARGET_SIGNAL_REALTIME_41 = 53,
  TARGET_SIGNAL_REALTIME_42 = 54,
  TARGET_SIGNAL_REALTIME_43 = 55,
  TARGET_SIGNAL_REALTIME_44 = 56,
  TARGET_SIGNAL_REALTIME_45 = 57,
  TARGET_SIGNAL_REALTIME_46 = 58,
  TARGET_SIGNAL_REALTIME_47 = 59,
  TARGET_SIGNAL_REALTIME_48 = 60,
  TARGET_SIGNAL_REALTIME_49 = 61,
  TARGET_SIGNAL_REALTIME_50 = 62,
  TARGET_SIGNAL_REALTIME_51 = 63,
  TARGET_SIGNAL_REALTIME_52 = 64,
  TARGET_SIGNAL_REALTIME_53 = 65,
  TARGET_SIGNAL_REALTIME_54 = 66,
  TARGET_SIGNAL_REALTIME_55 = 67,
  TARGET_SIGNAL_REALTIME_56 = 68,
  TARGET_SIGNAL_REALTIME_57 = 69,
  TARGET_SIGNAL_REALTIME_58 = 70,
  TARGET_SIGNAL_REALTIME_59 = 71,
  TARGET_SIGNAL_REALTIME_60 = 72,
  TARGET_SIGNAL_REALTIME_61 = 73,
  TARGET_SIGNAL_REALTIME_62 = 74,
  TARGET_SIGNAL_REALTIME_63 = 75,

  /* Some signal we don't know about.  */
  TARGET_SIGNAL_UNKNOWN,

  /* Use whatever signal we use when one is not specifically specified
     (for passing to proceed and so on).  */
  TARGET_SIGNAL_DEFAULT,

  /* Last and unused enum value, for sizing arrays, etc.  */
  TARGET_SIGNAL_LAST
};

struct target_waitstatus {
  enum target_waitkind kind;

  /* Exit status or signal number.  */
  union {
    int integer;
    enum target_signal sig;
  } value;
};

/* Return the string for a signal.  */
extern char *target_signal_to_string PARAMS ((enum target_signal));

/* Return the name (SIGHUP, etc.) for a signal.  */
extern char *target_signal_to_name PARAMS ((enum target_signal));

/* Given a name (SIGHUP, etc.), return its signal.  */
enum target_signal target_signal_from_name PARAMS ((char *));

/* If certain kinds of activity happen, target_wait should perform
   callbacks.  */
/* Right now we just call (*TARGET_ACTIVITY_FUNCTION) if I/O is possible
   on TARGET_ACTIVITY_FD.   */
extern int target_activity_fd;
/* Returns zero to leave the inferior alone, one to interrupt it.  */
extern int (*target_activity_function) PARAMS ((void));

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
  void 	      (*to_resume) PARAMS ((int, int, enum target_signal));
  int  	      (*to_wait) PARAMS ((int, struct target_waitstatus *));
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

#if 0
  /* Enable this after 4.12.  */

  /* Search target memory.  Start at STARTADDR and take LEN bytes of
     target memory, and them with MASK, and compare to DATA.  If they
     match, set *ADDR_FOUND to the address we found it at, store the data
     we found at LEN bytes starting at DATA_FOUND, and return.  If
     not, add INCREMENT to the search address and keep trying until
     the search address is outside of the range [LORANGE,HIRANGE).

     If we don't find anything, set *ADDR_FOUND to (CORE_ADDR)0 and return.  */
  void (*to_search) PARAMS ((int len, char *data, char *mask,
			     CORE_ADDR startaddr, int increment,
			     CORE_ADDR lorange, CORE_ADDR hirange,
			     CORE_ADDR *addr_found, char *data_found));

#define	target_search(len, data, mask, startaddr, increment, lorange, hirange, addr_found, data_found)	\
  (*current_target.to_search) (len, data, mask, startaddr, increment, \
				lorange, hirange, addr_found, data_found)
#endif /* 0 */

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
  int	      (*to_thread_alive) PARAMS ((int pid));
  void	      (*to_stop) PARAMS ((void));
  enum strata   to_stratum;
  struct target_ops
		*DONT_USE;	/* formerly to_next */
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

extern struct target_ops	current_target;

/* An item on the target stack.  */

struct target_stack_item
{
  struct target_stack_item *next;
  struct target_ops *target_ops;
};

/* The target stack.  */

extern struct target_stack_item *target_stack;

/* Define easy words for doing these operations on our current target.  */

#define	target_shortname	(current_target.to_shortname)
#define	target_longname		(current_target.to_longname)

/* The open routine takes the rest of the parameters from the command,
   and (if successful) pushes a new target onto the stack.
   Targets should supply this routine, if only to provide an error message.  */
#define	target_open(name, from_tty)	\
	(*current_target.to_open) (name, from_tty)

/* Does whatever cleanup is required for a target that we are no longer
   going to be calling.  Argument says whether we are quitting gdb and
   should not get hung in case of errors, or whether we want a clean
   termination even if it takes a while.  This routine is automatically
   always called just before a routine is popped off the target stack.
   Closing file descriptors and freeing memory are typical things it should
   do.  */

#define	target_close(quitting)	\
	(*current_target.to_close) (quitting)

/* Attaches to a process on the target side.  Arguments are as passed
   to the `attach' command by the user.  This routine can be called
   when the target is not on the target-stack, if the target_can_run
   routine returns 1; in that case, it must push itself onto the stack.  
   Upon exit, the target should be ready for normal operations, and
   should be ready to deliver the status of the process immediately 
   (without waiting) to an upcoming target_wait call.  */

#define	target_attach(args, from_tty)	\
	(*current_target.to_attach) (args, from_tty)

/* Takes a program previously attached to and detaches it.
   The program may resume execution (some targets do, some don't) and will
   no longer stop on signals, etc.  We better not have left any breakpoints
   in the program or it'll die when it hits one.  ARGS is arguments
   typed by the user (e.g. a signal to send the process).  FROM_TTY
   says whether to be verbose or not.  */

extern void
target_detach PARAMS ((char *, int));

/* Resume execution of the target process PID.  STEP says whether to
   single-step or to run free; SIGGNAL is the signal to be given to
   the target, or TARGET_SIGNAL_0 for no signal.  The caller may not
   pass TARGET_SIGNAL_DEFAULT.  */

#define	target_resume(pid, step, siggnal)	\
	(*current_target.to_resume) (pid, step, siggnal)

/* Wait for process pid to do something.  Pid = -1 to wait for any pid
   to do something.  Return pid of child, or -1 in case of error;
   store status through argument pointer STATUS.  Note that it is
   *not* OK to return_to_top_level out of target_wait without popping
   the debugging target from the stack; GDB isn't prepared to get back
   to the prompt with a debugging target but without the frame cache,
   stop_pc, etc., set up.  */

#define	target_wait(pid, status)		\
	(*current_target.to_wait) (pid, status)

/* Fetch register REGNO, or all regs if regno == -1.  No result.  */

#define	target_fetch_registers(regno)	\
	(*current_target.to_fetch_registers) (regno)

/* Store at least register REGNO, or all regs if REGNO == -1.
   It can store as many registers as it wants to, so target_prepare_to_store
   must have been previously called.  Calls error() if there are problems.  */

#define	target_store_registers(regs)	\
	(*current_target.to_store_registers) (regs)

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that REGISTERS contains all the registers from the program being
   debugged.  */

#define	target_prepare_to_store()	\
	(*current_target.to_prepare_to_store) ()

extern int target_read_string PARAMS ((CORE_ADDR, char **, int, int *));

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
	(*current_target.to_files_info) (&current_target)

/* Insert a breakpoint at address ADDR in the target machine.
   SAVE is a pointer to memory allocated for saving the
   target contents.  It is guaranteed by the caller to be long enough
   to save "sizeof BREAKPOINT" bytes.  Result is 0 for success, or
   an errno value.  */

#define	target_insert_breakpoint(addr, save)	\
	(*current_target.to_insert_breakpoint) (addr, save)

/* Remove a breakpoint at address ADDR in the target machine.
   SAVE is a pointer to the same save area 
   that was previously passed to target_insert_breakpoint.  
   Result is 0 for success, or an errno value.  */

#define	target_remove_breakpoint(addr, save)	\
	(*current_target.to_remove_breakpoint) (addr, save)

/* Initialize the terminal settings we record for the inferior,
   before we actually run the inferior.  */

#define target_terminal_init() \
	(*current_target.to_terminal_init) ()

/* Put the inferior's terminal settings into effect.
   This is preparation for starting or resuming the inferior.  */

#define target_terminal_inferior() \
	(*current_target.to_terminal_inferior) ()

/* Put some of our terminal settings into effect,
   enough to get proper results from our output,
   but do not change into or out of RAW mode
   so that no input is discarded.

   After doing this, either terminal_ours or terminal_inferior
   should be called to get back to a normal state of affairs.  */

#define target_terminal_ours_for_output() \
	(*current_target.to_terminal_ours_for_output) ()

/* Put our terminal settings into effect.
   First record the inferior's terminal settings
   so they can be restored properly later.  */

#define target_terminal_ours() \
	(*current_target.to_terminal_ours) ()

/* Print useful information about our terminal status, if such a thing
   exists.  */

#define target_terminal_info(arg, from_tty) \
	(*current_target.to_terminal_info) (arg, from_tty)

/* Kill the inferior process.   Make it go away.  */

#define target_kill() \
	(*current_target.to_kill) ()

/* Load an executable file into the target process.  This is expected to
   not only bring new code into the target process, but also to update
   GDB's symbol tables to match.  */

#define target_load(arg, from_tty) \
	(*current_target.to_load) (arg, from_tty)

/* Look up a symbol in the target's symbol table.  NAME is the symbol
   name.  ADDRP is a CORE_ADDR * pointing to where the value of the symbol
   should be returned.  The result is 0 if successful, nonzero if the
   symbol does not exist in the target environment.  This function should
   not call error() if communication with the target is interrupted, since
   it is called from symbol reading, but should return nonzero, possibly
   doing a complain().  */

#define target_lookup_symbol(name, addrp) 	\
  (*current_target.to_lookup_symbol) (name, addrp)

/* Start an inferior process and set inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().
   On VxWorks and various standalone systems, we ignore exec_file.  */
 
#define	target_create_inferior(exec_file, args, env)	\
	(*current_target.to_create_inferior) (exec_file, args, env)

/* The inferior process has died.  Do what is right.  */

#define	target_mourn_inferior()	\
	(*current_target.to_mourn_inferior) ()

/* Does target have enough data to do a run or attach command? */

#define target_can_run(t) \
  	((t)->to_can_run) ()

/* post process changes to signal handling in the inferior.  */

#define target_notice_signals(pid) \
  	(*current_target.to_notice_signals) (pid)

/* Check to see if a thread is still alive.  */

#define target_thread_alive(pid) \
	(*current_target.to_thread_alive) (pid)

/* Make target stop in a continuable fashion.  (For instance, under Unix, this
   should act like SIGSTOP).  This function is normally used by GUIs to
   implement a stop button.  */

#define target_stop() current_target.to_stop ()

/* Pointer to next target in the chain, e.g. a core file and an exec file.  */

#define	target_next \
	(current_target.to_next)

/* Does the target include all of memory, or only part of it?  This
   determines whether we look up the target chain for other parts of
   memory if this target can't satisfy a request.  */

#define	target_has_all_memory	\
	(current_target.to_has_all_memory)

/* Does the target include memory?  (Dummy targets don't.)  */

#define	target_has_memory	\
	(current_target.to_has_memory)

/* Does the target have a stack?  (Exec files don't, VxWorks doesn't, until
   we start a process.)  */
   
#define	target_has_stack	\
	(current_target.to_has_stack)

/* Does the target have registers?  (Exec files don't.)  */

#define	target_has_registers	\
	(current_target.to_has_registers)

/* Does the target have execution?  Can we make it jump (through
   hoops), or pop its stack a few times?  FIXME: If this is to work that
   way, it needs to check whether an inferior actually exists.
   remote-udi.c and probably other targets can be the current target
   when the inferior doesn't actually exist at the moment.  Right now
   this just tells us whether this target is *capable* of execution.  */

#define	target_has_execution	\
	(current_target.to_has_execution)

extern void target_link PARAMS ((char *, CORE_ADDR *));

/* Converts a process id to a string.  Usually, the string just contains
   `process xyz', but on some systems it may contain
   `process xyz thread abc'.  */

#ifndef target_pid_to_str
#define target_pid_to_str(PID) \
	normal_pid_to_str (PID)
extern char *normal_pid_to_str PARAMS ((int pid));
#endif

/* Hook to call target-dependant code after reading in a new symbol table. */

#ifndef TARGET_SYMFILE_POSTREAD
#define TARGET_SYMFILE_POSTREAD(OBJFILE)
#endif

/* Hook to call target dependant code just after inferior target process has
   started.  */

#ifndef TARGET_CREATE_INFERIOR_HOOK
#define TARGET_CREATE_INFERIOR_HOOK(PID)
#endif

/* Hardware watchpoint interfaces.  */

/* Returns non-zero if we were stopped by a hardware watchpoint (memory read or
   write).  */

#ifndef STOPPED_BY_WATCHPOINT
#define STOPPED_BY_WATCHPOINT(w) 0
#endif

/* Provide defaults for systems that don't support hardware watchpoints. */

#ifndef TARGET_HAS_HARDWARE_WATCHPOINTS

/* Returns non-zero if we can set a hardware watchpoint of type TYPE.  TYPE is
   one of bp_hardware_watchpoint, bp_read_watchpoint, bp_write_watchpoint, or
   bp_hardware_breakpoint.  CNT is the number of such watchpoints used so far
   (including this one?).  OTHERTYPE is who knows what...  */

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(TYPE,CNT,OTHERTYPE) 0

/* Set/clear a hardware watchpoint starting at ADDR, for LEN bytes.  TYPE is 1
   for read and 2 for read/write accesses.  Returns 0 for success, non-zero for
   failure.  */

#define target_remove_watchpoint(ADDR,LEN,TYPE) -1
#define target_insert_watchpoint(ADDR,LEN,TYPE) -1

#endif /* TARGET_HAS_HARDWARE_WATCHPOINTS */

#ifndef target_insert_hw_breakpoint
#define target_remove_hw_breakpoint(ADDR,SHADOW) -1
#define target_insert_hw_breakpoint(ADDR,SHADOW) -1
#endif

#ifndef target_stopped_data_address
#define target_stopped_data_address() 0
#endif

/* If defined, then we need to decr pc by this much after a hardware break-
   point.  Presumably this overrides DECR_PC_AFTER_BREAK...  */

#ifndef DECR_PC_AFTER_HW_BREAK
#define DECR_PC_AFTER_HW_BREAK 0
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

  sec_ptr the_bfd_section;

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

/* Stuff that should be shared among the various remote targets.  */

/* Debugging level.  0 is off, and non-zero values mean to print some debug
   information (higher values, more information).  */
extern int remote_debug;

/* Speed in bits per second, or -1 which means don't mess with the speed.  */
extern int baud_rate;

/* Functions for helping to write a native target.  */

/* This is for native targets which use a unix/POSIX-style waitstatus.  */
extern void store_waitstatus PARAMS ((struct target_waitstatus *, int));

/* Convert between host signal numbers and enum target_signal's.  */
extern enum target_signal target_signal_from_host PARAMS ((int));
extern int target_signal_to_host PARAMS ((enum target_signal));

/* Convert from a number used in a GDB command to an enum target_signal.  */
extern enum target_signal target_signal_from_command PARAMS ((int));

#endif	/* !defined (TARGET_H) */
