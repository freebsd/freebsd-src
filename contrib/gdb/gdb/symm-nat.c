/* Sequent Symmetry host interface, for GDB when running under Unix.
   Copyright 1986, 1987, 1989, 1991, 1992, 1994 Free Software Foundation, Inc.

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

/* FIXME, some 387-specific items of use taken from i387-tdep.c -- ought to be
   merged back in. */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"

/* FIXME: What is the _INKERNEL define for?  */
#define _INKERNEL
#include <signal.h>
#undef _INKERNEL
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/dir.h>
#include <sys/ioctl.h>
#include "gdb_stat.h"
#ifdef _SEQUENT_
#include <sys/ptrace.h>
#else
/* Dynix has only machine/ptrace.h, which is already included by sys/user.h  */
/* Dynix has no mptrace call */
#define mptrace ptrace
#endif
#include "gdbcore.h"
#include <fcntl.h>
#include <sgtty.h>
#define TERMINAL struct sgttyb

#include "gdbcore.h"

void
store_inferior_registers(regno)
int regno;
{
  struct pt_regset regs;
  int i;
  extern char registers[];

  /* FIXME: Fetching the registers is a kludge to initialize all elements
     in the fpu and fpa status. This works for normal debugging, but
     might cause problems when calling functions in the inferior.
     At least fpu_control and fpa_pcr (probably more) should be added 
     to the registers array to solve this properly.  */
  mptrace (XPT_RREGS, inferior_pid, (PTRACE_ARG3_TYPE) &regs, 0);

  regs.pr_eax = *(int *)&registers[REGISTER_BYTE(0)];
  regs.pr_ebx = *(int *)&registers[REGISTER_BYTE(5)];
  regs.pr_ecx = *(int *)&registers[REGISTER_BYTE(2)];
  regs.pr_edx = *(int *)&registers[REGISTER_BYTE(1)];
  regs.pr_esi = *(int *)&registers[REGISTER_BYTE(6)];
  regs.pr_edi = *(int *)&registers[REGISTER_BYTE(7)];
  regs.pr_esp = *(int *)&registers[REGISTER_BYTE(14)];
  regs.pr_ebp = *(int *)&registers[REGISTER_BYTE(15)];
  regs.pr_eip = *(int *)&registers[REGISTER_BYTE(16)];
  regs.pr_flags = *(int *)&registers[REGISTER_BYTE(17)];
  for (i = 0; i < 31; i++)
    {
      regs.pr_fpa.fpa_regs[i] =
	*(int *)&registers[REGISTER_BYTE(FP1_REGNUM+i)];
    }
  memcpy (regs.pr_fpu.fpu_stack[0], &registers[REGISTER_BYTE(ST0_REGNUM)], 10);
  memcpy (regs.pr_fpu.fpu_stack[1], &registers[REGISTER_BYTE(ST1_REGNUM)], 10);
  memcpy (regs.pr_fpu.fpu_stack[2], &registers[REGISTER_BYTE(ST2_REGNUM)], 10);
  memcpy (regs.pr_fpu.fpu_stack[3], &registers[REGISTER_BYTE(ST3_REGNUM)], 10);
  memcpy (regs.pr_fpu.fpu_stack[4], &registers[REGISTER_BYTE(ST4_REGNUM)], 10);
  memcpy (regs.pr_fpu.fpu_stack[5], &registers[REGISTER_BYTE(ST5_REGNUM)], 10);
  memcpy (regs.pr_fpu.fpu_stack[6], &registers[REGISTER_BYTE(ST6_REGNUM)], 10);
  memcpy (regs.pr_fpu.fpu_stack[7], &registers[REGISTER_BYTE(ST7_REGNUM)], 10);
  mptrace (XPT_WREGS, inferior_pid, (PTRACE_ARG3_TYPE) &regs, 0);
}

