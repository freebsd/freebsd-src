/* Target-struct-independent code to start (run) and stop an inferior process.
   Copyright 1986, 87, 88, 89, 91, 92, 93, 94, 95, 96, 97, 1998
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdb_string.h"
#include <ctype.h>
#include "symtab.h"
#include "frame.h"
#include "inferior.h"
#include "breakpoint.h"
#include "wait.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "target.h"
#include "gdbthread.h"
#include "annotate.h"
#include "symfile.h"		/* for overlay functions */

#include <signal.h>

/* Prototypes for local functions */

static void signals_info PARAMS ((char *, int));

static void handle_command PARAMS ((char *, int));

static void sig_print_info PARAMS ((enum target_signal));

static void sig_print_header PARAMS ((void));

static void resume_cleanups PARAMS ((int));

static int hook_stop_stub PARAMS ((PTR));

static void delete_breakpoint_current_contents PARAMS ((PTR));

int inferior_ignoring_startup_exec_events = 0;
int inferior_ignoring_leading_exec_events = 0;

#ifdef HPUXHPPA
/* wait_for_inferior and normal_stop use this to notify the user
   when the inferior stopped in a different thread than it had been
   running in. */
static int switched_from_inferior_pid;
#endif

/* resume and wait_for_inferior use this to ensure that when
   stepping over a hit breakpoint in a threaded application
   only the thread that hit the breakpoint is stepped and the
   other threads don't continue.  This prevents having another
   thread run past the breakpoint while it is temporarily
   removed.

   This is not thread-specific, so it isn't saved as part of
   the infrun state.

   Versions of gdb which don't use the "step == this thread steps
   and others continue" model but instead use the "step == this
   thread steps and others wait" shouldn't do this. */
static int thread_step_needed = 0;

void _initialize_infrun PARAMS ((void));

/* GET_LONGJMP_TARGET returns the PC at which longjmp() will resume the
   program.  It needs to examine the jmp_buf argument and extract the PC
   from it.  The return value is non-zero on success, zero otherwise. */

#ifndef GET_LONGJMP_TARGET
#define GET_LONGJMP_TARGET(PC_ADDR) 0
#endif


/* Some machines have trampoline code that sits between function callers
   and the actual functions themselves.  If this machine doesn't have
   such things, disable their processing.  */

#ifndef SKIP_TRAMPOLINE_CODE
#define	SKIP_TRAMPOLINE_CODE(pc)	0
#endif

/* Dynamic function trampolines are similar to solib trampolines in that they
   are between the caller and the callee.  The difference is that when you
   enter a dynamic trampoline, you can't determine the callee's address.  Some
   (usually complex) code needs to run in the dynamic trampoline to figure out
   the callee's address.  This macro is usually called twice.  First, when we
   enter the trampoline (looks like a normal function call at that point).  It
   should return the PC of a point within the trampoline where the callee's
   address is known.  Second, when we hit the breakpoint, this routine returns
   the callee's address.  At that point, things proceed as per a step resume
   breakpoint.  */

#ifndef DYNAMIC_TRAMPOLINE_NEXTPC
#define DYNAMIC_TRAMPOLINE_NEXTPC(pc) 0
#endif

/* On SVR4 based systems, determining the callee's address is exceedingly
   difficult and depends on the implementation of the run time loader.
   If we are stepping at the source level, we single step until we exit
   the run time loader code and reach the callee's address.  */

#ifndef IN_SOLIB_DYNSYM_RESOLVE_CODE
#define IN_SOLIB_DYNSYM_RESOLVE_CODE(pc) 0
#endif

/* For SVR4 shared libraries, each call goes through a small piece of
   trampoline code in the ".plt" section.  IN_SOLIB_CALL_TRAMPOLINE evaluates
   to nonzero if we are current stopped in one of these. */

#ifndef IN_SOLIB_CALL_TRAMPOLINE
#define IN_SOLIB_CALL_TRAMPOLINE(pc,name)	0
#endif

/* In some shared library schemes, the return path from a shared library
   call may need to go through a trampoline too.  */

#ifndef IN_SOLIB_RETURN_TRAMPOLINE
#define IN_SOLIB_RETURN_TRAMPOLINE(pc,name)	0
#endif

/* This function returns TRUE if pc is the address of an instruction
   that lies within the dynamic linker (such as the event hook, or the
   dld itself).

   This function must be used only when a dynamic linker event has
   been caught, and the inferior is being stepped out of the hook, or
   undefined results are guaranteed.  */

#ifndef SOLIB_IN_DYNAMIC_LINKER
#define SOLIB_IN_DYNAMIC_LINKER(pid,pc) 0
#endif

/* On MIPS16, a function that returns a floating point value may call
   a library helper function to copy the return value to a floating point
   register.  The IGNORE_HELPER_CALL macro returns non-zero if we
   should ignore (i.e. step over) this function call.  */
#ifndef IGNORE_HELPER_CALL
#define IGNORE_HELPER_CALL(pc)	0
#endif

/* On some systems, the PC may be left pointing at an instruction that  won't
   actually be executed.  This is usually indicated by a bit in the PSW.  If
   we find ourselves in such a state, then we step the target beyond the
   nullified instruction before returning control to the user so as to avoid
   confusion. */

#ifndef INSTRUCTION_NULLIFIED
#define INSTRUCTION_NULLIFIED 0
#endif

/* Tables of how to react to signals; the user sets them.  */

static unsigned char *signal_stop;
static unsigned char *signal_print;
static unsigned char *signal_program;

#define SET_SIGS(nsigs,sigs,flags) \
  do { \
    int signum = (nsigs); \
    while (signum-- > 0) \
      if ((sigs)[signum]) \
	(flags)[signum] = 1; \
  } while (0)

#define UNSET_SIGS(nsigs,sigs,flags) \
  do { \
    int signum = (nsigs); \
    while (signum-- > 0) \
      if ((sigs)[signum]) \
	(flags)[signum] = 0; \
  } while (0)


/* Command list pointer for the "stop" placeholder.  */

static struct cmd_list_element *stop_command;

/* Nonzero if breakpoints are now inserted in the inferior.  */

static int breakpoints_inserted;

/* Function inferior was in as of last step command.  */

static struct symbol *step_start_function;

/* Nonzero if we are expecting a trace trap and should proceed from it.  */

static int trap_expected;

#ifdef SOLIB_ADD
/* Nonzero if we want to give control to the user when we're notified
   of shared library events by the dynamic linker.  */
static int stop_on_solib_events;
#endif

#ifdef HP_OS_BUG
/* Nonzero if the next time we try to continue the inferior, it will
   step one instruction and generate a spurious trace trap.
   This is used to compensate for a bug in HP-UX.  */

static int trap_expected_after_continue;
#endif

/* Nonzero means expecting a trace trap
   and should stop the inferior and return silently when it happens.  */

int stop_after_trap;

/* Nonzero means expecting a trap and caller will handle it themselves.
   It is used after attach, due to attaching to a process;
   when running in the shell before the child program has been exec'd;
   and when running some kinds of remote stuff (FIXME?).  */

int stop_soon_quietly;

/* Nonzero if proceed is being used for a "finish" command or a similar
   situation when stop_registers should be saved.  */

int proceed_to_finish;

/* Save register contents here when about to pop a stack dummy frame,
   if-and-only-if proceed_to_finish is set.
   Thus this contains the return value from the called function (assuming
   values are returned in a register).  */

char stop_registers[REGISTER_BYTES];

/* Nonzero if program stopped due to error trying to insert breakpoints.  */

static int breakpoints_failed;

/* Nonzero after stop if current stack frame should be printed.  */

static int stop_print_frame;

static struct breakpoint *step_resume_breakpoint = NULL;
static struct breakpoint *through_sigtramp_breakpoint = NULL;

/* On some platforms (e.g., HP-UX), hardware watchpoints have bad
   interactions with an inferior that is running a kernel function
   (aka, a system call or "syscall").  wait_for_inferior therefore
   may have a need to know when the inferior is in a syscall.  This
   is a count of the number of inferior threads which are known to
   currently be running in a syscall. */
static int number_of_threads_in_syscalls;

/* This is used to remember when a fork, vfork or exec event
   was caught by a catchpoint, and thus the event is to be
   followed at the next resume of the inferior, and not
   immediately. */
static struct
  {
    enum target_waitkind kind;
    struct
      {
	int parent_pid;
	int saw_parent_fork;
	int child_pid;
	int saw_child_fork;
	int saw_child_exec;
      }
    fork_event;
    char *execd_pathname;
  }
pending_follow;

/* Some platforms don't allow us to do anything meaningful with a
   vforked child until it has exec'd.  Vforked processes on such
   platforms can only be followed after they've exec'd.

   When this is set to 0, a vfork can be immediately followed,
   and an exec can be followed merely as an exec.  When this is
   set to 1, a vfork event has been seen, but cannot be followed
   until the exec is seen.

   (In the latter case, inferior_pid is still the parent of the
   vfork, and pending_follow.fork_event.child_pid is the child.  The
   appropriate process is followed, according to the setting of
   follow-fork-mode.) */
static int follow_vfork_when_exec;

static char *follow_fork_mode_kind_names[] =
{
/* ??rehrauer:  The "both" option is broken, by what may be a 10.20
   kernel problem.  It's also not terribly useful without a GUI to
   help the user drive two debuggers.  So for now, I'm disabling
   the "both" option.
  "parent", "child", "both", "ask" };
  */
  "parent", "child", "ask"};

static char *follow_fork_mode_string = NULL;


#if defined(HPUXHPPA)
static void
follow_inferior_fork (parent_pid, child_pid, has_forked, has_vforked)
     int parent_pid;
     int child_pid;
     int has_forked;
     int has_vforked;
{
  int followed_parent = 0;
  int followed_child = 0;
  int ima_clone = 0;

  /* Which process did the user want us to follow? */
  char *follow_mode =
  savestring (follow_fork_mode_string, strlen (follow_fork_mode_string));

  /* Or, did the user not know, and want us to ask? */
  if (STREQ (follow_fork_mode_string, "ask"))
    {
      char requested_mode[100];

      free (follow_mode);
      error ("\"ask\" mode NYI");
      follow_mode = savestring (requested_mode, strlen (requested_mode));
    }

  /* If we're to be following the parent, then detach from child_pid.
     We're already following the parent, so need do nothing explicit
     for it. */
  if (STREQ (follow_mode, "parent"))
    {
      followed_parent = 1;

      /* We're already attached to the parent, by default. */

      /* Before detaching from the child, remove all breakpoints from
         it.  (This won't actually modify the breakpoint list, but will
         physically remove the breakpoints from the child.) */
      if (!has_vforked || !follow_vfork_when_exec)
	{
	  detach_breakpoints (child_pid);
	  SOLIB_REMOVE_INFERIOR_HOOK (child_pid);
	}

      /* Detach from the child. */
      dont_repeat ();

      target_require_detach (child_pid, "", 1);
    }

  /* If we're to be following the child, then attach to it, detach
     from inferior_pid, and set inferior_pid to child_pid. */
  else if (STREQ (follow_mode, "child"))
    {
      char child_pid_spelling[100];	/* Arbitrary length. */

      followed_child = 1;

      /* Before detaching from the parent, detach all breakpoints from
         the child.  But only if we're forking, or if we follow vforks
         as soon as they happen.  (If we're following vforks only when
         the child has exec'd, then it's very wrong to try to write
         back the "shadow contents" of inserted breakpoints now -- they
         belong to the child's pre-exec'd a.out.) */
      if (!has_vforked || !follow_vfork_when_exec)
	{
	  detach_breakpoints (child_pid);
	}

      /* Before detaching from the parent, remove all breakpoints from it. */
      remove_breakpoints ();

      /* Also reset the solib inferior hook from the parent. */
      SOLIB_REMOVE_INFERIOR_HOOK (inferior_pid);

      /* Detach from the parent. */
      dont_repeat ();
      target_detach (NULL, 1);

      /* Attach to the child. */
      inferior_pid = child_pid;
      sprintf (child_pid_spelling, "%d", child_pid);
      dont_repeat ();

      target_require_attach (child_pid_spelling, 1);

      /* Was there a step_resume breakpoint?  (There was if the user
         did a "next" at the fork() call.)  If so, explicitly reset its
         thread number.

         step_resumes are a form of bp that are made to be per-thread.
         Since we created the step_resume bp when the parent process
         was being debugged, and now are switching to the child process,
         from the breakpoint package's viewpoint, that's a switch of
         "threads".  We must update the bp's notion of which thread
         it is for, or it'll be ignored when it triggers... */
      if (step_resume_breakpoint &&
	  (!has_vforked || !follow_vfork_when_exec))
	breakpoint_re_set_thread (step_resume_breakpoint);

      /* Reinsert all breakpoints in the child.  (The user may've set
         breakpoints after catching the fork, in which case those
         actually didn't get set in the child, but only in the parent.) */
      if (!has_vforked || !follow_vfork_when_exec)
	{
	  breakpoint_re_set ();
	  insert_breakpoints ();
	}
    }

  /* If we're to be following both parent and child, then fork ourselves,
     and attach the debugger clone to the child. */
  else if (STREQ (follow_mode, "both"))
    {
      char pid_suffix[100];	/* Arbitrary length. */

      /* Clone ourselves to follow the child.  This is the end of our
       involvement with child_pid; our clone will take it from here... */
      dont_repeat ();
      target_clone_and_follow_inferior (child_pid, &followed_child);
      followed_parent = !followed_child;

      /* We continue to follow the parent.  To help distinguish the two
         debuggers, though, both we and our clone will reset our prompts. */
      sprintf (pid_suffix, "[%d] ", inferior_pid);
      set_prompt (strcat (get_prompt (), pid_suffix));
    }

  /* The parent and child of a vfork share the same address space.
     Also, on some targets the order in which vfork and exec events
     are received for parent in child requires some delicate handling
     of the events.

     For instance, on ptrace-based HPUX we receive the child's vfork
     event first, at which time the parent has been suspended by the
     OS and is essentially untouchable until the child's exit or second
     exec event arrives.  At that time, the parent's vfork event is
     delivered to us, and that's when we see and decide how to follow
     the vfork.  But to get to that point, we must continue the child
     until it execs or exits.  To do that smoothly, all breakpoints
     must be removed from the child, in case there are any set between
     the vfork() and exec() calls.  But removing them from the child
     also removes them from the parent, due to the shared-address-space
     nature of a vfork'd parent and child.  On HPUX, therefore, we must
     take care to restore the bp's to the parent before we continue it.
     Else, it's likely that we may not stop in the expected place.  (The
     worst scenario is when the user tries to step over a vfork() call;
     the step-resume bp must be restored for the step to properly stop
     in the parent after the call completes!)

     Sequence of events, as reported to gdb from HPUX:

           Parent        Child           Action for gdb to take
         -------------------------------------------------------
        1                VFORK               Continue child
        2                EXEC
        3                EXEC or EXIT
        4  VFORK */
  if (has_vforked)
    {
      target_post_follow_vfork (parent_pid,
				followed_parent,
				child_pid,
				followed_child);
    }

  pending_follow.fork_event.saw_parent_fork = 0;
  pending_follow.fork_event.saw_child_fork = 0;

  free (follow_mode);
}

static void
follow_fork (parent_pid, child_pid)
     int parent_pid;
     int child_pid;
{
  follow_inferior_fork (parent_pid, child_pid, 1, 0);
}


/* Forward declaration. */
static void follow_exec PARAMS ((int, char *));

static void
follow_vfork (parent_pid, child_pid)
     int parent_pid;
     int child_pid;
{
  follow_inferior_fork (parent_pid, child_pid, 0, 1);

  /* Did we follow the child?  Had it exec'd before we saw the parent vfork? */
  if (pending_follow.fork_event.saw_child_exec && (inferior_pid == child_pid))
    {
      pending_follow.fork_event.saw_child_exec = 0;
      pending_follow.kind = TARGET_WAITKIND_SPURIOUS;
      follow_exec (inferior_pid, pending_follow.execd_pathname);
      free (pending_follow.execd_pathname);
    }
}
#endif /* HPUXHPPA */

