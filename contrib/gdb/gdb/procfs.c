/* Machine independent support for SVR4 /proc (process file system) for GDB.
   Copyright 1991, 1992-98, 1999 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support.  Changes for sysv4.2mp procfs
   compatibility by Geoffrey Noer at Cygnus Solutions.

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


/*			N  O  T  E  S

For information on the details of using /proc consult section proc(4)
in the UNIX System V Release 4 System Administrator's Reference Manual.

The general register and floating point register sets are manipulated
separately.  This file makes the assumption that if FP0_REGNUM is
defined, then support for the floating point register set is desired,
regardless of whether or not the actual target has floating point hardware.

 */


#include "defs.h"

#include <sys/types.h>
#include <time.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <fcntl.h>
#include <errno.h>
#include "gdb_string.h"
#include <stropts.h>
#include <poll.h>
#include <unistd.h>
#include "gdb_stat.h"

#include "inferior.h"
#include "target.h"
#include "command.h"
#include "gdbcore.h"
#include "gdbthread.h"

#if !defined(SYS_lwp_create) && defined(SYS_lwpcreate)
# define SYS_lwp_create SYS_lwpcreate
#endif

#if !defined(SYS_lwp_exit) && defined(SYS_lwpexit)
# define SYS_lwp_exit SYS_lwpexit
#endif

#if !defined(SYS_lwp_wait) && defined(SYS_lwpwait)
# define SYS_lwp_wait SYS_lwpwait
#endif

#if !defined(SYS_lwp_self) && defined(SYS_lwpself)
# define SYS_lwp_self SYS_lwpself
#endif

#if !defined(SYS_lwp_info) && defined(SYS_lwpinfo)
# define SYS_lwp_info SYS_lwpinfo
#endif

#if !defined(SYS_lwp_private) && defined(SYS_lwpprivate)
# define SYS_lwp_private SYS_lwpprivate
#endif

#if !defined(SYS_lwp_kill) && defined(SYS_lwpkill)
# define SYS_lwp_kill SYS_lwpkill
#endif

#if !defined(SYS_lwp_suspend) && defined(SYS_lwpsuspend)
# define SYS_lwp_suspend SYS_lwpsuspend
#endif

#if !defined(SYS_lwp_continue) && defined(SYS_lwpcontinue)
# define SYS_lwp_continue SYS_lwpcontinue
#endif

/* the name of the proc status struct depends on the implementation */
/* Wrap Light Weight Process member in THE_PR_LWP macro for clearer code */
#ifndef HAVE_PSTATUS_T
  typedef prstatus_t gdb_prstatus_t;
#define THE_PR_LWP(a)	a
#else	/* HAVE_PSTATUS_T */
  typedef pstatus_t gdb_prstatus_t;
#define THE_PR_LWP(a)	a.pr_lwp
#if !defined(HAVE_PRRUN_T) && defined(HAVE_MULTIPLE_PROC_FDS)
  /* Fallback definitions - for using configure information directly */
#ifndef UNIXWARE
#define UNIXWARE	1
#endif
#if !defined(PROCFS_USE_READ_WRITE) && !defined(HAVE_PROCFS_PIOCSET)
#define PROCFS_USE_READ_WRITE	1
#endif
#endif	/* !HAVE_PRRUN_T && HAVE_MULTIPLE_PROC_FDS */
#endif	/* HAVE_PSTATUS_T */

#define MAX_SYSCALLS	256	/* Maximum number of syscalls for table */

/* proc name formats may vary depending on the proc implementation */
#ifdef HAVE_MULTIPLE_PROC_FDS
#  ifndef CTL_PROC_NAME_FMT
#  define CTL_PROC_NAME_FMT "/proc/%d/ctl"
#  define AS_PROC_NAME_FMT "/proc/%d/as"
#  define MAP_PROC_NAME_FMT "/proc/%d/map"
#  define STATUS_PROC_NAME_FMT "/proc/%d/status"
#  endif
#else /* HAVE_MULTIPLE_PROC_FDS */
#  ifndef CTL_PROC_NAME_FMT
#  define CTL_PROC_NAME_FMT "/proc/%05d"
#  define AS_PROC_NAME_FMT "/proc/%05d"
#  define MAP_PROC_NAME_FMT "/proc/%05d"
#  define STATUS_PROC_NAME_FMT "/proc/%05d"
#  endif
#endif /* HAVE_MULTIPLE_PROC_FDS */


/* These #ifdefs are for sol2.x in particular.  sol2.x has
   both a "gregset_t" and a "prgregset_t", which have
   similar uses but different layouts.  sol2.x gdb tries to
   use prgregset_t (and prfpregset_t) everywhere. */

#ifdef GDB_GREGSET_TYPE
  typedef GDB_GREGSET_TYPE gdb_gregset_t;
#else
  typedef gregset_t gdb_gregset_t;
#endif

#ifdef GDB_FPREGSET_TYPE
  typedef GDB_FPREGSET_TYPE gdb_fpregset_t;
#else
  typedef fpregset_t gdb_fpregset_t;
#endif


#define MAX_PROC_NAME_SIZE sizeof("/proc/1234567890/status")

struct target_ops procfs_ops;

int procfs_suppress_run = 0;	/* Non-zero if procfs should pretend not to
				   be a runnable target.  Used by targets
				   that can sit atop procfs, such as solaris
				   thread support.  */

#if 1	/* FIXME: Gross and ugly hack to resolve coredep.c global */
CORE_ADDR kernel_u_addr;
#endif

#ifdef BROKEN_SIGINFO_H		/* Workaround broken SGS <sys/siginfo.h> */
#undef si_pid
#define si_pid _data._proc.pid
#undef si_uid
#define si_uid _data._proc._pdata._kill.uid
#endif /* BROKEN_SIGINFO_H */

/* Define structures for passing commands to /proc/pid/ctl file.  Note that
   while we create these for the PROCFS_USE_READ_WRITE world, we use them
   and ignore the extra cmd int in other proc schemes.
*/
/* generic ctl msg */
struct proc_ctl {
        int     cmd;
        long    data;
};

/* set general registers */
struct greg_ctl {
        int             cmd;
        gdb_gregset_t	gregset;
};

/* set fp registers */
struct fpreg_ctl {
        int             cmd;
        gdb_fpregset_t	fpregset;
};

/* set signals to be traced */
struct sig_ctl {
        int             cmd;
        sigset_t        sigset;
};

/* set faults to be traced */
struct flt_ctl {
        int             cmd;
        fltset_t        fltset;
};

/* set system calls to be traced */
struct sys_ctl {
        int             cmd;
        sysset_t        sysset;
};

/* set current signal to be traced */
struct sigi_ctl {
        int             cmd;
        siginfo_t       siginfo;
};

/*  All access to the inferior, either one started by gdb or one that has
    been attached to, is controlled by an instance of a procinfo structure,
    defined below.  Since gdb currently only handles one inferior at a time,
    the procinfo structure for the inferior is statically allocated and
    only one exists at any given time.  There is a separate procinfo
    structure for use by the "info proc" command, so that we can print
    useful information about any random process without interfering with
    the inferior's procinfo information. */

struct procinfo {
  struct procinfo *next;
  int pid;			/* Process ID of inferior */
  int ctl_fd;			/* File descriptor for /proc ctl file */
  int status_fd;		/* File descriptor for /proc status file */
  int as_fd;			/* File descriptor for /proc as file */
  int map_fd;			/* File descriptor for /proc map file */
  char *pathname;		/* Pathname to /proc entry */
  int had_event;		/* poll/select says something happened */
  int was_stopped;		/* Nonzero if was stopped prior to attach */
  int nopass_next_sigstop;	/* Don't pass a sigstop on next resume */
#ifdef HAVE_PRRUN_T
  prrun_t prrun;		/* Control state when it is run */
#endif
  gdb_prstatus_t prstatus;	/* Current process status info */
  struct greg_ctl gregset;	/* General register set */
  struct fpreg_ctl fpregset;	/* Floating point register set */
  struct flt_ctl fltset;	/* Current traced hardware fault set */
  struct sig_ctl trace;		/* Current traced signal set */
  struct sys_ctl exitset;	/* Current traced system call exit set */
  struct sys_ctl entryset;	/* Current traced system call entry set */
  struct sig_ctl saved_sighold;	/* Saved held signal set */
  struct flt_ctl saved_fltset;  /* Saved traced hardware fault set */
  struct sig_ctl saved_trace;   /* Saved traced signal set */
  struct sys_ctl saved_exitset; /* Saved traced system call exit set */
  struct sys_ctl saved_entryset;/* Saved traced system call entry set */
  int num_syscall_handlers;	/* Number of syscall trap handlers
				   currently installed */
				/* Pointer to list of syscall trap handlers */
  struct procfs_syscall_handler *syscall_handlers; 
  int saved_rtnval;		/* return value and status for wait(), */
  int saved_statval;		/*  as supplied by a syscall handler. */
  int new_child;		/* Non-zero if it's a new thread */
};

/* List of inferior process information */
static struct procinfo *procinfo_list = NULL;
static struct pollfd *poll_list; /* pollfds used for waiting on /proc */

static int num_poll_list = 0;	/* Number of entries in poll_list */

/*  Much of the information used in the /proc interface, particularly for
    printing status information, is kept as tables of structures of the
    following form.  These tables can be used to map numeric values to
    their symbolic names and to a string that describes their specific use. */

struct trans {
  int value;			/* The numeric value */
  char *name;			/* The equivalent symbolic value */
  char *desc;			/* Short description of value */
};

/*  Translate bits in the pr_flags member of the prstatus structure, into the
    names and desc information. */

static struct trans pr_flag_table[] =
{
#if defined (PR_STOPPED)
  { PR_STOPPED, "PR_STOPPED", "Process is stopped" },
#endif
#if defined (PR_ISTOP)
  { PR_ISTOP, "PR_ISTOP", "Stopped on an event of interest" },
#endif
#if defined (PR_DSTOP)
  { PR_DSTOP, "PR_DSTOP", "A stop directive is in effect" },
#endif
#if defined (PR_ASLEEP)
  { PR_ASLEEP, "PR_ASLEEP", "Sleeping in an interruptible system call" },
#endif
#if defined (PR_FORK)
  { PR_FORK, "PR_FORK", "Inherit-on-fork is in effect" },
#endif
#if defined (PR_RLC)
  { PR_RLC, "PR_RLC", "Run-on-last-close is in effect" },
#endif
#if defined (PR_PTRACE)
  { PR_PTRACE, "PR_PTRACE", "Process is being controlled by ptrace" },
#endif
#if defined (PR_PCINVAL)
  { PR_PCINVAL, "PR_PCINVAL", "PC refers to an invalid virtual address" },
#endif
#if defined (PR_ISSYS)
  { PR_ISSYS, "PR_ISSYS", "Is a system process" },
#endif
#if defined (PR_STEP)
  { PR_STEP, "PR_STEP", "Process has single step pending" },
#endif
#if defined (PR_KLC)
  { PR_KLC, "PR_KLC", "Kill-on-last-close is in effect" },
#endif
#if defined (PR_ASYNC)
  { PR_ASYNC, "PR_ASYNC", "Asynchronous stop is in effect" },
#endif
#if defined (PR_PCOMPAT)
  { PR_PCOMPAT, "PR_PCOMPAT", "Ptrace compatibility mode in effect" },
#endif
#if defined (PR_MSACCT)
  { PR_MSACCT, "PR_MSACCT", "Microstate accounting enabled" },
#endif
#if defined (PR_BPTADJ)
  { PR_BPTADJ, "PR_BPTADJ", "Breakpoint PC adjustment in effect" },
#endif
#if defined (PR_ASLWP)
  { PR_ASLWP, "PR_ASLWP", "Asynchronus signal LWP" },
#endif
  { 0, NULL, NULL }
};

/*  Translate values in the pr_why field of the prstatus struct. */

static struct trans pr_why_table[] =
{
#if defined (PR_REQUESTED)
  { PR_REQUESTED, "PR_REQUESTED", "Directed to stop via PIOCSTOP/PIOCWSTOP" },
#endif
#if defined (PR_SIGNALLED)
  { PR_SIGNALLED, "PR_SIGNALLED", "Receipt of a traced signal" },
#endif
#if defined (PR_SYSENTRY)
  { PR_SYSENTRY, "PR_SYSENTRY", "Entry to a traced system call" },
#endif
#if defined (PR_SYSEXIT)
  { PR_SYSEXIT, "PR_SYSEXIT", "Exit from a traced system call" },
#endif
#if defined (PR_JOBCONTROL)
  { PR_JOBCONTROL, "PR_JOBCONTROL", "Default job control stop signal action" },
#endif
#if defined (PR_FAULTED)
  { PR_FAULTED, "PR_FAULTED", "Incurred a traced hardware fault" },
#endif
#if defined (PR_SUSPENDED)
  { PR_SUSPENDED, "PR_SUSPENDED", "Process suspended" },
#endif
#if defined (PR_CHECKPOINT)
  { PR_CHECKPOINT, "PR_CHECKPOINT", "(???)" },
#endif
  { 0, NULL, NULL }
};

/*  Hardware fault translation table. */

static struct trans faults_table[] =
{
#if defined (FLTILL)
  { FLTILL, "FLTILL", "Illegal instruction" },
#endif
#if defined (FLTPRIV)
  { FLTPRIV, "FLTPRIV", "Privileged instruction" },
#endif
#if defined (FLTBPT)
  { FLTBPT, "FLTBPT", "Breakpoint trap" },
#endif
#if defined (FLTTRACE)
  { FLTTRACE, "FLTTRACE", "Trace trap" },
#endif
#if defined (FLTACCESS)
  { FLTACCESS, "FLTACCESS", "Memory access fault" },
#endif
#if defined (FLTBOUNDS)
  { FLTBOUNDS, "FLTBOUNDS", "Memory bounds violation" },
#endif
#if defined (FLTIOVF)
  { FLTIOVF, "FLTIOVF", "Integer overflow" },
#endif
#if defined (FLTIZDIV)
  { FLTIZDIV, "FLTIZDIV", "Integer zero divide" },
#endif
#if defined (FLTFPE)
  { FLTFPE, "FLTFPE", "Floating-point exception" },
#endif
#if defined (FLTSTACK)
  { FLTSTACK, "FLTSTACK", "Unrecoverable stack fault" },
#endif
#if defined (FLTPAGE)
  { FLTPAGE, "FLTPAGE", "Recoverable page fault" },
#endif
  { 0, NULL, NULL }
};

/* Translation table for signal generation information.  See UNIX System
   V Release 4 Programmer's Reference Manual, siginfo(5).  */

static struct sigcode {
  int signo;
  int code;
  char *codename;
  char *desc;
} siginfo_table[] = {
#if defined (SIGILL) && defined (ILL_ILLOPC)
  { SIGILL, ILL_ILLOPC, "ILL_ILLOPC", "Illegal opcode" },
#endif
#if defined (SIGILL) && defined (ILL_ILLOPN)
  { SIGILL, ILL_ILLOPN, "ILL_ILLOPN", "Illegal operand", },
#endif
#if defined (SIGILL) && defined (ILL_ILLADR)
  { SIGILL, ILL_ILLADR, "ILL_ILLADR", "Illegal addressing mode" },
#endif
#if defined (SIGILL) && defined (ILL_ILLTRP)
  { SIGILL, ILL_ILLTRP, "ILL_ILLTRP", "Illegal trap" },
#endif
#if defined (SIGILL) && defined (ILL_PRVOPC)
  { SIGILL, ILL_PRVOPC, "ILL_PRVOPC", "Privileged opcode" },
#endif
#if defined (SIGILL) && defined (ILL_PRVREG)
  { SIGILL, ILL_PRVREG, "ILL_PRVREG", "Privileged register" },
#endif
#if defined (SIGILL) && defined (ILL_COPROC)
  { SIGILL, ILL_COPROC, "ILL_COPROC", "Coprocessor error" },
#endif
#if defined (SIGILL) && defined (ILL_BADSTK)
  { SIGILL, ILL_BADSTK, "ILL_BADSTK", "Internal stack error" },
#endif
#if defined (SIGFPE) && defined (FPE_INTDIV)
  { SIGFPE, FPE_INTDIV, "FPE_INTDIV", "Integer divide by zero" },
#endif
#if defined (SIGFPE) && defined (FPE_INTOVF)
  { SIGFPE, FPE_INTOVF, "FPE_INTOVF", "Integer overflow" },
#endif
#if defined (SIGFPE) && defined (FPE_FLTDIV)
  { SIGFPE, FPE_FLTDIV, "FPE_FLTDIV", "Floating point divide by zero" },
#endif
#if defined (SIGFPE) && defined (FPE_FLTOVF)
  { SIGFPE, FPE_FLTOVF, "FPE_FLTOVF", "Floating point overflow" },
#endif
#if defined (SIGFPE) && defined (FPE_FLTUND)
  { SIGFPE, FPE_FLTUND, "FPE_FLTUND", "Floating point underflow" },
#endif
#if defined (SIGFPE) && defined (FPE_FLTRES)
  { SIGFPE, FPE_FLTRES, "FPE_FLTRES", "Floating point inexact result" },
#endif
#if defined (SIGFPE) && defined (FPE_FLTINV)
  { SIGFPE, FPE_FLTINV, "FPE_FLTINV", "Invalid floating point operation" },
#endif
#if defined (SIGFPE) && defined (FPE_FLTSUB)
  { SIGFPE, FPE_FLTSUB, "FPE_FLTSUB", "Subscript out of range" },
#endif
#if defined (SIGSEGV) && defined (SEGV_MAPERR)
  { SIGSEGV, SEGV_MAPERR, "SEGV_MAPERR", "Address not mapped to object" },
#endif
#if defined (SIGSEGV) && defined (SEGV_ACCERR)
  { SIGSEGV, SEGV_ACCERR, "SEGV_ACCERR", "Invalid permissions for object" },
#endif
#if defined (SIGBUS) && defined (BUS_ADRALN)
  { SIGBUS, BUS_ADRALN, "BUS_ADRALN", "Invalid address alignment" },
#endif
#if defined (SIGBUS) && defined (BUS_ADRERR)
  { SIGBUS, BUS_ADRERR, "BUS_ADRERR", "Non-existent physical address" },
#endif
#if defined (SIGBUS) && defined (BUS_OBJERR)
  { SIGBUS, BUS_OBJERR, "BUS_OBJERR", "Object specific hardware error" },
#endif
#if defined (SIGTRAP) && defined (TRAP_BRKPT)
  { SIGTRAP, TRAP_BRKPT, "TRAP_BRKPT", "Process breakpoint" },
#endif
#if defined (SIGTRAP) && defined (TRAP_TRACE)
  { SIGTRAP, TRAP_TRACE, "TRAP_TRACE", "Process trace trap" },
#endif
#if defined (SIGCLD) && defined (CLD_EXITED)
  { SIGCLD, CLD_EXITED, "CLD_EXITED", "Child has exited" },
#endif
#if defined (SIGCLD) && defined (CLD_KILLED)
  { SIGCLD, CLD_KILLED, "CLD_KILLED", "Child was killed" },
#endif
#if defined (SIGCLD) && defined (CLD_DUMPED)
  { SIGCLD, CLD_DUMPED, "CLD_DUMPED", "Child has terminated abnormally" },
#endif
#if defined (SIGCLD) && defined (CLD_TRAPPED)
  { SIGCLD, CLD_TRAPPED, "CLD_TRAPPED", "Traced child has trapped" },
#endif
#if defined (SIGCLD) && defined (CLD_STOPPED)
  { SIGCLD, CLD_STOPPED, "CLD_STOPPED", "Child has stopped" },
#endif
#if defined (SIGCLD) && defined (CLD_CONTINUED)
  { SIGCLD, CLD_CONTINUED, "CLD_CONTINUED", "Stopped child had continued" },
#endif
#if defined (SIGPOLL) && defined (POLL_IN)
  { SIGPOLL, POLL_IN, "POLL_IN", "Input input available" },
#endif
#if defined (SIGPOLL) && defined (POLL_OUT)
  { SIGPOLL, POLL_OUT, "POLL_OUT", "Output buffers available" },
#endif
#if defined (SIGPOLL) && defined (POLL_MSG)
  { SIGPOLL, POLL_MSG, "POLL_MSG", "Input message available" },
#endif
#if defined (SIGPOLL) && defined (POLL_ERR)
  { SIGPOLL, POLL_ERR, "POLL_ERR", "I/O error" },
#endif
#if defined (SIGPOLL) && defined (POLL_PRI)
  { SIGPOLL, POLL_PRI, "POLL_PRI", "High priority input available" },
#endif
#if defined (SIGPOLL) && defined (POLL_HUP)
  { SIGPOLL, POLL_HUP, "POLL_HUP", "Device disconnected" },
#endif
  { 0, 0, NULL, NULL }
};

static char *syscall_table[MAX_SYSCALLS];

/* Prototypes for local functions */

static void procfs_stop PARAMS ((void));

static int procfs_thread_alive PARAMS ((int));

static int procfs_can_run PARAMS ((void));

static void procfs_mourn_inferior PARAMS ((void));

static void procfs_fetch_registers PARAMS ((int));

static int procfs_wait PARAMS ((int, struct target_waitstatus *));

static void procfs_open PARAMS ((char *, int));

static void procfs_files_info PARAMS ((struct target_ops *));

static void procfs_prepare_to_store PARAMS ((void));

static void procfs_detach PARAMS ((char *, int));

static void procfs_attach PARAMS ((char *, int));

static void proc_set_exec_trap PARAMS ((void));

static void  procfs_init_inferior PARAMS ((int));

static struct procinfo *create_procinfo PARAMS ((int));

static void procfs_store_registers PARAMS ((int));

static int procfs_xfer_memory PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

static void procfs_kill_inferior PARAMS ((void));

static char *sigcodedesc PARAMS ((siginfo_t *));

static char *sigcodename PARAMS ((siginfo_t *));

static struct procinfo *wait_fd PARAMS ((void));

static void remove_fd PARAMS ((struct procinfo *));

static void add_fd PARAMS ((struct procinfo *));

static void set_proc_siginfo PARAMS ((struct procinfo *, int));