void
fetch_inferior_registers (regno)
     int regno;
{
  int i;
  struct pt_regset regs;
  extern char registers[];

  registers_fetched ();

  mptrace (XPT_RREGS, inferior_pid, (PTRACE_ARG3_TYPE) &regs, 0);
  *(int *)&registers[REGISTER_BYTE(EAX_REGNUM)] = regs.pr_eax;
  *(int *)&registers[REGISTER_BYTE(EBX_REGNUM)] = regs.pr_ebx;
  *(int *)&registers[REGISTER_BYTE(ECX_REGNUM)] = regs.pr_ecx;
  *(int *)&registers[REGISTER_BYTE(EDX_REGNUM)] = regs.pr_edx;
  *(int *)&registers[REGISTER_BYTE(ESI_REGNUM)] = regs.pr_esi;
  *(int *)&registers[REGISTER_BYTE(EDI_REGNUM)] = regs.pr_edi;
  *(int *)&registers[REGISTER_BYTE(EBP_REGNUM)] = regs.pr_ebp;
  *(int *)&registers[REGISTER_BYTE(ESP_REGNUM)] = regs.pr_esp;
  *(int *)&registers[REGISTER_BYTE(EIP_REGNUM)] = regs.pr_eip;
  *(int *)&registers[REGISTER_BYTE(EFLAGS_REGNUM)] = regs.pr_flags;
  for (i = 0; i < FPA_NREGS; i++)
    {
      *(int *)&registers[REGISTER_BYTE(FP1_REGNUM+i)] =
	regs.pr_fpa.fpa_regs[i];
    }
  memcpy (&registers[REGISTER_BYTE(ST0_REGNUM)], regs.pr_fpu.fpu_stack[0], 10);
  memcpy (&registers[REGISTER_BYTE(ST1_REGNUM)], regs.pr_fpu.fpu_stack[1], 10);
  memcpy (&registers[REGISTER_BYTE(ST2_REGNUM)], regs.pr_fpu.fpu_stack[2], 10);
  memcpy (&registers[REGISTER_BYTE(ST3_REGNUM)], regs.pr_fpu.fpu_stack[3], 10);
  memcpy (&registers[REGISTER_BYTE(ST4_REGNUM)], regs.pr_fpu.fpu_stack[4], 10);
  memcpy (&registers[REGISTER_BYTE(ST5_REGNUM)], regs.pr_fpu.fpu_stack[5], 10);
  memcpy (&registers[REGISTER_BYTE(ST6_REGNUM)], regs.pr_fpu.fpu_stack[6], 10);
  memcpy (&registers[REGISTER_BYTE(ST7_REGNUM)], regs.pr_fpu.fpu_stack[7], 10);
}

/* FIXME:  This should be merged with i387-tdep.c as well. */
static
print_fpu_status(ep)
struct pt_regset ep;
{
    int i;
    int bothstatus;
    int top;
    int fpreg;
    unsigned char *p;
    
    printf_unfiltered("80387:");
    if (ep.pr_fpu.fpu_ip == 0) {
	printf_unfiltered(" not in use.\n");
	return;
    } else {
	printf_unfiltered("\n");
    }
    if (ep.pr_fpu.fpu_status != 0) {
	print_387_status_word (ep.pr_fpu.fpu_status);
    }
    print_387_control_word (ep.pr_fpu.fpu_control);
    printf_unfiltered ("last exception: ");
    printf_unfiltered ("opcode 0x%x; ", ep.pr_fpu.fpu_rsvd4);
    printf_unfiltered ("pc 0x%x:0x%x; ", ep.pr_fpu.fpu_cs, ep.pr_fpu.fpu_ip);
    printf_unfiltered ("operand 0x%x:0x%x\n", ep.pr_fpu.fpu_data_offset, ep.pr_fpu.fpu_op_sel);
    
    top = (ep.pr_fpu.fpu_status >> 11) & 7;
    
