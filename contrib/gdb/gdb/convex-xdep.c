/* Convex host-dependent code for GDB.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.

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
#include "command.h"
#include "symtab.h"
#include "value.h"
#include "frame.h"
#include "inferior.h"
#include "wait.h"

#include <signal.h>
#include <fcntl.h>
#include "gdbcore.h"

#include <sys/param.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/pcntl.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/file.h>
#include "gdb_stat.h"
#include <sys/mman.h>

#include <convex/vmparam.h>
#include <convex/filehdr.h>
#include <convex/opthdr.h>
#include <convex/scnhdr.h>
#include <convex/core.h>

/* Per-thread data, read from the inferior at each stop and written
   back at each resume.  */

/* Number of active threads.
   Tables are valid for thread numbers less than this.  */

static int n_threads;

#define MAXTHREADS 8
		
/* Thread state.  The remaining data is valid only if this is PI_TALIVE.  */

static int thread_state[MAXTHREADS];

/* Stop pc, signal, signal subcode */

static int thread_pc[MAXTHREADS];
static int thread_signal[MAXTHREADS];
static int thread_sigcode[MAXTHREADS];	

/* Thread registers.
   If thread is selected, the regs are in registers[] instead.  */

static char thread_regs[MAXTHREADS][REGISTER_BYTES];

/* 1 if the top frame on the thread's stack was a context frame,
   meaning that the kernel is up to something and we should not
   touch the thread at all except to resume it.  */

static char thread_is_in_kernel[MAXTHREADS];

/* The currently selected thread's number.  */

static int inferior_thread;

/* Inferior process's file handle and a process control block
   to feed args to ioctl with.  */

static int inferior_fd;
static struct pcntl ps;

/* SOFF file headers for exec or core file.  */

static FILEHDR filehdr;
static OPTHDR opthdr;
static SCNHDR scnhdr;

/* Address maps constructed from section headers of exec and core files.
   Defines process address -> file address translation.  */

struct pmap 
{
    long mem_addr;		/* process start address */
    long mem_end;		/* process end+1 address */
    long file_addr;		/* file start address */
    long thread;		/* -1 shared; 0,1,... thread-local */
    long type;			/* S_TEXT S_DATA S_BSS S_TBSS etc */
    long which;			/* used to sort map for info files */
};

static int n_exec, n_core;
static struct pmap exec_map[100];
static struct pmap core_map[100];

/* Offsets in the core file of core_context and core_tcontext blocks.  */

static int context_offset;
static int tcontext_offset[MAXTHREADS];

/* Core file control blocks.  */

static struct core_context_v70 c;
static struct core_tcontext_v70 tc;
static struct user u;
static thread_t th;
static proc_t pr;

/* The registers of the currently selected thread.  */

extern char registers[REGISTER_BYTES];

/* Vector and communication registers from core dump or from inferior.
   These are read on demand, ie, not normally valid.  */

static struct vecst vector_registers;
static struct creg_ctx comm_registers;

/* Flag, set on a vanilla CONT command and cleared when the inferior
   is continued.  */

static int all_continue;

/* Flag, set when the inferior is continued by a vanilla CONT command,
   cleared if it is continued for any other purpose.  */

static int thread_switch_ok;

/* Stack of signals recieved from threads but not yet delivered to gdb.  */

struct threadpid 
{
    int pid;
    int thread;
    int signo;
    int subsig;
    int pc;
};

static struct threadpid signal_stack_bot[100];
static struct threadpid *signal_stack = signal_stack_bot;

/* How to detect empty stack -- bottom frame is all zero.  */

#define signal_stack_is_empty() (signal_stack->pid == 0)

/* Mode controlled by SET PIPE command, controls the psw SEQ bit
   which forces each instruction to complete before the next one starts.  */

static int sequential = 0;

/* Mode controlled by the SET PARALLEL command.  Values are:
   0  concurrency limit 1 thread, dynamic scheduling
   1  no concurrency limit, dynamic scheduling
   2  no concurrency limit, fixed scheduling  */

static int parallel = 1;

/* Mode controlled by SET BASE command, output radix for unformatted
   integer typeout, as in argument lists, aggregates, and so on.
   Zero means guess whether it's an address (hex) or not (decimal).  */

static int output_radix = 0;

/* Signal subcode at last thread stop.  */

static int stop_sigcode;

/* Hack, see wait() below.  */

static int exec_trap_timer;

#include "gdbcmd.h"