static void init_syscall_table PARAMS ((void));

static char *syscallname PARAMS ((int));

static char *signalname PARAMS ((int));

static char *errnoname PARAMS ((int));

static int proc_address_to_fd PARAMS ((struct procinfo *, CORE_ADDR, int));

static int open_proc_file PARAMS ((int, struct procinfo *, int, int));

static void close_proc_file PARAMS ((struct procinfo *));

static void unconditionally_kill_inferior PARAMS ((struct procinfo *));

static NORETURN void proc_init_failed PARAMS ((struct procinfo *, char *, int)) ATTR_NORETURN;

static void info_proc PARAMS ((char *, int));

static void info_proc_flags PARAMS ((struct procinfo *, int));

static void info_proc_stop PARAMS ((struct procinfo *, int));

static void info_proc_siginfo PARAMS ((struct procinfo *, int));

static void info_proc_syscalls PARAMS ((struct procinfo *, int));

static void info_proc_mappings PARAMS ((struct procinfo *, int));

static void info_proc_signals PARAMS ((struct procinfo *, int));

static void info_proc_faults PARAMS ((struct procinfo *, int));

static char *mappingflags PARAMS ((long));

static char *lookupname PARAMS ((struct trans *, unsigned int, char *));

static char *lookupdesc PARAMS ((struct trans *, unsigned int));

static int do_attach PARAMS ((int pid));

static void do_detach PARAMS ((int siggnal));

static void procfs_create_inferior PARAMS ((char *, char *, char **));

static void procfs_notice_signals PARAMS ((int pid));

static void notice_signals PARAMS ((struct procinfo *, struct sig_ctl *));

static struct procinfo *find_procinfo PARAMS ((pid_t pid, int okfail));

static int procfs_write_pcwstop PARAMS ((struct procinfo *));
static int procfs_read_status PARAMS ((struct procinfo *));
static void procfs_write_pckill PARAMS ((struct procinfo *));

typedef int syscall_func_t PARAMS ((struct procinfo *pi, int syscall_num,
				    int why, int *rtnval, int *statval));

static void procfs_set_syscall_trap PARAMS ((struct procinfo *pi,
					     int syscall_num, int flags,
					     syscall_func_t *func));

static void procfs_clear_syscall_trap PARAMS ((struct procinfo *pi,
					       int syscall_num, int errok));

#define PROCFS_SYSCALL_ENTRY 0x1 /* Trap on entry to sys call */
#define PROCFS_SYSCALL_EXIT 0x2	/* Trap on exit from sys call */

static syscall_func_t procfs_exit_handler;

static syscall_func_t procfs_exec_handler;

#ifdef SYS_sproc
static syscall_func_t procfs_sproc_handler;
static syscall_func_t procfs_fork_handler;
#endif

#ifdef SYS_lwp_create
static syscall_func_t procfs_lwp_creation_handler;
#endif

static void modify_inherit_on_fork_flag PARAMS ((int fd, int flag));
static void modify_run_on_last_close_flag PARAMS ((int fd, int flag));

/* */

struct procfs_syscall_handler
{
  int syscall_num;		/* The number of the system call being handled */
				/* The function to be called */
  syscall_func_t *func;
};

static void procfs_resume PARAMS ((int pid, int step,
				   enum target_signal signo));

static void init_procfs_ops PARAMS ((void));

/* External function prototypes that can't be easily included in any
   header file because the args are typedefs in system include files. */

extern void supply_gregset PARAMS ((gdb_gregset_t *));

extern void fill_gregset PARAMS ((gdb_gregset_t *, int));

#ifdef FP0_REGNUM
extern void supply_fpregset PARAMS ((gdb_fpregset_t *));

extern void fill_fpregset PARAMS ((gdb_fpregset_t *, int));
#endif

/*

LOCAL FUNCTION

	find_procinfo -- convert a process id to a struct procinfo

SYNOPSIS

	static struct procinfo * find_procinfo (pid_t pid, int okfail);

DESCRIPTION
	
	Given a process id, look it up in the procinfo chain.  Returns
	a struct procinfo *.  If can't find pid, then call error(),
	unless okfail is set, in which case, return NULL;
 */

static struct procinfo *
find_procinfo (pid, okfail)
     pid_t pid;
     int okfail;
{
  struct procinfo *procinfo;

  for (procinfo = procinfo_list; procinfo; procinfo = procinfo->next)
    if (procinfo->pid == pid)
      return procinfo;

  if (okfail)
    return NULL;

  error ("procfs (find_procinfo):  Couldn't locate pid %d", pid);
}

/*

LOCAL MACRO

	current_procinfo -- convert inferior_pid to a struct procinfo

SYNOPSIS

	static struct procinfo * current_procinfo;

DESCRIPTION
	
	Looks up inferior_pid in the procinfo chain.  Always returns a
	struct procinfo *.  If process can't be found, we error() out.
 */

#define current_procinfo find_procinfo (inferior_pid, 0)

/*

LOCAL FUNCTION

	add_fd -- Add the fd to the poll/select list

SYNOPSIS

	static void add_fd (struct procinfo *);

DESCRIPTION
	
	Add the fd of the supplied procinfo to the list of fds used for
	poll/select operations.
 */

static void
add_fd (pi)
     struct procinfo *pi;
{
  if (num_poll_list <= 0)
    poll_list = (struct pollfd *) xmalloc (sizeof (struct pollfd));
  else
    poll_list = (struct pollfd *) xrealloc (poll_list,
					    (num_poll_list + 1)
					    * sizeof (struct pollfd));
  poll_list[num_poll_list].fd = pi->ctl_fd;
#ifdef UNIXWARE
  poll_list[num_poll_list].events = POLLWRNORM;
#else
  poll_list[num_poll_list].events = POLLPRI;
#endif

  num_poll_list++;
}

/*

LOCAL FUNCTION

	remove_fd -- Remove the fd from the poll/select list

SYNOPSIS

	static void remove_fd (struct procinfo *);

DESCRIPTION
	
	Remove the fd of the supplied procinfo from the list of fds used 
	for poll/select operations.
 */

static void
remove_fd (pi)
     struct procinfo *pi;
{
  int i;

  for (i = 0; i < num_poll_list; i++)
    {
      if (poll_list[i].fd == pi->ctl_fd)
	{
	  if (i != num_poll_list - 1)
	    memcpy (poll_list + i, poll_list + i + 1,
		    (num_poll_list - i - 1) * sizeof (struct pollfd));

	  num_poll_list--;

	  if (num_poll_list == 0)
	    free (poll_list);
	  else
	    poll_list = (struct pollfd *) xrealloc (poll_list,
						    num_poll_list
						    * sizeof (struct pollfd));
	  return;
	}
    }
}

/*

LOCAL FUNCTION

	procfs_read_status - get procfs fd status

SYNOPSIS

	static int procfs_read_status (pi) struct procinfo *pi;

DESCRIPTION
	
	Given a pointer to a procinfo struct, get the status of
	the status_fd in the appropriate way.  Returns 0 on failure,
	1 on success.
 */

static int
procfs_read_status (pi)
  struct procinfo *pi;
{
#ifdef PROCFS_USE_READ_WRITE
   if ((lseek (pi->status_fd, 0, SEEK_SET) < 0) ||
           (read (pi->status_fd, (char *) &pi->prstatus, 
             sizeof (gdb_prstatus_t)) != sizeof (gdb_prstatus_t)))
#else
  if (ioctl (pi->status_fd, PIOCSTATUS, &pi->prstatus) < 0)
#endif
    return 0;
  else
    return 1;
}

/*

LOCAL FUNCTION

	procfs_write_pcwstop - send a PCWSTOP to procfs fd

SYNOPSIS

	static int procfs_write_pcwstop (pi) struct procinfo *pi;

DESCRIPTION
	
	Given a pointer to a procinfo struct, send a PCWSTOP to
	the ctl_fd in the appropriate way.  Returns 0 on failure,
	1 on success.
 */

static int
procfs_write_pcwstop (pi)
  struct procinfo *pi;
{
#ifdef PROCFS_USE_READ_WRITE
  long cmd = PCWSTOP;
  if (write (pi->ctl_fd, (char *) &cmd, sizeof (long)) < 0)
#else
  if (ioctl (pi->ctl_fd, PIOCWSTOP, &pi->prstatus) < 0)
#endif
    return 0;
  else
    return 1;
}

/*

LOCAL FUNCTION

	procfs_write_pckill - send a kill to procfs fd

SYNOPSIS

	static void procfs_write_pckill (pi) struct procinfo *pi;

DESCRIPTION
	
	Given a pointer to a procinfo struct, send a kill to
	the ctl_fd in the appropriate way.  Returns 0 on failure,
	1 on success.
 */

static void
procfs_write_pckill (pi)
  struct procinfo *pi;
{
#ifdef PROCFS_USE_READ_WRITE
  struct proc_ctl pctl;
  pctl.cmd = PCKILL;
  pctl.data = SIGKILL;
  write (pi->ctl_fd, &pctl, sizeof (struct proc_ctl));
#else
  int signo = SIGKILL;
  ioctl (pi->ctl_fd, PIOCKILL, &signo);
#endif
}

static struct procinfo *
wait_fd ()
{
  struct procinfo *pi, *next_pi;
#ifndef LOSING_POLL
  int num_fds;
  int i;
#endif

  set_sigint_trap ();	/* Causes SIGINT to be passed on to the
			   attached process. */
  set_sigio_trap ();

 wait_again:
#ifndef LOSING_POLL
  while (1)
    {
      num_fds = poll (poll_list, num_poll_list, -1);
      if (num_fds > 0)
	break;
      if (num_fds < 0 && errno == EINTR)
	continue;
      print_sys_errmsg ("poll failed", errno);
      error ("Poll failed, returned %d", num_fds);
    }
#else /* LOSING_POLL */
  pi = current_procinfo;

  while (!procfs_write_pcwstop (pi))
    {
      if (errno == ENOENT)
	{
	  /* Process exited.  */
	  pi->prstatus.pr_flags = 0;
	  break;
	}
      else if (errno != EINTR)
	{
	  print_sys_errmsg (pi->pathname, errno);
	  error ("procfs_write_pcwstop failed");
	}
    }
  pi->had_event = 1;
#endif /* LOSING_POLL */
  
  clear_sigint_trap ();
  clear_sigio_trap ();

#ifndef LOSING_POLL

  for (i = 0; i < num_poll_list && num_fds > 0; i++)
    {
      if (0 == (poll_list[i].revents & 
		(POLLWRNORM | POLLPRI | POLLERR | POLLHUP | POLLNVAL)))
	continue;
      for (pi = procinfo_list; pi; pi = next_pi)
	{
	  next_pi = pi->next;
	  if (poll_list[i].fd == pi->ctl_fd)
	    {
	      num_fds--;
	      if ((poll_list[i].revents & POLLHUP) != 0	||
		  !procfs_read_status(pi))
		{ /* The LWP has apparently terminated.  */
		  if (num_poll_list <= 1)
		    {
		      pi->prstatus.pr_flags = 0;
		      pi->had_event = 1;
		      break;
		    }
		  if (info_verbose)
		    printf_filtered ("LWP %d exited.\n", 
				     (pi->pid >> 16) & 0xffff);
		  close_proc_file (pi);
		  i--;			/* don't skip deleted entry */
		  if (num_fds != 0)
		    break;		/* already another event to process */
		  else
		    goto wait_again; 	/* wait for another event */
		}
	      pi->had_event = 1;
	      break;
	    }
	}
      if (!pi)
	error ("wait_fd: Couldn't find procinfo for fd %d\n",
	       poll_list[i].fd);
    }
#endif /* LOSING_POLL */

  return pi;
}

/*

LOCAL FUNCTION

	lookupdesc -- translate a value to a summary desc string

SYNOPSIS

	static char *lookupdesc (struct trans *transp, unsigned int val);

DESCRIPTION
	
	Given a pointer to a translation table and a value to be translated,
	lookup the desc string and return it.
 */

static char *
lookupdesc (transp, val)
     struct trans *transp;
     unsigned int val;
{
  char *desc;
  
  for (desc = NULL; transp -> name != NULL; transp++)
    {
      if (transp -> value == val)
	{
	  desc = transp -> desc;
	  break;
	}
    }

  /* Didn't find a translation for the specified value, set a default one. */

  if (desc == NULL)
    {
      desc = "Unknown";
    }
  return (desc);
}

/*

LOCAL FUNCTION

	lookupname -- translate a value to symbolic name

SYNOPSIS

	static char *lookupname (struct trans *transp, unsigned int val,
				 char *prefix);

DESCRIPTION
	
	Given a pointer to a translation table, a value to be translated,
	and a default prefix to return if the value can't be translated,
	match the value with one of the translation table entries and
	return a pointer to the symbolic name.

	If no match is found it just returns the value as a printable string,
	with the given prefix.  The previous such value, if any, is freed
	at this time.
 */

static char *
lookupname (transp, val, prefix)
     struct trans *transp;
     unsigned int val;
     char *prefix;
{
  static char *locbuf;
  char *name;
  
  for (name = NULL; transp -> name != NULL; transp++)
    {
      if (transp -> value == val)
	{
	  name = transp -> name;
	  break;
	}
    }

  /* Didn't find a translation for the specified value, build a default
     one using the specified prefix and return it.  The lifetime of
     the value is only until the next one is needed. */

  if (name == NULL)
    {
      if (locbuf != NULL)
	{
	  free (locbuf);
	}
      locbuf = xmalloc (strlen (prefix) + 16);
      sprintf (locbuf, "%s %u", prefix, val);
      name = locbuf;
    }
  return (name);
}

static char *
sigcodename (sip)
     siginfo_t *sip;
{
  struct sigcode *scp;
  char *name = NULL;
  static char locbuf[32];
  
  for (scp = siginfo_table; scp -> codename != NULL; scp++)
    {
      if ((scp -> signo == sip -> si_signo) &&
	  (scp -> code == sip -> si_code))
	{
	  name = scp -> codename;
	  break;
	}
    }
  if (name == NULL)
    {
      sprintf (locbuf, "sigcode %u", sip -> si_signo);
      name = locbuf;
    }
  return (name);
}

static char *
sigcodedesc (sip)
     siginfo_t *sip;
{
  struct sigcode *scp;
  char *desc = NULL;
  
  for (scp = siginfo_table; scp -> codename != NULL; scp++)
    {
      if ((scp -> signo == sip -> si_signo) &&
	  (scp -> code == sip -> si_code))
	{
	  desc = scp -> desc;
	  break;
	}
    }
  if (desc == NULL)
    {
      desc = "Unrecognized signal or trap use";
    }
  return (desc);
}

/*

LOCAL FUNCTION

	syscallname - translate a system call number into a system call name

SYNOPSIS

	char *syscallname (int syscallnum)

DESCRIPTION

	Given a system call number, translate it into the printable name
	of a system call, or into "syscall <num>" if it is an unknown
	number.
 */

static char *
syscallname (syscallnum)
     int syscallnum;
{
  static char locbuf[32];
  
  if (syscallnum >= 0 && syscallnum < MAX_SYSCALLS
      && syscall_table[syscallnum] != NULL)
    return syscall_table[syscallnum];
  else
    {
      sprintf (locbuf, "syscall %u", syscallnum);
      return locbuf;
    }
}

/*

LOCAL FUNCTION

	init_syscall_table - initialize syscall translation table

SYNOPSIS

	void init_syscall_table (void)

DESCRIPTION

	Dynamically initialize the translation table to convert system
	call numbers into printable system call names.  Done once per
	gdb run, on initialization.

NOTES

	This is awfully ugly, but preprocessor tricks to make it prettier
	tend to be nonportable.
 */