    printf_unfiltered ("regno  tag  msb              lsb  value\n");
    for (fpreg = 7; fpreg >= 0; fpreg--) 
	{
	    double val;
	    
	    printf_unfiltered ("%s %d: ", fpreg == top ? "=>" : "  ", fpreg);
	    
	    switch ((ep.pr_fpu.fpu_tag >> (fpreg * 2)) & 3) 
		{
		case 0: printf_unfiltered ("valid "); break;
		case 1: printf_unfiltered ("zero  "); break;
		case 2: printf_unfiltered ("trap  "); break;
		case 3: printf_unfiltered ("empty "); break;
		}
	    for (i = 9; i >= 0; i--)
		printf_unfiltered ("%02x", ep.pr_fpu.fpu_stack[fpreg][i]);
	    
	    i387_to_double ((char *)ep.pr_fpu.fpu_stack[fpreg], (char *)&val);
	    printf_unfiltered ("  %g\n", val);
	}
    if (ep.pr_fpu.fpu_rsvd1)
	warning ("rsvd1 is 0x%x\n", ep.pr_fpu.fpu_rsvd1);
    if (ep.pr_fpu.fpu_rsvd2)
	warning ("rsvd2 is 0x%x\n", ep.pr_fpu.fpu_rsvd2);
    if (ep.pr_fpu.fpu_rsvd3)
	warning ("rsvd3 is 0x%x\n", ep.pr_fpu.fpu_rsvd3);
    if (ep.pr_fpu.fpu_rsvd5)
	warning ("rsvd5 is 0x%x\n", ep.pr_fpu.fpu_rsvd5);
}


print_1167_control_word(pcr)
unsigned int pcr;

{
    int pcr_tmp;

    pcr_tmp = pcr & FPA_PCR_MODE;
    printf_unfiltered("\tMODE= %#x; RND= %#x ", pcr_tmp, pcr_tmp & 12);
    switch (pcr_tmp & 12) {
    case 0:
	printf_unfiltered("RN (Nearest Value)");
	break;
    case 1:
	printf_unfiltered("RZ (Zero)");
	break;
    case 2:
	printf_unfiltered("RP (Positive Infinity)");
	break;
    case 3:
	printf_unfiltered("RM (Negative Infinity)");
	break;
    }
    printf_unfiltered("; IRND= %d ", pcr_tmp & 2);
    if (0 == pcr_tmp & 2) {
	printf_unfiltered("(same as RND)\n");
    } else {
	printf_unfiltered("(toward zero)\n");
    }
    pcr_tmp = pcr & FPA_PCR_EM;
    printf_unfiltered("\tEM= %#x", pcr_tmp);
    if (pcr_tmp & FPA_PCR_EM_DM) printf_unfiltered(" DM");
    if (pcr_tmp & FPA_PCR_EM_UOM) printf_unfiltered(" UOM");
    if (pcr_tmp & FPA_PCR_EM_PM) printf_unfiltered(" PM");
    if (pcr_tmp & FPA_PCR_EM_UM) printf_unfiltered(" UM");
    if (pcr_tmp & FPA_PCR_EM_OM) printf_unfiltered(" OM");
    if (pcr_tmp & FPA_PCR_EM_ZM) printf_unfiltered(" ZM");
    if (pcr_tmp & FPA_PCR_EM_IM) printf_unfiltered(" IM");
    printf_unfiltered("\n");
    pcr_tmp = FPA_PCR_CC;
    printf_unfiltered("\tCC= %#x", pcr_tmp);
    if (pcr_tmp & FPA_PCR_20MHZ) printf_unfiltered(" 20MHZ");
    if (pcr_tmp & FPA_PCR_CC_Z) printf_unfiltered(" Z");
    if (pcr_tmp & FPA_PCR_CC_C2) printf_unfiltered(" C2");

    /* Dynix defines FPA_PCR_CC_C0 to 0x100 and ptx defines
       FPA_PCR_CC_C1 to 0x100.  Use whichever is defined and assume
       the OS knows what it is doing.  */
#ifdef FPA_PCR_CC_C1
    if (pcr_tmp & FPA_PCR_CC_C1) printf_unfiltered(" C1");
#else
    if (pcr_tmp & FPA_PCR_CC_C0) printf_unfiltered(" C0");
#endif

    switch (pcr_tmp)
      {
      case FPA_PCR_CC_Z:
	printf_unfiltered(" (Equal)");
	break;
#ifdef FPA_PCR_CC_C1
      case FPA_PCR_CC_C1:
#else
      case FPA_PCR_CC_C0:
#endif
	printf_unfiltered(" (Less than)");
	break;
      case 0:
	printf_unfiltered(" (Greater than)");
	break;
      case FPA_PCR_CC_Z | 
#ifdef FPA_PCR_CC_C1
	FPA_PCR_CC_C1
#else
	FPA_PCR_CC_C0
#endif
	  | FPA_PCR_CC_C2:
	printf_unfiltered(" (Unordered)");
	break;
      default:
	printf_unfiltered(" (Undefined)");
	break;
      }
    printf_unfiltered("\n");
    pcr_tmp = pcr & FPA_PCR_AE;
    printf_unfiltered("\tAE= %#x", pcr_tmp);
    if (pcr_tmp & FPA_PCR_AE_DE) printf_unfiltered(" DE");
    if (pcr_tmp & FPA_PCR_AE_UOE) printf_unfiltered(" UOE");
    if (pcr_tmp & FPA_PCR_AE_PE) printf_unfiltered(" PE");
    if (pcr_tmp & FPA_PCR_AE_UE) printf_unfiltered(" UE");
    if (pcr_tmp & FPA_PCR_AE_OE) printf_unfiltered(" OE");
    if (pcr_tmp & FPA_PCR_AE_ZE) printf_unfiltered(" ZE");
    if (pcr_tmp & FPA_PCR_AE_EE) printf_unfiltered(" EE");
    if (pcr_tmp & FPA_PCR_AE_IE) printf_unfiltered(" IE");
    printf_unfiltered("\n");
}