static struct type *vector_type ();
static long *read_vector_register ();
static long *read_vector_register_1 ();
static void write_vector_register ();
static unsigned LONGEST read_comm_register ();
static void write_comm_register ();
static void convex_cont_command ();
static void thread_continue ();
static void select_thread ();
static void scan_stack ();
static void set_fixed_scheduling ();
static char *subsig_name ();
static void psw_info ();
static sig_noop ();
static ptr_cmp ();


/* Execute ptrace.  Convex V7 replaced ptrace with pattach.
   Allow ptrace (0) as a no-op.  */

int
call_ptrace (request, pid, procaddr, buf)
     int request, pid;
     PTRACE_ARG3_TYPE procaddr;
     int buf;
{
  if (request == 0)
    return;
  error ("no ptrace");
}

/* Replacement for system execle routine.
   Convert it to an equivalent exect, which pattach insists on.  */

execle (name, argv)
     char *name, *argv;
{
  char ***envp = (char ***) &argv;
  while (*envp++) ;

  signal (SIGTRAP, sig_noop);
  exect (name, &argv, *envp);
}

/* Stupid handler for stupid trace trap that otherwise causes
   startup to stupidly hang.  */

static sig_noop () 
{}

/* Read registers from inferior into registers[] array.
   For convex, they are already there, read in when the inferior stops.  */

void
fetch_inferior_registers (regno)
     int regno;
{
}

/* Store our register values back into the inferior.
   For Convex, do this only once, right before resuming inferior.  */

void
store_inferior_registers (regno)
     int regno;
{
}

/* Copy LEN bytes from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR. 
   On failure (cannot read from inferior, usually because address is out
   of bounds) returns the value of errno. */

int
read_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  errno = 0;
  while (len > 0)
    {
      /* little-known undocumented max request size */
      int i = (len < 12288) ? len : 12288;

      lseek (inferior_fd, memaddr, 0);
      read (inferior_fd, myaddr, i);

      memaddr += i;
      myaddr += i;
      len -= i;
    }
  if (errno) 
    memset (myaddr, '\0', len);
  return errno;
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.
   Returns errno on failure (cannot write the inferior) */

int
write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  errno = 0;
  lseek (inferior_fd, memaddr, 0);
  write (inferior_fd, myaddr, len);
  return errno;
}

/* Here from create_inferior when the inferior process has been created
   and started up.  We must do a pattach to grab it for debugging.

   Also, intercept the CONT command by altering its dispatch address.  */
/* FIXME: This used to be called from a macro CREATE_INFERIOR_HOOK.
   But now init_trace_fun is in the same place.  So re-write this to
   use the init_trace_fun (making convex a debugging target).  */

create_inferior_hook (pid)
    int pid;
{
  static char cont[] = "cont";
  static char cont1[] = "c";
  char *linep = cont;
  char *linep1 = cont1;
  char **line = &linep;
  char **line1 = &linep1;
  struct cmd_list_element *c;

  c = lookup_cmd (line, cmdlist, "", 0);
  c->function = convex_cont_command;
  c = lookup_cmd (line1, cmdlist, "", 0);
  c->function = convex_cont_command;

  inferior_fd = pattach (pid, O_EXCL);
  if (inferior_fd < 0)
    perror_with_name ("pattach");
  inferior_thread = 0;
  set_fixed_scheduling (pid, parallel == 2);
}

/* Attach process PID for debugging.  */

attach (pid)
    int pid;
{
  int fd = pattach (pid, O_EXCL);
  if (fd < 0)
    perror_with_name ("pattach");
  attach_flag = 1;
  /* wait for strange kernel reverberations to go away */
  sleep (1);

  setpgrp (pid, pid);

  inferior_fd = fd;
  inferior_thread = 0;
  return pid;
}

/* Stop debugging the process whose number is PID
   and continue it with signal number SIGNAL.
   SIGNAL = 0 means just continue it.  */

void
detach (signal)
     int signal;
{
  signal_stack = signal_stack_bot;
  thread_continue (-1, 0, signal);
  ioctl (inferior_fd, PIXDETACH, &ps);
  close (inferior_fd);
  inferior_fd = 0;
  attach_flag = 0;
}

/* Kill off the inferior process.  */

kill_inferior ()
{
  if (inferior_pid == 0)
    return;
  ioctl (inferior_fd, PIXTERMINATE, 0);
  wait (0);
  target_mourn_inferior ();
}

/* Read vector register REG, and return a pointer to the value.  */