static void
init_syscall_table ()
{
#if defined (SYS_exit)
  syscall_table[SYS_exit] = "exit";
#endif
#if defined (SYS_fork)
  syscall_table[SYS_fork] = "fork";
#endif
#if defined (SYS_read)
  syscall_table[SYS_read] = "read";
#endif
#if defined (SYS_write)
  syscall_table[SYS_write] = "write";
#endif
#if defined (SYS_open)
  syscall_table[SYS_open] = "open";
#endif
#if defined (SYS_close)
  syscall_table[SYS_close] = "close";
#endif
#if defined (SYS_wait)
  syscall_table[SYS_wait] = "wait";
#endif
#if defined (SYS_creat)
  syscall_table[SYS_creat] = "creat";
#endif
#if defined (SYS_link)
  syscall_table[SYS_link] = "link";
#endif
#if defined (SYS_unlink)
  syscall_table[SYS_unlink] = "unlink";
#endif
#if defined (SYS_exec)
  syscall_table[SYS_exec] = "exec";
#endif
#if defined (SYS_execv)
  syscall_table[SYS_execv] = "execv";
#endif
#if defined (SYS_execve)
  syscall_table[SYS_execve] = "execve";
#endif
#if defined (SYS_chdir)
  syscall_table[SYS_chdir] = "chdir";
#endif
#if defined (SYS_time)
  syscall_table[SYS_time] = "time";
#endif
#if defined (SYS_mknod)
  syscall_table[SYS_mknod] = "mknod";
#endif
#if defined (SYS_chmod)
  syscall_table[SYS_chmod] = "chmod";
#endif
#if defined (SYS_chown)
  syscall_table[SYS_chown] = "chown";
#endif
#if defined (SYS_brk)
  syscall_table[SYS_brk] = "brk";
#endif
#if defined (SYS_stat)
  syscall_table[SYS_stat] = "stat";
#endif
#if defined (SYS_lseek)
  syscall_table[SYS_lseek] = "lseek";
#endif
#if defined (SYS_getpid)
  syscall_table[SYS_getpid] = "getpid";
#endif
#if defined (SYS_mount)
  syscall_table[SYS_mount] = "mount";
#endif
#if defined (SYS_umount)
  syscall_table[SYS_umount] = "umount";
#endif
#if defined (SYS_setuid)
  syscall_table[SYS_setuid] = "setuid";
#endif
#if defined (SYS_getuid)
  syscall_table[SYS_getuid] = "getuid";
#endif
#if defined (SYS_stime)
  syscall_table[SYS_stime] = "stime";
#endif
#if defined (SYS_ptrace)
  syscall_table[SYS_ptrace] = "ptrace";
#endif
#if defined (SYS_alarm)
  syscall_table[SYS_alarm] = "alarm";
#endif
#if defined (SYS_fstat)
  syscall_table[SYS_fstat] = "fstat";
#endif
#if defined (SYS_pause)
  syscall_table[SYS_pause] = "pause";
#endif
#if defined (SYS_utime)
  syscall_table[SYS_utime] = "utime";
#endif
#if defined (SYS_stty)
  syscall_table[SYS_stty] = "stty";
#endif
#if defined (SYS_gtty)
  syscall_table[SYS_gtty] = "gtty";
#endif
#if defined (SYS_access)
  syscall_table[SYS_access] = "access";
#endif
#if defined (SYS_nice)
  syscall_table[SYS_nice] = "nice";
#endif
#if defined (SYS_statfs)
  syscall_table[SYS_statfs] = "statfs";
#endif
#if defined (SYS_sync)
  syscall_table[SYS_sync] = "sync";
#endif
#if defined (SYS_kill)
  syscall_table[SYS_kill] = "kill";
#endif
#if defined (SYS_fstatfs)
  syscall_table[SYS_fstatfs] = "fstatfs";
#endif
#if defined (SYS_pgrpsys)
  syscall_table[SYS_pgrpsys] = "pgrpsys";
#endif
#if defined (SYS_xenix)
  syscall_table[SYS_xenix] = "xenix";
#endif
#if defined (SYS_dup)
  syscall_table[SYS_dup] = "dup";
#endif
#if defined (SYS_pipe)
  syscall_table[SYS_pipe] = "pipe";
#endif
#if defined (SYS_times)
  syscall_table[SYS_times] = "times";
#endif
#if defined (SYS_profil)
  syscall_table[SYS_profil] = "profil";
#endif
#if defined (SYS_plock)
  syscall_table[SYS_plock] = "plock";
#endif
#if defined (SYS_setgid)
  syscall_table[SYS_setgid] = "setgid";
#endif
#if defined (SYS_getgid)
  syscall_table[SYS_getgid] = "getgid";
#endif
#if defined (SYS_signal)
  syscall_table[SYS_signal] = "signal";
#endif
#if defined (SYS_msgsys)
  syscall_table[SYS_msgsys] = "msgsys";
#endif
#if defined (SYS_sys3b)
  syscall_table[SYS_sys3b] = "sys3b";
#endif
#if defined (SYS_sysi86)
  syscall_table[SYS_sysi86] = "sysi86";
#endif
#if defined (SYS_acct)
  syscall_table[SYS_acct] = "acct";
#endif
#if defined (SYS_shmsys)
  syscall_table[SYS_shmsys] = "shmsys";
#endif
#if defined (SYS_semsys)
  syscall_table[SYS_semsys] = "semsys";
#endif
#if defined (SYS_ioctl)
  syscall_table[SYS_ioctl] = "ioctl";
#endif
#if defined (SYS_uadmin)
  syscall_table[SYS_uadmin] = "uadmin";
#endif
#if defined (SYS_utssys)
  syscall_table[SYS_utssys] = "utssys";
#endif
#if defined (SYS_fsync)
  syscall_table[SYS_fsync] = "fsync";
#endif
#if defined (SYS_umask)
  syscall_table[SYS_umask] = "umask";
#endif
#if defined (SYS_chroot)
  syscall_table[SYS_chroot] = "chroot";
#endif
#if defined (SYS_fcntl)
  syscall_table[SYS_fcntl] = "fcntl";
#endif
#if defined (SYS_ulimit)
  syscall_table[SYS_ulimit] = "ulimit";
#endif
#if defined (SYS_rfsys)
  syscall_table[SYS_rfsys] = "rfsys";
#endif
#if defined (SYS_rmdir)
  syscall_table[SYS_rmdir] = "rmdir";
#endif
#if defined (SYS_mkdir)
  syscall_table[SYS_mkdir] = "mkdir";
#endif
#if defined (SYS_getdents)
  syscall_table[SYS_getdents] = "getdents";
#endif
#if defined (SYS_sysfs)
  syscall_table[SYS_sysfs] = "sysfs";
#endif
#if defined (SYS_getmsg)
  syscall_table[SYS_getmsg] = "getmsg";
#endif
#if defined (SYS_putmsg)
  syscall_table[SYS_putmsg] = "putmsg";
#endif
#if defined (SYS_poll)
  syscall_table[SYS_poll] = "poll";
#endif
#if defined (SYS_lstat)
  syscall_table[SYS_lstat] = "lstat";
#endif
#if defined (SYS_symlink)
  syscall_table[SYS_symlink] = "symlink";
#endif
#if defined (SYS_readlink)
  syscall_table[SYS_readlink] = "readlink";
#endif
#if defined (SYS_setgroups)
  syscall_table[SYS_setgroups] = "setgroups";
#endif
#if defined (SYS_getgroups)
  syscall_table[SYS_getgroups] = "getgroups";
#endif
#if defined (SYS_fchmod)
  syscall_table[SYS_fchmod] = "fchmod";
#endif
#if defined (SYS_fchown)
  syscall_table[SYS_fchown] = "fchown";
#endif
#if defined (SYS_sigprocmask)
  syscall_table[SYS_sigprocmask] = "sigprocmask";
#endif
#if defined (SYS_sigsuspend)
  syscall_table[SYS_sigsuspend] = "sigsuspend";
#endif
#if defined (SYS_sigaltstack)
  syscall_table[SYS_sigaltstack] = "sigaltstack";
#endif
#if defined (SYS_sigaction)
  syscall_table[SYS_sigaction] = "sigaction";
#endif
#if defined (SYS_sigpending)
  syscall_table[SYS_sigpending] = "sigpending";
#endif
#if defined (SYS_context)
  syscall_table[SYS_context] = "context";
#endif
#if defined (SYS_evsys)
  syscall_table[SYS_evsys] = "evsys";
#endif
#if defined (SYS_evtrapret)
  syscall_table[SYS_evtrapret] = "evtrapret";
#endif
#if defined (SYS_statvfs)
  syscall_table[SYS_statvfs] = "statvfs";
#endif
#if defined (SYS_fstatvfs)
  syscall_table[SYS_fstatvfs] = "fstatvfs";
#endif
#if defined (SYS_nfssys)
  syscall_table[SYS_nfssys] = "nfssys";
#endif
#if defined (SYS_waitsys)
  syscall_table[SYS_waitsys] = "waitsys";
#endif
#if defined (SYS_sigsendsys)
  syscall_table[SYS_sigsendsys] = "sigsendsys";
#endif
#if defined (SYS_hrtsys)
  syscall_table[SYS_hrtsys] = "hrtsys";
#endif
#if defined (SYS_acancel)
  syscall_table[SYS_acancel] = "acancel";
#endif
#if defined (SYS_async)
  syscall_table[SYS_async] = "async";
#endif
#if defined (SYS_priocntlsys)
  syscall_table[SYS_priocntlsys] = "priocntlsys";
#endif
#if defined (SYS_pathconf)
  syscall_table[SYS_pathconf] = "pathconf";
#endif
#if defined (SYS_mincore)
  syscall_table[SYS_mincore] = "mincore";
#endif
#if defined (SYS_mmap)
  syscall_table[SYS_mmap] = "mmap";
#endif
#if defined (SYS_mprotect)
  syscall_table[SYS_mprotect] = "mprotect";
#endif
#if defined (SYS_munmap)
  syscall_table[SYS_munmap] = "munmap";
#endif
#if defined (SYS_fpathconf)
  syscall_table[SYS_fpathconf] = "fpathconf";
#endif
#if defined (SYS_vfork)
  syscall_table[SYS_vfork] = "vfork";
#endif
#if defined (SYS_fchdir)
  syscall_table[SYS_fchdir] = "fchdir";
#endif
#if defined (SYS_readv)
  syscall_table[SYS_readv] = "readv";
#endif
#if defined (SYS_writev)
  syscall_table[SYS_writev] = "writev";
#endif
#if defined (SYS_xstat)
  syscall_table[SYS_xstat] = "xstat";
#endif
#if defined (SYS_lxstat)
  syscall_table[SYS_lxstat] = "lxstat";
#endif
#if defined (SYS_fxstat)
  syscall_table[SYS_fxstat] = "fxstat";
#endif
#if defined (SYS_xmknod)
  syscall_table[SYS_xmknod] = "xmknod";
#endif
#if defined (SYS_clocal)
  syscall_table[SYS_clocal] = "clocal";
#endif
#if defined (SYS_setrlimit)
  syscall_table[SYS_setrlimit] = "setrlimit";
#endif
#if defined (SYS_getrlimit)
  syscall_table[SYS_getrlimit] = "getrlimit";
#endif
#if defined (SYS_lchown)
  syscall_table[SYS_lchown] = "lchown";
#endif
#if defined (SYS_memcntl)
  syscall_table[SYS_memcntl] = "memcntl";
#endif
#if defined (SYS_getpmsg)
  syscall_table[SYS_getpmsg] = "getpmsg";
#endif
#if defined (SYS_putpmsg)
  syscall_table[SYS_putpmsg] = "putpmsg";
#endif
#if defined (SYS_rename)
  syscall_table[SYS_rename] = "rename";
#endif
#if defined (SYS_uname)
  syscall_table[SYS_uname] = "uname";
#endif
#if defined (SYS_setegid)
  syscall_table[SYS_setegid] = "setegid";
#endif
#if defined (SYS_sysconfig)
  syscall_table[SYS_sysconfig] = "sysconfig";
#endif
#if defined (SYS_adjtime)
  syscall_table[SYS_adjtime] = "adjtime";
#endif
#if defined (SYS_systeminfo)
  syscall_table[SYS_systeminfo] = "systeminfo";
#endif
#if defined (SYS_seteuid)
  syscall_table[SYS_seteuid] = "seteuid";
#endif
#if defined (SYS_sproc)
  syscall_table[SYS_sproc] = "sproc";
#endif
#if defined (SYS_keyctl)
  syscall_table[SYS_keyctl] = "keyctl";
#endif
#if defined (SYS_secsys)
  syscall_table[SYS_secsys] = "secsys";
#endif
#if defined (SYS_filepriv)
  syscall_table[SYS_filepriv] = "filepriv";
#endif
#if defined (SYS_procpriv)
  syscall_table[SYS_procpriv] = "procpriv";
#endif
#if defined (SYS_devstat)
  syscall_table[SYS_devstat] = "devstat";
#endif
#if defined (SYS_aclipc)
  syscall_table[SYS_aclipc] = "aclipc";
#endif
#if defined (SYS_fdevstat)
  syscall_table[SYS_fdevstat] = "fdevstat";
#endif
#if defined (SYS_flvlfile)
  syscall_table[SYS_flvlfile] = "flvlfile";
#endif
#if defined (SYS_lvlfile)
  syscall_table[SYS_lvlfile] = "lvlfile";
#endif
#if defined (SYS_lvlequal)
  syscall_table[SYS_lvlequal] = "lvlequal";
#endif
#if defined (SYS_lvlproc)
  syscall_table[SYS_lvlproc] = "lvlproc";
#endif
#if defined (SYS_lvlipc)
  syscall_table[SYS_lvlipc] = "lvlipc";
#endif
#if defined (SYS_acl)
  syscall_table[SYS_acl] = "acl";
#endif
#if defined (SYS_auditevt)
  syscall_table[SYS_auditevt] = "auditevt";
#endif
#if defined (SYS_auditctl)
  syscall_table[SYS_auditctl] = "auditctl";
#endif
#if defined (SYS_auditdmp)
  syscall_table[SYS_auditdmp] = "auditdmp";
#endif
#if defined (SYS_auditlog)
  syscall_table[SYS_auditlog] = "auditlog";
#endif
#if defined (SYS_auditbuf)
  syscall_table[SYS_auditbuf] = "auditbuf";
#endif
#if defined (SYS_lvldom)
  syscall_table[SYS_lvldom] = "lvldom";
#endif
#if defined (SYS_lvlvfs)
  syscall_table[SYS_lvlvfs] = "lvlvfs";
#endif
#if defined (SYS_mkmld)
  syscall_table[SYS_mkmld] = "mkmld";
#endif
#if defined (SYS_mldmode)
  syscall_table[SYS_mldmode] = "mldmode";
#endif
#if defined (SYS_secadvise)
  syscall_table[SYS_secadvise] = "secadvise";
#endif
#if defined (SYS_online)
  syscall_table[SYS_online] = "online";
#endif
#if defined (SYS_setitimer)
  syscall_table[SYS_setitimer] = "setitimer";
#endif
#if defined (SYS_getitimer)
  syscall_table[SYS_getitimer] = "getitimer";
#endif
#if defined (SYS_gettimeofday)
  syscall_table[SYS_gettimeofday] = "gettimeofday";
#endif
#if defined (SYS_settimeofday)
  syscall_table[SYS_settimeofday] = "settimeofday";
#endif
#if defined (SYS_lwp_create)
  syscall_table[SYS_lwp_create] = "_lwp_create";
#endif
#if defined (SYS_lwp_exit)
  syscall_table[SYS_lwp_exit] = "_lwp_exit";
#endif
#if defined (SYS_lwp_wait)
  syscall_table[SYS_lwp_wait] = "_lwp_wait";
#endif
#if defined (SYS_lwp_self)
  syscall_table[SYS_lwp_self] = "_lwp_self";
#endif
#if defined (SYS_lwp_info)
  syscall_table[SYS_lwp_info] = "_lwp_info";
#endif
#if defined (SYS_lwp_private)
  syscall_table[SYS_lwp_private] = "_lwp_private";
#endif
#if defined (SYS_processor_bind)
  syscall_table[SYS_processor_bind] = "processor_bind";
#endif
#if defined (SYS_processor_exbind)
  syscall_table[SYS_processor_exbind] = "processor_exbind";
#endif
#if defined (SYS_prepblock)
  syscall_table[SYS_prepblock] = "prepblock";
#endif
#if defined (SYS_block)
  syscall_table[SYS_block] = "block";
#endif
#if defined (SYS_rdblock)
  syscall_table[SYS_rdblock] = "rdblock";
#endif
#if defined (SYS_unblock)
  syscall_table[SYS_unblock] = "unblock";
#endif
#if defined (SYS_cancelblock)
  syscall_table[SYS_cancelblock] = "cancelblock";
#endif
#if defined (SYS_pread)
  syscall_table[SYS_pread] = "pread";
#endif
#if defined (SYS_pwrite)
  syscall_table[SYS_pwrite] = "pwrite";
#endif
#if defined (SYS_truncate)
  syscall_table[SYS_truncate] = "truncate";
#endif
#if defined (SYS_ftruncate)
  syscall_table[SYS_ftruncate] = "ftruncate";
#endif
#if defined (SYS_lwp_kill)
  syscall_table[SYS_lwp_kill] = "_lwp_kill";
#endif
#if defined (SYS_sigwait)
  syscall_table[SYS_sigwait] = "sigwait";
#endif
#if defined (SYS_fork1)
  syscall_table[SYS_fork1] = "fork1";
#endif
#if defined (SYS_forkall)
  syscall_table[SYS_forkall] = "forkall";
#endif
#if defined (SYS_modload)
  syscall_table[SYS_modload] = "modload";
#endif
#if defined (SYS_moduload)
  syscall_table[SYS_moduload] = "moduload";
#endif
#if defined (SYS_modpath)
  syscall_table[SYS_modpath] = "modpath";
#endif
#if defined (SYS_modstat)
  syscall_table[SYS_modstat] = "modstat";
#endif
#if defined (SYS_modadm)
  syscall_table[SYS_modadm] = "modadm";
#endif
#if defined (SYS_getksym)
  syscall_table[SYS_getksym] = "getksym";
#endif
#if defined (SYS_lwp_suspend)
  syscall_table[SYS_lwp_suspend] = "_lwp_suspend";
#endif
#if defined (SYS_lwp_continue)
  syscall_table[SYS_lwp_continue] = "_lwp_continue";
#endif
#if defined (SYS_priocntllst)
  syscall_table[SYS_priocntllst] = "priocntllst";
#endif
#if defined (SYS_sleep)
  syscall_table[SYS_sleep] = "sleep";
#endif
#if defined (SYS_lwp_sema_wait)
  syscall_table[SYS_lwp_sema_wait] = "_lwp_sema_wait";
#endif
#if defined (SYS_lwp_sema_post)
  syscall_table[SYS_lwp_sema_post] = "_lwp_sema_post";
#endif
#if defined (SYS_lwp_sema_trywait)
  syscall_table[SYS_lwp_sema_trywait] = "lwp_sema_trywait";
#endif
#if defined(SYS_fstatvfs64)
  syscall_table[SYS_fstatvfs64] = "fstatvfs64";
#endif
#if defined(SYS_statvfs64)
  syscall_table[SYS_statvfs64] = "statvfs64";
#endif
#if defined(SYS_ftruncate64)
  syscall_table[SYS_ftruncate64] = "ftruncate64";
#endif
#if defined(SYS_truncate64)
  syscall_table[SYS_truncate64] = "truncate64";
#endif
#if defined(SYS_getrlimit64)
  syscall_table[SYS_getrlimit64] = "getrlimit64";
#endif
#if defined(SYS_setrlimit64)
  syscall_table[SYS_setrlimit64] = "setrlimit64";
#endif
#if defined(SYS_lseek64)
  syscall_table[SYS_lseek64] = "lseek64";
#endif
#if defined(SYS_mmap64)
  syscall_table[SYS_mmap64] = "mmap64";
#endif
#if defined(SYS_pread64)
  syscall_table[SYS_pread64] = "pread64";
#endif
#if defined(SYS_creat64)
  syscall_table[SYS_creat64] = "creat64";
#endif
#if defined(SYS_dshmsys)
  syscall_table[SYS_dshmsys] = "dshmsys";
#endif
#if defined(SYS_invlpg)
  syscall_table[SYS_invlpg] = "invlpg";
#endif
#if defined(SYS_cg_ids)
  syscall_table[SYS_cg_ids] = "cg_ids";
#endif
#if defined(SYS_cg_processors)
  syscall_table[SYS_cg_processors] = "cg_processors";
#endif
#if defined(SYS_cg_info)
  syscall_table[SYS_cg_info] = "cg_info";
#endif
#if defined(SYS_cg_bind)
  syscall_table[SYS_cg_bind] = "cg_bind";
#endif
#if defined(SYS_cg_current)
  syscall_table[SYS_cg_current] = "cg_current";
#endif
#if defined(SYS_cg_memloc)
  syscall_table[SYS_cg_memloc] = "cg_memloc";
#endif
}

/*

LOCAL FUNCTION

	procfs_kill_inferior - kill any currently inferior

SYNOPSIS

	void procfs_kill_inferior (void)

DESCRIPTION

	Kill any current inferior.

NOTES

	Kills even attached inferiors.  Presumably the user has already
	been prompted that the inferior is an attached one rather than
	one started by gdb.  (FIXME?)

*/

static void
procfs_kill_inferior ()
{
  target_mourn_inferior ();
}

/*

LOCAL FUNCTION

	unconditionally_kill_inferior - terminate the inferior

SYNOPSIS

	static void unconditionally_kill_inferior (struct procinfo *)

DESCRIPTION

	Kill the specified inferior.

NOTE

	A possibly useful enhancement would be to first try sending
	the inferior a terminate signal, politely asking it to commit
	suicide, before we murder it (we could call that
	politely_kill_inferior()).

*/

static void
unconditionally_kill_inferior (pi)
     struct procinfo *pi;
{
  int ppid;
  struct proc_ctl pctl;
  
  ppid = pi->prstatus.pr_ppid;

#ifdef PROCFS_NEED_CLEAR_CURSIG_FOR_KILL
  /* Alpha OSF/1-3.x procfs needs a clear of the current signal
     before the PIOCKILL, otherwise it might generate a corrupted core
     file for the inferior.  */
  ioctl (pi->ctl_fd, PIOCSSIG, NULL);
#endif
#ifdef PROCFS_NEED_PIOCSSIG_FOR_KILL
  /* Alpha OSF/1-2.x procfs needs a PIOCSSIG call with a SIGKILL signal
     to kill the inferior, otherwise it might remain stopped with a
     pending SIGKILL.
     We do not check the result of the PIOCSSIG, the inferior might have
     died already.  */
  {
    struct siginfo newsiginfo;

    memset ((char *) &newsiginfo, 0, sizeof (newsiginfo));
    newsiginfo.si_signo = SIGKILL;
    newsiginfo.si_code = 0;
    newsiginfo.si_errno = 0;
    newsiginfo.si_pid = getpid ();
    newsiginfo.si_uid = getuid ();
    ioctl (pi->ctl_fd, PIOCSSIG, &newsiginfo);
  }
#else /* PROCFS_NEED_PIOCSSIG_FOR_KILL */
  procfs_write_pckill (pi);
#endif /* PROCFS_NEED_PIOCSSIG_FOR_KILL */

  close_proc_file (pi);

/* Only wait() for our direct children.  Our grandchildren zombies are killed
   by the death of their parents.  */

  if (ppid == getpid())
    wait ((int *) 0);
}

/*

LOCAL FUNCTION

	procfs_xfer_memory -- copy data to or from inferior memory space

SYNOPSIS

	int procfs_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len,
		int dowrite, struct target_ops target)

DESCRIPTION

	Copy LEN bytes to/from inferior's memory starting at MEMADDR
	from/to debugger memory starting at MYADDR.  Copy from inferior
	if DOWRITE is zero or to inferior if DOWRITE is nonzero.
  
	Returns the length copied, which is either the LEN argument or
	zero.  This xfer function does not do partial moves, since procfs_ops
	doesn't allow memory operations to cross below us in the target stack
	anyway.

NOTES

	The /proc interface makes this an almost trivial task.
 */