static void
follow_exec (pid, execd_pathname)
     int pid;
     char *execd_pathname;
{
#ifdef HPUXHPPA
  int saved_pid = pid;
  extern struct target_ops child_ops;

  /* Did this exec() follow a vfork()?  If so, we must follow the
     vfork now too.  Do it before following the exec. */
  if (follow_vfork_when_exec &&
      (pending_follow.kind == TARGET_WAITKIND_VFORKED))
    {
      pending_follow.kind = TARGET_WAITKIND_SPURIOUS;
      follow_vfork (inferior_pid, pending_follow.fork_event.child_pid);
      follow_vfork_when_exec = 0;
      saved_pid = inferior_pid;

      /* Did we follow the parent?  If so, we're done.  If we followed
         the child then we must also follow its exec(). */
      if (inferior_pid == pending_follow.fork_event.parent_pid)
	return;
    }

  /* This is an exec event that we actually wish to pay attention to.
     Refresh our symbol table to the newly exec'd program, remove any
     momentary bp's, etc.

     If there are breakpoints, they aren't really inserted now,
     since the exec() transformed our inferior into a fresh set
     of instructions.

     We want to preserve symbolic breakpoints on the list, since
     we have hopes that they can be reset after the new a.out's
     symbol table is read.

     However, any "raw" breakpoints must be removed from the list
     (e.g., the solib bp's), since their address is probably invalid
     now.

     And, we DON'T want to call delete_breakpoints() here, since
     that may write the bp's "shadow contents" (the instruction
     value that was overwritten witha TRAP instruction).  Since
     we now have a new a.out, those shadow contents aren't valid. */
  update_breakpoints_after_exec ();

  /* If there was one, it's gone now.  We cannot truly step-to-next
     statement through an exec(). */
  step_resume_breakpoint = NULL;
  step_range_start = 0;
  step_range_end = 0;

  /* If there was one, it's gone now. */
  through_sigtramp_breakpoint = NULL;

  /* What is this a.out's name? */
  printf_unfiltered ("Executing new program: %s\n", execd_pathname);

  /* We've followed the inferior through an exec.  Therefore, the
     inferior has essentially been killed & reborn. */
  gdb_flush (gdb_stdout);
  target_mourn_inferior ();
  inferior_pid = saved_pid;	/* Because mourn_inferior resets inferior_pid. */
  push_target (&child_ops);

  /* That a.out is now the one to use. */
  exec_file_attach (execd_pathname, 0);

  /* And also is where symbols can be found. */
  symbol_file_command (execd_pathname, 0);

  /* Reset the shared library package.  This ensures that we get
     a shlib event when the child reaches "_start", at which point
     the dld will have had a chance to initialize the child. */
  SOLIB_RESTART ();
  SOLIB_CREATE_INFERIOR_HOOK (inferior_pid);

  /* Reinsert all breakpoints.  (Those which were symbolic have
     been reset to the proper address in the new a.out, thanks
     to symbol_file_command...) */
  insert_breakpoints ();

  /* The next resume of this inferior should bring it to the shlib
     startup breakpoints.  (If the user had also set bp's on
     "main" from the old (parent) process, then they'll auto-
     matically get reset there in the new process.) */
#endif
}

/* Non-zero if we just simulating a single-step.  This is needed
   because we cannot remove the breakpoints in the inferior process
   until after the `wait' in `wait_for_inferior'.  */
static int singlestep_breakpoints_inserted_p = 0;


/* Things to clean up if we QUIT out of resume ().  */
/* ARGSUSED */
static void
resume_cleanups (arg)
     int arg;
{
  normal_stop ();
}

static char schedlock_off[] = "off";
static char schedlock_on[] = "on";
static char schedlock_step[] = "step";
static char *scheduler_mode = schedlock_off;
static char *scheduler_enums[] =
{schedlock_off, schedlock_on, schedlock_step};

static void
set_schedlock_func (args, from_tty, c)
     char *args;
     int from_tty;
     struct cmd_list_element *c;
{
  if (c->type == set_cmd)
    if (!target_can_lock_scheduler)
      {
	scheduler_mode = schedlock_off;
	error ("Target '%s' cannot support this command.",
	       target_shortname);
      }
}


/* Resume the inferior, but allow a QUIT.  This is useful if the user
   wants to interrupt some lengthy single-stepping operation
   (for child processes, the SIGINT goes to the inferior, and so
   we get a SIGINT random_signal, but for remote debugging and perhaps
   other targets, that's not true).

   STEP nonzero if we should step (zero to continue instead).
   SIG is the signal to give the inferior (zero for none).  */
void
resume (step, sig)
     int step;
     enum target_signal sig;
{
  int should_resume = 1;
  struct cleanup *old_cleanups = make_cleanup ((make_cleanup_func)
					       resume_cleanups, 0);
  QUIT;

#ifdef CANNOT_STEP_BREAKPOINT
  /* Most targets can step a breakpoint instruction, thus executing it
     normally.  But if this one cannot, just continue and we will hit
     it anyway.  */
  if (step && breakpoints_inserted && breakpoint_here_p (read_pc ()))
    step = 0;
#endif

  if (SOFTWARE_SINGLE_STEP_P && step)
    {
      /* Do it the hard way, w/temp breakpoints */
      SOFTWARE_SINGLE_STEP (sig, 1 /*insert-breakpoints*/ );
      /* ...and don't ask hardware to do it.  */
      step = 0;
      /* and do not pull these breakpoints until after a `wait' in
         `wait_for_inferior' */
      singlestep_breakpoints_inserted_p = 1;
    }

  /* Handle any optimized stores to the inferior NOW...  */
#ifdef DO_DEFERRED_STORES
  DO_DEFERRED_STORES;
#endif

#ifdef HPUXHPPA
  /* If there were any forks/vforks/execs that were caught and are
     now to be followed, then do so. */
  switch (pending_follow.kind)
    {
    case (TARGET_WAITKIND_FORKED):
      pending_follow.kind = TARGET_WAITKIND_SPURIOUS;
      follow_fork (inferior_pid, pending_follow.fork_event.child_pid);
      break;

    case (TARGET_WAITKIND_VFORKED):
      {
	int saw_child_exec = pending_follow.fork_event.saw_child_exec;

	pending_follow.kind = TARGET_WAITKIND_SPURIOUS;
	follow_vfork (inferior_pid, pending_follow.fork_event.child_pid);

	/* Did we follow the child, but not yet see the child's exec event?
             If so, then it actually ought to be waiting for us; we respond to
             parent vfork events.  We don't actually want to resume the child
             in this situation; we want to just get its exec event. */
	if (!saw_child_exec &&
	    (inferior_pid == pending_follow.fork_event.child_pid))
	  should_resume = 0;
      }
      break;

    case (TARGET_WAITKIND_EXECD):
      /* If we saw a vfork event but couldn't follow it until we saw
           an exec, then now might be the time! */
      pending_follow.kind = TARGET_WAITKIND_SPURIOUS;
      /* follow_exec is called as soon as the exec event is seen. */
      break;

    default:
      break;
    }
#endif /* HPUXHPPA */

  /* Install inferior's terminal modes.  */
  target_terminal_inferior ();

  if (should_resume)
    {
#ifdef HPUXHPPA
      if (thread_step_needed)
	{
	  /* We stopped on a BPT instruction;
	     don't continue other threads and
	     just step this thread. */
	  thread_step_needed = 0;

	  if (!breakpoint_here_p (read_pc ()))
	    {
	      /* Breakpoint deleted: ok to do regular resume
		 where all the threads either step or continue. */
	      target_resume (-1, step, sig);
	    }
	  else
	    {
	      if (!step)
		{
		  warning ("Internal error, changing continue to step.");
		  remove_breakpoints ();
		  breakpoints_inserted = 0;
		  trap_expected = 1;
		  step = 1;
		}

	      target_resume (inferior_pid, step, sig);
	    }
	}
      else
#endif /* HPUXHPPA */
	{
	  /* Vanilla resume. */

	  if ((scheduler_mode == schedlock_on) ||
	      (scheduler_mode == schedlock_step && step != 0))
	    target_resume (inferior_pid, step, sig);
	  else
	    target_resume (-1, step, sig);
	}
    }

  discard_cleanups (old_cleanups);
}


/* Clear out all variables saying what to do when inferior is continued.
   First do this, then set the ones you want, then call `proceed'.  */

void
clear_proceed_status ()
{
  trap_expected = 0;
  step_range_start = 0;
  step_range_end = 0;
  step_frame_address = 0;
  step_over_calls = -1;
  stop_after_trap = 0;
  stop_soon_quietly = 0;
  proceed_to_finish = 0;
  breakpoint_proceeded = 1;	/* We're about to proceed... */

  /* Discard any remaining commands or status from previous stop.  */
  bpstat_clear (&stop_bpstat);
}

/* Basic routine for continuing the program in various fashions.

   ADDR is the address to resume at, or -1 for resume where stopped.
   SIGGNAL is the signal to give it, or 0 for none,
     or -1 for act according to how it stopped.
   STEP is nonzero if should trap after one instruction.
     -1 means return after that and print nothing.
     You should probably set various step_... variables
     before calling here, if you are stepping.

   You should call clear_proceed_status before calling proceed.  */

void
proceed (addr, siggnal, step)
     CORE_ADDR addr;
     enum target_signal siggnal;
     int step;
{
  int oneproc = 0;

  if (step > 0)
    step_start_function = find_pc_function (read_pc ());
  if (step < 0)
    stop_after_trap = 1;

  if (addr == (CORE_ADDR) - 1)
    {
      /* If there is a breakpoint at the address we will resume at,
	 step one instruction before inserting breakpoints
	 so that we do not stop right away (and report a second
         hit at this breakpoint).  */

      if (read_pc () == stop_pc && breakpoint_here_p (read_pc ()))
	oneproc = 1;

#ifndef STEP_SKIPS_DELAY
#define STEP_SKIPS_DELAY(pc) (0)
#define STEP_SKIPS_DELAY_P (0)
#endif
      /* Check breakpoint_here_p first, because breakpoint_here_p is fast
	 (it just checks internal GDB data structures) and STEP_SKIPS_DELAY
	 is slow (it needs to read memory from the target).  */
      if (STEP_SKIPS_DELAY_P
	  && breakpoint_here_p (read_pc () + 4)
	  && STEP_SKIPS_DELAY (read_pc ()))
	oneproc = 1;
    }
  else
    {
      write_pc (addr);

      /* New address; we don't need to single-step a thread
	 over a breakpoint we just hit, 'cause we aren't
	 continuing from there.

	 It's not worth worrying about the case where a user
	 asks for a "jump" at the current PC--if they get the
	 hiccup of re-hiting a hit breakpoint, what else do
	 they expect? */
      thread_step_needed = 0;
    }

#ifdef PREPARE_TO_PROCEED
  /* In a multi-threaded task we may select another thread
     and then continue or step.

     But if the old thread was stopped at a breakpoint, it
     will immediately cause another breakpoint stop without
     any execution (i.e. it will report a breakpoint hit
     incorrectly).  So we must step over it first.

     PREPARE_TO_PROCEED checks the current thread against the thread
     that reported the most recent event.  If a step-over is required
     it returns TRUE and sets the current thread to the old thread. */
  if (PREPARE_TO_PROCEED () && breakpoint_here_p (read_pc ()))
    {
      oneproc = 1;
      thread_step_needed = 1;
    }

#endif /* PREPARE_TO_PROCEED */

#ifdef HP_OS_BUG
  if (trap_expected_after_continue)
    {
      /* If (step == 0), a trap will be automatically generated after
	 the first instruction is executed.  Force step one
	 instruction to clear this condition.  This should not occur
	 if step is nonzero, but it is harmless in that case.  */
      oneproc = 1;
      trap_expected_after_continue = 0;
    }
#endif /* HP_OS_BUG */

  if (oneproc)
    /* We will get a trace trap after one instruction.
       Continue it automatically and insert breakpoints then.  */
    trap_expected = 1;
  else
    {
      int temp = insert_breakpoints ();
      if (temp)
	{
	  print_sys_errmsg ("ptrace", temp);
	  error ("Cannot insert breakpoints.\n\
The same program may be running in another process.");
	}

      breakpoints_inserted = 1;
    }

  if (siggnal != TARGET_SIGNAL_DEFAULT)
    stop_signal = siggnal;
  /* If this signal should not be seen by program,
     give it zero.  Used for debugging signals.  */
  else if (!signal_program[stop_signal])
    stop_signal = TARGET_SIGNAL_0;

  annotate_starting ();

  /* Make sure that output from GDB appears before output from the
     inferior.  */
  gdb_flush (gdb_stdout);

  /* Resume inferior.  */
  resume (oneproc || step || bpstat_should_step (), stop_signal);

  /* Wait for it to stop (if not standalone)
     and in any case decode why it stopped, and act accordingly.  */

  wait_for_inferior ();
  normal_stop ();
}

/* Record the pc and sp of the program the last time it stopped.
   These are just used internally by wait_for_inferior, but need
   to be preserved over calls to it and cleared when the inferior
   is started.  */
static CORE_ADDR prev_pc;
static CORE_ADDR prev_func_start;
static char *prev_func_name;


/* Start remote-debugging of a machine over a serial link.  */

void
start_remote ()
{
  init_thread_list ();
  init_wait_for_inferior ();
  stop_soon_quietly = 1;
  trap_expected = 0;
  wait_for_inferior ();
  normal_stop ();
}

/* Initialize static vars when a new inferior begins.  */

void
init_wait_for_inferior ()
{
  /* These are meaningless until the first time through wait_for_inferior.  */
  prev_pc = 0;
  prev_func_start = 0;
  prev_func_name = NULL;

#ifdef HP_OS_BUG
  trap_expected_after_continue = 0;
#endif
  breakpoints_inserted = 0;
  breakpoint_init_inferior (inf_starting);

  /* Don't confuse first call to proceed(). */
  stop_signal = TARGET_SIGNAL_0;

  /* The first resume is not following a fork/vfork/exec. */
  pending_follow.kind = TARGET_WAITKIND_SPURIOUS;	/* I.e., none. */
  pending_follow.fork_event.saw_parent_fork = 0;
  pending_follow.fork_event.saw_child_fork = 0;
  pending_follow.fork_event.saw_child_exec = 0;

  /* See wait_for_inferior's handling of SYSCALL_ENTRY/RETURN events. */
  number_of_threads_in_syscalls = 0;

  clear_proceed_status ();
}

static void
delete_breakpoint_current_contents (arg)
     PTR arg;
{
  struct breakpoint **breakpointp = (struct breakpoint **) arg;
  if (*breakpointp != NULL)
    {
      delete_breakpoint (*breakpointp);
      *breakpointp = NULL;
    }
}

/* Wait for control to return from inferior to debugger.
   If inferior gets a signal, we may decide to start it up again
   instead of returning.  That is why there is a loop in this function.
   When this function actually returns it means the inferior
   should be left stopped and GDB should read more commands.  */