static long *
read_vector_register (reg)
    int reg;
{
  if (have_inferior_p ())
    {
      errno = 0;
      ps.pi_buffer = (char *) &vector_registers;
      ps.pi_nbytes = sizeof vector_registers;
      ps.pi_offset = 0;
      ps.pi_thread = inferior_thread;
      ioctl (inferior_fd, PIXRDVREGS, &ps);
      if (errno)
	memset (&vector_registers, '\0', sizeof vector_registers);
    }
  else if (corechan >= 0)
    {
      lseek (corechan, tcontext_offset[inferior_thread], 0);
      if (myread (corechan, &tc, sizeof tc) < 0)
	perror_with_name (corefile);
      lseek (corechan, tc.core_thread_p, 0);
      if (myread (corechan, &th, sizeof th) < 0)
	perror_with_name (corefile);
      lseek (corechan, tc.core_vregs_p, 0);
      if (myread (corechan, &vector_registers, 16*128) < 0)
	perror_with_name (corefile);
      vector_registers.vm[0] = th.t_vect_ctx.vc_vm[0];
      vector_registers.vm[1] = th.t_vect_ctx.vc_vm[1];
      vector_registers.vls = th.t_vect_ctx.vc_vls;
    }

  return read_vector_register_1 (reg);
}

/* Return a pointer to vector register REG, which must already have been
   fetched from the inferior or core file.  */

static long *
read_vector_register_1 (reg) 
    int reg;
{
  switch (reg)
    {
    case VM_REGNUM:
      return (long *) vector_registers.vm;
    case VS_REGNUM:
      return (long *) &vector_registers.vls;
    case VL_REGNUM:
      return 1 + (long *) &vector_registers.vls;
    default:
      return (long *) &vector_registers.vr[reg];
    }
}

/* Write vector register REG, element ELEMENT, new value VAL.
   NB: must use read-modify-write on the entire vector state,
   since pattach does not do offsetted writes correctly.  */

static void
write_vector_register (reg, element, val)
    int reg, element;
    unsigned LONGEST val;
{
  if (have_inferior_p ())
    {
      errno = 0;
      ps.pi_thread = inferior_thread;
      ps.pi_offset = 0;
      ps.pi_buffer = (char *) &vector_registers;
      ps.pi_nbytes = sizeof vector_registers;

      ioctl (inferior_fd, PIXRDVREGS, &ps);

      switch (reg)
	{
	case VL_REGNUM:
	  vector_registers.vls =
	    (vector_registers.vls & 0xffffffff00000000LL)
	      + (unsigned long) val;
	  break;

	case VS_REGNUM:
	  vector_registers.vls =
	    (val << 32) + (unsigned long) vector_registers.vls;
	  break;
	    
	default:
	  vector_registers.vr[reg].el[element] = val;
	  break;
	}

      ioctl (inferior_fd, PIXWRVREGS, &ps);

      if (errno)
	perror_with_name ("writing vector register");
    }
}

/* Return the contents of communication register NUM.  */ 

static unsigned LONGEST 
read_comm_register (num)
     int num;
{
  if (have_inferior_p ())
    {
      ps.pi_buffer = (char *) &comm_registers;
      ps.pi_nbytes = sizeof comm_registers;
      ps.pi_offset = 0;
      ps.pi_thread = inferior_thread;
      ioctl (inferior_fd, PIXRDCREGS, &ps);
    }
  return comm_registers.crreg.r4[num];
}

/* Store a new value VAL into communication register NUM.  
   NB: Must use read-modify-write on the whole comm register set
   since pattach does not do offsetted writes correctly.  */

static void
write_comm_register (num, val)
     int num;
     unsigned LONGEST val;
{
  if (have_inferior_p ())
    {
      ps.pi_buffer = (char *) &comm_registers;
      ps.pi_nbytes = sizeof comm_registers;
      ps.pi_offset = 0;
      ps.pi_thread = inferior_thread;
      ioctl (inferior_fd, PIXRDCREGS, &ps);
      comm_registers.crreg.r4[num] = val;
      ioctl (inferior_fd, PIXWRCREGS, &ps);
    }
}

/* Resume execution of the inferior process.
   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.  */

void
resume (step, signal)
     int step;
     int signal;
{
  errno = 0;
  if (step || signal)
    thread_continue (inferior_thread, step, signal);
  else
    thread_continue (-1, 0, 0);
}