static int
procfs_xfer_memory (memaddr, myaddr, len, dowrite, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int dowrite;
     struct target_ops *target; /* ignored */
{
  int nbytes = 0;
  struct procinfo *pi;

  pi = current_procinfo;

  if (lseek(pi->as_fd, (off_t) memaddr, SEEK_SET) == (off_t) memaddr)
    {
      if (dowrite)
	{
	  nbytes = write (pi->as_fd, myaddr, len);
	}
      else
	{
	  nbytes = read (pi->as_fd, myaddr, len);
	}
      if (nbytes < 0)
	{
	  nbytes = 0;
	}
    }
  return (nbytes);
}

/*

LOCAL FUNCTION

	procfs_store_registers -- copy register values back to inferior

SYNOPSIS

	void procfs_store_registers (int regno)

DESCRIPTION

	Store our current register values back into the inferior.  If
	REGNO is -1 then store all the register, otherwise store just
	the value specified by REGNO.

NOTES

	If we are storing only a single register, we first have to get all
	the current values from the process, overwrite the desired register
	in the gregset with the one we want from gdb's registers, and then
	send the whole set back to the process.  For writing all the
	registers, all we have to do is generate the gregset and send it to
	the process.

	Also note that the process has to be stopped on an event of interest
	for this to work, which basically means that it has to have been
	run under the control of one of the other /proc ioctl calls and not
	ptrace.  Since we don't use ptrace anyway, we don't worry about this
	fine point, but it is worth noting for future reference.

	Gdb is confused about what this function is supposed to return.
	Some versions return a value, others return nothing.  Some are
	declared to return a value and actually return nothing.  Gdb ignores
	anything returned.  (FIXME)

 */

static void
procfs_store_registers (regno)
     int regno;
{
  struct procinfo *pi;
#ifdef PROCFS_USE_READ_WRITE
  struct greg_ctl greg;
  struct fpreg_ctl fpreg;
#endif

  pi = current_procinfo;

#ifdef PROCFS_USE_READ_WRITE
  if (regno != -1)
    {
      procfs_read_status (pi);
      memcpy ((char *) &greg.gregset,
         (char *) &pi->prstatus.pr_lwp.pr_context.uc_mcontext.gregs,
         sizeof (gdb_gregset_t));
    }
  fill_gregset (&greg.gregset, regno);
  greg.cmd = PCSREG;
  write (pi->ctl_fd, &greg, sizeof (greg));
#else /* PROCFS_USE_READ_WRITE */
  if (regno != -1)
    {
      ioctl (pi->ctl_fd, PIOCGREG, &pi->gregset.gregset);
    }
  fill_gregset (&pi->gregset.gregset, regno);
  ioctl (pi->ctl_fd, PIOCSREG, &pi->gregset.gregset);
#endif /* PROCFS_USE_READ_WRITE */

#if defined (FP0_REGNUM)

  /* Now repeat everything using the floating point register set, if the
     target has floating point hardware. Since we ignore the returned value,
     we'll never know whether it worked or not anyway. */

#ifdef PROCFS_USE_READ_WRITE
  if (regno != -1)
    {
      procfs_read_status (pi);
      memcpy ((char *) &fpreg.fpregset,
          (char *) &pi->prstatus.pr_lwp.pr_context.uc_mcontext.fpregs,
          sizeof (gdb_fpregset_t));
    }
  fill_fpregset (&fpreg.fpregset, regno);
  fpreg.cmd = PCSFPREG;
  write (pi->ctl_fd, &fpreg, sizeof (fpreg));
#else /* PROCFS_USE_READ_WRITE */
  if (regno != -1)
    {
      ioctl (pi->ctl_fd, PIOCGFPREG, &pi->fpregset.fpregset);
    }
  fill_fpregset (&pi->fpregset.fpregset, regno);
  ioctl (pi->ctl_fd, PIOCSFPREG, &pi->fpregset.fpregset);
#endif /* PROCFS_USE_READ_WRITE */

#endif	/* FP0_REGNUM */

}

/*

LOCAL FUNCTION

	init_procinfo - setup a procinfo struct and connect it to a process

SYNOPSIS

	struct procinfo * init_procinfo (int pid)

DESCRIPTION

	Allocate a procinfo structure, open the /proc file and then set up the
	set of signals and faults that are to be traced.  Returns a pointer to
	the new procinfo structure.  

NOTES

	If proc_init_failed ever gets called, control returns to the command
	processing loop via the standard error handling code.

 */

static struct procinfo *
init_procinfo (pid, kill)
     int pid;
     int kill;
{
  struct procinfo *pi = (struct procinfo *) 
    xmalloc (sizeof (struct procinfo));
  struct sig_ctl  sctl;
  struct flt_ctl  fctl;

  memset ((char *) pi, 0, sizeof (*pi));
  if (!open_proc_file (pid, pi, O_RDWR, 1))
    proc_init_failed (pi, "can't open process file", kill);

  /* open_proc_file may modify pid.  */

  pid = pi -> pid;

  /* Add new process to process info list */

  pi->next = procinfo_list;
  procinfo_list = pi;

  add_fd (pi);			/* Add to list for poll/select */

  /*  Remember some things about the inferior that we will, or might, change
      so that we can restore them when we detach. */
#ifdef UNIXWARE
  memcpy ((char *) &pi->saved_trace.sigset,
	  (char *) &pi->prstatus.pr_sigtrace, sizeof (sigset_t));
  memcpy ((char *) &pi->saved_fltset.fltset,
	  (char *) &pi->prstatus.pr_flttrace, sizeof (fltset_t));
  memcpy ((char *) &pi->saved_entryset.sysset,
	  (char *) &pi->prstatus.pr_sysentry, sizeof (sysset_t));
  memcpy ((char *) &pi->saved_exitset.sysset,
	  (char *) &pi->prstatus.pr_sysexit, sizeof (sysset_t));

  /* Set up trace and fault sets, as gdb expects them. */

  prfillset (&sctl.sigset);
  notice_signals (pi, &sctl);
  prfillset (&fctl.fltset);
  prdelset (&fctl.fltset, FLTPAGE);

#else /* ! UNIXWARE */
  ioctl (pi->ctl_fd, PIOCGTRACE, &pi->saved_trace.sigset);
  ioctl (pi->ctl_fd, PIOCGHOLD, &pi->saved_sighold.sigset);
  ioctl (pi->ctl_fd, PIOCGFAULT, &pi->saved_fltset.fltset);
  ioctl (pi->ctl_fd, PIOCGENTRY, &pi->saved_entryset.sysset);
  ioctl (pi->ctl_fd, PIOCGEXIT, &pi->saved_exitset.sysset);
  
  /* Set up trace and fault sets, as gdb expects them. */
  
  memset ((char *) &pi->prrun, 0, sizeof (pi->prrun));
  prfillset (&pi->prrun.pr_trace);
  procfs_notice_signals (pid);
  prfillset (&pi->prrun.pr_fault);
  prdelset (&pi->prrun.pr_fault, FLTPAGE);
#ifdef PROCFS_DONT_TRACE_FAULTS
  premptyset (&pi->prrun.pr_fault);
#endif
#endif /* UNIXWARE */

  if (!procfs_read_status (pi))
    proc_init_failed (pi, "procfs_read_status failed", kill);

  return pi;
}

/*

LOCAL FUNCTION

	create_procinfo - initialize access to a /proc entry

SYNOPSIS

	struct procinfo * create_procinfo (int pid)

DESCRIPTION

	Allocate a procinfo structure, open the /proc file and then set up the
	set of signals and faults that are to be traced.  Returns a pointer to
	the new procinfo structure.

NOTES

	If proc_init_failed ever gets called, control returns to the command
	processing loop via the standard error handling code.

 */

static struct procinfo *
create_procinfo (pid)
     int pid;
{
  struct procinfo *pi;
  struct sig_ctl  sctl;
  struct flt_ctl  fctl;

  pi = find_procinfo (pid, 1);
  if (pi != NULL)
    return pi;			/* All done!  It already exists */

  pi = init_procinfo (pid, 1);

#ifndef UNIXWARE
/* A bug in Solaris (2.5 at least) causes PIOCWSTOP to hang on LWPs that are
   already stopped, even if they all have PR_ASYNC set.  */
  if (!(pi->prstatus.pr_flags & PR_STOPPED))
#endif
    if (!procfs_write_pcwstop (pi))
      proc_init_failed (pi, "procfs_write_pcwstop failed", 1);

#ifdef PROCFS_USE_READ_WRITE
  fctl.cmd = PCSFAULT;
  if (write (pi->ctl_fd, (char *) &fctl, sizeof (struct flt_ctl)) < 0)
    proc_init_failed (pi, "PCSFAULT failed", 1);
#else
  if (ioctl (pi->ctl_fd, PIOCSFAULT, &pi->prrun.pr_fault) < 0)
    proc_init_failed (pi, "PIOCSFAULT failed", 1);
#endif

  return pi;
}

/*

LOCAL FUNCTION

	procfs_exit_handler - handle entry into the _exit syscall

SYNOPSIS

	int procfs_exit_handler (pi, syscall_num, why, rtnvalp, statvalp)

DESCRIPTION

	This routine is called when an inferior process enters the _exit()
	system call.  It continues the process, and then collects the exit
	status and pid which are returned in *statvalp and *rtnvalp.  After
	that it returns non-zero to indicate that procfs_wait should wake up.

NOTES
	There is probably a better way to do this.

 */

static int
procfs_exit_handler (pi, syscall_num, why, rtnvalp, statvalp)
     struct procinfo *pi;
     int syscall_num;
     int why;
     int *rtnvalp;
     int *statvalp;
{
  struct procinfo *temp_pi, *next_pi;
  struct proc_ctl pctl;

#ifdef UNIXWARE
  pctl.cmd = PCRUN;
  pctl.data = PRCFAULT;
#else
  pi->prrun.pr_flags = PRCFAULT;
#endif

#ifdef PROCFS_USE_READ_WRITE
  if (write (pi->ctl_fd, (char *)&pctl, sizeof (struct proc_ctl)) < 0)
#else
  if (ioctl (pi->ctl_fd, PIOCRUN, &pi->prrun) != 0)
#endif
    perror_with_name (pi->pathname);

  if (attach_flag)
    {
      /* Claim it exited (don't call wait). */
      if (info_verbose)
	printf_filtered ("(attached process has exited)\n");
      *statvalp = 0;
      *rtnvalp  = inferior_pid;
    }
  else
    {
      *rtnvalp = wait (statvalp);
      if (*rtnvalp >= 0)
	*rtnvalp = pi->pid;
    }

  /* Close ALL open proc file handles,
     except the one that called SYS_exit. */
  for (temp_pi = procinfo_list; temp_pi; temp_pi = next_pi)
    {
      next_pi = temp_pi->next;
      if (temp_pi == pi)
	continue;		/* Handled below */
      close_proc_file (temp_pi);
    }
  return 1;
}

/*

LOCAL FUNCTION

	procfs_exec_handler - handle exit from the exec family of syscalls

SYNOPSIS

	int procfs_exec_handler (pi, syscall_num, why, rtnvalp, statvalp)

DESCRIPTION

	This routine is called when an inferior process is about to finish any
	of the exec() family of	system calls.  It pretends that we got a
	SIGTRAP (for compatibility with ptrace behavior), and returns non-zero
	to tell procfs_wait to wake up.

NOTES
	This need for compatibility with ptrace is questionable.  In the
	future, it shouldn't be necessary.

 */

static int
procfs_exec_handler (pi, syscall_num, why, rtnvalp, statvalp)
     struct procinfo *pi;
     int syscall_num;
     int why;
     int *rtnvalp;
     int *statvalp;
{
  *statvalp = (SIGTRAP << 8) | 0177;

  return 1;
}

#if defined(SYS_sproc) && !defined(UNIXWARE)
/* IRIX lwp creation system call */

/*

LOCAL FUNCTION

	procfs_sproc_handler - handle exit from the sproc syscall

SYNOPSIS

	int procfs_sproc_handler (pi, syscall_num, why, rtnvalp, statvalp)

DESCRIPTION

	This routine is called when an inferior process is about to finish an
	sproc() system call.  This is the system call that IRIX uses to create
	a lightweight process.  When the target process gets this event, we can
	look at rval1 to find the new child processes ID, and create a new
	procinfo struct from that.

	After that, it pretends that we got a SIGTRAP, and returns non-zero
	to tell procfs_wait to wake up.  Subsequently, wait_for_inferior gets
	woken up, sees the new process and continues it.

NOTES
	We actually never see the child exiting from sproc because we will
	shortly stop the child with PIOCSTOP, which is then registered as the
	event of interest.
 */

static int
procfs_sproc_handler (pi, syscall_num, why, rtnvalp, statvalp)
     struct procinfo *pi;
     int syscall_num;
     int why;
     int *rtnvalp;
     int *statvalp;
{
/* We've just detected the completion of an sproc system call.  Now we need to
   setup a procinfo struct for this thread, and notify the thread system of the
   new arrival.  */

/* If sproc failed, then nothing interesting happened.  Continue the process
   and go back to sleep. */

  if (pi->prstatus.pr_errno != 0)
    {
      pi->prrun.pr_flags &= PRSTEP;
      pi->prrun.pr_flags |= PRCFAULT;

      if (ioctl (pi->ctl_fd, PIOCRUN, &pi->prrun) != 0)
	perror_with_name (pi->pathname);

      return 0;
    }

  /* At this point, the new thread is stopped at it's first instruction, and
     the parent is stopped at the exit from sproc.  */

  /* Notify the caller of the arrival of a new thread. */
  create_procinfo (pi->prstatus.pr_rval1);

  *rtnvalp = pi->prstatus.pr_rval1;
  *statvalp = (SIGTRAP << 8) | 0177;

  return 1;
}

/*

LOCAL FUNCTION

	procfs_fork_handler - handle exit from the fork syscall

SYNOPSIS

	int procfs_fork_handler (pi, syscall_num, why, rtnvalp, statvalp)

DESCRIPTION

	This routine is called when an inferior process is about to finish a
	fork() system call.  We will open up the new process, and then close
	it, which releases it from the clutches of the debugger.

	After that, we continue the target process as though nothing had
	happened.

NOTES
	This is necessary for IRIX because we have to set PR_FORK in order
	to catch the creation of lwps (via sproc()).  When an actual fork
	occurs, it becomes necessary to reset the forks debugger flags and
	continue it because we can't hack multiple processes yet.
 */

static int
procfs_fork_handler (pi, syscall_num, why, rtnvalp, statvalp)
     struct procinfo *pi;
     int syscall_num;
     int why;
     int *rtnvalp;
     int *statvalp;
{
  struct procinfo *pitemp;

/* At this point, we've detected the completion of a fork (or vfork) call in
   our child.  The grandchild is also stopped because we set inherit-on-fork
   earlier.  (Note that nobody has the grandchilds' /proc file open at this
   point.)  We will release the grandchild from the debugger by opening it's
   /proc file and then closing it.  Since run-on-last-close is set, the
   grandchild continues on its' merry way.  */


  pitemp = create_procinfo (pi->prstatus.pr_rval1);
  if (pitemp)
    close_proc_file (pitemp);

  if (ioctl (pi->ctl_fd, PIOCRUN, &pi->prrun) != 0)
    perror_with_name (pi->pathname);

  return 0;
}
#endif /* SYS_sproc && !UNIXWARE */

/*

LOCAL FUNCTION

	procfs_set_inferior_syscall_traps - setup the syscall traps 

SYNOPSIS

	void procfs_set_inferior_syscall_traps (struct procinfo *pip)

DESCRIPTION

	Called for each "procinfo" (process, thread, or LWP) in the
	inferior, to register for notification of and handlers for
	syscall traps in the inferior.

 */

static void
procfs_set_inferior_syscall_traps (pip)
     struct procinfo *pip;
{
  procfs_set_syscall_trap (pip, SYS_exit, PROCFS_SYSCALL_ENTRY,
			   procfs_exit_handler);

#ifndef PRFS_STOPEXEC
#ifdef SYS_exec
  procfs_set_syscall_trap (pip, SYS_exec, PROCFS_SYSCALL_EXIT,
			   procfs_exec_handler);
#endif
#ifdef SYS_execv
  procfs_set_syscall_trap (pip, SYS_execv, PROCFS_SYSCALL_EXIT,
			   procfs_exec_handler);
#endif
#ifdef SYS_execve
  procfs_set_syscall_trap (pip, SYS_execve, PROCFS_SYSCALL_EXIT,
			   procfs_exec_handler);
#endif
#endif  /* PRFS_STOPEXEC */

  /* Setup traps on exit from sproc() */

#ifdef SYS_sproc
  procfs_set_syscall_trap (pip, SYS_sproc, PROCFS_SYSCALL_EXIT,
			   procfs_sproc_handler);
  procfs_set_syscall_trap (pip, SYS_fork, PROCFS_SYSCALL_EXIT,
			   procfs_fork_handler);
#ifdef SYS_vfork
  procfs_set_syscall_trap (pip, SYS_vfork, PROCFS_SYSCALL_EXIT,
			   procfs_fork_handler);
#endif
/* Turn on inherit-on-fork flag so that all children of the target process
   start with tracing flags set.  This allows us to trap lwp creation.  Note
   that we also have to trap on fork and vfork in order to disable all tracing
   in the targets child processes.  */

  modify_inherit_on_fork_flag (pip->ctl_fd, 1);
#endif

#ifdef SYS_lwp_create
  procfs_set_syscall_trap (pip, SYS_lwp_create, PROCFS_SYSCALL_EXIT,
			   procfs_lwp_creation_handler);
#endif
}

/*

LOCAL FUNCTION

	procfs_init_inferior - initialize target vector and access to a
	/proc entry

SYNOPSIS

	void procfs_init_inferior (int pid)

DESCRIPTION

	When gdb starts an inferior, this function is called in the parent
	process immediately after the fork.  It waits for the child to stop
	on the return from the exec system call (the child itself takes care
	of ensuring that this is set up), then sets up the set of signals
	and faults that are to be traced.  Returns the pid, which may have had
	the thread-id added to it.

NOTES

	If proc_init_failed ever gets called, control returns to the command
	processing loop via the standard error handling code.

 */

static void 
procfs_init_inferior (pid)
     int pid;
{
  struct procinfo *pip;

  push_target (&procfs_ops);

  pip = create_procinfo (pid);

  procfs_set_inferior_syscall_traps (pip);

  /* create_procinfo may change the pid, so we have to update inferior_pid
     here before calling other gdb routines that need the right pid.  */

  pid = pip -> pid;
  inferior_pid = pid;

  add_thread (pip -> pid);	/* Setup initial thread */

#ifdef START_INFERIOR_TRAPS_EXPECTED
  startup_inferior (START_INFERIOR_TRAPS_EXPECTED);
#else
  /* One trap to exec the shell, one to exec the program being debugged.  */
  startup_inferior (2);
#endif
}

/*

GLOBAL FUNCTION

	procfs_notice_signals

SYNOPSIS

	static void procfs_notice_signals (int pid);

DESCRIPTION

	When the user changes the state of gdb's signal handling via the
	"handle" command, this function gets called to see if any change
	in the /proc interface is required.  It is also called internally
	by other /proc interface functions to initialize the state of
	the traced signal set.

	One thing it does is that signals for which the state is "nostop",
	"noprint", and "pass", have their trace bits reset in the pr_trace
	field, so that they are no longer traced.  This allows them to be
	delivered directly to the inferior without the debugger ever being
	involved.
 */

static void
procfs_notice_signals (pid)
     int pid;
{
  struct procinfo *pi;
  struct sig_ctl  sctl;

  pi = find_procinfo (pid, 0);

#ifndef HAVE_PRRUN_T
  premptyset (&sctl.sigset);
#else
  sctl.sigset = pi->prrun.pr_trace;
#endif

  notice_signals (pi, &sctl);

#ifdef HAVE_PRRUN_T
  pi->prrun.pr_trace = sctl.sigset;
#endif
}

static void
notice_signals (pi, sctl)
	struct procinfo *pi;
	struct sig_ctl *sctl;
{
  int signo;

  for (signo = 0; signo < NSIG; signo++)
    {
      if (signal_stop_state (target_signal_from_host (signo)) == 0 &&
	  signal_print_state (target_signal_from_host (signo)) == 0 &&
	  signal_pass_state (target_signal_from_host (signo)) == 1)
	{
	  prdelset (&sctl->sigset, signo);
	}
      else
	{
	  praddset (&sctl->sigset, signo);
	}
    }
#ifdef PROCFS_USE_READ_WRITE
  sctl->cmd = PCSTRACE;
  if (write (pi->ctl_fd, (char *) sctl, sizeof (struct sig_ctl)) < 0)
#else
  if (ioctl (pi->ctl_fd, PIOCSTRACE, &sctl->sigset))
#endif
    {
      print_sys_errmsg ("PIOCSTRACE failed", errno);
    }
}

/*

LOCAL FUNCTION

	proc_set_exec_trap -- arrange for exec'd child to halt at startup

SYNOPSIS

	void proc_set_exec_trap (void)

DESCRIPTION

	This function is called in the child process when starting up
	an inferior, prior to doing the exec of the actual inferior.
	It sets the child process's exitset to make exit from the exec
	system call an event of interest to stop on, and then simply
	returns.  The child does the exec, the system call returns, and
	the child stops at the first instruction, ready for the gdb
	parent process to take control of it.

NOTE

	We need to use all local variables since the child may be sharing
	it's data space with the parent, if vfork was used rather than
	fork.

	Also note that we want to turn off the inherit-on-fork flag in
	the child process so that any grand-children start with all
	tracing flags cleared.
 */

static void
proc_set_exec_trap ()
{
  struct sys_ctl exitset;
  struct sys_ctl entryset;
  char procname[MAX_PROC_NAME_SIZE];
  int fd;
  
  sprintf (procname, CTL_PROC_NAME_FMT, getpid ());
#ifdef UNIXWARE
  if ((fd = open (procname, O_WRONLY)) < 0)
#else
  if ((fd = open (procname, O_RDWR)) < 0)
#endif
    {
      perror (procname);
      gdb_flush (gdb_stderr);
      _exit (127);
    }
  premptyset (&exitset.sysset);
  premptyset (&entryset.sysset);

#ifdef PRFS_STOPEXEC
  /* Under Alpha OSF/1 we have to use a PIOCSSPCACT ioctl to trace
     exits from exec system calls because of the user level loader.  */
  {
    int prfs_flags;

    if (ioctl (fd, PIOCGSPCACT, &prfs_flags) < 0)
      {
	perror (procname);
	gdb_flush (gdb_stderr);
	_exit (127);
      }
    prfs_flags |= PRFS_STOPEXEC;
    if (ioctl (fd, PIOCSSPCACT, &prfs_flags) < 0)
      {
	perror (procname);
	gdb_flush (gdb_stderr);
	_exit (127);
      }
  }
#else /* PRFS_STOPEXEC */
  /* GW: Rationale...
     Not all systems with /proc have all the exec* syscalls with the same
     names.  On the SGI, for example, there is no SYS_exec, but there
     *is* a SYS_execv.  So, we try to account for that. */

#ifdef SYS_exec
  praddset (&exitset.sysset, SYS_exec);
#endif
#ifdef SYS_execve
  praddset (&exitset.sysset, SYS_execve);
#endif
#ifdef SYS_execv
  praddset (&exitset.sysset, SYS_execv);
#endif

#ifdef PROCFS_USE_READ_WRITE
  exitset.cmd = PCSEXIT;
  if (write (fd, (char *) &exitset, sizeof (struct sys_ctl)) < 0)
#else
  if (ioctl (fd, PIOCSEXIT, &exitset.sysset) < 0)
#endif
    {
      perror (procname);
      gdb_flush (gdb_stderr);
      _exit (127);
    }
#endif /* PRFS_STOPEXEC */

  praddset (&entryset.sysset, SYS_exit);

#ifdef PROCFS_USE_READ_WRITE
  entryset.cmd = PCSENTRY;
  if (write (fd, (char *) &entryset, sizeof (struct sys_ctl)) < 0)
#else
  if (ioctl (fd, PIOCSENTRY, &entryset.sysset) < 0)
#endif
    {
      perror (procname);
      gdb_flush (gdb_stderr);
      _exit (126);
    }

  /* Turn off inherit-on-fork flag so that all grand-children of gdb
     start with tracing flags cleared. */

  modify_inherit_on_fork_flag (fd, 0);

  /* Turn on run-on-last-close flag so that this process will not hang
     if GDB goes away for some reason.  */

  modify_run_on_last_close_flag (fd, 1);

#ifndef UNIXWARE	/* since this is a solaris-ism, we don't want it */
			/* NOTE: revisit when doing thread support for UW */
#ifdef PR_ASYNC
  {
    long pr_flags;
    struct proc_ctl pctl;

/* Solaris needs this to make procfs treat all threads seperately.  Without
   this, all threads halt whenever something happens to any thread.  Since
   GDB wants to control all this itself, it needs to set PR_ASYNC.  */

    pr_flags = PR_ASYNC;
#ifdef PROCFS_USE_READ_WRITE
    pctl.cmd = PCSET;
    pctl.data = PR_FORK|PR_ASYNC;
    write (fd, (char *) &pctl, sizeof (struct proc_ctl));
#else
    ioctl (fd, PIOCSET, &pr_flags);
#endif
  }
#endif	/* PR_ASYNC */
#endif	/* !UNIXWARE */
}

/*

GLOBAL FUNCTION

	proc_iterate_over_mappings -- call function for every mapped space

SYNOPSIS

	int proc_iterate_over_mappings (int (*func)())

DESCRIPTION

	Given a pointer to a function, call that function for every
	mapped address space, passing it an open file descriptor for
	the file corresponding to that mapped address space (if any)
	and the base address of the mapped space.  Quit when we hit
	the end of the mappings or the function returns nonzero.
 */

#ifdef UNIXWARE
int
proc_iterate_over_mappings (func)
     int (*func) PARAMS ((int, CORE_ADDR));
{
  int nmap;
  int fd;
  int funcstat = 0;
  prmap_t *prmaps;
  prmap_t *prmap;
  struct procinfo *pi;
  struct stat sbuf;

  pi = current_procinfo;

  if (fstat (pi->map_fd, &sbuf) < 0)
    return 0;

  nmap = sbuf.st_size / sizeof (prmap_t);
  prmaps = (prmap_t *) alloca (nmap * sizeof(prmap_t));
  if ((lseek (pi->map_fd, 0, SEEK_SET) == 0) &&
	(read (pi->map_fd, (char *) prmaps, nmap * sizeof (prmap_t)) ==
	(nmap * sizeof (prmap_t))))
    {
      int i = 0;
      for (prmap = prmaps; i < nmap && funcstat == 0; ++prmap, ++i)
        {
          char name[sizeof ("/proc/1234567890/object") +
		sizeof (prmap->pr_mapname)];
          sprintf (name, "/proc/%d/object/%s", pi->pid, prmap->pr_mapname);
          if ((fd = open (name, O_RDONLY)) == -1)
            {
              funcstat = 1;
              break;
            }
          funcstat = (*func) (fd, (CORE_ADDR) prmap->pr_vaddr);
          close (fd);
        }
    }
  return (funcstat);
}
#else /* UNIXWARE */
int
proc_iterate_over_mappings (func)
     int (*func) PARAMS ((int, CORE_ADDR));
{
  int nmap;
  int fd;
  int funcstat = 0;
  struct prmap *prmaps;
  struct prmap *prmap;
  struct procinfo *pi;

  pi = current_procinfo;

  if (ioctl (pi->map_fd, PIOCNMAP, &nmap) == 0)
    {
      prmaps = (struct prmap *) alloca ((nmap + 1) * sizeof (*prmaps));
      if (ioctl (pi->map_fd, PIOCMAP, prmaps) == 0)
	{
	  for (prmap = prmaps; prmap -> pr_size && funcstat == 0; ++prmap)
	    {
	      fd = proc_address_to_fd (pi, (CORE_ADDR) prmap -> pr_vaddr, 0);
	      funcstat = (*func) (fd, (CORE_ADDR) prmap -> pr_vaddr);
	      close (fd);
	    }
	}
    }
  return (funcstat);
}
#endif /* UNIXWARE */

#if 0	/* Currently unused */
/*

GLOBAL FUNCTION

	proc_base_address -- find base address for segment containing address

SYNOPSIS

	CORE_ADDR proc_base_address (CORE_ADDR addr)

DESCRIPTION

	Given an address of a location in the inferior, find and return
	the base address of the mapped segment containing that address.

	This is used for example, by the shared library support code,
	where we have the pc value for some location in the shared library
	where we are stopped, and need to know the base address of the
	segment containing that address.
*/

CORE_ADDR
proc_base_address (addr)
     CORE_ADDR addr;
{
  int nmap;
  struct prmap *prmaps;
  struct prmap *prmap;
  CORE_ADDR baseaddr = 0;
  struct procinfo *pi;

  pi = current_procinfo;

  if (ioctl (pi->map_fd, PIOCNMAP, &nmap) == 0)
    {
      prmaps = (struct prmap *) alloca ((nmap + 1) * sizeof (*prmaps));
      if (ioctl (pi->map_fd, PIOCMAP, prmaps) == 0)
	{
	  for (prmap = prmaps; prmap -> pr_size; ++prmap)
	    {
	      if ((prmap -> pr_vaddr <= (caddr_t) addr) &&
		  (prmap -> pr_vaddr + prmap -> pr_size > (caddr_t) addr))
		{
		  baseaddr = (CORE_ADDR) prmap -> pr_vaddr;
		  break;
		}
	    }
	}
    }
  return (baseaddr);
}

#endif	/* 0 */

#ifndef UNIXWARE
/*

LOCAL FUNCTION

	proc_address_to_fd -- return open fd for file mapped to address

SYNOPSIS

	int proc_address_to_fd (struct procinfo *pi, CORE_ADDR addr, complain)

DESCRIPTION

	Given an address in the current inferior's address space, use the
	/proc interface to find an open file descriptor for the file that
	this address was mapped in from.  Return -1 if there is no current
	inferior.  Print a warning message if there is an inferior but
	the address corresponds to no file (IE a bogus address).

*/

static int
proc_address_to_fd (pi, addr, complain)
     struct procinfo *pi;
     CORE_ADDR addr;
     int complain;
{
  int fd = -1;

  if ((fd = ioctl (pi->ctl_fd, PIOCOPENM, (caddr_t *) &addr)) < 0)
    {
      if (complain)
	{
	  print_sys_errmsg (pi->pathname, errno);
	  warning ("can't find mapped file for address 0x%x", addr);
	}
    }
  return (fd);
}
#endif /* !UNIXWARE */

/* Attach to process PID, then initialize for debugging it
   and wait for the trace-trap that results from attaching.  */

static void
procfs_attach (args, from_tty)
     char *args;
     int from_tty;
{
  char *exec_file;
  int pid;

  if (!args)
    error_no_arg ("process-id to attach");

  pid = atoi (args);

  if (pid == getpid())		/* Trying to masturbate? */
    error ("I refuse to debug myself!");

  if (from_tty)
    {
      exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf_unfiltered ("Attaching to program `%s', %s\n", exec_file, target_pid_to_str (pid));
      else
	printf_unfiltered ("Attaching to %s\n", target_pid_to_str (pid));

      gdb_flush (gdb_stdout);
    }

  inferior_pid = pid = do_attach (pid);
  push_target (&procfs_ops);
}


/* Take a program previously attached to and detaches it.
   The program resumes execution and will no longer stop
   on signals, etc.  We'd better not have left any breakpoints
   in the program or it'll die when it hits one.  For this
   to work, it may be necessary for the process to have been
   previously attached.  It *might* work if the program was
   started via the normal ptrace (PTRACE_TRACEME).  */

static void
procfs_detach (args, from_tty)
     char *args;
     int from_tty;
{
  int siggnal = 0;

  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf_unfiltered ("Detaching from program: %s %s\n",
	      exec_file, target_pid_to_str (inferior_pid));
      gdb_flush (gdb_stdout);
    }
  if (args)
    siggnal = atoi (args);
  
  do_detach (siggnal);
  inferior_pid = 0;
  unpush_target (&procfs_ops);		/* Pop out of handling an inferior */
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
procfs_prepare_to_store ()
{
#ifdef CHILD_PREPARE_TO_STORE
  CHILD_PREPARE_TO_STORE ();
#endif
}

/* Print status information about what we're accessing.  */

static void
procfs_files_info (ignore)
     struct target_ops *ignore;
{
  printf_unfiltered ("\tUsing the running image of %s %s via /proc.\n",
	  attach_flag? "attached": "child", target_pid_to_str (inferior_pid));
}

/* ARGSUSED */
static void
procfs_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  error ("Use the \"run\" command to start a Unix child process.");
}