print_1167_regs(regs)
long regs[FPA_NREGS];

{
    int i;

    union {
	double	d;
	long	l[2];
    } xd;
    union {
	float	f;
	long	l;
    } xf;


    for (i = 0; i < FPA_NREGS; i++) {
	xf.l = regs[i];
	printf_unfiltered("%%fp%d: raw= %#x, single= %f", i+1, regs[i], xf.f);
	if (!(i & 1)) {
	    printf_unfiltered("\n");
	} else {
	    xd.l[1] = regs[i];
	    xd.l[0] = regs[i+1];
	    printf_unfiltered(", double= %f\n", xd.d);
	}
    }
}

print_fpa_status(ep)
struct pt_regset ep;

{

    printf_unfiltered("WTL 1167:");
    if (ep.pr_fpa.fpa_pcr !=0) {
	printf_unfiltered("\n");
	print_1167_control_word(ep.pr_fpa.fpa_pcr);
	print_1167_regs(ep.pr_fpa.fpa_regs);
    } else {
	printf_unfiltered(" not in use.\n");
    }
}

#if 0 /* disabled because it doesn't go through the target vector.  */
i386_float_info ()
{
  char ubuf[UPAGES*NBPG];
  struct pt_regset regset;

  if (have_inferior_p())
    {
      PTRACE_READ_REGS (inferior_pid, (PTRACE_ARG3_TYPE) &regset);
    }
  else
    {
      int corechan = bfd_cache_lookup (core_bfd);
      if (lseek (corechan, 0, 0) < 0)
	{
	  perror ("seek on core file");
	}
      if (myread (corechan, ubuf, UPAGES*NBPG) < 0)
	{
	  perror ("read on core file");
	}
      /* only interested in the floating point registers */
      regset.pr_fpu = ((struct user *) ubuf)->u_fpusave;
      regset.pr_fpa = ((struct user *) ubuf)->u_fpasave;
    }
  print_fpu_status(regset);
  print_fpa_status(regset);
}
#endif

static volatile int got_sigchld;