/* Maybe resume some threads.
   THREAD is which thread to resume, or -1 to resume them all.
   STEP and SIGNAL are as in resume.

   Global variable ALL_CONTINUE is set when we are here to do a
   `cont' command; otherwise we may be doing `finish' or a call or
   something else that will not tolerate an automatic thread switch.

   If there are stopped threads waiting to deliver signals, and
   ALL_CONTINUE, do not actually resume anything.  gdb will do a wait
   and see one of the stopped threads in the queue.  */

static void
thread_continue (thread, step, signal)
     int thread, step, signal;
{
  int n;

  /* If we are to continue all threads, but not for the CONTINUE command,
     pay no attention and continue only the selected thread.  */

  if (thread < 0 && ! all_continue)
    thread = inferior_thread;

  /* If we are not stepping, we have now executed the continue part
     of a CONTINUE command.  */

  if (! step)
    all_continue = 0;

  /* Allow wait() to switch threads if this is an all-out continue.  */

  thread_switch_ok = thread < 0;

  /* If there are threads queued up, don't resume.  */

  if (thread_switch_ok && ! signal_stack_is_empty ())
    return;

  /* OK, do it.  */

  for (n = 0; n < n_threads; n++)
    if (thread_state[n] == PI_TALIVE)
      {
	select_thread (n);

	if ((thread < 0 || n == thread) && ! thread_is_in_kernel[n])
	  {
	    /* Blam the trace bits in the stack's saved psws to match 
	       the desired step mode.  This is required so that
	       single-stepping a return doesn't restore a psw with a
	       clear trace bit and fly away, and conversely,
	       proceeding through a return in a routine that was
	       stepped into doesn't cause a phantom break by restoring
	       a psw with the trace bit set. */
	    scan_stack (PSW_T_BIT, step);
	    scan_stack (PSW_S_BIT, sequential);
	  }

	ps.pi_buffer = registers;
	ps.pi_nbytes = REGISTER_BYTES;
	ps.pi_offset = 0;
	ps.pi_thread = n;
	if (! thread_is_in_kernel[n])
	  if (ioctl (inferior_fd, PIXWRREGS, &ps))
	    perror_with_name ("PIXWRREGS");

	if (thread < 0 || n == thread)
	  {
	    ps.pi_pc = 1;
	    ps.pi_signo = signal;
	    if (ioctl (inferior_fd, step ? PIXSTEP : PIXCONTINUE, &ps) < 0)
	      perror_with_name ("PIXCONTINUE");
	  }
      }

  if (ioctl (inferior_fd, PIXRUN, &ps) < 0)
    perror_with_name ("PIXRUN");
}

/* Replacement for system wait routine.  

   The system wait returns with one or more threads stopped by
   signals.  Put stopped threads on a stack and return them one by
   one, so that it appears that wait returns one thread at a time.

   Global variable THREAD_SWITCH_OK is set when gdb can tolerate wait
   returning a new thread.  If it is false, then only one thread is
   running; we will do a real wait, the thread will do something, and
   we will return that.  */

pid_t
wait (w)
    union wait *w;
{
  int pid;

  if (!w)
    return wait3 (0, 0, 0);

  /* Do a real wait if we were told to, or if there are no queued threads.  */