void
wait_for_inferior ()
{
  struct cleanup *old_cleanups;
  struct target_waitstatus w;
  int another_trap;
  int random_signal = 0;
  CORE_ADDR stop_func_start;
  CORE_ADDR stop_func_end;
  char *stop_func_name;
#if 0
  CORE_ADDR prologue_pc = 0;
#endif
  CORE_ADDR tmp;
  struct symtab_and_line sal;
  int remove_breakpoints_on_following_step = 0;
  int current_line;
  struct symtab *current_symtab;
  int handling_longjmp = 0;	/* FIXME */
  int pid;
  int saved_inferior_pid;
  int update_step_sp = 0;
  int stepping_through_solib_after_catch = 0;
  bpstat stepping_through_solib_catchpoints = NULL;
  int enable_hw_watchpoints_after_wait = 0;
  int stepping_through_sigtramp = 0;
  int new_thread_event;

#ifdef HAVE_NONSTEPPABLE_WATCHPOINT
  int stepped_after_stopped_by_watchpoint;
#endif

  old_cleanups = make_cleanup (delete_breakpoint_current_contents,
			       &step_resume_breakpoint);
  make_cleanup (delete_breakpoint_current_contents,
		&through_sigtramp_breakpoint);
  sal = find_pc_line (prev_pc, 0);
  current_line = sal.line;
  current_symtab = sal.symtab;

  /* Are we stepping?  */
#define CURRENTLY_STEPPING() \
  ((through_sigtramp_breakpoint == NULL \
    && !handling_longjmp \
    && ((step_range_end && step_resume_breakpoint == NULL) \
	|| trap_expected)) \
   || stepping_through_solib_after_catch \
   || bpstat_should_step ())
  ;
  thread_step_needed = 0;

#ifdef HPUXHPPA
  /* We'll update this if & when we switch to a new thread. */
  switched_from_inferior_pid = inferior_pid;
#endif

  while (1)
    {
      extern int overlay_cache_invalid;	/* declared in symfile.h */

      overlay_cache_invalid = 1;

      /* We have to invalidate the registers BEFORE calling target_wait because
	 they can be loaded from the target while in target_wait.  This makes
	 remote debugging a bit more efficient for those targets that provide
	 critical registers as part of their normal status mechanism. */

      registers_changed ();

      if (target_wait_hook)
	pid = target_wait_hook (-1, &w);
      else
	pid = target_wait (-1, &w);

      /* Since we've done a wait, we have a new event.  Don't carry
         over any expectations about needing to step over a
         breakpoint. */
      thread_step_needed = 0;

      /* See comments where a TARGET_WAITKIND_SYSCALL_RETURN event is
         serviced in this loop, below. */
      if (enable_hw_watchpoints_after_wait)
	{
	  TARGET_ENABLE_HW_WATCHPOINTS (inferior_pid);
	  enable_hw_watchpoints_after_wait = 0;
	}


#ifdef HAVE_NONSTEPPABLE_WATCHPOINT
      stepped_after_stopped_by_watchpoint = 0;
#endif

      /* Gross.

       We goto this label from elsewhere in wait_for_inferior when we want
       to continue the main loop without calling "wait" and trashing the
       waitstatus contained in W.  */
    have_waited:

      flush_cached_frames ();

      /* If it's a new process, add it to the thread database */

      new_thread_event = ((pid != inferior_pid) && !in_thread_list (pid));

      if (w.kind != TARGET_WAITKIND_EXITED
	  && w.kind != TARGET_WAITKIND_SIGNALLED
	  && new_thread_event)
	{
	  add_thread (pid);


#ifdef HPUXHPPA
	  fprintf_unfiltered (gdb_stderr, "[New %s]\n",
			      target_pid_or_tid_to_str (pid));

#else
	  printf_filtered ("[New %s]\n", target_pid_to_str (pid));
#endif

#if 0
	  /* NOTE: This block is ONLY meant to be invoked in case of a
	     "thread creation event"!  If it is invoked for any other
	     sort of event (such as a new thread landing on a breakpoint),
	     the event will be discarded, which is almost certainly
	     a bad thing!
	
	     To avoid this, the low-level module (eg. target_wait)
	     should call in_thread_list and add_thread, so that the
	     new thread is known by the time we get here.  */

	  /* We may want to consider not doing a resume here in order
	     to give the user a chance to play with the new thread.
	     It might be good to make that a user-settable option.  */

	  /* At this point, all threads are stopped (happens
	     automatically in either the OS or the native code).
	     Therefore we need to continue all threads in order to
	     make progress.  */

	  target_resume (-1, 0, TARGET_SIGNAL_0);
	  continue;
#endif
	}

      switch (w.kind)
	{
	case TARGET_WAITKIND_LOADED:
	  /* Ignore gracefully during startup of the inferior, as it
	     might be the shell which has just loaded some objects,
	     otherwise add the symbols for the newly loaded objects.  */
#ifdef SOLIB_ADD
	  if (!stop_soon_quietly)
	    {
	      extern int auto_solib_add;

	      /* Remove breakpoints, SOLIB_ADD might adjust
		 breakpoint addresses via breakpoint_re_set.  */
	      if (breakpoints_inserted)
		remove_breakpoints ();

	      /* Check for any newly added shared libraries if we're
		 supposed to be adding them automatically.  */
	      if (auto_solib_add)
		{
		  /* Switch terminal for any messages produced by
		     breakpoint_re_set.  */
		  target_terminal_ours_for_output ();
		  SOLIB_ADD (NULL, 0, NULL);
		  target_terminal_inferior ();
		}

	      /* Reinsert breakpoints and continue.  */
	      if (breakpoints_inserted)
		insert_breakpoints ();
	    }
#endif
	  resume (0, TARGET_SIGNAL_0);
	  continue;

	case TARGET_WAITKIND_SPURIOUS:
	  resume (0, TARGET_SIGNAL_0);
	  continue;

	case TARGET_WAITKIND_EXITED:
	  target_terminal_ours ();	/* Must do this before mourn anyway */
	  annotate_exited (w.value.integer);
	  if (w.value.integer)
	    printf_filtered ("\nProgram exited with code 0%o.\n",
			     (unsigned int) w.value.integer);
	  else
	    printf_filtered ("\nProgram exited normally.\n");

	  /* Record the exit code in the convenience variable $_exitcode, so
	     that the user can inspect this again later.  */
	  set_internalvar (lookup_internalvar ("_exitcode"),
			   value_from_longest (builtin_type_int,
					       (LONGEST) w.value.integer));
	  gdb_flush (gdb_stdout);
	  target_mourn_inferior ();
	  singlestep_breakpoints_inserted_p = 0;	/*SOFTWARE_SINGLE_STEP_P*/
	  stop_print_frame = 0;
	  goto stop_stepping;

	case TARGET_WAITKIND_SIGNALLED:
	  stop_print_frame = 0;
	  stop_signal = w.value.sig;
	  target_terminal_ours ();	/* Must do this before mourn anyway */
	  annotate_signalled ();

	  /* This looks pretty bogus to me.  Doesn't TARGET_WAITKIND_SIGNALLED
	     mean it is already dead?  This has been here since GDB 2.8, so
	     perhaps it means rms didn't understand unix waitstatuses?
	     For the moment I'm just kludging around this in remote.c
	     rather than trying to change it here --kingdon, 5 Dec 1994.  */
	  target_kill ();	/* kill mourns as well */

	  printf_filtered ("\nProgram terminated with signal ");
	  annotate_signal_name ();
	  printf_filtered ("%s", target_signal_to_name (stop_signal));
	  annotate_signal_name_end ();
	  printf_filtered (", ");
	  annotate_signal_string ();
	  printf_filtered ("%s", target_signal_to_string (stop_signal));
	  annotate_signal_string_end ();
	  printf_filtered (".\n");

	  printf_filtered ("The program no longer exists.\n");
	  gdb_flush (gdb_stdout);
	  singlestep_breakpoints_inserted_p = 0;	/*SOFTWARE_SINGLE_STEP_P*/
	  goto stop_stepping;

	  /* The following are the only cases in which we keep going;
           the above cases end in a continue or goto. */
	case TARGET_WAITKIND_FORKED:
	  stop_signal = TARGET_SIGNAL_TRAP;
	  pending_follow.kind = w.kind;

	  /* Ignore fork events reported for the parent; we're only
             interested in reacting to forks of the child.  Note that
             we expect the child's fork event to be available if we
             waited for it now. */
	  if (inferior_pid == pid)
	    {
	      pending_follow.fork_event.saw_parent_fork = 1;
	      pending_follow.fork_event.parent_pid = pid;
	      pending_follow.fork_event.child_pid = w.value.related_pid;
	      continue;
	    }
	  else
	    {
	      pending_follow.fork_event.saw_child_fork = 1;
	      pending_follow.fork_event.child_pid = pid;
	      pending_follow.fork_event.parent_pid = w.value.related_pid;
	    }

	  stop_pc = read_pc_pid (pid);
	  saved_inferior_pid = inferior_pid;
	  inferior_pid = pid;
	  stop_bpstat = bpstat_stop_status
	    (&stop_pc,
#if DECR_PC_AFTER_BREAK
	     (prev_pc != stop_pc - DECR_PC_AFTER_BREAK
	      && CURRENTLY_STEPPING ())
#else /* DECR_PC_AFTER_BREAK zero */
	     0
#endif /* DECR_PC_AFTER_BREAK zero */
	    );
	  random_signal = !bpstat_explains_signal (stop_bpstat);
	  inferior_pid = saved_inferior_pid;
	  goto process_event_stop_test;

	  /* If this a platform which doesn't allow a debugger to touch a
           vfork'd inferior until after it exec's, then we'd best keep
           our fingers entirely off the inferior, other than continuing
           it.  This has the unfortunate side-effect that catchpoints
           of vforks will be ignored.  But since the platform doesn't
           allow the inferior be touched at vfork time, there's really
           little choice. */
	case TARGET_WAITKIND_VFORKED:
	  stop_signal = TARGET_SIGNAL_TRAP;
	  pending_follow.kind = w.kind;

	  /* Is this a vfork of the parent?  If so, then give any
             vfork catchpoints a chance to trigger now.  (It's
             dangerous to do so if the child canot be touched until
             it execs, and the child has not yet exec'd.  We probably
             should warn the user to that effect when the catchpoint
             triggers...) */
	  if (pid == inferior_pid)
	    {
	      pending_follow.fork_event.saw_parent_fork = 1;
	      pending_follow.fork_event.parent_pid = pid;
	      pending_follow.fork_event.child_pid = w.value.related_pid;
	    }

	  /* If we've seen the child's vfork event but cannot really touch
             the child until it execs, then we must continue the child now.
             Else, give any vfork catchpoints a chance to trigger now. */
	  else
	    {
	      pending_follow.fork_event.saw_child_fork = 1;
	      pending_follow.fork_event.child_pid = pid;
	      pending_follow.fork_event.parent_pid = w.value.related_pid;
	      target_post_startup_inferior (pending_follow.fork_event.child_pid);
	      follow_vfork_when_exec = !target_can_follow_vfork_prior_to_exec ();
	      if (follow_vfork_when_exec)
		{
		  target_resume (pid, 0, TARGET_SIGNAL_0);
		  continue;
		}
	    }

	  stop_pc = read_pc ();
	  stop_bpstat = bpstat_stop_status
	    (&stop_pc,
#if DECR_PC_AFTER_BREAK
	     (prev_pc != stop_pc - DECR_PC_AFTER_BREAK
	      && CURRENTLY_STEPPING ())
#else /* DECR_PC_AFTER_BREAK zero */
	     0
#endif /* DECR_PC_AFTER_BREAK zero */
	    );
	  random_signal = !bpstat_explains_signal (stop_bpstat);
	  goto process_event_stop_test;

	case TARGET_WAITKIND_EXECD:
	  stop_signal = TARGET_SIGNAL_TRAP;

	  /* Is this a target which reports multiple exec events per actual
             call to exec()?  (HP-UX using ptrace does, for example.)  If so,
             ignore all but the last one.  Just resume the exec'r, and wait
             for the next exec event. */
	  if (inferior_ignoring_leading_exec_events)
	    {
	      inferior_ignoring_leading_exec_events--;
	      if (pending_follow.kind == TARGET_WAITKIND_VFORKED)
		ENSURE_VFORKING_PARENT_REMAINS_STOPPED (pending_follow.fork_event.parent_pid);
	      target_resume (pid, 0, TARGET_SIGNAL_0);
	      continue;
	    }
	  inferior_ignoring_leading_exec_events =
	    target_reported_exec_events_per_exec_call () - 1;

	  pending_follow.execd_pathname = savestring (w.value.execd_pathname,
					   strlen (w.value.execd_pathname));

	  /* Did inferior_pid exec, or did a (possibly not-yet-followed)
             child of a vfork exec?

             ??rehrauer: This is unabashedly an HP-UX specific thing.  On
             HP-UX, events associated with a vforking inferior come in
             threes: a vfork event for the child (always first), followed
             a vfork event for the parent and an exec event for the child.
             The latter two can come in either order.

             If we get the parent vfork event first, life's good: We follow
             either the parent or child, and then the child's exec event is
             a "don't care".

             But if we get the child's exec event first, then we delay
             responding to it until we handle the parent's vfork.  Because,
             otherwise we can't satisfy a "catch vfork". */
	  if (pending_follow.kind == TARGET_WAITKIND_VFORKED)
	    {
	      pending_follow.fork_event.saw_child_exec = 1;

	      /* On some targets, the child must be resumed before
                 the parent vfork event is delivered.  A single-step
                 suffices. */
	      if (RESUME_EXECD_VFORKING_CHILD_TO_GET_PARENT_VFORK ())
		target_resume (pid, 1, TARGET_SIGNAL_0);
	      /* We expect the parent vfork event to be available now. */
	      continue;
	    }

	  /* This causes the eventpoints and symbol table to be reset.  Must
             do this now, before trying to determine whether to stop. */
	  follow_exec (inferior_pid, pending_follow.execd_pathname);
	  free (pending_follow.execd_pathname);

	  stop_pc = read_pc_pid (pid);
	  saved_inferior_pid = inferior_pid;
	  inferior_pid = pid;
	  stop_bpstat = bpstat_stop_status
	    (&stop_pc,
#if DECR_PC_AFTER_BREAK
	     (prev_pc != stop_pc - DECR_PC_AFTER_BREAK
	      && CURRENTLY_STEPPING ())
#else /* DECR_PC_AFTER_BREAK zero */
	     0
#endif /* DECR_PC_AFTER_BREAK zero */
	    );
	  random_signal = !bpstat_explains_signal (stop_bpstat);
	  inferior_pid = saved_inferior_pid;
	  goto process_event_stop_test;

	  /* These syscall events are returned on HP-UX, as part of its
           implementation of page-protection-based "hardware" watchpoints.
           HP-UX has unfortunate interactions between page-protections and
           some system calls.  Our solution is to disable hardware watches
           when a system call is entered, and reenable them when the syscall
           completes.  The downside of this is that we may miss the precise
           point at which a watched piece of memory is modified.  "Oh well."

           Note that we may have multiple threads running, which may each
           enter syscalls at roughly the same time.  Since we don't have a
           good notion currently of whether a watched piece of memory is
           thread-private, we'd best not have any page-protections active
           when any thread is in a syscall.  Thus, we only want to reenable
           hardware watches when no threads are in a syscall.

           Also, be careful not to try to gather much state about a thread
           that's in a syscall.  It's frequently a losing proposition. */
	case TARGET_WAITKIND_SYSCALL_ENTRY:
	  number_of_threads_in_syscalls++;
	  if (number_of_threads_in_syscalls == 1)
	    {
	      TARGET_DISABLE_HW_WATCHPOINTS (inferior_pid);
	    }
	  resume (0, TARGET_SIGNAL_0);
	  continue;

	  /* Before examining the threads further, step this thread to
	   get it entirely out of the syscall.  (We get notice of the
	   event when the thread is just on the verge of exiting a
	   syscall.  Stepping one instruction seems to get it back
	   into user code.)

	   Note that although the logical place to reenable h/w watches
	   is here, we cannot.  We cannot reenable them before stepping
	   the thread (this causes the next wait on the thread to hang).

	   Nor can we enable them after stepping until we've done a wait.
	   Thus, we simply set the flag enable_hw_watchpoints_after_wait
	   here, which will be serviced immediately after the target
	   is waited on. */
	case TARGET_WAITKIND_SYSCALL_RETURN:
	  target_resume (pid, 1, TARGET_SIGNAL_0);

	  if (number_of_threads_in_syscalls > 0)
	    {
	      number_of_threads_in_syscalls--;
	      enable_hw_watchpoints_after_wait =
		(number_of_threads_in_syscalls == 0);
	    }
	  continue;

	case TARGET_WAITKIND_STOPPED:
	  stop_signal = w.value.sig;
	  break;
	}

      /* We may want to consider not doing a resume here in order to give
         the user a chance to play with the new thread.  It might be good
         to make that a user-settable option.  */

      /* At this point, all threads are stopped (happens automatically in
         either the OS or the native code).  Therefore we need to continue
         all threads in order to make progress.  */
      if (new_thread_event)
	{
	  target_resume (-1, 0, TARGET_SIGNAL_0);
	  continue;
	}

      stop_pc = read_pc_pid (pid);

      /* See if a thread hit a thread-specific breakpoint that was meant for
	 another thread.  If so, then step that thread past the breakpoint,
	 and continue it.  */

      if (stop_signal == TARGET_SIGNAL_TRAP)
	{
	  if (SOFTWARE_SINGLE_STEP_P && singlestep_breakpoints_inserted_p)
	    random_signal = 0;
	  else if (breakpoints_inserted
		   && breakpoint_here_p (stop_pc - DECR_PC_AFTER_BREAK))
	    {
	      random_signal = 0;
	      if (!breakpoint_thread_match (stop_pc - DECR_PC_AFTER_BREAK,
					    pid))
		{
		  int remove_status;

		  /* Saw a breakpoint, but it was hit by the wrong thread.
		       Just continue. */
		  write_pc_pid (stop_pc - DECR_PC_AFTER_BREAK, pid);

		  remove_status = remove_breakpoints ();
		  /* Did we fail to remove breakpoints?  If so, try
                       to set the PC past the bp.  (There's at least
                       one situation in which we can fail to remove
                       the bp's: On HP-UX's that use ttrace, we can't
                       change the address space of a vforking child
                       process until the child exits (well, okay, not
                       then either :-) or execs. */
		  if (remove_status != 0)
		    {
		      write_pc_pid (stop_pc - DECR_PC_AFTER_BREAK + 4, pid);
		    }
		  else
		    {		/* Single step */
		      target_resume (pid, 1, TARGET_SIGNAL_0);
		      /* FIXME: What if a signal arrives instead of the
			   single-step happening?  */

		      if (target_wait_hook)
			target_wait_hook (pid, &w);
		      else
			target_wait (pid, &w);
		      insert_breakpoints ();
		    }

		  /* We need to restart all the threads now.  */
		  target_resume (-1, 0, TARGET_SIGNAL_0);
		  continue;
		}
	      else
		{
		  /* This breakpoint matches--either it is the right
		       thread or it's a generic breakpoint for all threads.
		       Remember that we'll need to step just _this_ thread
		       on any following user continuation! */
		  thread_step_needed = 1;
		}
	    }
	}
      else
	random_signal = 1;

      /* See if something interesting happened to the non-current thread.  If
         so, then switch to that thread, and eventually give control back to
	 the user.

         Note that if there's any kind of pending follow (i.e., of a fork,
         vfork or exec), we don't want to do this now.  Rather, we'll let
         the next resume handle it. */
      if ((pid != inferior_pid) &&
	  (pending_follow.kind == TARGET_WAITKIND_SPURIOUS))
	{
	  int printed = 0;

	  /* If it's a random signal for a non-current thread, notify user
	     if he's expressed an interest. */
	  if (random_signal
	      && signal_print[stop_signal])
	    {
/* ??rehrauer: I don't understand the rationale for this code.  If the
   inferior will stop as a result of this signal, then the act of handling
   the stop ought to print a message that's couches the stoppage in user
   terms, e.g., "Stopped for breakpoint/watchpoint".  If the inferior
   won't stop as a result of the signal -- i.e., if the signal is merely
   a side-effect of something GDB's doing "under the covers" for the
   user, such as stepping threads over a breakpoint they shouldn't stop
   for -- then the message seems to be a serious annoyance at best.

   For now, remove the message altogether. */
#if 0
	      printed = 1;
	      target_terminal_ours_for_output ();
	      printf_filtered ("\nProgram received signal %s, %s.\n",
			       target_signal_to_name (stop_signal),
			       target_signal_to_string (stop_signal));
	      gdb_flush (gdb_stdout);
#endif
	    }

	  /* If it's not SIGTRAP and not a signal we want to stop for, then
	     continue the thread. */

	  if (stop_signal != TARGET_SIGNAL_TRAP
	      && !signal_stop[stop_signal])
	    {
	      if (printed)
		target_terminal_inferior ();

	      /* Clear the signal if it should not be passed.  */
	      if (signal_program[stop_signal] == 0)
		stop_signal = TARGET_SIGNAL_0;

	      target_resume (pid, 0, stop_signal);
	      continue;
	    }

	  /* It's a SIGTRAP or a signal we're interested in.  Switch threads,
	     and fall into the rest of wait_for_inferior().  */

	  /* Save infrun state for the old thread.  */
	  save_infrun_state (inferior_pid, prev_pc,
			     prev_func_start, prev_func_name,
			     trap_expected, step_resume_breakpoint,
			     through_sigtramp_breakpoint,
			     step_range_start, step_range_end,
			     step_frame_address, handling_longjmp,
			     another_trap,
			     stepping_through_solib_after_catch,
			     stepping_through_solib_catchpoints,
			     stepping_through_sigtramp);

#ifdef HPUXHPPA
	  switched_from_inferior_pid = inferior_pid;
#endif

	  inferior_pid = pid;

	  /* Load infrun state for the new thread.  */
	  load_infrun_state (inferior_pid, &prev_pc,
			     &prev_func_start, &prev_func_name,
			     &trap_expected, &step_resume_breakpoint,
			     &through_sigtramp_breakpoint,
			     &step_range_start, &step_range_end,
			     &step_frame_address, &handling_longjmp,
			     &another_trap,
			     &stepping_through_solib_after_catch,
			     &stepping_through_solib_catchpoints,
			     &stepping_through_sigtramp);

	  if (context_hook)
	    context_hook (pid_to_thread_id (pid));

	  printf_filtered ("[Switching to %s]\n", target_pid_to_str (pid));
	  flush_cached_frames ();
	}

      if (SOFTWARE_SINGLE_STEP_P && singlestep_breakpoints_inserted_p)
	{
	  /* Pull the single step breakpoints out of the target. */
	  SOFTWARE_SINGLE_STEP (0, 0);
	  singlestep_breakpoints_inserted_p = 0;
	}

      /* If PC is pointing at a nullified instruction, then step beyond
	 it so that the user won't be confused when GDB appears to be ready
	 to execute it. */

#if 0				/* XXX DEBUG */
      printf ("infrun.c:1607: pc = 0x%x\n", read_pc ());
#endif
      /*      if (INSTRUCTION_NULLIFIED && CURRENTLY_STEPPING ()) */
      if (INSTRUCTION_NULLIFIED)
	{
	  struct target_waitstatus tmpstatus;
#if 0
	  all_registers_info ((char *) 0, 0);
#endif
	  registers_changed ();
	  target_resume (pid, 1, TARGET_SIGNAL_0);

	  /* We may have received a signal that we want to pass to
	     the inferior; therefore, we must not clobber the waitstatus
	     in W.  So we call wait ourselves, then continue the loop
	     at the "have_waited" label.  */
	  if (target_wait_hook)
	    target_wait_hook (pid, &tmpstatus);
	  else
	    target_wait (pid, &tmpstatus);

	  goto have_waited;
	}

#ifdef HAVE_STEPPABLE_WATCHPOINT
      /* It may not be necessary to disable the watchpoint to stop over
	 it.  For example, the PA can (with some kernel cooperation)
	 single step over a watchpoint without disabling the watchpoint.  */
      if (STOPPED_BY_WATCHPOINT (w))
	{
	  resume (1, 0);
	  continue;
	}
#endif

#ifdef HAVE_NONSTEPPABLE_WATCHPOINT
      /* It is far more common to need to disable a watchpoint
	 to step the inferior over it.  FIXME.  What else might
	 a debug register or page protection watchpoint scheme need
	 here?  */
      if (STOPPED_BY_WATCHPOINT (w))
	{
/* At this point, we are stopped at an instruction which has attempted to write
   to a piece of memory under control of a watchpoint.  The instruction hasn't
   actually executed yet.  If we were to evaluate the watchpoint expression
   now, we would get the old value, and therefore no change would seem to have
   occurred.

   In order to make watchpoints work `right', we really need to complete the
   memory write, and then evaluate the watchpoint expression.  The following
   code does that by removing the watchpoint (actually, all watchpoints and
   breakpoints), single-stepping the target, re-inserting watchpoints, and then
   falling through to let normal single-step processing handle proceed.  Since
   this includes evaluating watchpoints, things will come to a stop in the
   correct manner.  */

	  write_pc (stop_pc - DECR_PC_AFTER_BREAK);

	  remove_breakpoints ();
	  registers_changed ();
	  target_resume (pid, 1, TARGET_SIGNAL_0);	/* Single step */

	  if (target_wait_hook)
	    target_wait_hook (pid, &w);
	  else
	    target_wait (pid, &w);
	  insert_breakpoints ();

	  /* FIXME-maybe: is this cleaner than setting a flag?  Does it
	     handle things like signals arriving and other things happening
	     in combination correctly?  */
	  stepped_after_stopped_by_watchpoint = 1;
	  goto have_waited;
	}
#endif

#ifdef HAVE_CONTINUABLE_WATCHPOINT
      /* It may be possible to simply continue after a watchpoint.  */
      STOPPED_BY_WATCHPOINT (w);
#endif

      stop_func_start = 0;
      stop_func_end = 0;
      stop_func_name = 0;
      /* Don't care about return value; stop_func_start and stop_func_name
	 will both be 0 if it doesn't work.  */
      find_pc_partial_function (stop_pc, &stop_func_name, &stop_func_start,
				&stop_func_end);
      stop_func_start += FUNCTION_START_OFFSET;
      another_trap = 0;
      bpstat_clear (&stop_bpstat);
      stop_step = 0;
      stop_stack_dummy = 0;
      stop_print_frame = 1;
      random_signal = 0;
      stopped_by_random_signal = 0;
      breakpoints_failed = 0;

      /* Look at the cause of the stop, and decide what to do.
	 The alternatives are:
	 1) break; to really stop and return to the debugger,
	 2) drop through to start up again
	 (set another_trap to 1 to single step once)
	 3) set random_signal to 1, and the decision between 1 and 2
	 will be made according to the signal handling tables.  */

      /* First, distinguish signals caused by the debugger from signals
	 that have to do with the program's own actions.
	 Note that breakpoint insns may cause SIGTRAP or SIGILL
	 or SIGEMT, depending on the operating system version.
	 Here we detect when a SIGILL or SIGEMT is really a breakpoint
	 and change it to SIGTRAP.  */

      if (stop_signal == TARGET_SIGNAL_TRAP
	  || (breakpoints_inserted &&
	      (stop_signal == TARGET_SIGNAL_ILL
	       || stop_signal == TARGET_SIGNAL_EMT
	      ))
	  || stop_soon_quietly)
	{
	  if (stop_signal == TARGET_SIGNAL_TRAP && stop_after_trap)
	    {
	      stop_print_frame = 0;
	      break;
	    }
	  if (stop_soon_quietly)
	    break;

	  /* Don't even think about breakpoints
	     if just proceeded over a breakpoint.

	     However, if we are trying to proceed over a breakpoint
	     and end up in sigtramp, then through_sigtramp_breakpoint
	     will be set and we should check whether we've hit the
	     step breakpoint.  */
	  if (stop_signal == TARGET_SIGNAL_TRAP && trap_expected
	      && through_sigtramp_breakpoint == NULL)
	    bpstat_clear (&stop_bpstat);
	  else
	    {
	      /* See if there is a breakpoint at the current PC.  */
	      stop_bpstat = bpstat_stop_status
		(&stop_pc,
		 (DECR_PC_AFTER_BREAK ?
	      /* Notice the case of stepping through a jump
		    that lands just after a breakpoint.
		    Don't confuse that with hitting the breakpoint.
		    What we check for is that 1) stepping is going on
		    and 2) the pc before the last insn does not match
		    the address of the breakpoint before the current pc
		    and 3) we didn't hit a breakpoint in a signal handler
		    without an intervening stop in sigtramp, which is
		    detected by a new stack pointer value below
		    any usual function calling stack adjustments.  */
		  (CURRENTLY_STEPPING ()
		   && prev_pc != stop_pc - DECR_PC_AFTER_BREAK
		   && !(step_range_end
			&& INNER_THAN (read_sp (), (step_sp - 16)))) :
		  0)
		);
	      /* Following in case break condition called a
		 function.  */
	      stop_print_frame = 1;
	    }

	  if (stop_signal == TARGET_SIGNAL_TRAP)
	    random_signal
	      = !(bpstat_explains_signal (stop_bpstat)
		  || trap_expected
#ifndef CALL_DUMMY_BREAKPOINT_OFFSET
		  || PC_IN_CALL_DUMMY (stop_pc, read_sp (),
				       FRAME_FP (get_current_frame ()))
#endif /* No CALL_DUMMY_BREAKPOINT_OFFSET.  */
		  || (step_range_end && step_resume_breakpoint == NULL));

	  else
	    {
	      random_signal
		= !(bpstat_explains_signal (stop_bpstat)
	      /* End of a stack dummy.  Some systems (e.g. Sony
		       news) give another signal besides SIGTRAP,
		       so check here as well as above.  */
#ifndef CALL_DUMMY_BREAKPOINT_OFFSET
		    || PC_IN_CALL_DUMMY (stop_pc, read_sp (),
					 FRAME_FP (get_current_frame ()))
#endif /* No CALL_DUMMY_BREAKPOINT_OFFSET.  */
		);
	      if (!random_signal)
		stop_signal = TARGET_SIGNAL_TRAP;
	    }
	}

      /* When we reach this point, we've pretty much decided
         that the reason for stopping must've been a random
         (unexpected) signal. */

      else
	random_signal = 1;
      /* If a fork, vfork or exec event was seen, then there are two
         possible responses we can make:

         1. If a catchpoint triggers for the event (random_signal == 0),
            then we must stop now and issue a prompt.  We will resume
            the inferior when the user tells us to.
         2. If no catchpoint triggers for the event (random_signal == 1),
            then we must resume the inferior now and keep checking.

         In either case, we must take appropriate steps to "follow" the
         the fork/vfork/exec when the inferior is resumed.  For example,
         if follow-fork-mode is "child", then we must detach from the
         parent inferior and follow the new child inferior.

         In either case, setting pending_follow causes the next resume()
         to take the appropriate following action. */
    process_event_stop_test:
      if (w.kind == TARGET_WAITKIND_FORKED)
	{
	  if (random_signal)	/* I.e., no catchpoint triggered for this. */
	    {
	      trap_expected = 1;
	      stop_signal = TARGET_SIGNAL_0;
	      goto keep_going;
	    }
	}
      else if (w.kind == TARGET_WAITKIND_VFORKED)
	{
	  if (random_signal)	/* I.e., no catchpoint triggered for this. */
	    {
	      stop_signal = TARGET_SIGNAL_0;
	      goto keep_going;
	    }
	}
      else if (w.kind == TARGET_WAITKIND_EXECD)
	{
	  pending_follow.kind = w.kind;
	  if (random_signal)	/* I.e., no catchpoint triggered for this. */
	    {
	      trap_expected = 1;
	      stop_signal = TARGET_SIGNAL_0;
	      goto keep_going;
	    }
	}

      /* For the program's own signals, act according to
	 the signal handling tables.  */

      if (random_signal)
	{
	  /* Signal not for debugging purposes.  */
	  int printed = 0;

	  stopped_by_random_signal = 1;

	  if (signal_print[stop_signal])
	    {
	      printed = 1;
	      target_terminal_ours_for_output ();
	      annotate_signal ();
	      printf_filtered ("\nProgram received signal ");
	      annotate_signal_name ();
	      printf_filtered ("%s", target_signal_to_name (stop_signal));
	      annotate_signal_name_end ();
	      printf_filtered (", ");
	      annotate_signal_string ();
	      printf_filtered ("%s", target_signal_to_string (stop_signal));
	      annotate_signal_string_end ();
	      printf_filtered (".\n");
	      gdb_flush (gdb_stdout);
	    }
	  if (signal_stop[stop_signal])
	    break;
	  /* If not going to stop, give terminal back
	     if we took it away.  */
	  else if (printed)
	    target_terminal_inferior ();

	  /* Clear the signal if it should not be passed.  */
	  if (signal_program[stop_signal] == 0)
	    stop_signal = TARGET_SIGNAL_0;

	  /* If we're in the middle of a "next" command, let the code for
             stepping over a function handle this. pai/1997-09-10

             A previous comment here suggested it was possible to change
             this to jump to keep_going in all cases. */

	  if (step_over_calls > 0)
	    goto step_over_function;
	  else
	    goto check_sigtramp2;
	}

      /* Handle cases caused by hitting a breakpoint.  */
      {
	CORE_ADDR jmp_buf_pc;
	struct bpstat_what what;

	what = bpstat_what (stop_bpstat);

	if (what.call_dummy)
	  {
	    stop_stack_dummy = 1;
#ifdef HP_OS_BUG
	    trap_expected_after_continue = 1;
#endif
	  }

	switch (what.main_action)
	  {
	  case BPSTAT_WHAT_SET_LONGJMP_RESUME:
	    /* If we hit the breakpoint at longjmp, disable it for the
	       duration of this command.  Then, install a temporary
	       breakpoint at the target of the jmp_buf. */
	    disable_longjmp_breakpoint ();
	    remove_breakpoints ();
	    breakpoints_inserted = 0;
	    if (!GET_LONGJMP_TARGET (&jmp_buf_pc))
	      goto keep_going;

	    /* Need to blow away step-resume breakpoint, as it
	       interferes with us */
	    if (step_resume_breakpoint != NULL)
	      {
		delete_breakpoint (step_resume_breakpoint);
		step_resume_breakpoint = NULL;
	      }
	    /* Not sure whether we need to blow this away too, but probably
	       it is like the step-resume breakpoint.  */
	    if (through_sigtramp_breakpoint != NULL)
	      {
		delete_breakpoint (through_sigtramp_breakpoint);
		through_sigtramp_breakpoint = NULL;
	      }

#if 0
	    /* FIXME - Need to implement nested temporary breakpoints */
	    if (step_over_calls > 0)
	      set_longjmp_resume_breakpoint (jmp_buf_pc,
					     get_current_frame ());
	    else
#endif /* 0 */
	      set_longjmp_resume_breakpoint (jmp_buf_pc, NULL);
	    handling_longjmp = 1;	/* FIXME */
	    goto keep_going;

	  case BPSTAT_WHAT_CLEAR_LONGJMP_RESUME:
	  case BPSTAT_WHAT_CLEAR_LONGJMP_RESUME_SINGLE:
	    remove_breakpoints ();
	    breakpoints_inserted = 0;
#if 0
	    /* FIXME - Need to implement nested temporary breakpoints */
	    if (step_over_calls
		&& (INNER_THAN (FRAME_FP (get_current_frame ()),
				step_frame_address)))
	      {
		another_trap = 1;
		goto keep_going;
	      }
#endif /* 0 */
	    disable_longjmp_breakpoint ();
	    handling_longjmp = 0;	/* FIXME */
	    if (what.main_action == BPSTAT_WHAT_CLEAR_LONGJMP_RESUME)
	      break;
	    /* else fallthrough */

	  case BPSTAT_WHAT_SINGLE:
	    if (breakpoints_inserted)
	      {
		thread_step_needed = 1;
		remove_breakpoints ();
	      }
	    breakpoints_inserted = 0;
	    another_trap = 1;
	    /* Still need to check other stuff, at least the case
	       where we are stepping and step out of the right range.  */
	    break;

	  case BPSTAT_WHAT_STOP_NOISY:
	    stop_print_frame = 1;

	    /* We are about to nuke the step_resume_breakpoint and
	       through_sigtramp_breakpoint via the cleanup chain, so
	       no need to worry about it here.  */

	    goto stop_stepping;

	  case BPSTAT_WHAT_STOP_SILENT:
	    stop_print_frame = 0;

	    /* We are about to nuke the step_resume_breakpoint and
	       through_sigtramp_breakpoint via the cleanup chain, so
	       no need to worry about it here.  */

	    goto stop_stepping;

	  case BPSTAT_WHAT_STEP_RESUME:
	    /* This proably demands a more elegant solution, but, yeah
               right...

               This function's use of the simple variable
               step_resume_breakpoint doesn't seem to accomodate
               simultaneously active step-resume bp's, although the
               breakpoint list certainly can.

               If we reach here and step_resume_breakpoint is already
               NULL, then apparently we have multiple active
               step-resume bp's.  We'll just delete the breakpoint we
               stopped at, and carry on.  */
	    if (step_resume_breakpoint == NULL)
	      {
		step_resume_breakpoint =
		  bpstat_find_step_resume_breakpoint (stop_bpstat);
	      }
	    delete_breakpoint (step_resume_breakpoint);
	    step_resume_breakpoint = NULL;
	    break;

	  case BPSTAT_WHAT_THROUGH_SIGTRAMP:
	    if (through_sigtramp_breakpoint)
	      delete_breakpoint (through_sigtramp_breakpoint);
	    through_sigtramp_breakpoint = NULL;

	    /* If were waiting for a trap, hitting the step_resume_break
	       doesn't count as getting it.  */
	    if (trap_expected)
	      another_trap = 1;
	    break;

	  case BPSTAT_WHAT_CHECK_SHLIBS:
	  case BPSTAT_WHAT_CHECK_SHLIBS_RESUME_FROM_HOOK:
#ifdef SOLIB_ADD
	    {
	      extern int auto_solib_add;

	      /* Remove breakpoints, we eventually want to step over the
		 shlib event breakpoint, and SOLIB_ADD might adjust
		 breakpoint addresses via breakpoint_re_set.  */
	      if (breakpoints_inserted)
		remove_breakpoints ();
	      breakpoints_inserted = 0;

	      /* Check for any newly added shared libraries if we're
		 supposed to be adding them automatically.  */
	      if (auto_solib_add)
		{
		  /* Switch terminal for any messages produced by
		     breakpoint_re_set.  */
		  target_terminal_ours_for_output ();
		  SOLIB_ADD (NULL, 0, NULL);
		  target_terminal_inferior ();
		}

	      /* Try to reenable shared library breakpoints, additional
		 code segments in shared libraries might be mapped in now. */
	      re_enable_breakpoints_in_shlibs ();

	      /* If requested, stop when the dynamic linker notifies
		 gdb of events.  This allows the user to get control
		 and place breakpoints in initializer routines for
		 dynamically loaded objects (among other things).  */
	      if (stop_on_solib_events)
		{
		  stop_print_frame = 0;
		  goto stop_stepping;
		}

	      /* If we stopped due to an explicit catchpoint, then the
                 (see above) call to SOLIB_ADD pulled in any symbols
                 from a newly-loaded library, if appropriate.

                 We do want the inferior to stop, but not where it is
                 now, which is in the dynamic linker callback.  Rather,
                 we would like it stop in the user's program, just after
                 the call that caused this catchpoint to trigger.  That
                 gives the user a more useful vantage from which to
                 examine their program's state. */
	      else if (what.main_action == BPSTAT_WHAT_CHECK_SHLIBS_RESUME_FROM_HOOK)
		{
		  /* ??rehrauer: If I could figure out how to get the
                     right return PC from here, we could just set a temp
                     breakpoint and resume.  I'm not sure we can without
                     cracking open the dld's shared libraries and sniffing
                     their unwind tables and text/data ranges, and that's
                     not a terribly portable notion.

                     Until that time, we must step the inferior out of the
                     dld callback, and also out of the dld itself (and any
                     code or stubs in libdld.sl, such as "shl_load" and
                     friends) until we reach non-dld code.  At that point,
                     we can stop stepping. */
		  bpstat_get_triggered_catchpoints (stop_bpstat,
				       &stepping_through_solib_catchpoints);
		  stepping_through_solib_after_catch = 1;

		  /* Be sure to lift all breakpoints, so the inferior does
                     actually step past this point... */
		  another_trap = 1;
		  break;
		}
	      else
		{
		  /* We want to step over this breakpoint, then keep going.  */
		  another_trap = 1;
		  break;
		}
	    }
#endif
	    break;

	  case BPSTAT_WHAT_LAST:
	    /* Not a real code, but listed here to shut up gcc -Wall.  */

	  case BPSTAT_WHAT_KEEP_CHECKING:
	    break;
	  }
      }

      /* We come here if we hit a breakpoint but should not
	 stop for it.  Possibly we also were stepping
	 and should stop for that.  So fall through and
	 test for stepping.  But, if not stepping,
	 do not stop.  */

      /* Are we stepping to get the inferior out of the dynamic
         linker's hook (and possibly the dld itself) after catching
         a shlib event? */
      if (stepping_through_solib_after_catch)
	{
#if defined(SOLIB_ADD)
	  /* Have we reached our destination?  If not, keep going. */
	  if (SOLIB_IN_DYNAMIC_LINKER (pid, stop_pc))
	    {
	      another_trap = 1;
	      goto keep_going;
	    }
#endif
	  /* Else, stop and report the catchpoint(s) whose triggering
             caused us to begin stepping. */
	  stepping_through_solib_after_catch = 0;
	  bpstat_clear (&stop_bpstat);
	  stop_bpstat = bpstat_copy (stepping_through_solib_catchpoints);
	  bpstat_clear (&stepping_through_solib_catchpoints);
	  stop_print_frame = 1;
	  goto stop_stepping;
	}

#ifndef CALL_DUMMY_BREAKPOINT_OFFSET
      /* This is the old way of detecting the end of the stack dummy.
	 An architecture which defines CALL_DUMMY_BREAKPOINT_OFFSET gets
	 handled above.  As soon as we can test it on all of them, all
	 architectures should define it.  */

      /* If this is the breakpoint at the end of a stack dummy,
	 just stop silently, unless the user was doing an si/ni, in which
	 case she'd better know what she's doing.  */

      if (CALL_DUMMY_HAS_COMPLETED (stop_pc, read_sp (),
				    FRAME_FP (get_current_frame ()))
	  && !step_range_end)
	{
	  stop_print_frame = 0;
	  stop_stack_dummy = 1;
#ifdef HP_OS_BUG
	  trap_expected_after_continue = 1;
#endif
	  break;
	}
#endif /* No CALL_DUMMY_BREAKPOINT_OFFSET.  */

      if (step_resume_breakpoint)
	/* Having a step-resume breakpoint overrides anything
	   else having to do with stepping commands until
	   that breakpoint is reached.  */
	/* I'm not sure whether this needs to be check_sigtramp2 or
	   whether it could/should be keep_going.  */
	goto check_sigtramp2;

      if (step_range_end == 0)
	/* Likewise if we aren't even stepping.  */
	/* I'm not sure whether this needs to be check_sigtramp2 or
	   whether it could/should be keep_going.  */
	goto check_sigtramp2;

      /* If stepping through a line, keep going if still within it.

         Note that step_range_end is the address of the first instruction
         beyond the step range, and NOT the address of the last instruction
         within it! */
      if (stop_pc >= step_range_start
	  && stop_pc < step_range_end
#if 0
/* I haven't a clue what might trigger this clause, and it seems wrong
   anyway, so I've disabled it until someone complains.  -Stu 10/24/95 */

      /* The step range might include the start of the
	     function, so if we are at the start of the
	     step range and either the stack or frame pointers
	     just changed, we've stepped outside */
	  && !(stop_pc == step_range_start
	       && FRAME_FP (get_current_frame ())
	       && (INNER_THAN (read_sp (), step_sp)
		   || FRAME_FP (get_current_frame ()) != step_frame_address))
#endif
	)
	{
	  /* We might be doing a BPSTAT_WHAT_SINGLE and getting a signal.
	     So definately need to check for sigtramp here.  */
	  goto check_sigtramp2;
	}

      /* We stepped out of the stepping range.  */

      /* If we are stepping at the source level and entered the runtime
         loader dynamic symbol resolution code, we keep on single stepping
	 until we exit the run time loader code and reach the callee's
	 address.  */
      if (step_over_calls < 0 && IN_SOLIB_DYNSYM_RESOLVE_CODE (stop_pc))
	goto keep_going;

      /* We can't update step_sp every time through the loop, because
	 reading the stack pointer would slow down stepping too much.
	 But we can update it every time we leave the step range.  */
      update_step_sp = 1;

      /* Did we just take a signal?  */
      if (IN_SIGTRAMP (stop_pc, stop_func_name)
	  && !IN_SIGTRAMP (prev_pc, prev_func_name)
	  && INNER_THAN (read_sp (), step_sp))
	{
	  /* We've just taken a signal; go until we are back to
	     the point where we took it and one more.  */

	  /* Note: The test above succeeds not only when we stepped
             into a signal handler, but also when we step past the last
             statement of a signal handler and end up in the return stub
             of the signal handler trampoline.  To distinguish between
             these two cases, check that the frame is INNER_THAN the
             previous one below. pai/1997-09-11 */


	  {
	    CORE_ADDR current_frame = FRAME_FP (get_current_frame ());

	    if (INNER_THAN (current_frame, step_frame_address))
	      {
		/* We have just taken a signal; go until we are back to
                   the point where we took it and one more.  */

		/* This code is needed at least in the following case:
                   The user types "next" and then a signal arrives (before
                   the "next" is done).  */

		/* Note that if we are stopped at a breakpoint, then we need
                   the step_resume breakpoint to override any breakpoints at
                   the same location, so that we will still step over the
                   breakpoint even though the signal happened.  */
		struct symtab_and_line sr_sal;

		INIT_SAL (&sr_sal);
		sr_sal.symtab = NULL;
		sr_sal.line = 0;
		sr_sal.pc = prev_pc;
		/* We could probably be setting the frame to
                   step_frame_address; I don't think anyone thought to
                   try it.  */
		step_resume_breakpoint =
		  set_momentary_breakpoint (sr_sal, NULL, bp_step_resume);
		if (breakpoints_inserted)
		  insert_breakpoints ();
	      }
	    else
	      {
		/* We just stepped out of a signal handler and into
                   its calling trampoline.

                   Normally, we'd jump to step_over_function from
                   here, but for some reason GDB can't unwind the
                   stack correctly to find the real PC for the point
                   user code where the signal trampoline will return
                   -- FRAME_SAVED_PC fails, at least on HP-UX 10.20.
                   But signal trampolines are pretty small stubs of
                   code, anyway, so it's OK instead to just
                   single-step out.  Note: assuming such trampolines
                   don't exhibit recursion on any platform... */
		find_pc_partial_function (stop_pc, &stop_func_name,
					  &stop_func_start,
					  &stop_func_end);
		/* Readjust stepping range */
		step_range_start = stop_func_start;
		step_range_end = stop_func_end;
		stepping_through_sigtramp = 1;
	      }
	  }


	  /* If this is stepi or nexti, make sure that the stepping range
	     gets us past that instruction.  */
	  if (step_range_end == 1)
	    /* FIXME: Does this run afoul of the code below which, if
	       we step into the middle of a line, resets the stepping
	       range?  */
	    step_range_end = (step_range_start = prev_pc) + 1;

	  remove_breakpoints_on_following_step = 1;
	  goto keep_going;
	}

#if 0
      /* I disabled this test because it was too complicated and slow.
	 The SKIP_PROLOGUE was especially slow, because it caused
	 unnecessary prologue examination on various architectures.
	 The code in the #else clause has been tested on the Sparc,
	 Mips, PA, and Power architectures, so it's pretty likely to
	 be correct.  -Stu 10/24/95 */

      /* See if we left the step range due to a subroutine call that
	 we should proceed to the end of.  */

      if (stop_func_start)
	{
	  struct symtab *s;

	  /* Do this after the IN_SIGTRAMP check; it might give
	     an error.  */
	  prologue_pc = stop_func_start;

	  /* Don't skip the prologue if this is assembly source */
	  s = find_pc_symtab (stop_pc);
	  if (s && s->language != language_asm)
	    SKIP_PROLOGUE (prologue_pc);
	}

      if (!(INNER_THAN (step_sp, read_sp ()))	/* don't mistake (sig)return
						   as a call */
	  && (			/* Might be a non-recursive call.  If the symbols are missing
		 enough that stop_func_start == prev_func_start even though
		 they are really two functions, we will treat some calls as
		 jumps.  */
	       stop_func_start != prev_func_start

      /* Might be a recursive call if either we have a prologue
		 or the call instruction itself saves the PC on the stack.  */
	       || prologue_pc != stop_func_start
	       || read_sp () != step_sp)
	  && (			/* PC is completely out of bounds of any known objfiles.  Treat
		 like a subroutine call. */
	       !stop_func_start

      /* If we do a call, we will be at the start of a function...  */
	       || stop_pc == stop_func_start

      /* ...except on the Alpha with -O (and also Irix 5 and
		 perhaps others), in which we might call the address
		 after the load of gp.  Since prologues don't contain
		 calls, we can't return to within one, and we don't
		 jump back into them, so this check is OK.  */

	       || stop_pc < prologue_pc

      /* ...and if it is a leaf function, the prologue might
 		 consist of gp loading only, so the call transfers to
 		 the first instruction after the prologue.  */
	       || (stop_pc == prologue_pc

      /* Distinguish this from the case where we jump back
		     to the first instruction after the prologue,
		     within a function.  */
		   && stop_func_start != prev_func_start)

      /* If we end up in certain places, it means we did a subroutine
		 call.  I'm not completely sure this is necessary now that we
		 have the above checks with stop_func_start (and now that
		 find_pc_partial_function is pickier).  */
	       || IN_SOLIB_CALL_TRAMPOLINE (stop_pc, stop_func_name)

      /* If none of the above apply, it is a jump within a function,
		 or a return from a subroutine.  The other case is longjmp,
		 which can no longer happen here as long as the
		 handling_longjmp stuff is working.  */
	  ))
#else
      /* This test is a much more streamlined, (but hopefully correct)
	   replacement for the code above.  It's been tested on the Sparc,
	   Mips, PA, and Power architectures with good results.  */

      if (stop_pc == stop_func_start	/* Quick test */
	  || (in_prologue (stop_pc, stop_func_start) &&
	      !IN_SOLIB_RETURN_TRAMPOLINE (stop_pc, stop_func_name))
	  || IN_SOLIB_CALL_TRAMPOLINE (stop_pc, stop_func_name)
	  || stop_func_name == 0)
#endif

	{
	  /* It's a subroutine call.  */

	  if (step_over_calls == 0)
	    {
	      /* I presume that step_over_calls is only 0 when we're
		 supposed to be stepping at the assembly language level
		 ("stepi").  Just stop.  */
	      stop_step = 1;
	      break;
	    }

	  if (step_over_calls > 0 || IGNORE_HELPER_CALL (stop_pc))
	    /* We're doing a "next".  */
	    goto step_over_function;

	  /* If we are in a function call trampoline (a stub between
	     the calling routine and the real function), locate the real
	     function.  That's what tells us (a) whether we want to step
	     into it at all, and (b) what prologue we want to run to
	     the end of, if we do step into it.  */
	  tmp = SKIP_TRAMPOLINE_CODE (stop_pc);
	  if (tmp != 0)
	    stop_func_start = tmp;
	  else
	    {
	      tmp = DYNAMIC_TRAMPOLINE_NEXTPC (stop_pc);
	      if (tmp)
		{
		  struct symtab_and_line xxx;
		  /* Why isn't this s_a_l called "sr_sal", like all of the
		     other s_a_l's where this code is duplicated?  */
		  INIT_SAL (&xxx);	/* initialize to zeroes */
		  xxx.pc = tmp;
		  xxx.section = find_pc_overlay (xxx.pc);
		  step_resume_breakpoint =
		    set_momentary_breakpoint (xxx, NULL, bp_step_resume);
		  insert_breakpoints ();
		  goto keep_going;
		}
	    }

	  /* If we have line number information for the function we
	     are thinking of stepping into, step into it.

	     If there are several symtabs at that PC (e.g. with include
	     files), just want to know whether *any* of them have line
	     numbers.  find_pc_line handles this.  */
	  {
	    struct symtab_and_line tmp_sal;

	    tmp_sal = find_pc_line (stop_func_start, 0);
	    if (tmp_sal.line != 0)
	      goto step_into_function;
	  }

	step_over_function:
	  /* A subroutine call has happened.  */
	  {
	    /* Set a special breakpoint after the return */
	    struct symtab_and_line sr_sal;

	    INIT_SAL (&sr_sal);
	    sr_sal.symtab = NULL;
	    sr_sal.line = 0;

	    /* If we came here after encountering a signal in the middle of
               a "next", use the stashed-away previous frame pc */
	    sr_sal.pc
	      = stopped_by_random_signal
	      ? prev_pc
	      : ADDR_BITS_REMOVE (SAVED_PC_AFTER_CALL (get_current_frame ()));

	    step_resume_breakpoint =
	      set_momentary_breakpoint (sr_sal,
					stopped_by_random_signal ?
					NULL : get_current_frame (),
					bp_step_resume);

	    /* We've just entered a callee, and we wish to resume until
               it returns to the caller.  Setting a step_resume bp on
               the return PC will catch a return from the callee.

               However, if the callee is recursing, we want to be
               careful not to catch returns of those recursive calls,
               but of THIS instance of the call.

               To do this, we set the step_resume bp's frame to our
               current caller's frame (step_frame_address, which is
               set by the "next" or "until" command, before execution
               begins).

               But ... don't do it if we're single-stepping out of a
               sigtramp, because the reason we're single-stepping is
               precisely because unwinding is a problem (HP-UX 10.20,
               e.g.) and the frame address is likely to be incorrect.
               No danger of sigtramp recursion.  */

	    if (stepping_through_sigtramp)
	      {
		step_resume_breakpoint->frame = (CORE_ADDR) NULL;
		stepping_through_sigtramp = 0;
	      }
	    else if (!IN_SOLIB_DYNSYM_RESOLVE_CODE (sr_sal.pc))
	      step_resume_breakpoint->frame = step_frame_address;

	    if (breakpoints_inserted)
	      insert_breakpoints ();
	  }
	  goto keep_going;

	step_into_function:
	  /* Subroutine call with source code we should not step over.
	     Do step to the first line of code in it.  */
	  {
	    struct symtab *s;

	    s = find_pc_symtab (stop_pc);
	    if (s && s->language != language_asm)
	      SKIP_PROLOGUE (stop_func_start);
	  }
	  sal = find_pc_line (stop_func_start, 0);
	  /* Use the step_resume_break to step until
	     the end of the prologue, even if that involves jumps
	     (as it seems to on the vax under 4.2).  */
	  /* If the prologue ends in the middle of a source line,
	     continue to the end of that source line (if it is still
	     within the function).  Otherwise, just go to end of prologue.  */
#ifdef PROLOGUE_FIRSTLINE_OVERLAP
	  /* no, don't either.  It skips any code that's
	     legitimately on the first line.  */
#else
	  if (sal.end && sal.pc != stop_func_start && sal.end < stop_func_end)
	    stop_func_start = sal.end;
#endif

	  if (stop_func_start == stop_pc)
	    {
	      /* We are already there: stop now.  */
	      stop_step = 1;
	      break;
	    }
	  else
	    /* Put the step-breakpoint there and go until there. */
	    {
	      struct symtab_and_line sr_sal;

	      INIT_SAL (&sr_sal);	/* initialize to zeroes */
	      sr_sal.pc = stop_func_start;
	      sr_sal.section = find_pc_overlay (stop_func_start);
	      /* Do not specify what the fp should be when we stop
		 since on some machines the prologue
		 is where the new fp value is established.  */
	      step_resume_breakpoint =
		set_momentary_breakpoint (sr_sal, NULL, bp_step_resume);
	      if (breakpoints_inserted)
		insert_breakpoints ();

	      /* And make sure stepping stops right away then.  */
	      step_range_end = step_range_start;
	    }
	  goto keep_going;
	}

      /* We've wandered out of the step range.  */

      sal = find_pc_line (stop_pc, 0);

      if (step_range_end == 1)
	{
	  /* It is stepi or nexti.  We always want to stop stepping after
	     one instruction.  */
	  stop_step = 1;
	  break;
	}

      /* If we're in the return path from a shared library trampoline,
	 we want to proceed through the trampoline when stepping.  */
      if (IN_SOLIB_RETURN_TRAMPOLINE (stop_pc, stop_func_name))
	{
	  CORE_ADDR tmp;

	  /* Determine where this trampoline returns.  */
	  tmp = SKIP_TRAMPOLINE_CODE (stop_pc);

	  /* Only proceed through if we know where it's going.  */
	  if (tmp)
	    {
	      /* And put the step-breakpoint there and go until there. */
	      struct symtab_and_line sr_sal;

	      INIT_SAL (&sr_sal);	/* initialize to zeroes */
	      sr_sal.pc = tmp;
	      sr_sal.section = find_pc_overlay (sr_sal.pc);
	      /* Do not specify what the fp should be when we stop
		 since on some machines the prologue
		 is where the new fp value is established.  */
	      step_resume_breakpoint =
		set_momentary_breakpoint (sr_sal, NULL, bp_step_resume);
	      if (breakpoints_inserted)
		insert_breakpoints ();

	      /* Restart without fiddling with the step ranges or
		 other state.  */
	      goto keep_going;
	    }
	}

      if (sal.line == 0)
	{
	  /* We have no line number information.  That means to stop
	     stepping (does this always happen right after one instruction,
	     when we do "s" in a function with no line numbers,
	     or can this happen as a result of a return or longjmp?).  */
	  stop_step = 1;
	  break;
	}

      if ((stop_pc == sal.pc)
	  && (current_line != sal.line || current_symtab != sal.symtab))
	{
	  /* We are at the start of a different line.  So stop.  Note that
	     we don't stop if we step into the middle of a different line.
	     That is said to make things like for (;;) statements work
	     better.  */
	  stop_step = 1;
	  break;
	}

      /* We aren't done stepping.

	 Optimize by setting the stepping range to the line.
	 (We might not be in the original line, but if we entered a
	 new line in mid-statement, we continue stepping.  This makes
	 things like for(;;) statements work better.)  */

      if (stop_func_end && sal.end >= stop_func_end)
	{
	  /* If this is the last line of the function, don't keep stepping
	     (it would probably step us out of the function).
	     This is particularly necessary for a one-line function,
	     in which after skipping the prologue we better stop even though
	     we will be in mid-line.  */
	  stop_step = 1;
	  break;
	}
      step_range_start = sal.pc;
      step_range_end = sal.end;
      step_frame_address = FRAME_FP (get_current_frame ());
      current_line = sal.line;
      current_symtab = sal.symtab;

      /* In the case where we just stepped out of a function into the middle
         of a line of the caller, continue stepping, but step_frame_address
         must be modified to current frame */
      {
	CORE_ADDR current_frame = FRAME_FP (get_current_frame ());
	if (!(INNER_THAN (current_frame, step_frame_address)))
	  step_frame_address = current_frame;
      }


      goto keep_going;

    check_sigtramp2:
      if (trap_expected
	  && IN_SIGTRAMP (stop_pc, stop_func_name)
	  && !IN_SIGTRAMP (prev_pc, prev_func_name)
	  && INNER_THAN (read_sp (), step_sp))
	{
	  /* What has happened here is that we have just stepped the inferior
	     with a signal (because it is a signal which shouldn't make
	     us stop), thus stepping into sigtramp.

	     So we need to set a step_resume_break_address breakpoint
	     and continue until we hit it, and then step.  FIXME: This should
	     be more enduring than a step_resume breakpoint; we should know
	     that we will later need to keep going rather than re-hitting
	     the breakpoint here (see testsuite/gdb.t06/signals.exp where
	     it says "exceedingly difficult").  */
	  struct symtab_and_line sr_sal;

	  INIT_SAL (&sr_sal);	/* initialize to zeroes */
	  sr_sal.pc = prev_pc;
	  sr_sal.section = find_pc_overlay (sr_sal.pc);
	  /* We perhaps could set the frame if we kept track of what
	     the frame corresponding to prev_pc was.  But we don't,
	     so don't.  */
	  through_sigtramp_breakpoint =
	    set_momentary_breakpoint (sr_sal, NULL, bp_through_sigtramp);
	  if (breakpoints_inserted)
	    insert_breakpoints ();

	  remove_breakpoints_on_following_step = 1;
	  another_trap = 1;
	}

    keep_going:
      /* Come to this label when you need to resume the inferior.
	 It's really much cleaner to do a goto than a maze of if-else
	 conditions.  */

      /* ??rehrauer: ttrace on HP-UX theoretically allows one to debug
         a vforked child beetween its creation and subsequent exit or
         call to exec().  However, I had big problems in this rather
         creaky exec engine, getting that to work.  The fundamental
         problem is that I'm trying to debug two processes via an
         engine that only understands a single process with possibly
         multiple threads.

         Hence, this spot is known to have problems when
         target_can_follow_vfork_prior_to_exec returns 1. */

      /* Save the pc before execution, to compare with pc after stop.  */
      prev_pc = read_pc ();	/* Might have been DECR_AFTER_BREAK */
      prev_func_start = stop_func_start;	/* Ok, since if DECR_PC_AFTER
					  BREAK is defined, the
					  original pc would not have
					  been at the start of a
					  function. */
      prev_func_name = stop_func_name;

      if (update_step_sp)
	step_sp = read_sp ();
      update_step_sp = 0;

      /* If we did not do break;, it means we should keep
	 running the inferior and not return to debugger.  */

      if (trap_expected && stop_signal != TARGET_SIGNAL_TRAP)
	{
	  /* We took a signal (which we are supposed to pass through to
	     the inferior, else we'd have done a break above) and we
	     haven't yet gotten our trap.  Simply continue.  */
	  resume (CURRENTLY_STEPPING (), stop_signal);
	}
      else
	{
	  /* Either the trap was not expected, but we are continuing
	     anyway (the user asked that this signal be passed to the
	     child)
	       -- or --
	     The signal was SIGTRAP, e.g. it was our signal, but we
	     decided we should resume from it.

	     We're going to run this baby now!

	     Insert breakpoints now, unless we are trying
	     to one-proceed past a breakpoint.  */
	  /* If we've just finished a special step resume and we don't
	     want to hit a breakpoint, pull em out.  */
	  if (step_resume_breakpoint == NULL
	      && through_sigtramp_breakpoint == NULL
	      && remove_breakpoints_on_following_step)
	    {
	      remove_breakpoints_on_following_step = 0;
	      remove_breakpoints ();
	      breakpoints_inserted = 0;
	    }
	  else if (!breakpoints_inserted &&
		   (through_sigtramp_breakpoint != NULL || !another_trap))
	    {
	      breakpoints_failed = insert_breakpoints ();
	      if (breakpoints_failed)
		break;
	      breakpoints_inserted = 1;
	    }

	  trap_expected = another_trap;

          /* Do not deliver SIGNAL_TRAP (except when the user
	     explicitly specifies that such a signal should be
	     delivered to the target program).

	     Typically, this would occure when a user is debugging a
	     target monitor on a simulator: the target monitor sets a
	     breakpoint; the simulator encounters this break-point and
	     halts the simulation handing control to GDB; GDB, noteing
	     that the break-point isn't valid, returns control back to
	     the simulator; the simulator then delivers the hardware
	     equivalent of a SIGNAL_TRAP to the program being
	     debugged. */

	  if (stop_signal == TARGET_SIGNAL_TRAP
	      && !signal_program[stop_signal])
	    stop_signal = TARGET_SIGNAL_0;

#ifdef SHIFT_INST_REGS
	  /* I'm not sure when this following segment applies.  I do know,
	     now, that we shouldn't rewrite the regs when we were stopped
	     by a random signal from the inferior process.  */
	  /* FIXME: Shouldn't this be based on the valid bit of the SXIP?
	     (this is only used on the 88k).  */

	  if (!bpstat_explains_signal (stop_bpstat)
	      && (stop_signal != TARGET_SIGNAL_CHLD)
	      && !stopped_by_random_signal)
	    SHIFT_INST_REGS ();
#endif /* SHIFT_INST_REGS */

	  resume (CURRENTLY_STEPPING (), stop_signal);
	}
    }

stop_stepping:
  if (target_has_execution)
    {
      /* Are we stopping for a vfork event?  We only stop when we see
         the child's event.  However, we may not yet have seen the
         parent's event.  And, inferior_pid is still set to the parent's
         pid, until we resume again and follow either the parent or child.

         To ensure that we can really touch inferior_pid (aka, the
         parent process) -- which calls to functions like read_pc
         implicitly do -- wait on the parent if necessary. */
      if ((pending_follow.kind == TARGET_WAITKIND_VFORKED)
	  && !pending_follow.fork_event.saw_parent_fork)
	{
	  int parent_pid;

	  do
	    {
	      if (target_wait_hook)
		parent_pid = target_wait_hook (-1, &w);
	      else
		parent_pid = target_wait (-1, &w);
	    }
	  while (parent_pid != inferior_pid);
	}


      /* Assuming the inferior still exists, set these up for next
	 time, just like we did above if we didn't break out of the
	 loop.  */
      prev_pc = read_pc ();
      prev_func_start = stop_func_start;
      prev_func_name = stop_func_name;
    }
  do_cleanups (old_cleanups);
}