/*ARGSUSED*/
/* This will eventually be more interesting. */
void
sigchld_handler(signo)
	int signo;
{
	got_sigchld++;
}

/*
 * Signals for which the default action does not cause the process
 * to die.  See <sys/signal.h> for where this came from (alas, we
 * can't use those macros directly)
 */
#ifndef sigmask
#define sigmask(s) (1 << ((s) - 1))
#endif
#define SIGNALS_DFL_SAFE sigmask(SIGSTOP) | sigmask(SIGTSTP) | \
	sigmask(SIGTTIN) | sigmask(SIGTTOU) | sigmask(SIGCHLD) | \
	sigmask(SIGCONT) | sigmask(SIGWINCH) | sigmask(SIGPWR) | \
	sigmask(SIGURG) | sigmask(SIGPOLL)

#ifdef ATTACH_DETACH
/*
 * Thanks to XPT_MPDEBUGGER, we have to mange child_wait().
 */
int
child_wait(pid, status)
     int pid;
     struct target_waitstatus *status;
{
  int save_errno, rv, xvaloff, saoff, sa_hand;
  struct pt_stop pt;
  struct user u;
  sigset_t set;
  /* Host signal number for a signal which the inferior terminates with, or
     0 if it hasn't terminated due to a signal.  */
  static int death_by_signal = 0;
#ifdef SVR4_SHARED_LIBS		/* use this to distinguish ptx 2 vs ptx 4 */
  prstatus_t pstatus;
#endif

  do {
    set_sigint_trap();	/* Causes SIGINT to be passed on to the
			   attached process. */
    save_errno = errno;

    got_sigchld = 0;

    sigemptyset(&set);

    while (got_sigchld == 0) {
	    sigsuspend(&set);
    }
    
    clear_sigint_trap();

    rv = mptrace(XPT_STOPSTAT, 0, (char *)&pt, 0);
    if (-1 == rv) {
	    printf("XPT_STOPSTAT: errno %d\n", errno); /* DEBUG */
	    continue;
    }

    pid = pt.ps_pid;

    if (pid != inferior_pid) {
	    /* NOTE: the mystery fork in csh/tcsh needs to be ignored.
	     * We should not return new children for the initial run
	     * of a process until it has done the exec.
	     */
	    /* inferior probably forked; send it on its way */
	    rv = mptrace(XPT_UNDEBUG, pid, 0, 0);
	    if (-1 == rv) {
		    printf("child_wait: XPT_UNDEBUG: pid %d: %s\n", pid,
			   safe_strerror(errno));
	    }
	    continue;
    }
    /* FIXME: Do we deal with fork notification correctly?  */
    switch (pt.ps_reason) {
    case PTS_FORK:
	/* multi proc: treat like PTS_EXEC */
	    /*
	     * Pretend this didn't happen, since gdb isn't set up
	     * to deal with stops on fork.
	     */
	    rv = ptrace(PT_CONTSIG, pid, 1, 0);
	    if (-1 == rv) {
		    printf("PTS_FORK: PT_CONTSIG: error %d\n", errno);
	    }
	    continue;
    case PTS_EXEC:
	    /*
	     * Pretend this is a SIGTRAP.
	     */
	    status->kind = TARGET_WAITKIND_STOPPED;
	    status->value.sig = TARGET_SIGNAL_TRAP;
	    break;
    case PTS_EXIT:
	    /*
	     * Note: we stop before the exit actually occurs.  Extract
	     * the exit code from the uarea.  If we're stopped in the
	     * exit() system call, the exit code will be in
	     * u.u_ap[0].  An exit due to an uncaught signal will have
	     * something else in here, see the comment in the default:
	     * case, below.  Finally,let the process exit.
	     */
	    if (death_by_signal)
	      {
		status->kind = TARGET_WAITKIND_SIGNALED;
		status->value.sig = target_signal_from_host (death_by_signal);
		death_by_signal = 0;
		break;
	      }
	    xvaloff = (unsigned long)&u.u_ap[0] - (unsigned long)&u;
	    errno = 0;
	    rv = ptrace(PT_RUSER, pid, (char *)xvaloff, 0);
	    status->kind = TARGET_WAITKIND_EXITED;
	    status->value.integer = rv;
	    /*
	     * addr & data to mptrace() don't matter here, since
	     * the process is already dead.
	     */
	    rv = mptrace(XPT_UNDEBUG, pid, 0, 0);
	    if (-1 == rv) {
		    printf("child_wait: PTS_EXIT: XPT_UNDEBUG: pid %d error %d\n", pid,
			   errno);
	    }
	    break;
    case PTS_WATCHPT_HIT:
	    fatal("PTS_WATCHPT_HIT\n");
	    break;
    default:
	    /* stopped by signal */
	    status->kind = TARGET_WAITKIND_STOPPED;
	    status->value.sig = target_signal_from_host (pt.ps_reason);
	    death_by_signal = 0;

	    if (0 == (SIGNALS_DFL_SAFE & sigmask(pt.ps_reason))) {
		    break;
	    }
	    /* else default action of signal is to die */
#ifdef SVR4_SHARED_LIBS
	    rv = ptrace(PT_GET_PRSTATUS, pid, (char *)&pstatus, 0);
	    if (-1 == rv)
		error("child_wait: signal %d PT_GET_PRSTATUS: %s\n",
			pt.ps_reason, safe_strerror(errno));
	    if (pstatus.pr_cursig != pt.ps_reason) {
		printf("pstatus signal %d, pt signal %d\n",
			pstatus.pr_cursig, pt.ps_reason);
	    }
	    sa_hand = (int)pstatus.pr_action.sa_handler;
#else
	    saoff = (unsigned long)&u.u_sa[0] - (unsigned long)&u;
	    saoff += sizeof(struct sigaction) * (pt.ps_reason - 1);
	    errno = 0;
	    sa_hand = ptrace(PT_RUSER, pid, (char *)saoff, 0);
	    if (errno)
		    error("child_wait: signal %d: RUSER: %s\n",
			   pt.ps_reason, safe_strerror(errno));
#endif
	    if ((int)SIG_DFL == sa_hand) {
		    /* we will be dying */
		    death_by_signal = pt.ps_reason;
	    }
	    break;
    }

  } while (pid != inferior_pid); /* Some other child died or stopped */

