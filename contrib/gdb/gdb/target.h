/* Interface between GDB and target environments, including files and processes
   Copyright 1990, 91, 92, 93, 94, 1999 Free Software Foundation, Inc.
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
#include "symtab.h"

enum strata {
	dummy_stratum,		/* The lowest of the low */
	file_stratum,		/* Executable files, etc */
	core_stratum,		/* Core dump files */
	kcore_stratum,		/* Kernel core files */
	download_stratum,	/* Downloading of remote targets */
	process_stratum		/* Executing processes */
};

enum thread_control_capabilities {
	tc_none = 0, 		/* Default: can't control thread execution. */
	tc_schedlock = 1,	/* Can lock the thread scheduler. */
	tc_switch = 2 		/* Can switch the running thread on demand. */
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

  /* The program has forked.  A "related" process' ID is in value.related_pid.
     I.e., if the child forks, value.related_pid is the parent's ID.
     */
  TARGET_WAITKIND_FORKED,

  /* The program has vforked.  A "related" process's ID is in value.related_pid.
     */
  TARGET_WAITKIND_VFORKED,

  /* The program has exec'ed a new executable file.  The new file's pathname
     is pointed to by value.execd_pathname.
     */
  TARGET_WAITKIND_EXECD,

  /* The program has entered or returned from a system call.  On HP-UX, this
     is used in the hardware watchpoint implementation.  The syscall's unique
     integer ID number is in value.syscall_id;
     */
  TARGET_WAITKIND_SYSCALL_ENTRY,
  TARGET_WAITKIND_SYSCALL_RETURN,

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
#if defined(MACH) || defined(__MACH__)
  /* Mach exceptions */
  TARGET_EXC_BAD_ACCESS = 76,
  TARGET_EXC_BAD_INSTRUCTION = 77,
  TARGET_EXC_ARITHMETIC = 78,
  TARGET_EXC_EMULATION = 79,
  TARGET_EXC_SOFTWARE = 80,
  TARGET_EXC_BREAKPOINT = 81,
#endif
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