/*

LOCAL FUNCTION

	do_attach -- attach to an already existing process

SYNOPSIS

	int do_attach (int pid)

DESCRIPTION

	Attach to an already existing process with the specified process
	id.  If the process is not already stopped, query whether to
	stop it or not.

NOTES

	The option of stopping at attach time is specific to the /proc
	versions of gdb.  Versions using ptrace force the attachee
	to stop.  (I have changed this version to do so, too.  All you
	have to do is "continue" to make it go on. -- gnu@cygnus.com)

*/

static int
do_attach (pid)
     int pid;
{
  struct procinfo *pi;
  struct sig_ctl sctl;
  struct flt_ctl fctl;
  int nlwp, *lwps;

  pi  = init_procinfo (pid, 0);

#ifdef PIOCLWPIDS
  nlwp = pi->prstatus.pr_nlwp;
  lwps = alloca ((2 * nlwp + 2) * sizeof (id_t));

  if (ioctl (pi->ctl_fd, PIOCLWPIDS, lwps))
    {
      print_sys_errmsg (pi -> pathname, errno);
      error ("PIOCLWPIDS failed");
    }
#else /* PIOCLWPIDS */
  nlwp = 1;
  lwps = alloca ((2 * nlwp + 2) * sizeof *lwps);
  lwps[0] = 0;
#endif
  for (; nlwp > 0; nlwp--, lwps++)
    {
      /* First one has already been created above.  */
      if ((pi = find_procinfo ((*lwps << 16) | pid, 1)) == 0)
	pi = init_procinfo ((*lwps << 16) | pid, 0);

      if (THE_PR_LWP(pi->prstatus).pr_flags & (PR_STOPPED | PR_ISTOP))
	{
	  pi->was_stopped = 1;
	}
      else
	{
	  pi->was_stopped = 0;
	  if (1 || query ("Process is currently running, stop it? "))
	    {
	      long cmd;
	      /* Make it run again when we close it.  */
	      modify_run_on_last_close_flag (pi->ctl_fd, 1);
#ifdef PROCFS_USE_READ_WRITE
	      cmd = PCSTOP;
	      if (write (pi->ctl_fd, (char *) &cmd, sizeof (long)) < 0)
#else
	      if (ioctl (pi->ctl_fd, PIOCSTOP, &pi->prstatus) < 0)
#endif
		{
		  print_sys_errmsg (pi->pathname, errno);
		  close_proc_file (pi);
		  error ("PIOCSTOP failed");
		}
#ifdef UNIXWARE
	      if (!procfs_read_status (pi))
		{
		  print_sys_errmsg (pi->pathname, errno);
		  close_proc_file (pi);
		  error ("procfs_read_status failed");
		} 
#endif
	      pi->nopass_next_sigstop = 1;
	    }
	  else
	    {
	      printf_unfiltered ("Ok, gdb will wait for %s to stop.\n", 
				 target_pid_to_str (pi->pid));
	    }
	}

#ifdef PROCFS_USE_READ_WRITE
      fctl.cmd = PCSFAULT;
      if (write (pi->ctl_fd, (char *) &fctl, sizeof (struct flt_ctl)) < 0)
	print_sys_errmsg ("PCSFAULT failed", errno);
#else /* PROCFS_USE_READ_WRITE */
      if (ioctl (pi->ctl_fd, PIOCSFAULT, &pi->prrun.pr_fault))
	{
	  print_sys_errmsg ("PIOCSFAULT failed", errno);
	}
      if (ioctl (pi->ctl_fd, PIOCSTRACE, &pi->prrun.pr_trace))
	{
	  print_sys_errmsg ("PIOCSTRACE failed", errno);
	}
      add_thread (pi->pid);
      procfs_set_inferior_syscall_traps (pi);
#endif /* PROCFS_USE_READ_WRITE */
    }
  attach_flag = 1;
  return (pi->pid);
}

/*

LOCAL FUNCTION

	do_detach -- detach from an attached-to process

SYNOPSIS

	void do_detach (int signal)

DESCRIPTION

	Detach from the current attachee.

	If signal is non-zero, the attachee is started running again and sent
	the specified signal.

	If signal is zero and the attachee was not already stopped when we
	attached to it, then we make it runnable again when we detach.

	Otherwise, we query whether or not to make the attachee runnable
	again, since we may simply want to leave it in the state it was in
	when we attached.

	We report any problems, but do not consider them errors, since we
	MUST detach even if some things don't seem to go right.  This may not
	be the ideal situation.  (FIXME).
 */

static void
do_detach (signal)
     int signal;
{
  struct procinfo *pi;

  for (pi = procinfo_list; pi; pi = pi->next)
    {
      if (signal)
	{
	  set_proc_siginfo (pi, signal);
	}
#ifdef PROCFS_USE_READ_WRITE
      pi->saved_exitset.cmd = PCSEXIT;
      if (write (pi->ctl_fd, (char *) &pi->saved_exitset,
		 sizeof (struct sys_ctl)) < 0)
#else
	if (ioctl (pi->ctl_fd, PIOCSEXIT, &pi->saved_exitset.sysset) < 0)
#endif
	  {
	    print_sys_errmsg (pi->pathname, errno);
	    printf_unfiltered ("PIOCSEXIT failed.\n");
	  }
#ifdef PROCFS_USE_READ_WRITE
      pi->saved_entryset.cmd = PCSENTRY;
      if (write (pi->ctl_fd, (char *) &pi->saved_entryset,
		 sizeof (struct sys_ctl)) < 0)
#else
	if (ioctl (pi->ctl_fd, PIOCSENTRY, &pi->saved_entryset.sysset) < 0)
#endif
	  {
	    print_sys_errmsg (pi->pathname, errno);
	    printf_unfiltered ("PIOCSENTRY failed.\n");
	  }
#ifdef PROCFS_USE_READ_WRITE
      pi->saved_trace.cmd = PCSTRACE;
      if (write (pi->ctl_fd, (char *) &pi->saved_trace,
		 sizeof (struct sig_ctl)) < 0)
#else
	if (ioctl (pi->ctl_fd, PIOCSTRACE, &pi->saved_trace.sigset) < 0)
#endif
	  {
	    print_sys_errmsg (pi->pathname, errno);
	    printf_unfiltered ("PIOCSTRACE failed.\n");
	  }
#ifndef UNIXWARE
      if (ioctl (pi->ctl_fd, PIOCSHOLD, &pi->saved_sighold.sigset) < 0)
	{
	  print_sys_errmsg (pi->pathname, errno);
	  printf_unfiltered ("PIOSCHOLD failed.\n");
	}
#endif
#ifdef PROCFS_USE_READ_WRITE
      pi->saved_fltset.cmd = PCSFAULT;
      if (write (pi->ctl_fd, (char *) &pi->saved_fltset,
		 sizeof (struct flt_ctl)) < 0)
#else
      if (ioctl (pi->ctl_fd, PIOCSFAULT, &pi->saved_fltset.fltset) < 0)
#endif
	{
	  print_sys_errmsg (pi->pathname, errno);
	  printf_unfiltered ("PIOCSFAULT failed.\n");
	}
      if (!procfs_read_status (pi))
	{
	  print_sys_errmsg (pi->pathname, errno);
	  printf_unfiltered ("procfs_read_status failed.\n");
	}
      else
	{
	  if (signal
	  || (THE_PR_LWP(pi->prstatus).pr_flags & (PR_STOPPED | PR_ISTOP)))
	    {
	      long cmd;
	      struct proc_ctl pctl;

	      if (signal || !pi->was_stopped ||
		  query ("Was stopped when attached, make it runnable again? "))
		{
		  /* Clear any pending signal if we want to detach without
		     a signal.  */
		  if (signal == 0)
		    set_proc_siginfo (pi, signal);

		  /* Clear any fault that might have stopped it.  */
#ifdef PROCFS_USE_READ_WRITE
		  cmd = PCCFAULT;
		  if (write (pi->ctl_fd, (char *) &cmd, sizeof (long)) < 0)
#else
		  if (ioctl (pi->ctl_fd, PIOCCFAULT, 0))
#endif
		    {
		      print_sys_errmsg (pi->pathname, errno);
		      printf_unfiltered ("PIOCCFAULT failed.\n");
		    }

		  /* Make it run again when we close it.  */

		  modify_run_on_last_close_flag (pi->ctl_fd, 1);
		}
	    }
	}
      close_proc_file (pi);
    }
  attach_flag = 0;
}

/*  emulate wait() as much as possible.
    Wait for child to do something.  Return pid of child, or -1 in case
    of error; store status in *OURSTATUS.

    Not sure why we can't
    just use wait(), but it seems to have problems when applied to a
    process being controlled with the /proc interface.

    We have a race problem here with no obvious solution.  We need to let
    the inferior run until it stops on an event of interest, which means
    that we need to use the PIOCWSTOP ioctl.  However, we cannot use this
    ioctl if the process is already stopped on something that is not an
    event of interest, or the call will hang indefinitely.  Thus we first
    use PIOCSTATUS to see if the process is not stopped.  If not, then we
    use PIOCWSTOP.  But during the window between the two, if the process
    stops for any reason that is not an event of interest (such as a job
    control signal) then gdb will hang.  One possible workaround is to set
    an alarm to wake up every minute of so and check to see if the process
    is still running, and if so, then reissue the PIOCWSTOP.  But this is
    a real kludge, so has not been implemented.  FIXME: investigate
    alternatives.

    FIXME:  Investigate why wait() seems to have problems with programs
    being control by /proc routines.  */
static int
procfs_wait (pid, ourstatus)
     int pid;
     struct target_waitstatus *ourstatus;
{
  short what;
  short why;
  int statval = 0;
  int checkerr = 0;
  int rtnval = -1;
  struct procinfo *pi;
  struct proc_ctl pctl;

scan_again:

  /* handle all syscall events first, otherwise we might not
     notice a thread was created until too late. */

  for (pi = procinfo_list; pi; pi = pi->next)
    {
      if (!pi->had_event)
	continue;

      if (! (THE_PR_LWP(pi->prstatus).pr_flags & (PR_STOPPED | PR_ISTOP)) )
	continue;

      why = THE_PR_LWP(pi->prstatus).pr_why;
      what = THE_PR_LWP(pi->prstatus).pr_what;
      if (why == PR_SYSENTRY || why == PR_SYSEXIT)
	{
	  int i;
	  int found_handler = 0;

	  for (i = 0; i < pi->num_syscall_handlers; i++)
	    if (pi->syscall_handlers[i].syscall_num == what)
	      {
		found_handler = 1;
		pi->saved_rtnval = pi->pid;
		pi->saved_statval = 0;
		if (!pi->syscall_handlers[i].func
		    (pi, what, why, &pi->saved_rtnval, &pi->saved_statval))
		  pi->had_event = 0;
		break;
	      }

	  if (!found_handler)
	    {
	      if (why == PR_SYSENTRY)
		error ("PR_SYSENTRY, unhandled system call %d", what);
	      else
		error ("PR_SYSEXIT, unhandled system call %d", what);
	    }
	}
    }

  /* find a relevant process with an event */

  for (pi = procinfo_list; pi; pi = pi->next)
    if (pi->had_event && (pid == -1 || pi->pid == pid))
      break;

  if (!pi)
    {
      wait_fd ();
      goto scan_again;
    }

  if (!checkerr
  && !(THE_PR_LWP(pi->prstatus).pr_flags & (PR_STOPPED | PR_ISTOP)))
    {
      if (!procfs_write_pcwstop (pi))
	{
	  checkerr++;
	}
    }
  if (checkerr)
    {
      if (errno == ENOENT)
	{
	  /* XXX Fixme -- what to do if attached?  Can't call wait... */
	  rtnval = wait (&statval);
	  if ((rtnval) != (PIDGET (inferior_pid)))
	    {
	      print_sys_errmsg (pi->pathname, errno);
	      error ("procfs_wait: wait failed, returned %d", rtnval);
	      /* NOTREACHED */
	    }
	}
      else
	{
	  print_sys_errmsg (pi->pathname, errno);
	  error ("PIOCSTATUS or PIOCWSTOP failed.");
	  /* NOTREACHED */
	}
    }
  else if (THE_PR_LWP(pi->prstatus).pr_flags & (PR_STOPPED | PR_ISTOP))
    {
#ifdef UNIXWARE
      rtnval = pi->prstatus.pr_pid;
#else
      rtnval = pi->pid;
#endif
      why = THE_PR_LWP(pi->prstatus).pr_why;
      what = THE_PR_LWP(pi->prstatus).pr_what;

      switch (why)
	{
	case PR_SIGNALLED:
	  statval = (what << 8) | 0177;
	  break;
	case PR_SYSENTRY:
	case PR_SYSEXIT:
	  rtnval = pi->saved_rtnval;
	  statval = pi->saved_statval;
	  break;
	case PR_REQUESTED:
	  statval = (SIGSTOP << 8) | 0177;
	  break;
	case PR_JOBCONTROL:
	  statval = (what << 8) | 0177;
	  break;
	case PR_FAULTED:
	  switch (what)
	    {
#ifdef FLTWATCH
	    case FLTWATCH:
	      statval = (SIGTRAP << 8) | 0177;
	      break;
#endif
#ifdef FLTKWATCH
	    case FLTKWATCH:
	      statval = (SIGTRAP << 8) | 0177;
	      break;
#endif
#ifndef FAULTED_USE_SIGINFO
	      /* Irix, contrary to the documentation, fills in 0 for si_signo.
		 Solaris fills in si_signo.  I'm not sure about others.  */
	    case FLTPRIV:
	    case FLTILL:
	      statval = (SIGILL << 8) | 0177;
	      break;
	    case FLTBPT:
	    case FLTTRACE:
	      statval = (SIGTRAP << 8) | 0177;
	      break;
	    case FLTSTACK:
	    case FLTACCESS:
	    case FLTBOUNDS:
	      statval = (SIGSEGV << 8) | 0177;
	      break;
	    case FLTIOVF:
	    case FLTIZDIV:
	    case FLTFPE:
	      statval = (SIGFPE << 8) | 0177;
	      break;
	    case FLTPAGE:		/* Recoverable page fault */
#endif /* not FAULTED_USE_SIGINFO */
	    default:
	      /* Use the signal which the kernel assigns.  This is better than
		 trying to second-guess it from the fault.  In fact, I suspect
		 that FLTACCESS can be either SIGSEGV or SIGBUS.  */
              statval =
		 ((THE_PR_LWP(pi->prstatus).pr_info.si_signo) << 8) | 0177;
	      break;
	    }
	  break;
	default:
	  error ("PIOCWSTOP, unknown why %d, what %d", why, what);
	}
      /* Stop all the other threads when any of them stops.  */

      {
	struct procinfo *procinfo, *next_pi;

	for (procinfo = procinfo_list; procinfo; procinfo = next_pi)
	  {
	    next_pi = procinfo->next;
	    if (!procinfo->had_event)
	      {
#ifdef PROCFS_USE_READ_WRITE
		long cmd = PCSTOP;
		if (write (pi->ctl_fd, (char *) &cmd, sizeof (long)) < 0)
		  {
		    print_sys_errmsg (procinfo->pathname, errno);
		    error ("PCSTOP failed");
		  }
#else
		/* A bug in Solaris (2.5) causes us to hang when trying to
		   stop a stopped process.  So, we have to check first in
		   order to avoid the hang. */
		if (!procfs_read_status (procinfo))
		  {
		    /* The LWP has apparently terminated.  */
		    if (info_verbose)
		      printf_filtered ("LWP %d doesn't respond.\n", 
				       (procinfo->pid >> 16) & 0xffff);
		    close_proc_file (procinfo);
		    continue;
		  }

		if (!(procinfo->prstatus.pr_flags & PR_STOPPED))
		  if (ioctl (procinfo->ctl_fd, PIOCSTOP, &procinfo->prstatus)
		      < 0)
		    {
		      print_sys_errmsg (procinfo->pathname, errno);
		      warning ("PIOCSTOP failed");
		    }
#endif
	      }
	  }
      }
    }
  else
    {
      error ("PIOCWSTOP, stopped for unknown/unhandled reason, flags %#x",
	     THE_PR_LWP(pi->prstatus).pr_flags);
    }