  return pid;
}
#else /* !ATTACH_DETACH */
/*
 * Simple child_wait() based on inftarg.c child_wait() for use until
 * the MPDEBUGGER child_wait() works properly.  This will go away when
 * that is fixed.
 */
child_wait (pid, ourstatus)
     int pid;
     struct target_waitstatus *ourstatus;
{
  int save_errno;
  int status;

  do {
    pid = wait (&status);
    save_errno = errno;

    if (pid == -1)
      {
	if (save_errno == EINTR)
	  continue;
	fprintf (stderr, "Child process unexpectedly missing: %s.\n",
		 safe_strerror (save_errno));
	ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
	ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
        return -1;
      }
  } while (pid != inferior_pid); /* Some other child died or stopped */
  store_waitstatus (ourstatus, status);
  return pid;
}
#endif /* ATTACH_DETACH */



/* This function simply calls ptrace with the given arguments.  
   It exists so that all calls to ptrace are isolated in this 
   machine-dependent file. */
int
call_ptrace (request, pid, addr, data)
     int request, pid;
     PTRACE_ARG3_TYPE addr;
     int data;
{
  return ptrace (request, pid, addr, data);
}

int
call_mptrace(request, pid, addr, data)
	int request, pid;
	PTRACE_ARG3_TYPE addr;
	int data;
{
	return mptrace(request, pid, addr, data);
}

#if defined (DEBUG_PTRACE)
/* For the rest of the file, use an extra level of indirection */
/* This lets us breakpoint usefully on call_ptrace. */
#define ptrace call_ptrace
#define mptrace call_mptrace
#endif