  if (! thread_switch_ok || signal_stack_is_empty ())
    {
      int thread;

      pid = wait3 (w, 0, 0);

      if (!WIFSTOPPED (*w) || pid != inferior_pid)
	return pid;

      /* The inferior has done something and stopped.  Read in all the
	 threads' registers, and queue up any signals that happened.  */

      if (ioctl (inferior_fd, PIXGETTHCOUNT, &ps) < 0)
	perror_with_name ("PIXGETTHCOUNT");
      
      n_threads = ps.pi_othdcnt;
      for (thread = 0; thread < n_threads; thread++)
	{
	  ps.pi_thread = thread;
	  if (ioctl (inferior_fd, PIXGETSUBCODE, &ps) < 0)
	    perror_with_name ("PIXGETSUBCODE");
	  thread_state[thread] = ps.pi_otstate;

	  if (ps.pi_otstate == PI_TALIVE)
	    {
	      select_thread (thread);
	      ps.pi_buffer = registers;
	      ps.pi_nbytes = REGISTER_BYTES;
	      ps.pi_offset = 0;
	      ps.pi_thread = thread;
	      if (ioctl (inferior_fd, PIXRDREGS, &ps) < 0)
		perror_with_name ("PIXRDREGS");

	      registers_fetched ();

	      thread_pc[thread] = read_pc ();
	      thread_signal[thread] = ps.pi_osigno;
	      thread_sigcode[thread] = ps.pi_osigcode;

	      /* If the thread's stack has a context frame
		 on top, something fucked is going on.  I do not
		 know what, but do I know this: the only thing you
		 can do with such a thread is continue it.  */

	      thread_is_in_kernel[thread] = 
		((read_register (PS_REGNUM) >> 25) & 3) == 0;

	      /* Signals push an extended frame and then fault
		 with a ridiculous pc.  Pop the frame.  */

	      if (thread_pc[thread] > STACK_END_ADDR)
		{
		  POP_FRAME;
		  if (is_break_pc (thread_pc[thread]))
		    thread_pc[thread] = read_pc () - 2;
		  else
		    thread_pc[thread] = read_pc ();
		  write_register (PC_REGNUM, thread_pc[thread]);
		}
	      
	      if (ps.pi_osigno || ps.pi_osigcode)
		{
		  signal_stack++;
		  signal_stack->pid = pid;
		  signal_stack->thread = thread;
		  signal_stack->signo = thread_signal[thread];
		  signal_stack->subsig = thread_sigcode[thread];
		  signal_stack->pc = thread_pc[thread];
		}

	      /* The following hackery is caused by a unix 7.1 feature:
		 the inferior's fixed scheduling mode is cleared when
		 it execs the shell (since the shell is not a parallel
		 program).  So, note the 5.4 trap we get when
		 the shell does its exec, then catch the 5.0 trap 
		 that occurs when the debuggee starts, and set fixed
		 scheduling mode properly.  */

	      if (ps.pi_osigno == 5 && ps.pi_osigcode == 4)
		exec_trap_timer = 1;
	      else
		exec_trap_timer--;
	      
	      if (ps.pi_osigno == 5 && exec_trap_timer == 0)
		set_fixed_scheduling (pid, parallel == 2);
	    }
	}

      if (signal_stack_is_empty ())
	error ("no active threads?!");
    }

  /* Select the thread that stopped, and return *w saying why.  */

  select_thread (signal_stack->thread);

 FIXME: need to convert from host sig.
  stop_signal = signal_stack->signo;
  stop_sigcode = signal_stack->subsig;

  WSETSTOP (*w, signal_stack->signo);
  w->w_thread = signal_stack->thread;
  return (signal_stack--)->pid;
}

/* Select thread THREAD -- its registers, stack, per-thread memory.
   This is the only routine that may assign to inferior_thread
   or thread_regs[].  */

static void
select_thread (thread)
     int thread;
{
  if (thread == inferior_thread)
    return;

  memcpy (thread_regs[inferior_thread], registers, REGISTER_BYTES);
  ps.pi_thread = inferior_thread = thread;
  if (have_inferior_p ())
    ioctl (inferior_fd, PISETRWTID, &ps);
  memcpy (registers, thread_regs[thread], REGISTER_BYTES);
}
  
/* Routine to set or clear a psw bit in the psw and also all psws
   saved on the stack.  Quits when we get to a frame in which the
   saved psw is correct. */

static void
scan_stack (bit, val)
    long bit, val;
{
  long ps = read_register (PS_REGNUM);
  long fp;
  if (val ? !(ps & bit) : (ps & bit))
    {    
      ps ^= bit;
      write_register (PS_REGNUM, ps);

      fp = read_register (FP_REGNUM);
      while (fp & 0x80000000)
	{
	  ps = read_memory_integer (fp + 4, 4);
	  if (val ? (ps & bit) : !(ps & bit))
	    break;
	  ps ^= bit;
	  write_memory (fp + 4, &ps, 4);
	  fp = read_memory_integer (fp + 8, 4);
	}
    }
}

/* Set fixed scheduling (alliant mode) of process PID to ARG (0 or 1).  */

static void
set_fixed_scheduling (pid, arg)
      int arg;
{
  struct pattributes pattr;
  getpattr (pid, &pattr);
  pattr.pattr_pfixed = arg;
  setpattr (pid, &pattr);
}