/* This function returns TRUE if ep is an internal breakpoint
   set to catch generic shared library (aka dynamically-linked
   library) events.  (This is *NOT* the same as a catchpoint for a
   shlib event.  The latter is something a user can set; this is
   something gdb sets for its own use, and isn't ever shown to a
   user.) */
static int
is_internal_shlib_eventpoint (ep)
     struct breakpoint *ep;
{
  return
    (ep->type == bp_shlib_event)
    ;
}

/* This function returns TRUE if bs indicates that the inferior
   stopped due to a shared library (aka dynamically-linked library)
   event. */
static int
stopped_for_internal_shlib_event (bs)
     bpstat bs;
{
  /* Note that multiple eventpoints may've caused the stop.  Any
     that are associated with shlib events will be accepted. */
  for (; bs != NULL; bs = bs->next)
    {
      if ((bs->breakpoint_at != NULL)
	  && is_internal_shlib_eventpoint (bs->breakpoint_at))
	return 1;
    }

  /* If we get here, then no candidate was found. */
  return 0;
}

/* This function returns TRUE if bs indicates that the inferior
   stopped due to a shared library (aka dynamically-linked library)
   event caught by a catchpoint.

   If TRUE, cp_p is set to point to the catchpoint.

   Else, the value of cp_p is undefined. */