  store_waitstatus (ourstatus, statval);

  if (rtnval == -1)		/* No more children to wait for */
    {
      warning ("Child process unexpectedly missing");
      /* Claim it exited with unknown signal.  */
      ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
      ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
      return rtnval;
    }

  pi->had_event = 0;		/* Indicate that we've seen this one */
  return (rtnval);
}

/*

LOCAL FUNCTION

	set_proc_siginfo - set a process's current signal info

SYNOPSIS

	void set_proc_siginfo (struct procinfo *pip, int signo);

DESCRIPTION

	Given a pointer to a process info struct in PIP and a signal number
	in SIGNO, set the process's current signal and its associated signal
	information.  The signal will be delivered to the process immediately
	after execution is resumed, even if it is being held.  In addition,
	this particular delivery will not cause another PR_SIGNALLED stop
	even if the signal is being traced.

	If we are not delivering the same signal that the prstatus siginfo
	struct contains information about, then synthesize a siginfo struct
	to match the signal we are doing to deliver, make it of the type
	"generated by a user process", and send this synthesized copy.  When
	used to set the inferior's signal state, this will be required if we
	are not currently stopped because of a traced signal, or if we decide
	to continue with a different signal.

	Note that when continuing the inferior from a stop due to receipt
	of a traced signal, we either have set PRCSIG to clear the existing
	signal, or we have to call this function to do a PIOCSSIG with either
	the existing siginfo struct from pr_info, or one we have synthesized
	appropriately for the signal we want to deliver.  Otherwise if the
	signal is still being traced, the inferior will immediately stop
	again.

	See siginfo(5) for more details.
*/

static void
set_proc_siginfo (pip, signo)
     struct procinfo *pip;
     int signo;
{
  struct siginfo newsiginfo;
  struct siginfo *sip;
  struct sigi_ctl sictl;

#ifdef PROCFS_DONT_PIOCSSIG_CURSIG
  /* With Alpha OSF/1 procfs, the kernel gets really confused if it
     receives a PIOCSSIG with a signal identical to the current signal,
     it messes up the current signal. Work around the kernel bug.  */
  if (signo == THE_PR_LWP(pip->prstatus).pr_cursig)
    return;
#endif

#ifdef UNIXWARE
  if (signo == THE_PR_LWP(pip->prstatus).pr_info.si_signo)
    {
      memcpy ((char *) &sictl.siginfo, (char *) &pip->prstatus.pr_lwp.pr_info,
		sizeof (siginfo_t));
    }
#else
  if (signo == THE_PR_LWP(pip->prstatus).pr_info.si_signo)
    {
      sip = &pip -> prstatus.pr_info;
    }
#endif
  else
    {
#ifdef UNIXWARE
      siginfo_t *sip = &sictl.siginfo;
      memset ((char *) sip, 0, sizeof (siginfo_t));
#else
      memset ((char *) &newsiginfo, 0, sizeof (newsiginfo));
      sip = &newsiginfo;
#endif
      sip -> si_signo = signo;
      sip -> si_code = 0;
      sip -> si_errno = 0;
      sip -> si_pid = getpid ();
      sip -> si_uid = getuid ();
    }
#ifdef PROCFS_USE_READ_WRITE
  sictl.cmd = PCSSIG;
  if (write (pip->ctl_fd, (char *) &sictl, sizeof (struct sigi_ctl)) < 0)
#else
  if (ioctl (pip->ctl_fd, PIOCSSIG, sip) < 0)
#endif
    {
      print_sys_errmsg (pip -> pathname, errno);
      warning ("PIOCSSIG failed");
    }
}

/* Resume execution of process PID.  If STEP is nozero, then
   just single step it.  If SIGNAL is nonzero, restart it with that
   signal activated.  */

static void
procfs_resume (pid, step, signo)
     int pid;
     int step;
     enum target_signal signo;
{
  int signal_to_pass;
  struct procinfo *pi, *procinfo, *next_pi;
  struct proc_ctl pctl;

  pi = find_procinfo (pid == -1 ? inferior_pid : pid, 0);

  errno = 0;
#ifdef UNIXWARE
  pctl.cmd = PCRUN;
  pctl.data = PRCFAULT;
#else
  pi->prrun.pr_flags = PRSTRACE | PRSFAULT | PRCFAULT;
#endif

#if 0
  /* It should not be necessary.  If the user explicitly changes the value,
     value_assign calls write_register_bytes, which writes it.  */
/*	It may not be absolutely necessary to specify the PC value for
	restarting, but to be safe we use the value that gdb considers
	to be current.  One case where this might be necessary is if the
	user explicitly changes the PC value that gdb considers to be
	current.  FIXME:  Investigate if this is necessary or not.  */

#ifdef PRSVADDR_BROKEN
/* Can't do this under Solaris running on a Sparc, as there seems to be no
   place to put nPC.  In fact, if you use this, nPC seems to be set to some
   random garbage.  We have to rely on the fact that PC and nPC have been
   written previously via PIOCSREG during a register flush. */

  pi->prrun.pr_vaddr = (caddr_t) *(int *) &registers[REGISTER_BYTE (PC_REGNUM)];
  pi->prrun.pr_flags != PRSVADDR;
#endif
#endif

  if (signo == TARGET_SIGNAL_STOP && pi->nopass_next_sigstop)
    /* When attaching to a child process, if we forced it to stop with
       a PIOCSTOP, then we will have set the nopass_next_sigstop flag.
       Upon resuming the first time after such a stop, we explicitly
       inhibit sending it another SIGSTOP, which would be the normal
       result of default signal handling.  One potential drawback to
       this is that we will also ignore any attempt to by the user
       to explicitly continue after the attach with a SIGSTOP.  Ultimately
       this problem should be dealt with by making the routines that
       deal with the inferior a little smarter, and possibly even allow
       an inferior to continue running at the same time as gdb.  (FIXME?)  */
    signal_to_pass = 0;
  else if (signo == TARGET_SIGNAL_TSTP
	   && THE_PR_LWP(pi->prstatus).pr_cursig == SIGTSTP
	   && THE_PR_LWP(pi->prstatus).pr_action.sa_handler == SIG_DFL
	   )

    /* We are about to pass the inferior a SIGTSTP whose action is
       SIG_DFL.  The SIG_DFL action for a SIGTSTP is to stop
       (notifying the parent via wait()), and then keep going from the
       same place when the parent is ready for you to keep going.  So
       under the debugger, it should do nothing (as if the program had
       been stopped and then later resumed.  Under ptrace, this
       happens for us, but under /proc, the system obligingly stops
       the process, and wait_for_inferior would have no way of
       distinguishing that type of stop (which indicates that we
       should just start it again), with a stop due to the pr_trace
       field of the prrun_t struct.

       Note that if the SIGTSTP is being caught, we *do* need to pass it,
       because the handler needs to get executed.  */
    signal_to_pass = 0;
  else
    signal_to_pass = target_signal_to_host (signo);

  if (signal_to_pass)
    {
      set_proc_siginfo (pi, signal_to_pass);
    }
  else
    {
#ifdef UNIXWARE
      pctl.data |= PRCSIG;
#else
      pi->prrun.pr_flags |= PRCSIG;
#endif
    }
  pi->nopass_next_sigstop = 0;
  if (step)
    {
#ifdef UNIXWARE
      pctl.data |= PRSTEP;
#else
      pi->prrun.pr_flags |= PRSTEP;
#endif
    }
  pi->had_event = 0;
  /* Don't try to start a process unless it's stopped on an
     `event of interest'.  Doing so will cause errors.  */

  if (!procfs_read_status (pi))
    {
      /* The LWP has apparently terminated.  */
      if (info_verbose)
	printf_filtered ("LWP %d doesn't respond.\n", 
			 (pi->pid >> 16) & 0xffff);
      close_proc_file (pi);
    }
  else
    {
#ifdef PROCFS_USE_READ_WRITE
      if (write (pi->ctl_fd, (char *) &pctl, sizeof (struct proc_ctl)) < 0)
#else
      if ((pi->prstatus.pr_flags & PR_ISTOP)
	  && ioctl (pi->ctl_fd, PIOCRUN, &pi->prrun) != 0)
#endif
	{
	  /* The LWP has apparently terminated.  */
	  if (info_verbose)
	    printf_filtered ("LWP %d doesn't respond.\n", 
			     (pi->pid >> 16) & 0xffff);
	  close_proc_file (pi);
	}
    }

  /* Continue all the other threads that haven't had an event of interest.
     Also continue them if they have NOPASS_NEXT_SIGSTOP set; this is only
     set by do_attach, and means this is the first resume after an attach.  
     All threads were CSTOP'd by do_attach, and should be resumed now.  */

  if (pid == -1)
    for (procinfo = procinfo_list; procinfo; procinfo = next_pi)
      {
	next_pi = procinfo->next;
	if (pi != procinfo)
	  if (!procinfo->had_event || 
	      (procinfo->nopass_next_sigstop && signo == TARGET_SIGNAL_STOP))
	    {
	      procinfo->had_event = procinfo->nopass_next_sigstop = 0;
#ifdef PROCFS_USE_READ_WRITE
	      pctl.data = PRCFAULT | PRCSIG;
	      if (write (procinfo->ctl_fd, (char *) &pctl,
			 sizeof (struct proc_ctl)) < 0)
		{
		  if (!procfs_read_status (procinfo))
		    fprintf_unfiltered(gdb_stderr, 
				       "procfs_read_status failed, errno=%d\n",
				       errno);
		  print_sys_errmsg (procinfo->pathname, errno);
		  error ("PCRUN failed");
		}
#else
	      procinfo->prrun.pr_flags &= PRSTEP;
	      procinfo->prrun.pr_flags |= PRCFAULT | PRCSIG;
	      if (!procfs_read_status (procinfo))
		{
		  /* The LWP has apparently terminated.  */
		  if (info_verbose)
		    printf_filtered ("LWP %d doesn't respond.\n", 
				     (procinfo->pid >> 16) & 0xffff);
		  close_proc_file (procinfo);
		  continue;
		}

	      /* Don't try to start a process unless it's stopped on an
		 `event of interest'.  Doing so will cause errors.  */

	      if ((procinfo->prstatus.pr_flags & PR_ISTOP)
		  && ioctl (procinfo->ctl_fd, PIOCRUN, &procinfo->prrun) < 0)
		{
		  if (!procfs_read_status (procinfo))
		    fprintf_unfiltered(gdb_stderr, 
				       "procfs_read_status failed, errno=%d\n",
				       errno);
		  print_sys_errmsg (procinfo->pathname, errno);
		  warning ("PIOCRUN failed");
		}
#endif
	    }
	procfs_read_status (procinfo);
      }
}

/*

LOCAL FUNCTION

	procfs_fetch_registers -- fetch current registers from inferior

SYNOPSIS

	void procfs_fetch_registers (int regno)

DESCRIPTION

	Read the current values of the inferior's registers, both the
	general register set and floating point registers (if supported)
	and update gdb's idea of their current values.

*/

static void
procfs_fetch_registers (regno)
     int regno;
{
  struct procinfo *pi;

  pi = current_procinfo;

#ifdef UNIXWARE
  if (procfs_read_status (pi))
    {
      supply_gregset (&pi->prstatus.pr_lwp.pr_context.uc_mcontext.gregs);
#if defined (FP0_REGNUM)
      supply_fpregset (&pi->prstatus.pr_lwp.pr_context.uc_mcontext.fpregs); 
#endif
    }
#else /* UNIXWARE */
  if (ioctl (pi->ctl_fd, PIOCGREG, &pi->gregset.gregset) != -1)
    {
      supply_gregset (&pi->gregset.gregset);
    }
#if defined (FP0_REGNUM)
  if (ioctl (pi->ctl_fd, PIOCGFPREG, &pi->fpregset.fpregset) != -1)
    {
      supply_fpregset (&pi->fpregset.fpregset);
    }
#endif
#endif /* UNIXWARE */
}

/*

LOCAL FUNCTION

	proc_init_failed - called when /proc access initialization fails
fails

SYNOPSIS

	static void proc_init_failed (struct procinfo *pi, 
				      char *why, int kill_p)

DESCRIPTION

	This function is called whenever initialization of access to a /proc
	entry fails.  It prints a suitable error message, does some cleanup,
	and then invokes the standard error processing routine which dumps
	us back into the command loop.  If KILL_P is true, sends SIGKILL.
 */

static void
proc_init_failed (pi, why, kill_p)
     struct procinfo *pi;
     char *why;
     int  kill_p;
{
  print_sys_errmsg (pi->pathname, errno);
  if (kill_p)
    kill (pi->pid, SIGKILL);
  close_proc_file (pi);
  error (why);
  /* NOTREACHED */
}

/*

LOCAL FUNCTION

	close_proc_file - close any currently open /proc entry

SYNOPSIS

	static void close_proc_file (struct procinfo *pip)

DESCRIPTION

	Close any currently open /proc entry and mark the process information
	entry as invalid.  In order to ensure that we don't try to reuse any
	stale information, the pid, fd, and pathnames are explicitly
	invalidated, which may be overkill.

 */

static void
close_proc_file (pip)
     struct procinfo *pip;
{
  struct procinfo *procinfo;

  delete_thread (pip->pid);	/* remove thread from GDB's thread list */
  remove_fd (pip);		/* Remove fd from poll/select list */

  close (pip->ctl_fd);
#ifdef HAVE_MULTIPLE_PROC_FDS
  close (pip->as_fd);
  close (pip->status_fd);
  close (pip->map_fd);
#endif

  free (pip -> pathname);

  /* Unlink pip from the procinfo chain.  Note pip might not be on the list. */

  if (procinfo_list == pip)
    procinfo_list = pip->next;
  else
    {
      for (procinfo = procinfo_list; procinfo; procinfo = procinfo->next)
        {
          if (procinfo->next == pip)
	    {
	      procinfo->next = pip->next;
	      break;
	    }
        }
      free (pip);
    }
}

/*

LOCAL FUNCTION

	open_proc_file - open a /proc entry for a given process id

SYNOPSIS

	static int open_proc_file (int pid, struct procinfo *pip, int mode)

DESCRIPTION

	Given a process id and a mode, close the existing open /proc
	entry (if any) and open one for the new process id, in the
	specified mode.  Once it is open, then mark the local process
	information structure as valid, which guarantees that the pid,
	fd, and pathname fields match an open /proc entry.  Returns
	zero if the open fails, nonzero otherwise.

	Note that the pathname is left intact, even when the open fails,
	so that callers can use it to construct meaningful error messages
	rather than just "file open failed".

	Note that for Solaris, the process-id also includes an LWP-id, so we
	actually attempt to open that.  If we are handed a pid with a 0 LWP-id,
	then we will ask the kernel what it is and add it to the pid.  Hence,
	the pid can be changed by us.
 */

static int
open_proc_file (pid, pip, mode, control)
     int pid;
     struct procinfo *pip;
     int mode;
     int control;
{
  int tmp, tmpfd;

  pip -> next = NULL;
  pip -> had_event = 0;
  pip -> pathname = xmalloc (MAX_PROC_NAME_SIZE);
  pip -> pid = pid;

#ifndef PIOCOPENLWP
  tmp = pid;
#else
  tmp = pid & 0xffff;
#endif

#ifdef HAVE_MULTIPLE_PROC_FDS
  sprintf (pip->pathname, STATUS_PROC_NAME_FMT, tmp);
  if ((pip->status_fd = open (pip->pathname, O_RDONLY)) < 0)
    {
      return 0;
    }

  sprintf (pip->pathname, AS_PROC_NAME_FMT, tmp);
  if ((pip->as_fd = open (pip->pathname, O_RDWR)) < 0)
    {
      close (pip->status_fd);
      return 0;
    }

  sprintf (pip->pathname, MAP_PROC_NAME_FMT, tmp);
  if ((pip->map_fd = open (pip->pathname, O_RDONLY)) < 0)
    {
      close (pip->status_fd);
      close (pip->as_fd);
      return 0;
    }

  if (control)
    {
      sprintf (pip->pathname, CTL_PROC_NAME_FMT, tmp);
      if ((pip->ctl_fd = open (pip->pathname, O_WRONLY)) < 0)
	{
	  close (pip->status_fd);
          close (pip->as_fd);
          close (pip->map_fd);
          return 0;
        }
    }

#else /* HAVE_MULTIPLE_PROC_FDS */
  sprintf (pip -> pathname, CTL_PROC_NAME_FMT, tmp);

  if ((tmpfd = open (pip -> pathname, mode)) < 0)
    return 0;

#ifndef PIOCOPENLWP
    pip -> ctl_fd = tmpfd;
    pip -> as_fd = tmpfd;
    pip -> map_fd = tmpfd;
    pip -> status_fd = tmpfd;
#else
  tmp = (pid >> 16) & 0xffff;	/* Extract thread id */

  if (tmp == 0)
    {				/* Don't know thread id yet */
      if (ioctl (tmpfd, PIOCSTATUS, &pip -> prstatus) < 0)
	{
	  print_sys_errmsg (pip -> pathname, errno);
	  close (tmpfd);
	  error ("open_proc_file: PIOCSTATUS failed");
	}

      tmp = pip -> prstatus.pr_who; /* Get thread id from prstatus_t */
      pip -> pid = (tmp << 16) | pid; /* Update pip */
    }

  if ((pip -> ctl_fd = ioctl (tmpfd, PIOCOPENLWP, &tmp)) < 0)
    {
      close (tmpfd);
      return 0;
    }

#ifdef PIOCSET			/* New method */
  {
      long pr_flags;
      pr_flags = PR_ASYNC;
      ioctl (pip -> ctl_fd, PIOCSET, &pr_flags);
  }
#endif

  /* keep extra fds in sync */
  pip->as_fd = pip->ctl_fd;
  pip->map_fd = pip->ctl_fd;
  pip->status_fd = pip->ctl_fd;

  close (tmpfd);		/* All done with main pid */
#endif	/* PIOCOPENLWP */

#endif /* HAVE_MULTIPLE_PROC_FDS */

  return 1;
}

static char *
mappingflags (flags)
     long flags;
{
  static char asciiflags[8];
  
  strcpy (asciiflags, "-------");
#if defined (MA_PHYS)
  if (flags & MA_PHYS)   asciiflags[0] = 'd';
#endif
  if (flags & MA_STACK)  asciiflags[1] = 's';
  if (flags & MA_BREAK)  asciiflags[2] = 'b';
  if (flags & MA_SHARED) asciiflags[3] = 's';
  if (flags & MA_READ)   asciiflags[4] = 'r';
  if (flags & MA_WRITE)  asciiflags[5] = 'w';
  if (flags & MA_EXEC)   asciiflags[6] = 'x';
  return (asciiflags);
}

static void
info_proc_flags (pip, summary)
     struct procinfo *pip;
     int summary;
{
  struct trans *transp;
#ifdef UNIXWARE
  long flags = pip->prstatus.pr_flags | pip->prstatus.pr_lwp.pr_flags;
#else
  long flags = pip->prstatus.pr_flags;
#endif

  printf_filtered ("%-32s", "Process status flags:");
  if (!summary)
    {
      printf_filtered ("\n\n");
    }
  for (transp = pr_flag_table; transp -> name != NULL; transp++)
    {
      if (flags & transp -> value)
	{
	  if (summary)
	    {
	      printf_filtered ("%s ", transp -> name);
	    }
	  else
	    {
	      printf_filtered ("\t%-16s %s.\n", transp -> name, transp -> desc);
	    }
	}
    }
  printf_filtered ("\n");
}

static void
info_proc_stop (pip, summary)
     struct procinfo *pip;
     int summary;
{
  struct trans *transp;
  int why;
  int what;

  why = THE_PR_LWP(pip->prstatus).pr_why;
  what = THE_PR_LWP(pip->prstatus).pr_what;

  if (THE_PR_LWP(pip->prstatus).pr_flags & PR_STOPPED)
    {
      printf_filtered ("%-32s", "Reason for stopping:");
      if (!summary)
	{
	  printf_filtered ("\n\n");
	}
      for (transp = pr_why_table; transp -> name != NULL; transp++)
	{
	  if (why == transp -> value)
	    {
	      if (summary)
		{
		  printf_filtered ("%s ", transp -> name);
		}
	      else
		{
		  printf_filtered ("\t%-16s %s.\n",
				   transp -> name, transp -> desc);
		}
	      break;
	    }
	}
      
      /* Use the pr_why field to determine what the pr_what field means, and
	 print more information. */
      
      switch (why)
	{
	  case PR_REQUESTED:
	    /* pr_what is unused for this case */
	    break;
	  case PR_JOBCONTROL:
	  case PR_SIGNALLED:
	    if (summary)
	      {
		printf_filtered ("%s ", signalname (what));
	      }
	    else
	      {
		printf_filtered ("\t%-16s %s.\n", signalname (what),
				 safe_strsignal (what));
	      }
	    break;
	  case PR_SYSENTRY:
	    if (summary)
	      {
		printf_filtered ("%s ", syscallname (what));
	      }
	    else
	      {
		printf_filtered ("\t%-16s %s.\n", syscallname (what),
				 "Entered this system call");
	      }
	    break;
	  case PR_SYSEXIT:
	    if (summary)
	      {
		printf_filtered ("%s ", syscallname (what));
	      }
	    else
	      {
		printf_filtered ("\t%-16s %s.\n", syscallname (what),
				 "Returned from this system call");
	      }
	    break;
	  case PR_FAULTED:
	    if (summary)
	      {
		printf_filtered ("%s ",
				 lookupname (faults_table, what, "fault"));
	      }
	    else
	      {
		printf_filtered ("\t%-16s %s.\n",
				 lookupname (faults_table, what, "fault"),
				 lookupdesc (faults_table, what));
	      }
	    break;
	  }
      printf_filtered ("\n");
    }
}

