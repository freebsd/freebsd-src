/* Variables that describe the inferior process running under GDB:
   Where it is, why it stopped, and how to step it.
   Copyright 1986, 1989, 1992 Free Software Foundation, Inc.

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

#if !defined (INFERIOR_H)
#define INFERIOR_H 1

/* For bpstat.  */
#include "breakpoint.h"

/* For FRAME_ADDR.  */
#include "frame.h"

/* For enum target_signal.  */
#include "target.h"

/*
 * Structure in which to save the status of the inferior.  Save
 * through "save_inferior_status", restore through
 * "restore_inferior_status".
 * This pair of routines should be called around any transfer of
 * control to the inferior which you don't want showing up in your
 * control variables.
 */
struct inferior_status {
  enum target_signal stop_signal;
  CORE_ADDR stop_pc;
  FRAME_ADDR stop_frame_address;
  bpstat stop_bpstat;
  int stop_step;
  int stop_stack_dummy;
  int stopped_by_random_signal;
  int trap_expected;
  CORE_ADDR step_range_start;
  CORE_ADDR step_range_end;
  FRAME_ADDR step_frame_address;
  int step_over_calls;
  CORE_ADDR step_resume_break_address;
  int stop_after_trap;
  int stop_soon_quietly;
  FRAME_ADDR selected_frame_address;
  int selected_level;
  char stop_registers[REGISTER_BYTES];

  /* These are here because if call_function_by_hand has written some
     registers and then decides to call error(), we better not have changed
     any registers.  */
  char registers[REGISTER_BYTES];

  int breakpoint_proceeded;
  int restore_stack_info;
  int proceed_to_finish;
};

/* This macro gives the number of registers actually in use by the
   inferior.  This may be less than the total number of registers,
   perhaps depending on the actual CPU in use or program being run.  */

#ifndef ARCH_NUM_REGS
#define ARCH_NUM_REGS NUM_REGS
#endif

extern void
save_inferior_status PARAMS ((struct inferior_status *, int));

extern void
restore_inferior_status PARAMS ((struct inferior_status *));

extern void set_sigint_trap PARAMS ((void));
extern void clear_sigint_trap PARAMS ((void));

extern void set_sigio_trap PARAMS ((void));
extern void clear_sigio_trap PARAMS ((void));

/* File name for default use for standard in/out in the inferior.  */

extern char *inferior_io_terminal;

/* Pid of our debugged inferior, or 0 if no inferior now.  */

extern int inferior_pid;

/* Character array containing an image of the inferior programs' registers.  */

extern char registers[];

/* Array of validity bits (one per register).  Nonzero at position XXX_REGNUM
   means that `registers' contains a valid copy of inferior register XXX.  */

extern char register_valid[NUM_REGS];

extern void
clear_proceed_status PARAMS ((void));

extern void
proceed PARAMS ((CORE_ADDR, enum target_signal, int));

extern void
kill_inferior PARAMS ((void));

extern void
generic_mourn_inferior PARAMS ((void));

extern void
terminal_ours PARAMS ((void));

extern int run_stack_dummy PARAMS ((CORE_ADDR, char [REGISTER_BYTES]));

extern CORE_ADDR
read_pc PARAMS ((void));

extern CORE_ADDR
read_pc_pid PARAMS ((int));

extern void
write_pc PARAMS ((CORE_ADDR));

extern CORE_ADDR
read_sp PARAMS ((void));

extern void
write_sp PARAMS ((CORE_ADDR));

extern CORE_ADDR
read_fp PARAMS ((void));

extern void
write_fp PARAMS ((CORE_ADDR));

extern void
wait_for_inferior PARAMS ((void));

extern void
init_wait_for_inferior PARAMS ((void));

extern void
close_exec_file PARAMS ((void));

extern void
reopen_exec_file PARAMS ((void));

/* The `resume' routine should only be called in special circumstances.
   Normally, use `proceed', which handles a lot of bookkeeping.  */
extern void
resume PARAMS ((int, enum target_signal));

/* From misc files */