static int
stopped_for_shlib_catchpoint (bs, cp_p)
     bpstat bs;
     struct breakpoint **cp_p;
{
  /* Note that multiple eventpoints may've caused the stop.  Any
     that are associated with shlib events will be accepted. */
  *cp_p = NULL;

  for (; bs != NULL; bs = bs->next)
    {
      if ((bs->breakpoint_at != NULL)
	  && ep_is_shlib_catchpoint (bs->breakpoint_at))
	{
	  *cp_p = bs->breakpoint_at;
	  return 1;
	}
    }

  /* If we get here, then no candidate was found. */
  return 0;
}


/* Here to return control to GDB when the inferior stops for real.
   Print appropriate messages, remove breakpoints, give terminal our modes.

   STOP_PRINT_FRAME nonzero means print the executing frame
   (pc, function, args, file, line number and line text).
   BREAKPOINTS_FAILED nonzero means stop was due to error
   attempting to insert breakpoints.  */

void
normal_stop ()
{

#ifdef HPUXHPPA
  /* As with the notification of thread events, we want to delay
     notifying the user that we've switched thread context until
     the inferior actually stops.

     (Note that there's no point in saying anything if the inferior
     has exited!) */
  if ((switched_from_inferior_pid != inferior_pid) &&
      target_has_execution)
    {
      target_terminal_ours_for_output ();
      printf_filtered ("[Switched to %s]\n",
		       target_pid_or_tid_to_str (inferior_pid));
      switched_from_inferior_pid = inferior_pid;
    }
#endif

  /* Make sure that the current_frame's pc is correct.  This
     is a correction for setting up the frame info before doing
     DECR_PC_AFTER_BREAK */
  if (target_has_execution && get_current_frame ())
    (get_current_frame ())->pc = read_pc ();

  if (breakpoints_failed)
    {
      target_terminal_ours_for_output ();
      print_sys_errmsg ("ptrace", breakpoints_failed);
      printf_filtered ("Stopped; cannot insert breakpoints.\n\
The same program may be running in another process.\n");
    }

  if (target_has_execution && breakpoints_inserted)
    {
      if (remove_breakpoints ())
	{
	  target_terminal_ours_for_output ();
	  printf_filtered ("Cannot remove breakpoints because ");
	  printf_filtered ("program is no longer writable.\n");
	  printf_filtered ("It might be running in another process.\n");
	  printf_filtered ("Further execution is probably impossible.\n");
	}
    }
  breakpoints_inserted = 0;

  /* Delete the breakpoint we stopped at, if it wants to be deleted.
     Delete any breakpoint that is to be deleted at the next stop.  */

  breakpoint_auto_delete (stop_bpstat);

  /* If an auto-display called a function and that got a signal,
     delete that auto-display to avoid an infinite recursion.  */

  if (stopped_by_random_signal)
    disable_current_display ();

  /* Don't print a message if in the middle of doing a "step n"
     operation for n > 1 */
  if (step_multi && stop_step)
    goto done;

  target_terminal_ours ();

  /* Did we stop because the user set the stop_on_solib_events
     variable?  (If so, we report this as a generic, "Stopped due
     to shlib event" message.) */
  if (stopped_for_internal_shlib_event (stop_bpstat))
    {
      printf_filtered ("Stopped due to shared library event\n");
    }

  /* Look up the hook_stop and run it if it exists.  */

  if (stop_command && stop_command->hook)
    {
      catch_errors (hook_stop_stub, stop_command->hook,
		    "Error while running hook_stop:\n", RETURN_MASK_ALL);
    }

  if (!target_has_stack)
    {

      goto done;
    }

  /* Select innermost stack frame - i.e., current frame is frame 0,
     and current location is based on that.
     Don't do this on return from a stack dummy routine,
     or if the program has exited. */

  if (!stop_stack_dummy)
    {
      select_frame (get_current_frame (), 0);

      /* Print current location without a level number, if
	 we have changed functions or hit a breakpoint.
	 Print source line if we have one.
	 bpstat_print() contains the logic deciding in detail
	 what to print, based on the event(s) that just occurred. */

      if (stop_print_frame)
	{
	  int bpstat_ret;
	  int source_flag;

	  bpstat_ret = bpstat_print (stop_bpstat);
	  /* bpstat_print() returned one of:
             -1: Didn't print anything
              0: Printed preliminary "Breakpoint n, " message, desires
                 location tacked on
              1: Printed something, don't tack on location */

	  if (bpstat_ret == -1)
	    if (stop_step
		&& step_frame_address == FRAME_FP (get_current_frame ())
		&& step_start_function == find_pc_function (stop_pc))
	      source_flag = -1;	/* finished step, just print source line */
	    else
	      source_flag = 1;	/* print location and source line */
	  else if (bpstat_ret == 0)	/* hit bpt, desire location */
	    source_flag = 1;	/* print location and source line */
	  else			/* bpstat_ret == 1, hit bpt, do not desire location */
	    source_flag = -1;	/* just print source line */

	  /* The behavior of this routine with respect to the source
	     flag is:
	     -1: Print only source line
	     0: Print only location
	     1: Print location and source line */
	  show_and_print_stack_frame (selected_frame, -1, source_flag);

	  /* Display the auto-display expressions.  */
	  do_displays ();
	}
    }

  /* Save the function value return registers, if we care.
     We might be about to restore their previous contents.  */
  if (proceed_to_finish)
    read_register_bytes (0, stop_registers, REGISTER_BYTES);

  if (stop_stack_dummy)
    {
      /* Pop the empty frame that contains the stack dummy.
         POP_FRAME ends with a setting of the current frame, so we
	 can use that next. */
      POP_FRAME;
      /* Set stop_pc to what it was before we called the function.
	 Can't rely on restore_inferior_status because that only gets
	 called if we don't stop in the called function.  */
      stop_pc = read_pc ();
      select_frame (get_current_frame (), 0);
    }


  TUIDO (((TuiOpaqueFuncPtr) tui_vCheckDataValues, selected_frame));

done:
  annotate_stopped ();
}