static void
info_proc_siginfo (pip, summary)
     struct procinfo *pip;
     int summary;
{
  struct siginfo *sip;

  if ((THE_PR_LWP(pip->prstatus).pr_flags & PR_STOPPED) &&
      (THE_PR_LWP(pip->prstatus).pr_why == PR_SIGNALLED ||
       THE_PR_LWP(pip->prstatus).pr_why == PR_FAULTED))
    {
      printf_filtered ("%-32s", "Additional signal/fault info:");
      sip = &(THE_PR_LWP(pip->prstatus).pr_info);
      if (summary)
	{
	  printf_filtered ("%s ", signalname (sip -> si_signo));
	  if (sip -> si_errno > 0)
	    {
	      printf_filtered ("%s ", errnoname (sip -> si_errno));
	    }
	  if (sip -> si_code <= 0)
	    {
	      printf_filtered ("sent by %s, uid %d ",
			       target_pid_to_str (sip -> si_pid),
			       sip -> si_uid);
	    }
	  else
	    {
	      printf_filtered ("%s ", sigcodename (sip));
	      if ((sip -> si_signo == SIGILL) ||
		  (sip -> si_signo == SIGFPE) ||
		  (sip -> si_signo == SIGSEGV) ||
		  (sip -> si_signo == SIGBUS))
		{
		  printf_filtered ("addr=%#lx ",
				   (unsigned long) sip -> si_addr);
		}
	      else if ((sip -> si_signo == SIGCHLD))
		{
		  printf_filtered ("child %s, status %u ",
				   target_pid_to_str (sip -> si_pid),
				   sip -> si_status);
		}
	      else if ((sip -> si_signo == SIGPOLL))
		{
		  printf_filtered ("band %u ", sip -> si_band);
		}
	    }
	}
      else
	{
	  printf_filtered ("\n\n");
	  printf_filtered ("\t%-16s %s.\n", signalname (sip -> si_signo),
			   safe_strsignal (sip -> si_signo));
	  if (sip -> si_errno > 0)
	    {
	      printf_filtered ("\t%-16s %s.\n",
			       errnoname (sip -> si_errno),
			       safe_strerror (sip -> si_errno));
	    }
	  if (sip -> si_code <= 0)
	    {
	      printf_filtered ("\t%-16u %s\n", sip -> si_pid, /* XXX need target_pid_to_str() */
			       "PID of process sending signal");
	      printf_filtered ("\t%-16u %s\n", sip -> si_uid,
			       "UID of process sending signal");
	    }
	  else
	    {
	      printf_filtered ("\t%-16s %s.\n", sigcodename (sip),
			       sigcodedesc (sip));
	      if ((sip -> si_signo == SIGILL) ||
		  (sip -> si_signo == SIGFPE))
		{
		  printf_filtered ("\t%#-16lx %s.\n",
				   (unsigned long) sip -> si_addr,
				   "Address of faulting instruction");
		}
	      else if ((sip -> si_signo == SIGSEGV) ||
		       (sip -> si_signo == SIGBUS))
		{
		  printf_filtered ("\t%#-16lx %s.\n",
				   (unsigned long) sip -> si_addr,
				   "Address of faulting memory reference");
		}
	      else if ((sip -> si_signo == SIGCHLD))
		{
		  printf_filtered ("\t%-16u %s.\n", sip -> si_pid, /* XXX need target_pid_to_str() */
				   "Child process ID");
		  printf_filtered ("\t%-16u %s.\n", sip -> si_status,
				   "Child process exit value or signal");
		}
	      else if ((sip -> si_signo == SIGPOLL))
		{
		  printf_filtered ("\t%-16u %s.\n", sip -> si_band,
				   "Band event for POLL_{IN,OUT,MSG}");
		}
	    }
	}
      printf_filtered ("\n");
    }
}

static void
info_proc_syscalls (pip, summary)
     struct procinfo *pip;
     int summary;
{
  int syscallnum;

  if (!summary)
    {

#if 0	/* FIXME:  Needs to use gdb-wide configured info about system calls. */
      if (pip -> prstatus.pr_flags & PR_ASLEEP)
	{
	  int syscallnum = pip -> prstatus.pr_reg[R_D0];
	  if (summary)
	    {
	      printf_filtered ("%-32s", "Sleeping in system call:");
	      printf_filtered ("%s", syscallname (syscallnum));
	    }
	  else
	    {
	      printf_filtered ("Sleeping in system call '%s'.\n",
			       syscallname (syscallnum));
	    }
	}
#endif

#ifndef UNIXWARE
      if (ioctl (pip -> ctl_fd, PIOCGENTRY, &pip -> entryset) < 0)
	{
	  print_sys_errmsg (pip -> pathname, errno);
	  error ("PIOCGENTRY failed");
	}
      
      if (ioctl (pip -> ctl_fd, PIOCGEXIT, &pip -> exitset) < 0)
	{
	  print_sys_errmsg (pip -> pathname, errno);
	  error ("PIOCGEXIT failed");
	}
#endif
      
      printf_filtered ("System call tracing information:\n\n");
      
      printf_filtered ("\t%-12s %-8s %-8s\n",
		       "System call",
		       "Entry",
		       "Exit");
      for (syscallnum = 0; syscallnum < MAX_SYSCALLS; syscallnum++)
	{
	  QUIT;
	  if (syscall_table[syscallnum] != NULL)
	    printf_filtered ("\t%-12s ", syscall_table[syscallnum]);
	  else
	    printf_filtered ("\t%-12d ", syscallnum);

#ifdef UNIXWARE
	  printf_filtered ("%-8s ",
			   prismember (&pip->prstatus.pr_sysentry, syscallnum)
			   ? "on" : "off");
	  printf_filtered ("%-8s ",
			   prismember (&pip->prstatus.pr_sysexit, syscallnum)
			   ? "on" : "off");
#else
	  printf_filtered ("%-8s ",
			   prismember (&pip -> entryset, syscallnum)
			   ? "on" : "off");
	  printf_filtered ("%-8s ",
			   prismember (&pip -> exitset, syscallnum)
			   ? "on" : "off");
#endif
	  printf_filtered ("\n");
	}
      printf_filtered ("\n");
    }
}

static char *
signalname (signo)
     int signo;
{
  const char *name;
  static char locbuf[32];

  name = strsigno (signo);
  if (name == NULL)
    {
      sprintf (locbuf, "Signal %d", signo);
    }
  else
    {
      sprintf (locbuf, "%s (%d)", name, signo);
    }
  return (locbuf);
}

static char *
errnoname (errnum)
     int errnum;
{
  const char *name;
  static char locbuf[32];

  name = strerrno (errnum);
  if (name == NULL)
    {
      sprintf (locbuf, "Errno %d", errnum);
    }
  else
    {
      sprintf (locbuf, "%s (%d)", name, errnum);
    }
  return (locbuf);
}

static void
info_proc_signals (pip, summary)
     struct procinfo *pip;
     int summary;
{
  int signo;

  if (!summary)
    {
#ifndef PROCFS_USE_READ_WRITE
      if (ioctl (pip -> ctl_fd, PIOCGTRACE, &pip -> trace) < 0)
	{
	  print_sys_errmsg (pip -> pathname, errno);
	  error ("PIOCGTRACE failed");
	}
#endif
      
      printf_filtered ("Disposition of signals:\n\n");
      printf_filtered ("\t%-15s %-8s %-8s %-8s  %s\n\n",
		       "Signal", "Trace", "Hold", "Pending", "Description");
      for (signo = 0; signo < NSIG; signo++)
	{
	  QUIT;
	  printf_filtered ("\t%-15s ", signalname (signo));
#ifdef UNIXWARE
	  printf_filtered ("%-8s ",
			   prismember (&pip -> prstatus.pr_sigtrace, signo)
			   ? "on" : "off");
	  printf_filtered ("%-8s ",
			   prismember (&pip -> prstatus.pr_lwp.pr_context.uc_sigmask, signo)
			   ? "on" : "off");
#else
	  printf_filtered ("%-8s ",
			   prismember (&pip -> trace, signo)
			   ? "on" : "off");
	  printf_filtered ("%-8s ",
			   prismember (&pip -> prstatus.pr_sighold, signo)
			   ? "on" : "off");
#endif

#ifdef UNIXWARE
	  if (prismember (&pip->prstatus.pr_sigpend, signo) ||
		prismember (&pip->prstatus.pr_lwp.pr_lwppend, signo))
	    printf_filtered("%-8s ", "yes");
	  else
	    printf_filtered("%-8s ", "no");
#else /* UNIXWARE */
#ifdef PROCFS_SIGPEND_OFFSET
	  /* Alpha OSF/1 numbers the pending signals from 1.  */
	  printf_filtered ("%-8s ",
			   (signo ? prismember (&pip -> prstatus.pr_sigpend,
						signo - 1)
				  : 0)
			   ? "yes" : "no");
#else
	  printf_filtered ("%-8s ",
			   prismember (&pip -> prstatus.pr_sigpend, signo)
			   ? "yes" : "no");
#endif
#endif /* UNIXWARE */
	  printf_filtered (" %s\n", safe_strsignal (signo));
	}
      printf_filtered ("\n");
    }
}

static void
info_proc_faults (pip, summary)
     struct procinfo *pip;
     int summary;
{
  struct trans *transp;

  if (!summary)
    {
#ifndef UNIXWARE
      if (ioctl (pip -> ctl_fd, PIOCGFAULT, &pip->fltset.fltset) < 0)
	{
	  print_sys_errmsg (pip -> pathname, errno);
	  error ("PIOCGFAULT failed");
	}
#endif
      
      printf_filtered ("Current traced hardware fault set:\n\n");
      printf_filtered ("\t%-12s %-8s\n", "Fault", "Trace");

      for (transp = faults_table; transp -> name != NULL; transp++)
	{
	  QUIT;
	  printf_filtered ("\t%-12s ", transp -> name);
#ifdef UNIXWARE
	  printf_filtered ("%-8s", prismember (&pip->prstatus.pr_flttrace, transp -> value)
			   ? "on" : "off");
#else
	  printf_filtered ("%-8s", prismember (&pip->fltset.fltset, transp -> value)
			   ? "on" : "off");
#endif
	  printf_filtered ("\n");
	}
      printf_filtered ("\n");
    }
}