void
core_file_command (filename, from_tty)
     char *filename;
     int from_tty;
{
  int n;

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
  n_core = 0;

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

      if (myread (corechan, &filehdr, sizeof filehdr) < 0)
	perror_with_name (filename);

      if (!IS_CORE_SOFF_MAGIC (filehdr.h_magic))
	error ("%s: not a core file.\n", filename);

      if (myread (corechan, &opthdr, filehdr.h_opthdr) < 0)
	perror_with_name (filename);

      /* Read through the section headers.
	 For text, data, etc, record an entry in the core file map.
	 For context and tcontext, record the file address of
	 the context blocks.  */

      lseek (corechan, (long) filehdr.h_scnptr, 0);

      n_threads = 0;
      for (n = 0; n < filehdr.h_nscns; n++)
	{
	  if (myread (corechan, &scnhdr, sizeof scnhdr) < 0)
	    perror_with_name (filename);
	  if ((scnhdr.s_flags & S_TYPMASK) >= S_TEXT
	      && (scnhdr.s_flags & S_TYPMASK) <= S_COMON)
	    {
	      core_map[n_core].mem_addr = scnhdr.s_vaddr;
	      core_map[n_core].mem_end = scnhdr.s_vaddr + scnhdr.s_size;
	      core_map[n_core].file_addr = scnhdr.s_scnptr;
	      core_map[n_core].type = scnhdr.s_flags & S_TYPMASK;
	      if (core_map[n_core].type != S_TBSS
		  && core_map[n_core].type != S_TDATA
		  && core_map[n_core].type != S_TTEXT)
		core_map[n_core].thread = -1;
	      else if (n_core == 0
		       || core_map[n_core-1].mem_addr != scnhdr.s_vaddr)
		core_map[n_core].thread = 0;
	      else 
		core_map[n_core].thread = core_map[n_core-1].thread + 1;
	      n_core++;
	    }
	  else if ((scnhdr.s_flags & S_TYPMASK) == S_CONTEXT)
	    context_offset = scnhdr.s_scnptr;
	  else if ((scnhdr.s_flags & S_TYPMASK) == S_TCONTEXT) 
	    tcontext_offset[n_threads++] = scnhdr.s_scnptr;
	}

      /* Read the context block, struct user, struct proc,
	 and the comm regs.  */

      lseek (corechan, context_offset, 0);
      if (myread (corechan, &c, sizeof c) < 0)
	perror_with_name (filename);
      lseek (corechan, c.core_user_p, 0);
      if (myread (corechan, &u, sizeof u) < 0)
	perror_with_name (filename);
      lseek (corechan, c.core_proc_p, 0);
      if (myread (corechan, &pr, sizeof pr) < 0)
	perror_with_name (filename);
      comm_registers = pr.p_creg;

      /* Core file apparently is really there.  Make it really exist
	 for xfer_core_file so we can do read_memory on it. */

      if (filename[0] == '/')
	corefile = savestring (filename, strlen (filename));
      else
	corefile = concat (current_directory, "/", filename, NULL);

      printf_filtered ("Program %s ", u.u_comm);

      /* Read the thread registers and fill in the thread_xxx[] data.  */

      for (n = 0; n < n_threads; n++)
	{
	  select_thread (n);

	  lseek (corechan, tcontext_offset[n], 0);
	  if (myread (corechan, &tc, sizeof tc) < 0)
	    perror_with_name (corefile);
	  lseek (corechan, tc.core_thread_p, 0);
	  if (myread (corechan, &th, sizeof th) < 0)
	    perror_with_name (corefile);

	  lseek (corechan, tc.core_syscall_context_p, 0);
	  if (myread (corechan, registers, REGISTER_BYTES) < 0)
	    perror_with_name (corefile);

	  thread_signal[n] = th.t_cursig;
	  thread_sigcode[n] = th.t_code;
	  thread_state[n] = th.t_state;
	  thread_pc[n] = read_pc ();

	  if (thread_pc[n] > STACK_END_ADDR)
	    {
	      POP_FRAME;
	      if (is_break_pc (thread_pc[n]))
		thread_pc[n] = read_pc () - 2;
	      else
		thread_pc[n] = read_pc ();
	      write_register (PC_REGNUM, thread_pc[n]);
	    }

	  printf_filtered ("thread %d received signal %d, %s\n",
			   n, thread_signal[n],
			   safe_strsignal (thread_signal[n]));
	}

      /* Select an interesting thread -- also-rans died with SIGKILL,
	 so find one that didn't.  */

      for (n = 0; n < n_threads; n++)
	if (thread_signal[n] != 0 && thread_signal[n] != SIGKILL)
	  {
	    select_thread (n);
	    stop_signal = thread_signal[n];
	    stop_sigcode = thread_sigcode[n];
	    break;
	  }

      core_aouthdr.a_magic = 0;

      flush_cached_frames ();
      select_frame (get_current_frame (), 0);
      validate_files ();

      print_stack_frame (selected_frame, selected_frame_level, -1);
    }
  else if (from_tty)
    printf_filtered ("No core file now.\n");
}