void
kill_inferior ()
{
  if (inferior_pid == 0)
    return;

  /* For MPDEBUGGER, don't use PT_KILL, since the child will stop
     again with a PTS_EXIT.  Just hit him with SIGKILL (so he stops)
     and detach. */

  kill (inferior_pid, SIGKILL);
#ifdef ATTACH_DETACH
  detach(SIGKILL);
#else /* ATTACH_DETACH */
  ptrace(PT_KILL, inferior_pid, 0, 0);
  wait((int *)NULL);
#endif /* ATTACH_DETACH */
  target_mourn_inferior ();
}

/* Resume execution of the inferior process.
   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.  */

void
child_resume (pid, step, signal)
     int pid;
     int step;
     enum target_signal signal;
{
  errno = 0;

  if (pid == -1)
    pid = inferior_pid;

  /* An address of (PTRACE_ARG3_TYPE)1 tells ptrace to continue from where
     it was.  (If GDB wanted it to start some other way, we have already
     written a new PC value to the child.)

     If this system does not support PT_SSTEP, a higher level function will
     have called single_step() to transmute the step request into a
     continue request (by setting breakpoints on all possible successor
     instructions), so we don't have to worry about that here.  */

  if (step)
    ptrace (PT_SSTEP,     pid, (PTRACE_ARG3_TYPE) 1, signal);
  else
    ptrace (PT_CONTSIG, pid, (PTRACE_ARG3_TYPE) 1, signal);

  if (errno)
    perror_with_name ("ptrace");
}

#ifdef ATTACH_DETACH
/* Start debugging the process whose number is PID.  */
int
attach (pid)
     int pid;
{
	sigset_t set;
	int rv;

	rv = mptrace(XPT_DEBUG, pid, 0, 0);
	if (-1 == rv) {
		error("mptrace(XPT_DEBUG): %s", safe_strerror(errno));
	}
	rv = mptrace(XPT_SIGNAL, pid, 0, SIGSTOP);
	if (-1 == rv) {
		error("mptrace(XPT_SIGNAL): %s", safe_strerror(errno));
	}
	attach_flag = 1;
	return pid;
}

void
detach (signo)
     int signo;
{
	int rv;

	rv = mptrace(XPT_UNDEBUG, inferior_pid, 1, signo);
	if (-1 == rv) {
		error("mptrace(XPT_UNDEBUG): %s", safe_strerror(errno));
	}
	attach_flag = 0;
}

#endif /* ATTACH_DETACH */

/* Default the type of the ptrace transfer to int.  */
#ifndef PTRACE_XFER_TYPE
#define PTRACE_XFER_TYPE int
#endif


/* NOTE! I tried using PTRACE_READDATA, etc., to read and write memory
   in the NEW_SUN_PTRACE case.
   It ought to be straightforward.  But it appears that writing did
   not write the data that I specified.  I cannot understand where
   it got the data that it actually did write.  */

/* Copy LEN bytes to or from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.   Copy to inferior if
   WRITE is nonzero.
  
   Returns the length copied, which is either the LEN argument or zero.
   This xfer function does not do partial moves, since child_ops
   doesn't allow memory operations to cross below us in the target stack
   anyway.  */