extern void
store_inferior_registers PARAMS ((int));

extern void
fetch_inferior_registers PARAMS ((int));

extern void 
solib_create_inferior_hook PARAMS ((void));

extern void
child_terminal_info PARAMS ((char *, int));

extern void
term_info PARAMS ((char *, int));

extern void
terminal_ours_for_output PARAMS ((void));

extern void
terminal_inferior PARAMS ((void));

extern void
terminal_init_inferior PARAMS ((void));

/* From infptrace.c */

extern int
attach PARAMS ((int));

void
detach PARAMS ((int));

extern void
child_resume PARAMS ((int, int, enum target_signal));

#ifndef PTRACE_ARG3_TYPE
#define PTRACE_ARG3_TYPE int	/* Correct definition for most systems. */
#endif

extern int
call_ptrace PARAMS ((int, int, PTRACE_ARG3_TYPE, int));

/* From procfs.c */

extern int
proc_iterate_over_mappings PARAMS ((int (*) (int, CORE_ADDR)));

/* From fork-child.c */

extern void fork_inferior PARAMS ((char *, char *, char **,
				   void (*) (void),
				   void (*) (int), char *));

extern void startup_inferior PARAMS ((int));

/* From inflow.c */

extern void
new_tty_prefork PARAMS ((char *));

extern int gdb_has_a_terminal PARAMS ((void));

/* From infrun.c */

extern void
start_remote PARAMS ((void));

extern void
normal_stop PARAMS ((void));

extern int
signal_stop_state PARAMS ((int));

extern int
signal_print_state PARAMS ((int));

extern int
signal_pass_state PARAMS ((int));

/* From infcmd.c */

extern void
tty_command PARAMS ((char *, int));

extern void
attach_command PARAMS ((char *, int));

/* Last signal that the inferior received (why it stopped).  */

extern enum target_signal stop_signal;

/* Address at which inferior stopped.  */

extern CORE_ADDR stop_pc;

/* Stack frame when program stopped.  */

extern FRAME_ADDR stop_frame_address;

/* Chain containing status of breakpoint(s) that we have stopped at.  */

extern bpstat stop_bpstat;

/* Flag indicating that a command has proceeded the inferior past the
   current breakpoint.  */

extern int breakpoint_proceeded;

/* Nonzero if stopped due to a step command.  */

extern int stop_step;

/* Nonzero if stopped due to completion of a stack dummy routine.  */

extern int stop_stack_dummy;

/* Nonzero if program stopped due to a random (unexpected) signal in
   inferior process.  */

extern int stopped_by_random_signal;

/* Range to single step within.
   If this is nonzero, respond to a single-step signal
   by continuing to step if the pc is in this range.

   If step_range_start and step_range_end are both 1, it means to step for
   a single instruction (FIXME: it might clean up wait_for_inferior in a
   minor way if this were changed to the address of the instruction and
   that address plus one.  But maybe not.).  */

extern CORE_ADDR step_range_start; /* Inclusive */
extern CORE_ADDR step_range_end; /* Exclusive */

/* Stack frame address as of when stepping command was issued.
   This is how we know when we step into a subroutine call,
   and how to set the frame for the breakpoint used to step out.  */

extern FRAME_ADDR step_frame_address;

/* 1 means step over all subroutine calls.
   -1 means step over calls to undebuggable functions.  */

extern int step_over_calls;

/* If stepping, nonzero means step count is > 1
   so don't print frame next time inferior stops
   if it stops due to stepping.  */

extern int step_multi;

/* Nonzero means expecting a trap and caller will handle it themselves.
   It is used after attach, due to attaching to a process;
   when running in the shell before the child program has been exec'd;
   and when running some kinds of remote stuff (FIXME?).  */

extern int stop_soon_quietly;

/* Nonzero if proceed is being used for a "finish" command or a similar
   situation when stop_registers should be saved.  */

extern int proceed_to_finish;

/* Save register contents here when about to pop a stack dummy frame,
   if-and-only-if proceed_to_finish is set.
   Thus this contains the return value from the called function (assuming
   values are returned in a register).  */