static int
hook_stop_stub (cmd)
     PTR cmd;
{
  execute_user_command ((struct cmd_list_element *) cmd, 0);
  return (0);
}

int 
signal_stop_state (signo)
     int signo;
{
  return signal_stop[signo];
}

int 
signal_print_state (signo)
     int signo;
{
  return signal_print[signo];
}

int 
signal_pass_state (signo)
     int signo;
{
  return signal_program[signo];
}

static void
sig_print_header ()
{
  printf_filtered ("\
Signal        Stop\tPrint\tPass to program\tDescription\n");
}

static void
sig_print_info (oursig)
     enum target_signal oursig;
{
  char *name = target_signal_to_name (oursig);
  int name_padding = 13 - strlen (name);
  if (name_padding <= 0)
    name_padding = 0;

  printf_filtered ("%s", name);
  printf_filtered ("%*.*s ", name_padding, name_padding,
		   "                 ");
  printf_filtered ("%s\t", signal_stop[oursig] ? "Yes" : "No");
  printf_filtered ("%s\t", signal_print[oursig] ? "Yes" : "No");
  printf_filtered ("%s\t\t", signal_program[oursig] ? "Yes" : "No");
  printf_filtered ("%s\n", target_signal_to_string (oursig));
}

/* Specify how various signals in the inferior should be handled.  */