int
child_xfer_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;		/* ignored */
{
  register int i;
  /* Round starting address down to longword boundary.  */
  register CORE_ADDR addr = memaddr & - sizeof (PTRACE_XFER_TYPE);
  /* Round ending address up; get number of longwords that makes.  */
  register int count
    = (((memaddr + len) - addr) + sizeof (PTRACE_XFER_TYPE) - 1)
      / sizeof (PTRACE_XFER_TYPE);
  /* Allocate buffer of that many longwords.  */
  register PTRACE_XFER_TYPE *buffer
    = (PTRACE_XFER_TYPE *) alloca (count * sizeof (PTRACE_XFER_TYPE));

  if (write)
    {
      /* Fill start and end extra bytes of buffer with existing memory data.  */

      if (addr != memaddr || len < (int) sizeof (PTRACE_XFER_TYPE)) {
	/* Need part of initial word -- fetch it.  */
        buffer[0] = ptrace (PT_RTEXT, inferior_pid, (PTRACE_ARG3_TYPE) addr,
			    0);
      }

      if (count > 1)		/* FIXME, avoid if even boundary */
	{
	  buffer[count - 1]
	    = ptrace (PT_RTEXT, inferior_pid,
		      ((PTRACE_ARG3_TYPE)
		       (addr + (count - 1) * sizeof (PTRACE_XFER_TYPE))),
		      0);
	}

      /* Copy data to be written over corresponding part of buffer */

      memcpy ((char *) buffer + (memaddr & (sizeof (PTRACE_XFER_TYPE) - 1)),
	      myaddr,
	      len);

      /* Write the entire buffer.  */

      for (i = 0; i < count; i++, addr += sizeof (PTRACE_XFER_TYPE))
	{
	  errno = 0;
	  ptrace (PT_WDATA, inferior_pid, (PTRACE_ARG3_TYPE) addr,
		  buffer[i]);
	  if (errno)
	    {
	      /* Using the appropriate one (I or D) is necessary for
		 Gould NP1, at least.  */
	      errno = 0;
	      ptrace (PT_WTEXT, inferior_pid, (PTRACE_ARG3_TYPE) addr,
		      buffer[i]);
	    }
	  if (errno)
	    return 0;
	}
    }
  else
    {
      /* Read all the longwords */
      for (i = 0; i < count; i++, addr += sizeof (PTRACE_XFER_TYPE))
	{
	  errno = 0;
	  buffer[i] = ptrace (PT_RTEXT, inferior_pid,
			      (PTRACE_ARG3_TYPE) addr, 0);
	  if (errno)
	    return 0;
	  QUIT;
	}

      /* Copy appropriate bytes out of the buffer.  */
      memcpy (myaddr,
	      (char *) buffer + (memaddr & (sizeof (PTRACE_XFER_TYPE) - 1)),
	      len);
    }
  return len;
}


void
_initialize_symm_nat ()
{
#ifdef ATTACH_DETACH
/*
 * the MPDEBUGGER is necessary for process tree debugging and attach
 * to work, but it alters the behavior of debugged processes, so other
 * things (at least child_wait()) will have to change to accomodate
 * that.
 *
 * Note that attach is not implemented in dynix 3, and not in ptx
 * until version 2.1 of the OS.
 */
	int rv;
	sigset_t set;
	struct sigaction sact;

	rv = mptrace(XPT_MPDEBUGGER, 0, 0, 0);
	if (-1 == rv) {
		fatal("_initialize_symm_nat(): mptrace(XPT_MPDEBUGGER): %s",
		      safe_strerror(errno));
	}

	/*
	 * Under MPDEBUGGER, we get SIGCLHD when a traced process does
	 * anything of interest.
	 */

	/*
	 * Block SIGCHLD.  We leave it blocked all the time, and then
	 * call sigsuspend() in child_wait() to wait for the child
	 * to do something.  None of these ought to fail, but check anyway.
	 */
	sigemptyset(&set);
	rv = sigaddset(&set, SIGCHLD);
	if (-1 == rv) {
		fatal("_initialize_symm_nat(): sigaddset(SIGCHLD): %s",
		      safe_strerror(errno));
	}
	rv = sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);
	if (-1 == rv) {
		fatal("_initialize_symm_nat(): sigprocmask(SIG_BLOCK): %s",
		      safe_strerror(errno));
	}

	sact.sa_handler = sigchld_handler;
	sigemptyset(&sact.sa_mask);
	sact.sa_flags = SA_NOCLDWAIT; /* keep the zombies away */
	rv = sigaction(SIGCHLD, &sact, (struct sigaction *)NULL);
	if (-1 == rv) {
		fatal("_initialize_symm_nat(): sigaction(SIGCHLD): %s",
		      safe_strerror(errno));
	}
#endif
}