extern char stop_registers[REGISTER_BYTES];

/* Nonzero if the child process in inferior_pid was attached rather
   than forked.  */

extern int attach_flag;

/* Sigtramp is a routine that the kernel calls (which then calls the
   signal handler).  On most machines it is a library routine that
   is linked into the executable.

   This macro, given a program counter value and the name of the
   function in which that PC resides (which can be null if the
   name is not known), returns nonzero if the PC and name show
   that we are in sigtramp.

   On most machines just see if the name is sigtramp (and if we have
   no name, assume we are not in sigtramp).  */
#if !defined (IN_SIGTRAMP)
#  if defined (SIGTRAMP_START)
#    define IN_SIGTRAMP(pc, name) \
       ((pc) >= SIGTRAMP_START   \
        && (pc) < SIGTRAMP_END \
        )
#  else
#    define IN_SIGTRAMP(pc, name) \
       (name && STREQ ("_sigtramp", name))
#  endif
#endif

/* Possible values for CALL_DUMMY_LOCATION.  */
#define ON_STACK 1
#define BEFORE_TEXT_END 2
#define AFTER_TEXT_END 3
#define AT_ENTRY_POINT 4

#if !defined (CALL_DUMMY_LOCATION)
#define CALL_DUMMY_LOCATION ON_STACK
#endif /* No CALL_DUMMY_LOCATION.  */

/* Are we in a call dummy?  The code below which allows DECR_PC_AFTER_BREAK
   below is for infrun.c, which may give the macro a pc without that
   subtracted out.  */
#if !defined (PC_IN_CALL_DUMMY)
#if CALL_DUMMY_LOCATION == BEFORE_TEXT_END
extern CORE_ADDR text_end;
#define PC_IN_CALL_DUMMY(pc, sp, frame_address) \
  ((pc) >= text_end - CALL_DUMMY_LENGTH         \
   && (pc) <= text_end + DECR_PC_AFTER_BREAK)
#endif /* Before text_end.  */

#if CALL_DUMMY_LOCATION == AFTER_TEXT_END
extern CORE_ADDR text_end;
#define PC_IN_CALL_DUMMY(pc, sp, frame_address) \
  ((pc) >= text_end   \
   && (pc) <= text_end + CALL_DUMMY_LENGTH + DECR_PC_AFTER_BREAK)
#endif /* After text_end.  */

#if CALL_DUMMY_LOCATION == ON_STACK
/* Is the PC in a call dummy?  SP and FRAME_ADDRESS are the bottom and
   top of the stack frame which we are checking, where "bottom" and
   "top" refer to some section of memory which contains the code for
   the call dummy.  Calls to this macro assume that the contents of
   SP_REGNUM and FP_REGNUM (or the saved values thereof), respectively,
   are the things to pass.

   This won't work on the 29k, where SP_REGNUM and FP_REGNUM don't
   have that meaning, but the 29k doesn't use ON_STACK.  This could be
   fixed by generalizing this scheme, perhaps by passing in a frame
   and adding a few fields, at least on machines which need them for
   PC_IN_CALL_DUMMY.

   Something simpler, like checking for the stack segment, doesn't work,
   since various programs (threads implementations, gcc nested function
   stubs, etc) may either allocate stack frames in another segment, or
   allocate other kinds of code on the stack.  */

#define PC_IN_CALL_DUMMY(pc, sp, frame_address) \
  ((sp) INNER_THAN (pc) && (frame_address != 0) && (pc) INNER_THAN (frame_address))
#endif /* On stack.  */

#if CALL_DUMMY_LOCATION == AT_ENTRY_POINT
#define PC_IN_CALL_DUMMY(pc, sp, frame_address)			\
  ((pc) >= CALL_DUMMY_ADDRESS ()				\
   && (pc) <= (CALL_DUMMY_ADDRESS () + DECR_PC_AFTER_BREAK))
#endif /* At entry point.  */
#endif /* No PC_IN_CALL_DUMMY.  */

#endif	/* !defined (INFERIOR_H) */