static void
handle_command (args, from_tty)
     char *args;
     int from_tty;
{
  char **argv;
  int digits, wordlen;
  int sigfirst, signum, siglast;
  enum target_signal oursig;
  int allsigs;
  int nsigs;
  unsigned char *sigs;
  struct cleanup *old_chain;

  if (args == NULL)
    {
      error_no_arg ("signal to handle");
    }

  /* Allocate and zero an array of flags for which signals to handle. */

  nsigs = (int) TARGET_SIGNAL_LAST;
  sigs = (unsigned char *) alloca (nsigs);
  memset (sigs, 0, nsigs);

  /* Break the command line up into args. */

  argv = buildargv (args);
  if (argv == NULL)
    {
      nomem (0);
    }
  old_chain = make_cleanup ((make_cleanup_func) freeargv, (char *) argv);

  /* Walk through the args, looking for signal oursigs, signal names, and
     actions.  Signal numbers and signal names may be interspersed with
     actions, with the actions being performed for all signals cumulatively
     specified.  Signal ranges can be specified as <LOW>-<HIGH>. */

  while (*argv != NULL)
    {
      wordlen = strlen (*argv);
      for (digits = 0; isdigit ((*argv)[digits]); digits++)
	{;
	}
      allsigs = 0;
      sigfirst = siglast = -1;

      if (wordlen >= 1 && !strncmp (*argv, "all", wordlen))
	{
	  /* Apply action to all signals except those used by the
	     debugger.  Silently skip those. */
	  allsigs = 1;
	  sigfirst = 0;
	  siglast = nsigs - 1;
	}
      else if (wordlen >= 1 && !strncmp (*argv, "stop", wordlen))
	{
	  SET_SIGS (nsigs, sigs, signal_stop);
	  SET_SIGS (nsigs, sigs, signal_print);
	}
      else if (wordlen >= 1 && !strncmp (*argv, "ignore", wordlen))
	{
	  UNSET_SIGS (nsigs, sigs, signal_program);
	}
      else if (wordlen >= 2 && !strncmp (*argv, "print", wordlen))
	{
	  SET_SIGS (nsigs, sigs, signal_print);
	}
      else if (wordlen >= 2 && !strncmp (*argv, "pass", wordlen))
	{
	  SET_SIGS (nsigs, sigs, signal_program);
	}
      else if (wordlen >= 3 && !strncmp (*argv, "nostop", wordlen))
	{
	  UNSET_SIGS (nsigs, sigs, signal_stop);
	}
      else if (wordlen >= 3 && !strncmp (*argv, "noignore", wordlen))
	{
	  SET_SIGS (nsigs, sigs, signal_program);
	}
      else if (wordlen >= 4 && !strncmp (*argv, "noprint", wordlen))
	{
	  UNSET_SIGS (nsigs, sigs, signal_print);
	  UNSET_SIGS (nsigs, sigs, signal_stop);
	}
      else if (wordlen >= 4 && !strncmp (*argv, "nopass", wordlen))
	{
	  UNSET_SIGS (nsigs, sigs, signal_program);
	}
      else if (digits > 0)
	{
	  /* It is numeric.  The numeric signal refers to our own
	     internal signal numbering from target.h, not to host/target
	     signal  number.  This is a feature; users really should be
	     using symbolic names anyway, and the common ones like
	     SIGHUP, SIGINT, SIGALRM, etc. will work right anyway.  */

	  sigfirst = siglast = (int)
	    target_signal_from_command (atoi (*argv));
	  if ((*argv)[digits] == '-')
	    {
	      siglast = (int)
		target_signal_from_command (atoi ((*argv) + digits + 1));
	    }
	  if (sigfirst > siglast)
	    {
	      /* Bet he didn't figure we'd think of this case... */
	      signum = sigfirst;
	      sigfirst = siglast;
	      siglast = signum;
	    }
	}
      else
	{
	  oursig = target_signal_from_name (*argv);
	  if (oursig != TARGET_SIGNAL_UNKNOWN)
	    {
	      sigfirst = siglast = (int) oursig;
	    }
	  else
	    {
	      /* Not a number and not a recognized flag word => complain.  */
	      error ("Unrecognized or ambiguous flag word: \"%s\".", *argv);
	    }
	}

      /* If any signal numbers or symbol names were found, set flags for
	 which signals to apply actions to. */

      for (signum = sigfirst; signum >= 0 && signum <= siglast; signum++)
	{
	  switch ((enum target_signal) signum)
	    {
	    case TARGET_SIGNAL_TRAP:
	    case TARGET_SIGNAL_INT:
	      if (!allsigs && !sigs[signum])
		{
		  if (query ("%s is used by the debugger.\n\
Are you sure you want to change it? ",
			     target_signal_to_name
			     ((enum target_signal) signum)))
		    {
		      sigs[signum] = 1;
		    }
		  else
		    {
		      printf_unfiltered ("Not confirmed, unchanged.\n");
		      gdb_flush (gdb_stdout);
		    }
		}
	      break;
	    case TARGET_SIGNAL_0:
	    case TARGET_SIGNAL_DEFAULT:
	    case TARGET_SIGNAL_UNKNOWN:
	      /* Make sure that "all" doesn't print these.  */
	      break;
	    default:
	      sigs[signum] = 1;
	      break;
	    }
	}

      argv++;
    }

  target_notice_signals (inferior_pid);

  if (from_tty)
    {
      /* Show the results.  */
      sig_print_header ();
      for (signum = 0; signum < nsigs; signum++)
	{
	  if (sigs[signum])
	    {
	      sig_print_info (signum);
	    }
	}
    }

  do_cleanups (old_chain);
}