  /* Forked child pid, execd pathname, exit status or signal number.  */
  union {
    int integer;
    enum target_signal sig;
    int  related_pid;
    char *  execd_pathname;
    int  syscall_id;
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
  void        (*to_post_attach) PARAMS ((int));
  void 	      (*to_require_attach) PARAMS ((char *, int));
  void 	      (*to_detach) PARAMS ((char *, int));
  void 	      (*to_require_detach) PARAMS ((int, char *, int));
  void 	      (*to_resume) PARAMS ((int, int, enum target_signal));
  int  	      (*to_wait) PARAMS ((int, struct target_waitstatus *));
  void        (*to_post_wait) PARAMS ((int, int));
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
  void        (*to_post_startup_inferior) PARAMS ((int));
  void        (*to_acknowledge_created_inferior) PARAMS ((int));
  void        (*to_clone_and_follow_inferior) PARAMS ((int, int *));
  void        (*to_post_follow_inferior_by_clone) PARAMS ((void));
  int         (*to_insert_fork_catchpoint) PARAMS ((int));
  int         (*to_remove_fork_catchpoint) PARAMS ((int));
  int         (*to_insert_vfork_catchpoint) PARAMS ((int));
  int         (*to_remove_vfork_catchpoint) PARAMS ((int));
  int         (*to_has_forked) PARAMS ((int, int *));
  int         (*to_has_vforked) PARAMS ((int, int *));
  int         (*to_can_follow_vfork_prior_to_exec) PARAMS ((void));
  void        (*to_post_follow_vfork) PARAMS ((int, int, int, int));
  int         (*to_insert_exec_catchpoint) PARAMS ((int));
  int         (*to_remove_exec_catchpoint) PARAMS ((int));
  int         (*to_has_execd) PARAMS ((int, char **));
  int         (*to_reported_exec_events_per_exec_call) PARAMS ((void));
  int         (*to_has_syscall_event) PARAMS ((int, enum target_waitkind *, int *));
  int         (*to_has_exited) PARAMS ((int, int, int *));
  void 	      (*to_mourn_inferior) PARAMS ((void));
  int	      (*to_can_run) PARAMS ((void));
  void	      (*to_notice_signals) PARAMS ((int pid));
  int	      (*to_thread_alive) PARAMS ((int pid));
  void	      (*to_stop) PARAMS ((void));
  int 	      (*to_query) PARAMS ((int/*char*/, char *, char *, int *));
  struct symtab_and_line * (*to_enable_exception_callback) PARAMS ((enum exception_event_kind, int));
  struct exception_event_record * (*to_get_current_exception_event) PARAMS ((void));
  char *      (*to_pid_to_exec_file) PARAMS ((int pid));
  char *      (*to_core_file_to_sym_file) PARAMS ((char *));
  enum strata   to_stratum;
  struct target_ops
		*DONT_USE;	/* formerly to_next */
  int		to_has_all_memory;
  int		to_has_memory;
  int		to_has_stack;
  int		to_has_registers;
  int		to_has_execution;
  int		to_has_thread_control;	/* control thread execution */
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

/* The target_attach operation places a process under debugger control,
   and stops the process.

   This operation provides a target-specific hook that allows the
   necessary bookkeeping to be performed after an attach completes.
   */
#define target_post_attach(pid) \
        (*current_target.to_post_attach) (pid)

/* Attaches to a process on the target side, if not already attached.
   (If already attached, takes no action.)

   This operation can be used to follow the child process of a fork.
   On some targets, such child processes of an original inferior process
   are automatically under debugger control, and thus do not require an
   actual attach operation.  */

#define	target_require_attach(args, from_tty)	\
	(*current_target.to_require_attach) (args, from_tty)

/* Takes a program previously attached to and detaches it.
   The program may resume execution (some targets do, some don't) and will
   no longer stop on signals, etc.  We better not have left any breakpoints
   in the program or it'll die when it hits one.  ARGS is arguments
   typed by the user (e.g. a signal to send the process).  FROM_TTY
   says whether to be verbose or not.  */

extern void
target_detach PARAMS ((char *, int));

/* Detaches from a process on the target side, if not already dettached.
   (If already detached, takes no action.)

   This operation can be used to follow the parent process of a fork.
   On some targets, such child processes of an original inferior process
   are automatically under debugger control, and thus do require an actual
   detach operation.

   PID is the process id of the child to detach from.
   ARGS is arguments typed by the user (e.g. a signal to send the process).
   FROM_TTY says whether to be verbose or not.  */

#define target_require_detach(pid, args, from_tty) \
	(*current_target.to_require_detach) (pid, args, from_tty)

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

/* The target_wait operation waits for a process event to occur, and
   thereby stop the process.

   On some targets, certain events may happen in sequences.  gdb's
   correct response to any single event of such a sequence may require
   knowledge of what earlier events in the sequence have been seen.

   This operation provides a target-specific hook that allows the
   necessary bookkeeping to be performed to track such sequences.
   */

#define target_post_wait(pid, status) \
        (*current_target.to_post_wait) (pid, status)

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
target_read_memory PARAMS ((CORE_ADDR memaddr, char *myaddr, int len));

extern int
target_read_memory_section PARAMS ((CORE_ADDR memaddr, char *myaddr, int len,
				    asection *bfd_section));

extern int
target_read_memory_partial PARAMS ((CORE_ADDR, char *, int, int *));

extern int
target_write_memory PARAMS ((CORE_ADDR, char *, int));

extern int
xfer_memory PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

extern int
child_xfer_memory PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

extern char *
child_pid_to_exec_file PARAMS ((int));

extern char *
child_core_file_to_sym_file PARAMS ((char *));

#if defined(CHILD_POST_ATTACH)
extern void
child_post_attach PARAMS ((int));
#endif

extern void
child_post_wait PARAMS ((int, int));

extern void
child_post_startup_inferior PARAMS ((int));

extern void
child_acknowledge_created_inferior PARAMS ((int));

extern void
child_clone_and_follow_inferior PARAMS ((int, int *));

extern void
child_post_follow_inferior_by_clone PARAMS ((void));

extern int
child_insert_fork_catchpoint PARAMS ((int));

extern int
child_remove_fork_catchpoint PARAMS ((int));

extern int
child_insert_vfork_catchpoint PARAMS ((int));

extern int
child_remove_vfork_catchpoint PARAMS ((int));

extern int
child_has_forked PARAMS ((int, int *));

extern int
child_has_vforked PARAMS ((int, int *));

extern void
child_acknowledge_created_inferior PARAMS ((int));

extern int
child_can_follow_vfork_prior_to_exec PARAMS ((void));

extern void
child_post_follow_vfork PARAMS ((int, int, int, int));

extern int
child_insert_exec_catchpoint PARAMS ((int));

extern int
child_remove_exec_catchpoint PARAMS ((int));

extern int
child_has_execd PARAMS ((int, char **));

extern int
child_reported_exec_events_per_exec_call PARAMS ((void));

extern int
child_has_syscall_event PARAMS ((int, enum target_waitkind *, int *));

extern int
child_has_exited PARAMS ((int, int, int *));

extern int
child_thread_alive PARAMS ((int));

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


/* Some targets (such as ttrace-based HPUX) don't allow us to request
   notification of inferior events such as fork and vork immediately
   after the inferior is created.  (This because of how gdb gets an
   inferior created via invoking a shell to do it.  In such a scenario,
   if the shell init file has commands in it, the shell will fork and
   exec for each of those commands, and we will see each such fork
   event.  Very bad.)
   
   Such targets will supply an appropriate definition for this function.
   */
#define target_post_startup_inferior(pid) \
        (*current_target.to_post_startup_inferior) (pid)

/* On some targets, the sequence of starting up an inferior requires
   some synchronization between gdb and the new inferior process, PID.
   */
#define target_acknowledge_created_inferior(pid) \
        (*current_target.to_acknowledge_created_inferior) (pid)

/* An inferior process has been created via a fork() or similar
   system call.  This function will clone the debugger, then ensure
   that CHILD_PID is attached to by that debugger.

   FOLLOWED_CHILD is set TRUE on return *for the clone debugger only*,
   and FALSE otherwise.  (The original and clone debuggers can use this
   to determine which they are, if need be.)

   (This is not a terribly useful feature without a GUI to prevent
   the two debuggers from competing for shell input.)
   */
#define target_clone_and_follow_inferior(child_pid,followed_child) \
        (*current_target.to_clone_and_follow_inferior) (child_pid, followed_child)

/* This operation is intended to be used as the last in a sequence of
   steps taken when following both parent and child of a fork.  This
   is used by a clone of the debugger, which will follow the child.

   The original debugger has detached from this process, and the
   clone has attached to it.

   On some targets, this requires a bit of cleanup to make it work
   correctly.
   */
#define target_post_follow_inferior_by_clone() \
        (*current_target.to_post_follow_inferior_by_clone) ()

/* On some targets, we can catch an inferior fork or vfork event when it
   occurs.  These functions insert/remove an already-created catchpoint for
   such events.
   */
#define target_insert_fork_catchpoint(pid) \
        (*current_target.to_insert_fork_catchpoint) (pid)

#define target_remove_fork_catchpoint(pid) \
        (*current_target.to_remove_fork_catchpoint) (pid)

#define target_insert_vfork_catchpoint(pid) \
        (*current_target.to_insert_vfork_catchpoint) (pid)

#define target_remove_vfork_catchpoint(pid) \
        (*current_target.to_remove_vfork_catchpoint) (pid)

/* Returns TRUE if PID has invoked the fork() system call.  And,
   also sets CHILD_PID to the process id of the other ("child")
   inferior process that was created by that call.
   */
#define target_has_forked(pid,child_pid) \
        (*current_target.to_has_forked) (pid,child_pid)

/* Returns TRUE if PID has invoked the vfork() system call.  And,
   also sets CHILD_PID to the process id of the other ("child")
   inferior process that was created by that call.
   */
#define target_has_vforked(pid,child_pid) \
        (*current_target.to_has_vforked) (pid,child_pid)

/* Some platforms (such as pre-10.20 HP-UX) don't allow us to do
   anything to a vforked child before it subsequently calls exec().
   On such platforms, we say that the debugger cannot "follow" the
   child until it has vforked.

   This function should be defined to return 1 by those targets
   which can allow the debugger to immediately follow a vforked
   child, and 0 if they cannot.
   */
#define target_can_follow_vfork_prior_to_exec() \
        (*current_target.to_can_follow_vfork_prior_to_exec) ()

/* An inferior process has been created via a vfork() system call.
   The debugger has followed the parent, the child, or both.  The
   process of setting up for that follow may have required some
   target-specific trickery to track the sequence of reported events.
   If so, this function should be defined by those targets that
   require the debugger to perform cleanup or initialization after
   the vfork follow.
   */
#define target_post_follow_vfork(parent_pid,followed_parent,child_pid,followed_child) \
        (*current_target.to_post_follow_vfork) (parent_pid,followed_parent,child_pid,followed_child)

/* On some targets, we can catch an inferior exec event when it
   occurs.  These functions insert/remove an already-created catchpoint
   for such events.
   */
#define target_insert_exec_catchpoint(pid) \
        (*current_target.to_insert_exec_catchpoint) (pid)
 
#define target_remove_exec_catchpoint(pid) \
        (*current_target.to_remove_exec_catchpoint) (pid)

/* Returns TRUE if PID has invoked a flavor of the exec() system call.
   And, also sets EXECD_PATHNAME to the pathname of the executable file
   that was passed to exec(), and is now being executed.
   */
#define target_has_execd(pid,execd_pathname) \
        (*current_target.to_has_execd) (pid,execd_pathname)

/* Returns the number of exec events that are reported when a process
   invokes a flavor of the exec() system call on this target, if exec
   events are being reported.
   */
#define target_reported_exec_events_per_exec_call() \
        (*current_target.to_reported_exec_events_per_exec_call) ()

/* Returns TRUE if PID has reported a syscall event.  And, also sets
   KIND to the appropriate TARGET_WAITKIND_, and sets SYSCALL_ID to
   the unique integer ID of the syscall.
   */
#define target_has_syscall_event(pid,kind,syscall_id) \
  (*current_target.to_has_syscall_event) (pid,kind,syscall_id)

/* Returns TRUE if PID has exited.  And, also sets EXIT_STATUS to the
   exit code of PID, if any.
   */
#define target_has_exited(pid,wait_status,exit_status) \
        (*current_target.to_has_exited) (pid,wait_status,exit_status)

/* The debugger has completed a blocking wait() call.  There is now
   some process event that must be processed.  This function should
   be defined by those targets that require the debugger to perform
   cleanup or internal state changes in response to the process event.
   */

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

#define target_stop current_target.to_stop

/* Queries the target side for some information.  The first argument is a
   letter specifying the type of the query, which is used to determine who
   should process it.  The second argument is a string that specifies which 
   information is desired and the third is a buffer that carries back the 
   response from the target side. The fourth parameter is the size of the
   output buffer supplied. */
 
#define	target_query(query_type, query, resp_buffer, bufffer_size)	\
	(*current_target.to_query) (query_type, query, resp_buffer, bufffer_size)

/* Get the symbol information for a breakpointable routine called when
   an exception event occurs. 
   Intended mainly for C++, and for those
   platforms/implementations where such a callback mechanism is available,
   e.g. HP-UX with ANSI C++ (aCC).  Some compilers (e.g. g++) support
   different mechanisms for debugging exceptions. */

#define target_enable_exception_callback(kind, enable) \
        (*current_target.to_enable_exception_callback) (kind, enable)

/* Get the current exception event kind -- throw or catch, etc. */
   
#define target_get_current_exception_event() \
        (*current_target.to_get_current_exception_event) ()

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

/* Can the target support the debugger control of thread execution?
   a) Can it lock the thread scheduler?
   b) Can it switch the currently running thread?  */

#define target_can_lock_scheduler \
 	(current_target.to_has_thread_control & tc_schedlock)

#define target_can_switch_threads \
 	(current_target.to_has_thread_control & tc_switch)

extern void target_link PARAMS ((char *, CORE_ADDR *));

/* Converts a process id to a string.  Usually, the string just contains
   `process xyz', but on some systems it may contain
   `process xyz thread abc'.  */

#ifndef target_pid_to_str
#define target_pid_to_str(PID) \
	normal_pid_to_str (PID)
extern char *normal_pid_to_str PARAMS ((int pid));
#endif

#ifndef target_tid_to_str
#define target_tid_to_str(PID) \
        normal_pid_to_str (PID)
extern char *normal_pid_to_str PARAMS ((int pid));
#endif
 

#ifndef target_new_objfile
#define target_new_objfile(OBJFILE)
#endif

#ifndef target_pid_or_tid_to_str
#define target_pid_or_tid_to_str(ID) \
	normal_pid_to_str (ID)
#endif

/* Attempts to find the pathname of the executable file
   that was run to create a specified process.

   The process PID must be stopped when this operation is used.
   
   If the executable file cannot be determined, NULL is returned.

   Else, a pointer to a character string containing the pathname
   is returned.  This string should be copied into a buffer by
   the client if the string will not be immediately used, or if
   it must persist.
   */

#define target_pid_to_exec_file(pid) \
	(current_target.to_pid_to_exec_file) (pid)

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

/* HP-UX supplies these operations, which respectively disable and enable
   the memory page-protections that are used to implement hardware watchpoints
   on that platform.  See wait_for_inferior's use of these.
   */
#if !defined(TARGET_DISABLE_HW_WATCHPOINTS)
#define TARGET_DISABLE_HW_WATCHPOINTS(pid)
#endif

#if !defined(TARGET_ENABLE_HW_WATCHPOINTS)
#define TARGET_ENABLE_HW_WATCHPOINTS(pid)
#endif

/* Provide defaults for systems that don't support hardware watchpoints. */

#ifndef TARGET_HAS_HARDWARE_WATCHPOINTS

/* Returns non-zero if we can set a hardware watchpoint of type TYPE.  TYPE is
   one of bp_hardware_watchpoint, bp_read_watchpoint, bp_write_watchpoint, or
   bp_hardware_breakpoint.  CNT is the number of such watchpoints used so far
   (including this one?).  OTHERTYPE is who knows what...  */

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(TYPE,CNT,OTHERTYPE) 0

#if !defined(TARGET_REGION_SIZE_OK_FOR_HW_WATCHPOINT)
#define TARGET_REGION_SIZE_OK_FOR_HW_WATCHPOINT(byte_count) \
        (LONGEST)(byte_count) <= REGISTER_SIZE
#endif

/* However, some addresses may not be profitable to use hardware to watch,
   or may be difficult to understand when the addressed object is out of
   scope, and hence should be unwatched.  On some targets, this may have
   severe performance penalties, such that we might as well use regular
   watchpoints, and save (possibly precious) hardware watchpoints for other
   locations.
   */
#if !defined(TARGET_RANGE_PROFITABLE_FOR_HW_WATCHPOINT)
#define TARGET_RANGE_PROFITABLE_FOR_HW_WATCHPOINT(pid,start,len) 0
#endif


/* Set/clear a hardware watchpoint starting at ADDR, for LEN bytes.  TYPE is 0
   for write, 1 for read, and 2 for read/write accesses.  Returns 0 for
   success, non-zero for failure.  */

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

/* Sometimes gdb may pick up what appears to be a valid target address
   from a minimal symbol, but the value really means, essentially,
   "This is an index into a table which is populated when the inferior
   is run.  Therefore, do not attempt to use this as a PC."
   */
#if !defined(PC_REQUIRES_RUN_BEFORE_USE)
#define PC_REQUIRES_RUN_BEFORE_USE(pc) (0)
#endif

/* This will only be defined by a target that supports catching vfork events,
   such as HP-UX.

   On some targets (such as HP-UX 10.20 and earlier), resuming a newly vforked
   child process after it has exec'd, causes the parent process to resume as
   well.  To prevent the parent from running spontaneously, such targets should
   define this to a function that prevents that from happening.
   */
#if !defined(ENSURE_VFORKING_PARENT_REMAINS_STOPPED)
#define ENSURE_VFORKING_PARENT_REMAINS_STOPPED(PID) (0)
#endif

/* This will only be defined by a target that supports catching vfork events,
   such as HP-UX.

   On some targets (such as HP-UX 10.20 and earlier), a newly vforked child
   process must be resumed when it delivers its exec event, before the parent
   vfork event will be delivered to us.
   */
#if !defined(RESUME_EXECD_VFORKING_CHILD_TO_GET_PARENT_VFORK)
#define RESUME_EXECD_VFORKING_CHILD_TO_GET_PARENT_VFORK() (0)
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

extern int memory_remove_breakpoint PARAMS ((CORE_ADDR, char *));

extern int memory_insert_breakpoint PARAMS ((CORE_ADDR, char *));

extern breakpoint_from_pc_fn memory_breakpoint_from_pc;
#ifndef BREAKPOINT_FROM_PC
#define BREAKPOINT_FROM_PC(pcptr, lenptr) memory_breakpoint_from_pc (pcptr, lenptr)
#endif


/* From target.c */

extern void
initialize_targets PARAMS ((void));

extern void
noprocess PARAMS ((void));

extern void
find_default_attach PARAMS ((char *, int));

void
find_default_require_attach PARAMS ((char *, int));

void
find_default_require_detach PARAMS ((int, char *, int));

extern void
find_default_create_inferior PARAMS ((char *, char *, char **));

void
find_default_clone_and_follow_inferior PARAMS ((int, int *));

extern struct target_ops *
find_core_target PARAMS ((void));

/* Stuff that should be shared among the various remote targets.  */

/* Debugging level.  0 is off, and non-zero values mean to print some debug
   information (higher values, more information).  */
extern int remote_debug;

/* Speed in bits per second, or -1 which means don't mess with the speed.  */
extern int baud_rate;
/* Timeout limit for response from target. */
extern int remote_timeout;

extern asection *target_memory_bfd_section;

/* Functions for helping to write a native target.  */

/* This is for native targets which use a unix/POSIX-style waitstatus.  */
extern void store_waitstatus PARAMS ((struct target_waitstatus *, int));

/* Convert between host signal numbers and enum target_signal's.  */
extern enum target_signal target_signal_from_host PARAMS ((int));
extern int target_signal_to_host PARAMS ((enum target_signal));

/* Convert from a number used in a GDB command to an enum target_signal.  */
extern enum target_signal target_signal_from_command PARAMS ((int));

/* Any target can call this to switch to remote protocol (in remote.c). */
extern void push_remote_target PARAMS ((char *name, int from_tty));

/* Imported from machine dependent code */

#ifndef SOFTWARE_SINGLE_STEP_P
#define SOFTWARE_SINGLE_STEP_P 0
#define SOFTWARE_SINGLE_STEP(sig,bp_p) abort ()
#endif /* SOFTWARE_SINGLE_STEP_P */

/* Blank target vector entries are initialized to target_ignore. */
void target_ignore PARAMS ((void));

/* Macro for getting target's idea of a frame pointer.
   FIXME: GDB's whole scheme for dealing with "frames" and
   "frame pointers" needs a serious shakedown.  */
#ifndef TARGET_VIRTUAL_FRAME_POINTER
#define TARGET_VIRTUAL_FRAME_POINTER(ADDR, REGP, OFFP) \
   do { *(REGP) = FP_REGNUM; *(OFFP) =  0; } while (0)
#endif /* TARGET_VIRTUAL_FRAME_POINTER */

#endif	/* !defined (TARGET_H) */