static void
info_proc_mappings (pip, summary)
     struct procinfo *pip;
     int summary;
{
  int nmap;
  struct prmap *prmaps;
  struct prmap *prmap;
  struct stat sbuf;

  if (!summary)
    {
      printf_filtered ("Mapped address spaces:\n\n");
#ifdef BFD_HOST_64_BIT
      printf_filtered ("  %18s %18s %10s %10s %7s\n",
#else
      printf_filtered ("\t%10s %10s %10s %10s %7s\n",
#endif
		       "Start Addr",
		       "  End Addr",
		       "      Size",
		       "    Offset",
		       "Flags");
#ifdef PROCFS_USE_READ_WRITE
      if (fstat (pip->map_fd, &sbuf) == 0)
        {
          nmap = sbuf.st_size / sizeof (prmap_t);
	  prmaps = (struct prmap *) alloca ((nmap + 1) * sizeof (*prmaps));
          if ((lseek (pip->map_fd, 0, SEEK_SET) == 0) &&
		(read (pip->map_fd, (char *) prmaps,
		nmap * sizeof (*prmaps)) == (nmap * sizeof (*prmaps))))
	    {
	      int i = 0;
	      for (prmap = prmaps; i < nmap; ++prmap, ++i)
#else
      if (ioctl (pip -> ctl_fd, PIOCNMAP, &nmap) == 0)
	{
	  prmaps = (struct prmap *) alloca ((nmap + 1) * sizeof (*prmaps));
	  if (ioctl (pip -> ctl_fd, PIOCMAP, prmaps) == 0)
	    {
	      for (prmap = prmaps; prmap -> pr_size; ++prmap)
#endif /* PROCFS_USE_READ_WRITE */
		{
#ifdef BFD_HOST_64_BIT
		  printf_filtered ("  %#18lx %#18lx %#10x %#10x %7s\n",
#else
		  printf_filtered ("\t%#10lx %#10lx %#10x %#10x %7s\n",
#endif
				   (unsigned long)prmap -> pr_vaddr,
				   (unsigned long)prmap -> pr_vaddr
				     + prmap -> pr_size - 1,
				   prmap -> pr_size,
				   prmap -> pr_off,
				   mappingflags (prmap -> pr_mflags));
		}
	    }
	}
      printf_filtered ("\n");
    }
}

/*

LOCAL FUNCTION

	info_proc -- implement the "info proc" command

SYNOPSIS

	void info_proc (char *args, int from_tty)

DESCRIPTION

	Implement gdb's "info proc" command by using the /proc interface
	to print status information about any currently running process.

	Examples of the use of "info proc" are:

	info proc		(prints summary info for current inferior)
	info proc 123		(prints summary info for process with pid 123)
	info proc mappings	(prints address mappings)
	info proc times		(prints process/children times)
	info proc id		(prints pid, ppid, gid, sid, etc)
		FIXME:  i proc id not implemented.
	info proc status	(prints general process state info)
		FIXME:  i proc status not implemented.
	info proc signals	(prints info about signal handling)
	info proc all		(prints all info)

 */

static void
info_proc (args, from_tty)
     char *args;
     int from_tty;
{
  int pid;
  struct procinfo *pip;
  struct cleanup *old_chain;
  char **argv;
  int argsize;
  int summary = 1;
  int flags = 0;
  int syscalls = 0;
  int signals = 0;
  int faults = 0;
  int mappings = 0;
  int times = 0;
  int id = 0;
  int status = 0;
  int all = 0;
  int nlwp;
  int *lwps;

  old_chain = make_cleanup (null_cleanup, 0);

  /* Default to using the current inferior if no pid specified.  Note
     that inferior_pid may be 0, hence we set okerr.  */

  pid = inferior_pid & 0x7fffffff;		/* strip off sol-thread bit */
  if (!(pip = find_procinfo (pid, 1)))		/* inferior_pid no good?  */
    pip = procinfo_list;			/* take first available */
  pid = pid & 0xffff;				/* extract "real" pid */

  if (args != NULL)
    {
      if ((argv = buildargv (args)) == NULL)
	{
	  nomem (0);
	}
      make_cleanup (freeargv, (char *) argv);

      while (*argv != NULL)
	{
	  argsize = strlen (*argv);
	  if (argsize >= 1 && strncmp (*argv, "all", argsize) == 0)
	    {
	      summary = 0;
	      all = 1;
	    }
	  else if (argsize >= 2 && strncmp (*argv, "faults", argsize) == 0)
	    {
	      summary = 0;
	      faults = 1;
	    }
	  else if (argsize >= 2 && strncmp (*argv, "flags", argsize) == 0)
	    {
	      summary = 0;
	      flags = 1;
	    }
	  else if (argsize >= 1 && strncmp (*argv, "id", argsize) == 0)
	    {
	      summary = 0;
	      id = 1;
	    }
	  else if (argsize >= 1 && strncmp (*argv, "mappings", argsize) == 0)
	    {
	      summary = 0;
	      mappings = 1;
	    }
	  else if (argsize >= 2 && strncmp (*argv, "signals", argsize) == 0)
	    {
	      summary = 0;
	      signals = 1;
	    }
	  else if (argsize >= 2 && strncmp (*argv, "status", argsize) == 0)
	    {
	      summary = 0;
	      status = 1;
	    }
	  else if (argsize >= 2 && strncmp (*argv, "syscalls", argsize) == 0)
	    {
	      summary = 0;
	      syscalls = 1;
	    }
	  else if (argsize >= 1 && strncmp (*argv, "times", argsize) == 0)
	    {
	      summary = 0;
	      times = 1;
	    }
	  else if ((pid = atoi (*argv)) > 0)
	    {
	      pip = (struct procinfo *) xmalloc (sizeof (struct procinfo));
	      memset (pip, 0, sizeof (*pip));

	      pip->pid = pid;
	      if (!open_proc_file (pid, pip, O_RDONLY, 0))
		{
		  perror_with_name (pip -> pathname);
		  /* NOTREACHED */
		}
	      pid = pip->pid;
	      make_cleanup (close_proc_file, pip);
	    }
	  else if (**argv != '\000')
	    {
	      error ("Unrecognized or ambiguous keyword `%s'.", *argv);
	    }
	  argv++;
	}
    }

  /* If we don't have a valid open process at this point, then we have no
     inferior or didn't specify a specific pid. */

  if (!pip)
    {
      error ("\
No process.  Start debugging a program or specify an explicit process ID.");
    }

  if (!procfs_read_status (pip))
    {
      print_sys_errmsg (pip -> pathname, errno);
      error ("procfs_read_status failed");
    }

#ifndef PROCFS_USE_READ_WRITE
#ifdef PIOCLWPIDS
  nlwp = pip->prstatus.pr_nlwp;
  lwps = alloca ((2 * nlwp + 2) * sizeof (*lwps));

  if (ioctl (pip->ctl_fd, PIOCLWPIDS, lwps))
    {
      print_sys_errmsg (pip -> pathname, errno);
      error ("PIOCLWPIDS failed");
    }
#else /* PIOCLWPIDS */
  nlwp = 1;
  lwps = alloca ((2 * nlwp + 2) * sizeof *lwps);
  lwps[0] = 0;
#endif /* PIOCLWPIDS */

  for (; nlwp > 0; nlwp--, lwps++)
    {
      pip = find_procinfo ((*lwps << 16) | pid, 1);

      if (!pip)
	{
	  pip = (struct procinfo *) xmalloc (sizeof (struct procinfo));
	  memset (pip, 0, sizeof (*pip));
	  if (!open_proc_file ((*lwps << 16) | pid, pip, O_RDONLY, 0))
	    continue;

	  make_cleanup (close_proc_file, pip);

	  if (!procfs_read_status (pip))
	    {
	      print_sys_errmsg (pip -> pathname, errno);
	      error ("procfs_read_status failed");
	    }
	}

#endif /* PROCFS_USE_READ_WRITE */

      /* Print verbose information of the requested type(s), or just a summary
	 of the information for all types. */

      printf_filtered ("\nInformation for %s.%d:\n\n", pip -> pathname, *lwps);
      if (summary || all || flags)
	{
	  info_proc_flags (pip, summary);
	}
      if (summary || all)
	{
	  info_proc_stop (pip, summary);
#ifdef UNIXWARE
	  supply_gregset (&pip->prstatus.pr_lwp.pr_context.uc_mcontext.gregs);
#else
	  supply_gregset (&pip->prstatus.pr_reg);
#endif
	  printf_filtered ("PC: ");
	  print_address (read_pc (), gdb_stdout);
	  printf_filtered ("\n");
	}
      if (summary || all || signals || faults)
	{
	  info_proc_siginfo (pip, summary);
	}
      if (summary || all || syscalls)
	{
	  info_proc_syscalls (pip, summary);
	}
      if (summary || all || mappings)
	{
	  info_proc_mappings (pip, summary);
	}
      if (summary || all || signals)
	{
	  info_proc_signals (pip, summary);
	}
      if (summary || all || faults)
	{
	  info_proc_faults (pip, summary);
	}
      printf_filtered ("\n");

      /* All done, deal with closing any temporary process info structure,
	 freeing temporary memory , etc. */

      do_cleanups (old_chain);
#ifndef PROCFS_USE_READ_WRITE
    }
#endif
}

/*

LOCAL FUNCTION

	modify_inherit_on_fork_flag - Change the inherit-on-fork flag

SYNOPSIS

	void modify_inherit_on_fork_flag (fd, flag)

DESCRIPTION

	Call this routine to modify the inherit-on-fork flag.  This routine is
	just a nice wrapper to hide the #ifdefs needed by various systems to
	control this flag.

 */

static void
modify_inherit_on_fork_flag (fd, flag)
     int fd;
     int flag;
{
#if defined (PIOCSET) || defined (PCSET)
  long pr_flags;
#endif
  int retval = 0;
  struct proc_ctl pctl;

#if defined (PIOCSET) || defined (PCSET)	/* New method */
  pr_flags = PR_FORK;
  if (flag)
    {
#ifdef PROCFS_USE_READ_WRITE
      pctl.cmd = PCSET;
      pctl.data = PR_FORK;
      if (write (fd, (char *) &pctl, sizeof (struct proc_ctl)) < 0)
	retval = -1;
#else
      retval = ioctl (fd, PIOCSET, &pr_flags);
#endif
    }
  else
    {
#ifdef PROCFS_USE_READ_WRITE
      pctl.cmd = PCRESET;
      pctl.data = PR_FORK;
      if (write (fd, (char *) &pctl, sizeof (struct proc_ctl)) < 0)
	retval = -1;
#else
      retval = ioctl (fd, PIOCRESET, &pr_flags);
#endif
    }

#else
#ifdef PIOCSFORK		/* Original method */
  if (flag)
    {
      retval = ioctl (fd, PIOCSFORK, NULL);
    }
  else
    {
      retval = ioctl (fd, PIOCRFORK, NULL);
    }
#else
  Neither PR_FORK nor PIOCSFORK exist!!!
#endif
#endif

  if (!retval)
    return;

  print_sys_errmsg ("modify_inherit_on_fork_flag", errno);
  error ("PIOCSFORK or PR_FORK modification failed");
}

/*

LOCAL FUNCTION

	modify_run_on_last_close_flag - Change the run-on-last-close flag

SYNOPSIS

	void modify_run_on_last_close_flag (fd, flag)

DESCRIPTION

	Call this routine to modify the run-on-last-close flag.  This routine
	is just a nice wrapper to hide the #ifdefs needed by various systems to
	control this flag.

 */

static void
modify_run_on_last_close_flag (fd, flag)
     int fd;
     int flag;
{
#if defined (PIOCSET) || defined (PCSET)
  long pr_flags;
#endif
  int retval = 0;
  struct proc_ctl pctl;

#if defined (PIOCSET) || defined (PCSET)	/* New method */
  pr_flags = PR_RLC;
  if (flag)
    {
#ifdef PROCFS_USE_READ_WRITE
      pctl.cmd = PCSET;
      pctl.data = PR_RLC;
      if (write (fd, (char *) &pctl, sizeof (struct proc_ctl)) < 0)
	retval = -1;
#else
      retval = ioctl (fd, PIOCSET, &pr_flags);
#endif
    }
  else
    {
#ifdef PROCFS_USE_READ_WRITE
      pctl.cmd = PCRESET;
      pctl.data = PR_RLC;
      if (write (fd, (char *) &pctl, sizeof (struct proc_ctl)) < 0)
	retval = -1;
#else
      retval = ioctl (fd, PIOCRESET, &pr_flags);
#endif
    }

#else
#ifdef PIOCSRLC			/* Original method */
  if (flag)
    retval = ioctl (fd, PIOCSRLC, NULL);
  else
    retval = ioctl (fd, PIOCRRLC, NULL);
#else
  Neither PR_RLC nor PIOCSRLC exist!!!
#endif
#endif

  if (!retval)
    return;

  print_sys_errmsg ("modify_run_on_last_close_flag", errno);
  error ("PIOCSRLC or PR_RLC modification failed");
}

/*

LOCAL FUNCTION

	procfs_clear_syscall_trap -- Deletes the trap for the specified system call.

SYNOPSIS

	void procfs_clear_syscall_trap (struct procinfo *, int syscall_num, int errok)

DESCRIPTION

	This function function disables traps for the specified system call.
	errok is non-zero if errors should be ignored.
 */

static void
procfs_clear_syscall_trap (pi, syscall_num, errok)
     struct procinfo *pi;
     int syscall_num;
     int errok;
{
  sysset_t sysset;
  int goterr, i;

#ifndef UNIXWARE
  goterr = ioctl (pi->ctl_fd, PIOCGENTRY, &sysset) < 0;

  if (goterr && !errok)
    {
      print_sys_errmsg (pi->pathname, errno);
      error ("PIOCGENTRY failed");
    }

  if (!goterr)
    {
      prdelset (&sysset, syscall_num);

      if ((ioctl (pi->ctl_fd, PIOCSENTRY, &sysset) < 0) && !errok)
	{
	  print_sys_errmsg (pi->pathname, errno);
	  error ("PIOCSENTRY failed");
	}
    }

  goterr = ioctl (pi->ctl_fd, PIOCGEXIT, &sysset) < 0;

  if (goterr && !errok)
    {
      procfs_clear_syscall_trap (pi, syscall_num, 1);
      print_sys_errmsg (pi->pathname, errno);
      error ("PIOCGEXIT failed");
    }

  if (!goterr)
    {
      praddset (&sysset, syscall_num);

      if ((ioctl (pi->ctl_fd, PIOCSEXIT, &sysset) < 0) && !errok)
	{
	  procfs_clear_syscall_trap (pi, syscall_num, 1);
	  print_sys_errmsg (pi->pathname, errno);
	  error ("PIOCSEXIT failed");
	}
    }
#endif

  if (!pi->syscall_handlers)
    {
      if (!errok)
	error ("procfs_clear_syscall_trap:  syscall_handlers is empty");
      return;
    }

  /* Remove handler func from the handler list */

  for (i = 0; i < pi->num_syscall_handlers; i++)
    if (pi->syscall_handlers[i].syscall_num == syscall_num)
      {
	if (i + 1 != pi->num_syscall_handlers)
	  {			/* Not the last entry.
				   Move subsequent entries fwd. */
	    memcpy (&pi->syscall_handlers[i], &pi->syscall_handlers[i + 1],
		    (pi->num_syscall_handlers - i - 1)
		    * sizeof (struct procfs_syscall_handler));
	  }

	pi->syscall_handlers = xrealloc (pi->syscall_handlers,
					 (pi->num_syscall_handlers - 1)
					 * sizeof (struct procfs_syscall_handler));
	pi->num_syscall_handlers--;
	return;
      }

  if (!errok)
    error ("procfs_clear_syscall_trap:  Couldn't find handler for sys call %d",
	   syscall_num);
}

/*

LOCAL FUNCTION

	procfs_set_syscall_trap -- arrange for a function to be called when the
				   child executes the specified system call.

SYNOPSIS

	void procfs_set_syscall_trap (struct procinfo *, int syscall_num, int flags,
				      syscall_func_t *function)

DESCRIPTION

	This function sets up an entry and/or exit trap for the specified system
	call.  When the child executes the specified system call, your function
	will be	called with the call #, a flag that indicates entry or exit, and
	pointers to rtnval and statval (which are used by procfs_wait).  The
	function should return non-zero if something interesting happened, zero
	otherwise.
 */

static void
procfs_set_syscall_trap (pi, syscall_num, flags, func)
     struct procinfo *pi;
     int syscall_num;
     int flags;
     syscall_func_t *func;
{
  sysset_t sysset;

#ifndef UNIXWARE
  if (flags & PROCFS_SYSCALL_ENTRY)
    {
      if (ioctl (pi->ctl_fd, PIOCGENTRY, &sysset) < 0)
	{
	  print_sys_errmsg (pi->pathname, errno);
	  error ("PIOCGENTRY failed");
	}

      praddset (&sysset, syscall_num);

      if (ioctl (pi->ctl_fd, PIOCSENTRY, &sysset) < 0)
	{
	  print_sys_errmsg (pi->pathname, errno);
	  error ("PIOCSENTRY failed");
	}
    }

  if (flags & PROCFS_SYSCALL_EXIT)
    {
      if (ioctl (pi->ctl_fd, PIOCGEXIT, &sysset) < 0)
	{
	  procfs_clear_syscall_trap (pi, syscall_num, 1);
	  print_sys_errmsg (pi->pathname, errno);
	  error ("PIOCGEXIT failed");
	}

      praddset (&sysset, syscall_num);

      if (ioctl (pi->ctl_fd, PIOCSEXIT, &sysset) < 0)
	{
	  procfs_clear_syscall_trap (pi, syscall_num, 1);
	  print_sys_errmsg (pi->pathname, errno);
	  error ("PIOCSEXIT failed");
	}
    }
#endif

  if (!pi->syscall_handlers)
    {
      pi->syscall_handlers = xmalloc (sizeof (struct procfs_syscall_handler));
      pi->syscall_handlers[0].syscall_num = syscall_num;
      pi->syscall_handlers[0].func = func;
      pi->num_syscall_handlers = 1;
    }
  else
    {
      int i;

      for (i = 0; i < pi->num_syscall_handlers; i++)
	if (pi->syscall_handlers[i].syscall_num == syscall_num)
	  {
	    pi->syscall_handlers[i].func = func;
	    return;
	  }

      pi->syscall_handlers = xrealloc (pi->syscall_handlers, (i + 1)
				       * sizeof (struct procfs_syscall_handler));
      pi->syscall_handlers[i].syscall_num = syscall_num;
      pi->syscall_handlers[i].func = func;
      pi->num_syscall_handlers++;
    }
}

#ifdef SYS_lwp_create

/*

LOCAL FUNCTION

	procfs_lwp_creation_handler - handle exit from the _lwp_create syscall

SYNOPSIS

	int procfs_lwp_creation_handler (pi, syscall_num, why, rtnvalp, statvalp)

DESCRIPTION

	This routine is called both when an inferior process and it's new lwp
	are about to finish a _lwp_create() system call.  This is the system
	call that Solaris uses to create a lightweight process.  When the
	target process gets this event, we can look at sysarg[2] to find the
	new childs lwp ID, and create a procinfo struct from that.  After that,
	we pretend that we got a SIGTRAP, and return non-zero to tell
	procfs_wait to wake up.  Subsequently, wait_for_inferior gets woken up,
	sees the new process and continues it.

	When we see the child exiting from lwp_create, we just contine it,
	since everything was handled when the parent trapped.

NOTES
	In effect, we are only paying attention to the parent's completion of
	the lwp_create syscall.  If we only paid attention to the child
	instead, then we wouldn't detect the creation of a suspended thread.
 */

static int
procfs_lwp_creation_handler (pi, syscall_num, why, rtnvalp, statvalp)
     struct procinfo *pi;
     int syscall_num;
     int why;
     int *rtnvalp;
     int *statvalp;
{
  int lwp_id;
  struct procinfo *childpi;
  struct proc_ctl pctl;

  /* We've just detected the completion of an lwp_create system call.  Now we
     need to setup a procinfo struct for this thread, and notify the thread
     system of the new arrival.  */

  /* If lwp_create failed, then nothing interesting happened.  Continue the
     process and go back to sleep. */

#ifdef UNIXWARE
  /* Joel ... can you check this logic out please? JKJ */
  if (pi->prstatus.pr_lwp.pr_context.uc_mcontext.gregs[R_EFL] & 1)
    { /* _lwp_create failed */
      pctl.cmd = PCRUN;
      pctl.data = PRCFAULT;

      if (write (pi->ctl_fd, (char *) &pctl, sizeof (struct proc_ctl)) < 0)
	perror_with_name (pi->pathname);

      return 0;
    }
#else /* UNIXWARE */
  if (PROCFS_GET_CARRY (pi->prstatus.pr_reg))
    {				/* _lwp_create failed */
      pi->prrun.pr_flags &= PRSTEP;
      pi->prrun.pr_flags |= PRCFAULT;

      if (ioctl (pi->ctl_fd, PIOCRUN, &pi->prrun) != 0)
	perror_with_name (pi->pathname);

      return 0;
    }
#endif

  /* At this point, the new thread is stopped at it's first instruction, and
     the parent is stopped at the exit from lwp_create.  */

  if (pi->new_child)		/* Child? */
    {				/* Yes, just continue it */
#ifdef UNIXWARE
      pctl.cmd = PCRUN;
      pctl.data = PRCFAULT;

      if (write(pi->ctl_fd, (char *)&pctl, sizeof (struct proc_ctl)) < 0)
#else /* !UNIXWARE */
      pi->prrun.pr_flags &= PRSTEP;
      pi->prrun.pr_flags |= PRCFAULT;

      if ((pi->prstatus.pr_flags & PR_ISTOP)
	  && ioctl (pi->ctl_fd, PIOCRUN, &pi->prrun) != 0)
#endif /* !UNIXWARE */
	perror_with_name (pi->pathname);

      pi->new_child = 0;	/* No longer new */

      return 0;
    }

  /* We're the proud parent of a new thread.  Setup an exit trap for lwp_create
     in the child and continue the parent.  */

  /* Third arg is pointer to new thread id. */
  lwp_id = read_memory_integer (
     THE_PR_LWP(pi->prstatus).pr_sysarg[2], sizeof (int));

  lwp_id = (lwp_id << 16) | PIDGET (pi->pid);

  childpi = create_procinfo (lwp_id);

  /* The new process has actually inherited the lwp_create syscall trap from
     it's parent, but we still have to call this to register handlers for
     that child.  */

  procfs_set_inferior_syscall_traps (childpi);
  add_thread (lwp_id);
  printf_filtered ("[New %s]\n", target_pid_to_str (lwp_id));

  /* Continue the parent */
#ifdef UNIXWARE
  pctl.cmd = PCRUN;
  pctl.data = PRCFAULT;

  if (write(pi->ctl_fd, (char *)&pctl, sizeof (struct proc_ctl)) < 0)
#else
  pi->prrun.pr_flags &= PRSTEP;
  pi->prrun.pr_flags |= PRCFAULT;
  if (ioctl (pi->ctl_fd, PIOCRUN, &pi->prrun) != 0)
#endif
    perror_with_name (pi->pathname);

  /* The new child may have been created in one of two states: 
     SUSPENDED or RUNNABLE.  If runnable, we will simply signal it to run.
     If suspended, we flag it to be continued later, when it has an event.  */

  if (THE_PR_LWP(childpi->prstatus).pr_why == PR_SUSPENDED)
    childpi->new_child = 1;	/* Flag this as an unseen child process */
  else
    {
      /* Continue the child */
#ifdef UNIXWARE
      pctl.cmd = PCRUN;
      pctl.data = PRCFAULT;

      if (write(pi->ctl_fd, (char *)&pctl, sizeof (struct proc_ctl)) < 0)
#else
      childpi->prrun.pr_flags &= PRSTEP;
      childpi->prrun.pr_flags |= PRCFAULT;

      if (ioctl (childpi->ctl_fd, PIOCRUN, &childpi->prrun) != 0)
#endif
	perror_with_name (childpi->pathname);
    }
  return 0;
}
#endif /* SYS_lwp_create */

/* Fork an inferior process, and start debugging it with /proc.  */

static void
procfs_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  char *shell_file = getenv ("SHELL");
  char *tryname;
  if (shell_file != NULL && strchr (shell_file, '/') == NULL)
    {

      /* We will be looking down the PATH to find shell_file.  If we
	 just do this the normal way (via execlp, which operates by
	 attempting an exec for each element of the PATH until it
	 finds one which succeeds), then there will be an exec for
	 each failed attempt, each of which will cause a PR_SYSEXIT
	 stop, and we won't know how to distinguish the PR_SYSEXIT's
	 for these failed execs with the ones for successful execs
	 (whether the exec has succeeded is stored at that time in the
	 carry bit or some such architecture-specific and
	 non-ABI-specified place).

	 So I can't think of anything better than to search the PATH
	 now.  This has several disadvantages: (1) There is a race
	 condition; if we find a file now and it is deleted before we
	 exec it, we lose, even if the deletion leaves a valid file
	 further down in the PATH, (2) there is no way to know exactly
	 what an executable (in the sense of "capable of being
	 exec'd") file is.  Using access() loses because it may lose
	 if the caller is the superuser; failing to use it loses if
	 there are ACLs or some such.  */

      char *p;
      char *p1;
      /* FIXME-maybe: might want "set path" command so user can change what
	 path is used from within GDB.  */
      char *path = getenv ("PATH");
      int len;
      struct stat statbuf;

      if (path == NULL)
	path = "/bin:/usr/bin";

      tryname = alloca (strlen (path) + strlen (shell_file) + 2);
      for (p = path; p != NULL; p = p1 ? p1 + 1: NULL)
	{
	  p1 = strchr (p, ':');
	  if (p1 != NULL)
	    len = p1 - p;
	  else
	    len = strlen (p);
	  strncpy (tryname, p, len);
	  tryname[len] = '\0';
	  strcat (tryname, "/");
	  strcat (tryname, shell_file);
	  if (access (tryname, X_OK) < 0)
	    continue;
	  if (stat (tryname, &statbuf) < 0)
	    continue;
	  if (!S_ISREG (statbuf.st_mode))
	    /* We certainly need to reject directories.  I'm not quite
	       as sure about FIFOs, sockets, etc., but I kind of doubt
	       that people want to exec() these things.  */
	    continue;
	  break;
	}
      if (p == NULL)
	/* Not found.  This must be an error rather than merely passing
	   the file to execlp(), because execlp() would try all the
	   exec()s, causing GDB to get confused.  */
	error ("Can't find shell %s in PATH", shell_file);

      shell_file = tryname;
    }

  fork_inferior (exec_file, allargs, env,
		 proc_set_exec_trap, procfs_init_inferior, NULL, shell_file);

  /* We are at the first instruction we care about.  */
  /* Pedal to the metal... */

  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_0, 0);
}

/* Clean up after the inferior dies.  */

static void
procfs_mourn_inferior ()
{
  struct procinfo *pi;
  struct procinfo *next_pi;

  for (pi = procinfo_list; pi; pi = next_pi)
    {
      next_pi = pi->next;
      unconditionally_kill_inferior (pi);
    }

  unpush_target (&procfs_ops);
  generic_mourn_inferior ();
}


/* Mark our target-struct as eligible for stray "run" and "attach" commands.  */
static int
procfs_can_run ()
{
  /* This variable is controlled by modules that sit atop procfs that may layer
     their own process structure atop that provided here.  sol-thread.c does
     this because of the Solaris two-level thread model.  */

  return !procfs_suppress_run;
}
#ifdef TARGET_HAS_HARDWARE_WATCHPOINTS
#ifndef UNIXWARE

/* Insert a watchpoint */
int
procfs_set_watchpoint(pid, addr, len, rw)
     int		pid;
     CORE_ADDR		addr;
     int		len;
     int		rw;
{
  struct procinfo	*pi;
  prwatch_t		wpt;

  pi = find_procinfo (pid == -1 ? inferior_pid : pid, 0);
  wpt.pr_vaddr = (caddr_t)addr;
  wpt.pr_size = len;
  wpt.pr_wflags = ((rw & 1) ? MA_READ : 0) | ((rw & 2) ? MA_WRITE : 0);
  if (ioctl (pi->ctl_fd, PIOCSWATCH, &wpt) < 0)
    {
      if (errno == E2BIG)
	return -1;
      /* Currently it sometimes happens that the same watchpoint gets
	 deleted twice - don't die in this case (FIXME please) */
      if (errno == ESRCH && len == 0)
	return 0;
      print_sys_errmsg (pi->pathname, errno);
      error ("PIOCSWATCH failed");
    }
  return 0;
}

int
procfs_stopped_by_watchpoint(pid)
    int			pid;
{
  struct procinfo	*pi;
  short 		what;
  short 		why;

  pi = find_procinfo (pid == -1 ? inferior_pid : pid, 0);
  if (pi->prstatus.pr_flags & (PR_STOPPED | PR_ISTOP))
    {
      why = pi->prstatus.pr_why;
      what = pi->prstatus.pr_what;
      if (why == PR_FAULTED 
#if defined (FLTWATCH) && defined (FLTKWATCH)
	  && (what == FLTWATCH || what == FLTKWATCH)
#else
#ifdef FLTWATCH
	  && (what == FLTWATCH) 
#endif
#ifdef FLTKWATCH
	  && (what == FLTKWATCH)
#endif
#endif
	  )
	return what;
    }
  return 0;
}
#endif /* !UNIXWARE */
#endif /* TARGET_HAS_HARDWARE_WATCHPOINTS */

/* Why is this necessary?  Shouldn't dead threads just be removed from the
   thread database?  */

static int
procfs_thread_alive (pid)
     int pid;
{
  struct procinfo *pi, *next_pi;

  for (pi = procinfo_list; pi; pi = next_pi)
    {
      next_pi = pi->next;
      if (pi -> pid == pid)
	if (procfs_read_status (pi))	/* alive */
	  return 1;
	else				/* defunct (exited) */
	  {
	    close_proc_file (pi);
	    return 0;
	  }
    }
  return 0;
}

int
procfs_first_available ()
{
  struct procinfo *pi;

  for (pi = procinfo_list; pi; pi = pi->next)
    {
      if (procfs_read_status (pi))
	return pi->pid;
    }
  return -1;
}

int
procfs_get_pid_fd (pid)
     int pid;
{
  struct procinfo *pi = find_procinfo (pid, 1);

  if (pi == NULL)
    return -1;

  return pi->ctl_fd;
}

/* Send a SIGINT to the process group.  This acts just like the user typed a
   ^C on the controlling terminal.

   XXX - This may not be correct for all systems.  Some may want to use
   killpg() instead of kill (-pgrp). */

static void
procfs_stop ()
{
  extern pid_t inferior_process_group;

  kill (-inferior_process_group, SIGINT);
}

/* Convert a pid to printable form. */

#ifdef TIDGET
char *
procfs_pid_to_str (pid)
     int pid;
{
  static char buf[100];

  sprintf (buf, "Kernel thread %d", TIDGET (pid));

  return buf;
}
#endif /* TIDGET */


static void
init_procfs_ops ()
{
  procfs_ops.to_shortname = "procfs";
  procfs_ops.to_longname = "Unix /proc child process";
  procfs_ops.to_doc = "Unix /proc child process (started by the \"run\" command).";
  procfs_ops.to_open = procfs_open;
  procfs_ops.to_attach = procfs_attach;
  procfs_ops.to_detach = procfs_detach;
  procfs_ops.to_resume = procfs_resume;
  procfs_ops.to_wait = procfs_wait;
  procfs_ops.to_fetch_registers = procfs_fetch_registers;
  procfs_ops.to_store_registers = procfs_store_registers;
  procfs_ops.to_prepare_to_store = procfs_prepare_to_store;
  procfs_ops.to_xfer_memory = procfs_xfer_memory;
  procfs_ops.to_files_info = procfs_files_info;
  procfs_ops.to_insert_breakpoint = memory_insert_breakpoint;
  procfs_ops.to_remove_breakpoint = memory_remove_breakpoint;
  procfs_ops.to_terminal_init = terminal_init_inferior;
  procfs_ops.to_terminal_inferior = terminal_inferior;
  procfs_ops.to_terminal_ours_for_output = terminal_ours_for_output;
  procfs_ops.to_terminal_ours = terminal_ours;
  procfs_ops.to_terminal_info = child_terminal_info;
  procfs_ops.to_kill = procfs_kill_inferior;
  procfs_ops.to_create_inferior = procfs_create_inferior;
  procfs_ops.to_mourn_inferior = procfs_mourn_inferior;
  procfs_ops.to_can_run = procfs_can_run;
  procfs_ops.to_notice_signals = procfs_notice_signals;
  procfs_ops.to_thread_alive = procfs_thread_alive;
  procfs_ops.to_stop = procfs_stop;
  procfs_ops.to_stratum = process_stratum;
  procfs_ops.to_has_all_memory = 1;
  procfs_ops.to_has_memory = 1;
  procfs_ops.to_has_stack = 1;
  procfs_ops.to_has_registers = 1;
  procfs_ops.to_has_execution = 1;
  procfs_ops.to_magic = OPS_MAGIC;
}

void
_initialize_procfs ()
{
#ifdef HAVE_OPTIONAL_PROC_FS
  char procname[MAX_PROC_NAME_SIZE];
  int fd;

  /* If we have an optional /proc filesystem (e.g. under OSF/1),
     don't add procfs support if we cannot access the running
     GDB via /proc.  */
  sprintf (procname, STATUS_PROC_NAME_FMT, getpid ());
  if ((fd = open (procname, O_RDONLY)) < 0)
    return;
  close (fd);
#endif

  init_procfs_ops ();
  add_target (&procfs_ops);

  add_info ("processes", info_proc, 
"Show process status information using /proc entry.\n\
Specify process id or use current inferior by default.\n\
Specify keywords for detailed information; default is summary.\n\
Keywords are: `all', `faults', `flags', `id', `mappings', `signals',\n\
`status', `syscalls', and `times'.\n\
Unambiguous abbreviations may be used.");

  init_syscall_table ();
}