static void
xdb_handle_command (args, from_tty)
     char *args;
     int from_tty;
{
  char **argv;
  struct cleanup *old_chain;

  /* Break the command line up into args. */

  argv = buildargv (args);
  if (argv == NULL)
    {
      nomem (0);
    }
  old_chain = make_cleanup ((make_cleanup_func) freeargv, (char *) argv);
  if (argv[1] != (char *) NULL)
    {
      char *argBuf;
      int bufLen;

      bufLen = strlen (argv[0]) + 20;
      argBuf = (char *) xmalloc (bufLen);
      if (argBuf)
	{
	  int validFlag = 1;
	  enum target_signal oursig;

	  oursig = target_signal_from_name (argv[0]);
	  memset (argBuf, 0, bufLen);
	  if (strcmp (argv[1], "Q") == 0)
	    sprintf (argBuf, "%s %s", argv[0], "noprint");
	  else
	    {
	      if (strcmp (argv[1], "s") == 0)
		{
		  if (!signal_stop[oursig])
		    sprintf (argBuf, "%s %s", argv[0], "stop");
		  else
		    sprintf (argBuf, "%s %s", argv[0], "nostop");
		}
	      else if (strcmp (argv[1], "i") == 0)
		{
		  if (!signal_program[oursig])
		    sprintf (argBuf, "%s %s", argv[0], "pass");
		  else
		    sprintf (argBuf, "%s %s", argv[0], "nopass");
		}
	      else if (strcmp (argv[1], "r") == 0)
		{
		  if (!signal_print[oursig])
		    sprintf (argBuf, "%s %s", argv[0], "print");
		  else
		    sprintf (argBuf, "%s %s", argv[0], "noprint");
		}
	      else
		validFlag = 0;
	    }
	  if (validFlag)
	    handle_command (argBuf, from_tty);
	  else
	    printf_filtered ("Invalid signal handling flag.\n");
	  if (argBuf)
	    free (argBuf);
	}
    }
  do_cleanups (old_chain);
}

/* Print current contents of the tables set by the handle command.
   It is possible we should just be printing signals actually used
   by the current target (but for things to work right when switching
   targets, all signals should be in the signal tables).  */

static void
signals_info (signum_exp, from_tty)
     char *signum_exp;
     int from_tty;
{
  enum target_signal oursig;
  sig_print_header ();

  if (signum_exp)
    {
      /* First see if this is a symbol name.  */
      oursig = target_signal_from_name (signum_exp);
      if (oursig == TARGET_SIGNAL_UNKNOWN)
	{
	  /* No, try numeric.  */
	  oursig =
	    target_signal_from_command (parse_and_eval_address (signum_exp));
	}
      sig_print_info (oursig);
      return;
    }

  printf_filtered ("\n");
  /* These ugly casts brought to you by the native VAX compiler.  */
  for (oursig = TARGET_SIGNAL_FIRST;
       (int) oursig < (int) TARGET_SIGNAL_LAST;
       oursig = (enum target_signal) ((int) oursig + 1))
    {
      QUIT;

      if (oursig != TARGET_SIGNAL_UNKNOWN
	  && oursig != TARGET_SIGNAL_DEFAULT
	  && oursig != TARGET_SIGNAL_0)
	sig_print_info (oursig);
    }

  printf_filtered ("\nUse the \"handle\" command to change these tables.\n");
}

/* Save all of the information associated with the inferior<==>gdb
   connection.  INF_STATUS is a pointer to a "struct inferior_status"
   (defined in inferior.h).  */

void
save_inferior_status (inf_status, restore_stack_info)
     struct inferior_status *inf_status;
     int restore_stack_info;
{
  inf_status->stop_signal = stop_signal;
  inf_status->stop_pc = stop_pc;
  inf_status->stop_step = stop_step;
  inf_status->stop_stack_dummy = stop_stack_dummy;
  inf_status->stopped_by_random_signal = stopped_by_random_signal;
  inf_status->trap_expected = trap_expected;
  inf_status->step_range_start = step_range_start;
  inf_status->step_range_end = step_range_end;
  inf_status->step_frame_address = step_frame_address;
  inf_status->step_over_calls = step_over_calls;
  inf_status->stop_after_trap = stop_after_trap;
  inf_status->stop_soon_quietly = stop_soon_quietly;
  /* Save original bpstat chain here; replace it with copy of chain.
     If caller's caller is walking the chain, they'll be happier if we
     hand them back the original chain when restore_i_s is called.  */
  inf_status->stop_bpstat = stop_bpstat;
  stop_bpstat = bpstat_copy (stop_bpstat);
  inf_status->breakpoint_proceeded = breakpoint_proceeded;
  inf_status->restore_stack_info = restore_stack_info;
  inf_status->proceed_to_finish = proceed_to_finish;

  memcpy (inf_status->stop_registers, stop_registers, REGISTER_BYTES);

  read_register_bytes (0, inf_status->registers, REGISTER_BYTES);

  record_selected_frame (&(inf_status->selected_frame_address),
			 &(inf_status->selected_level));
  return;
}

struct restore_selected_frame_args
{
  CORE_ADDR frame_address;
  int level;
};

static int restore_selected_frame PARAMS ((PTR));

/* Restore the selected frame.  args is really a struct
   restore_selected_frame_args * (declared as char * for catch_errors)
   telling us what frame to restore.  Returns 1 for success, or 0 for
   failure.  An error message will have been printed on error.  */

static int
restore_selected_frame (args)
     PTR args;
{
  struct restore_selected_frame_args *fr =
  (struct restore_selected_frame_args *) args;
  struct frame_info *frame;
  int level = fr->level;

  frame = find_relative_frame (get_current_frame (), &level);

  /* If inf_status->selected_frame_address is NULL, there was no
     previously selected frame.  */
  if (frame == NULL ||
  /*  FRAME_FP (frame) != fr->frame_address || */
  /* elz: deleted this check as a quick fix to the problem that
	 for function called by hand gdb creates no internal frame
	 structure and the real stack and gdb's idea of stack are
	 different if nested calls by hands are made.

	 mvs: this worries me.  */
      level != 0)
    {
      warning ("Unable to restore previously selected frame.\n");
      return 0;
    }

  select_frame (frame, fr->level);

  return (1);
}

void
restore_inferior_status (inf_status)
     struct inferior_status *inf_status;
{
  stop_signal = inf_status->stop_signal;
  stop_pc = inf_status->stop_pc;
  stop_step = inf_status->stop_step;
  stop_stack_dummy = inf_status->stop_stack_dummy;
  stopped_by_random_signal = inf_status->stopped_by_random_signal;
  trap_expected = inf_status->trap_expected;
  step_range_start = inf_status->step_range_start;
  step_range_end = inf_status->step_range_end;
  step_frame_address = inf_status->step_frame_address;
  step_over_calls = inf_status->step_over_calls;
  stop_after_trap = inf_status->stop_after_trap;
  stop_soon_quietly = inf_status->stop_soon_quietly;
  bpstat_clear (&stop_bpstat);
  stop_bpstat = inf_status->stop_bpstat;
  breakpoint_proceeded = inf_status->breakpoint_proceeded;
  proceed_to_finish = inf_status->proceed_to_finish;

  memcpy (stop_registers, inf_status->stop_registers, REGISTER_BYTES);

  /* The inferior can be gone if the user types "print exit(0)"
     (and perhaps other times).  */
  if (target_has_execution)
    write_register_bytes (0, inf_status->registers, REGISTER_BYTES);

  /* The inferior can be gone if the user types "print exit(0)"
     (and perhaps other times).  */

  /* FIXME: If we are being called after stopping in a function which
     is called from gdb, we should not be trying to restore the
     selected frame; it just prints a spurious error message (The
     message is useful, however, in detecting bugs in gdb (like if gdb
     clobbers the stack)).  In fact, should we be restoring the
     inferior status at all in that case?  .  */

  if (target_has_stack && inf_status->restore_stack_info)
    {
      struct restore_selected_frame_args fr;
      fr.level = inf_status->selected_level;
      fr.frame_address = inf_status->selected_frame_address;
      /* The point of catch_errors is that if the stack is clobbered,
	 walking the stack might encounter a garbage pointer and error()
	 trying to dereference it.  */
      if (catch_errors (restore_selected_frame, &fr,
			"Unable to restore previously selected frame:\n",
			RETURN_MASK_ERROR) == 0)
	/* Error in restoring the selected frame.  Select the innermost
	   frame.  */


	select_frame (get_current_frame (), 0);

    }
}



void
set_follow_fork_mode_command (arg, from_tty, c)
     char *arg;
     int from_tty;
     struct cmd_list_element *c;
{
  if (!STREQ (arg, "parent") &&
      !STREQ (arg, "child") &&
      !STREQ (arg, "both") &&
      !STREQ (arg, "ask"))
    error ("follow-fork-mode must be one of \"parent\", \"child\", \"both\" or \"ask\".");

  if (follow_fork_mode_string != NULL)
    free (follow_fork_mode_string);
  follow_fork_mode_string = savestring (arg, strlen (arg));
}



void
_initialize_infrun ()
{
  register int i;
  register int numsigs;
  struct cmd_list_element *c;

  add_info ("signals", signals_info,
	    "What debugger does when program gets various signals.\n\
Specify a signal as argument to print info on that signal only.");
  add_info_alias ("handle", "signals", 0);

  add_com ("handle", class_run, handle_command,
	   concat ("Specify how to handle a signal.\n\
Args are signals and actions to apply to those signals.\n\
Symbolic signals (e.g. SIGSEGV) are recommended but numeric signals\n\
from 1-15 are allowed for compatibility with old versions of GDB.\n\
Numeric ranges may be specified with the form LOW-HIGH (e.g. 1-5).\n\
The special arg \"all\" is recognized to mean all signals except those\n\
used by the debugger, typically SIGTRAP and SIGINT.\n",
		   "Recognized actions include \"stop\", \"nostop\", \"print\", \"noprint\",\n\
\"pass\", \"nopass\", \"ignore\", or \"noignore\".\n\
Stop means reenter debugger if this signal happens (implies print).\n\
Print means print a message if this signal happens.\n\
Pass means let program see this signal; otherwise program doesn't know.\n\
Ignore is a synonym for nopass and noignore is a synonym for pass.\n\
Pass and Stop may be combined.", NULL));
  if (xdb_commands)
    {
      add_com ("lz", class_info, signals_info,
	       "What debugger does when program gets various signals.\n\
Specify a signal as argument to print info on that signal only.");
      add_com ("z", class_run, xdb_handle_command,
	       concat ("Specify how to handle a signal.\n\
Args are signals and actions to apply to those signals.\n\
Symbolic signals (e.g. SIGSEGV) are recommended but numeric signals\n\
from 1-15 are allowed for compatibility with old versions of GDB.\n\
Numeric ranges may be specified with the form LOW-HIGH (e.g. 1-5).\n\
The special arg \"all\" is recognized to mean all signals except those\n\
used by the debugger, typically SIGTRAP and SIGINT.\n",
		       "Recognized actions include \"s\" (toggles between stop and nostop), \n\
\"r\" (toggles between print and noprint), \"i\" (toggles between pass and \
nopass), \"Q\" (noprint)\n\
Stop means reenter debugger if this signal happens (implies print).\n\
Print means print a message if this signal happens.\n\
Pass means let program see this signal; otherwise program doesn't know.\n\
Ignore is a synonym for nopass and noignore is a synonym for pass.\n\
Pass and Stop may be combined.", NULL));
    }

  if (!dbx_commands)
    stop_command = add_cmd ("stop", class_obscure, not_just_help_class_command,
			    "There is no `stop' command, but you can set a hook on `stop'.\n\
This allows you to set a list of commands to be run each time execution\n\
of the program stops.", &cmdlist);

  numsigs = (int) TARGET_SIGNAL_LAST;
  signal_stop = (unsigned char *)
    xmalloc (sizeof (signal_stop[0]) * numsigs);
  signal_print = (unsigned char *)
    xmalloc (sizeof (signal_print[0]) * numsigs);
  signal_program = (unsigned char *)
    xmalloc (sizeof (signal_program[0]) * numsigs);
  for (i = 0; i < numsigs; i++)
    {
      signal_stop[i] = 1;
      signal_print[i] = 1;
      signal_program[i] = 1;
    }

  /* Signals caused by debugger's own actions
     should not be given to the program afterwards.  */
  signal_program[TARGET_SIGNAL_TRAP] = 0;
  signal_program[TARGET_SIGNAL_INT] = 0;

  /* Signals that are not errors should not normally enter the debugger.  */
  signal_stop[TARGET_SIGNAL_ALRM] = 0;
  signal_print[TARGET_SIGNAL_ALRM] = 0;
  signal_stop[TARGET_SIGNAL_VTALRM] = 0;
  signal_print[TARGET_SIGNAL_VTALRM] = 0;
  signal_stop[TARGET_SIGNAL_PROF] = 0;
  signal_print[TARGET_SIGNAL_PROF] = 0;
  signal_stop[TARGET_SIGNAL_CHLD] = 0;
  signal_print[TARGET_SIGNAL_CHLD] = 0;
  signal_stop[TARGET_SIGNAL_IO] = 0;
  signal_print[TARGET_SIGNAL_IO] = 0;
  signal_stop[TARGET_SIGNAL_POLL] = 0;
  signal_print[TARGET_SIGNAL_POLL] = 0;
  signal_stop[TARGET_SIGNAL_URG] = 0;
  signal_print[TARGET_SIGNAL_URG] = 0;
  signal_stop[TARGET_SIGNAL_WINCH] = 0;
  signal_print[TARGET_SIGNAL_WINCH] = 0;

#ifdef SOLIB_ADD
  add_show_from_set
    (add_set_cmd ("stop-on-solib-events", class_support, var_zinteger,
		  (char *) &stop_on_solib_events,
		  "Set stopping for shared library events.\n\
If nonzero, gdb will give control to the user when the dynamic linker\n\
notifies gdb of shared library events.  The most common event of interest\n\
to the user would be loading/unloading of a new library.\n",
		  &setlist),
     &showlist);
#endif

  c = add_set_enum_cmd ("follow-fork-mode",
			class_run,
			follow_fork_mode_kind_names,
			(char *) &follow_fork_mode_string,
/* ??rehrauer:  The "both" option is broken, by what may be a 10.20
   kernel problem.  It's also not terribly useful without a GUI to
   help the user drive two debuggers.  So for now, I'm disabling
   the "both" option.  */
/*			"Set debugger response to a program call of fork \
or vfork.\n\
A fork or vfork creates a new process.  follow-fork-mode can be:\n\
  parent  - the original process is debugged after a fork\n\
  child   - the new process is debugged after a fork\n\
  both    - both the parent and child are debugged after a fork\n\
  ask     - the debugger will ask for one of the above choices\n\
For \"both\", another copy of the debugger will be started to follow\n\
the new child process.  The original debugger will continue to follow\n\
the original parent process.  To distinguish their prompts, the\n\
debugger copy's prompt will be changed.\n\
For \"parent\" or \"child\", the unfollowed process will run free.\n\
By default, the debugger will follow the parent process.",
*/
			"Set debugger response to a program call of fork \
or vfork.\n\
A fork or vfork creates a new process.  follow-fork-mode can be:\n\
  parent  - the original process is debugged after a fork\n\
  child   - the new process is debugged after a fork\n\
  ask     - the debugger will ask for one of the above choices\n\
For \"parent\" or \"child\", the unfollowed process will run free.\n\
By default, the debugger will follow the parent process.",
			&setlist);
/*  c->function.sfunc = ;*/
  add_show_from_set (c, &showlist);

  set_follow_fork_mode_command ("parent", 0, NULL);

  c = add_set_enum_cmd ("scheduler-locking", class_run,
			scheduler_enums,	/* array of string names */
			(char *) &scheduler_mode,	/* current mode  */
			"Set mode for locking scheduler during execution.\n\
off  == no locking (threads may preempt at any time)\n\
on   == full locking (no thread except the current thread may run)\n\
step == scheduler locked during every single-step operation.\n\
	In this mode, no other thread may run during a step command.\n\
	Other threads may run while stepping over a function call ('next').",
			&setlist);

  c->function.sfunc = set_schedlock_func;	/* traps on target vector */
  add_show_from_set (c, &showlist);
}
