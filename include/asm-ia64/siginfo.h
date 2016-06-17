#ifndef _ASM_IA64_SIGINFO_H
#define _ASM_IA64_SIGINFO_H

/*
 * Based on <asm-i386/siginfo.h>.
 *
 * Modified 1998-2002
 *	David Mosberger-Tang <davidm@hpl.hp.com>, Hewlett-Packard Co
 */

#include <linux/types.h>

typedef union sigval {
	int sival_int;
	void *sival_ptr;
} sigval_t;

#define SI_MAX_SIZE	128
#define SI_PAD_SIZE	((SI_MAX_SIZE/sizeof(int)) - 4)

typedef struct siginfo {
	int si_signo;
	int si_errno;
	int si_code;
	int __pad0;

	union {
		int _pad[SI_PAD_SIZE];

		/* kill() */
		struct {
			pid_t _pid;		/* sender's pid */
			uid_t _uid;		/* sender's uid */
		} _kill;

		/* POSIX.1b timers */
		struct {
			unsigned int _timer1;
			unsigned int _timer2;
		} _timer;

		/* POSIX.1b signals */
		struct {
			pid_t _pid;		/* sender's pid */
			uid_t _uid;		/* sender's uid */
			sigval_t _sigval;
		} _rt;

		/* SIGCHLD */
		struct {
			pid_t _pid;		/* which child */
			uid_t _uid;		/* sender's uid */
			int _status;		/* exit code */
			clock_t _utime;
			clock_t _stime;
		} _sigchld;

		/* SIGILL, SIGFPE, SIGSEGV, SIGBUS */
		struct {
			void *_addr;		/* faulting insn/memory ref. */
			int _imm;		/* immediate value for "break" */
			unsigned int _flags;	/* see below */
			unsigned long _isr;	/* isr */
		} _sigfault;

		/* SIGPOLL */
		struct {
			long _band;	/* POLL_IN, POLL_OUT, POLL_MSG (XPG requires a "long") */
			int _fd;
		} _sigpoll;
		/* SIGPROF */
		struct {
			pid_t _pid;		/* which child */
			uid_t _uid;		/* sender's uid */
			unsigned long _pfm_ovfl_counters[4]; /* which PMU counter overflowed */
		} _sigprof;
	} _sifields;
} siginfo_t;

/*
 * How these fields are to be accessed.
 */
#define si_pid		_sifields._kill._pid
#define si_uid		_sifields._kill._uid
#define si_status	_sifields._sigchld._status
#define si_utime	_sifields._sigchld._utime
#define si_stime	_sifields._sigchld._stime
#define si_value	_sifields._rt._sigval
#define si_int		_sifields._rt._sigval.sival_int
#define si_ptr		_sifields._rt._sigval.sival_ptr
#define si_addr		_sifields._sigfault._addr
#define si_imm		_sifields._sigfault._imm	/* as per UNIX SysV ABI spec */
#define si_flags	_sifields._sigfault._flags
/*
 * si_isr is valid for SIGILL, SIGFPE, SIGSEGV, SIGBUS, and SIGTRAP provided that
 * si_code is non-zero and __ISR_VALID is set in si_flags.
 */
#define si_isr		_sifields._sigfault._isr
#define si_band		_sifields._sigpoll._band
#define si_fd		_sifields._sigpoll._fd
#define si_pfm_ovfl	_sifields._sigprof._pfm_ovfl_counters

/*
 * Flag values for si_flags:
 */
#define __ISR_VALID_BIT	0
#define __ISR_VALID	(1 << __ISR_VALID_BIT)

/*
 * si_code values
 * Positive values for kernel-generated signals.
 */
#ifdef __KERNEL__
#define __SI_MASK	0xffff0000
#define __SI_KILL	(0 << 16)
#define __SI_TIMER	(1 << 16)
#define __SI_POLL	(2 << 16)
#define __SI_FAULT	(3 << 16)
#define __SI_CHLD	(4 << 16)
#define __SI_RT		(5 << 16)
#define __SI_PROF	(6 << 16)
#define __SI_CODE(T,N)	((T) << 16 | ((N) & 0xffff))
#else
#define __SI_KILL	0
#define __SI_TIMER	0
#define __SI_POLL	0
#define __SI_FAULT	0
#define __SI_CHLD	0
#define __SI_RT		0
#define __SI_CODE(T,N)	(N)
#endif

#define SI_USER		0		/* sent by kill, sigsend, raise */
#define SI_KERNEL	0x80		/* sent by the kernel from somewhere */
#define SI_QUEUE	(-1)		/* sent by sigqueue */
#define SI_TIMER __SI_CODE(__SI_TIMER,-2) /* sent by timer expiration */
#define SI_MESGQ	(-3)		/* sent by real time mesq state change */
#define SI_ASYNCIO	(-4)		/* sent by AIO completion */
#define SI_SIGIO	(-5)		/* sent by queued SIGIO */
#define SI_TKILL	(-6)		/* sent by tkill system call */

#define SI_FROMUSER(siptr)	((siptr)->si_code <= 0)
#define SI_FROMKERNEL(siptr)	((siptr)->si_code > 0)

/*
 * SIGILL si_codes
 */
#define ILL_ILLOPC	(__SI_FAULT|1)	/* illegal opcode */
#define ILL_ILLOPN	(__SI_FAULT|2)	/* illegal operand */
#define ILL_ILLADR	(__SI_FAULT|3)	/* illegal addressing mode */
#define ILL_ILLTRP	(__SI_FAULT|4)	/* illegal trap */
#define ILL_PRVOPC	(__SI_FAULT|5)	/* privileged opcode */
#define ILL_PRVREG	(__SI_FAULT|6)	/* privileged register */
#define ILL_COPROC	(__SI_FAULT|7)	/* coprocessor error */
#define ILL_BADSTK	(__SI_FAULT|8)	/* internal stack error */
#define ILL_BADIADDR	(__SI_FAULT|9)	/* unimplemented instruction address */
#define __ILL_BREAK	(__SI_FAULT|10)	/* illegal break */
#define __ILL_BNDMOD	(__SI_FAULT|11)	/* bundle-update (modification) in progress */
#define NSIGILL		11

/*
 * SIGFPE si_codes
 */
#define FPE_INTDIV	(__SI_FAULT|1)	/* integer divide by zero */
#define FPE_INTOVF	(__SI_FAULT|2)	/* integer overflow */
#define FPE_FLTDIV	(__SI_FAULT|3)	/* floating point divide by zero */
#define FPE_FLTOVF	(__SI_FAULT|4)	/* floating point overflow */
#define FPE_FLTUND	(__SI_FAULT|5)	/* floating point underflow */
#define FPE_FLTRES	(__SI_FAULT|6)	/* floating point inexact result */
#define FPE_FLTINV	(__SI_FAULT|7)	/* floating point invalid operation */
#define FPE_FLTSUB	(__SI_FAULT|8)	/* subscript out of range */
#define __FPE_DECOVF	(__SI_FAULT|9)	/* decimal overflow */
#define __FPE_DECDIV	(__SI_FAULT|10)	/* decimal division by zero */
#define __FPE_DECERR	(__SI_FAULT|11)	/* packed decimal error */
#define __FPE_INVASC	(__SI_FAULT|12)	/* invalid ASCII digit */
#define __FPE_INVDEC	(__SI_FAULT|13)	/* invalid decimal digit */
#define NSIGFPE		13

/*
 * SIGSEGV si_codes
 */
#define SEGV_MAPERR	(__SI_FAULT|1)	/* address not mapped to object */
#define SEGV_ACCERR	(__SI_FAULT|2)	/* invalid permissions for mapped object */
#define __SEGV_PSTKOVF	(__SI_FAULT|3)	/* paragraph stack overflow */
#define NSIGSEGV	3

/*
 * SIGBUS si_codes
 */
#define BUS_ADRALN	(__SI_FAULT|1)	/* invalid address alignment */
#define BUS_ADRERR	(__SI_FAULT|2)	/* non-existant physical address */
#define BUS_OBJERR	(__SI_FAULT|3)	/* object specific hardware error */
#define NSIGBUS		3

/*
 * SIGTRAP si_codes
 */
#define TRAP_BRKPT	(__SI_FAULT|1)	/* process breakpoint */
#define TRAP_TRACE	(__SI_FAULT|2)	/* process trace trap */
#define TRAP_BRANCH	(__SI_FAULT|3)	/* process taken branch trap */
#define TRAP_HWBKPT	(__SI_FAULT|4)	/* hardware breakpoint or watchpoint */
#define NSIGTRAP	4

/*
 * SIGCHLD si_codes
 */
#define CLD_EXITED	(__SI_CHLD|1)	/* child has exited */
#define CLD_KILLED	(__SI_CHLD|2)	/* child was killed */
#define CLD_DUMPED	(__SI_CHLD|3)	/* child terminated abnormally */
#define CLD_TRAPPED	(__SI_CHLD|4)	/* traced child has trapped */
#define CLD_STOPPED	(__SI_CHLD|5)	/* child has stopped */
#define CLD_CONTINUED	(__SI_CHLD|6)	/* stopped child has continued */
#define NSIGCHLD	6

/*
 * SIGPOLL si_codes
 */
#define POLL_IN		(__SI_POLL|1)	/* data input available */
#define POLL_OUT	(__SI_POLL|2)	/* output buffers available */
#define POLL_MSG	(__SI_POLL|3)	/* input message available */
#define POLL_ERR	(__SI_POLL|4)	/* i/o error */
#define POLL_PRI	(__SI_POLL|5)	/* high priority input available */
#define POLL_HUP	(__SI_POLL|6)	/* device disconnected */
#define NSIGPOLL	6

/*
 * SIGPROF si_codes
 */
#define PROF_OVFL	(__SI_PROF|1)  /* some counters overflowed */

/*
 * sigevent definitions
 *
 * It seems likely that SIGEV_THREAD will have to be handled from userspace, libpthread
 * transmuting it to SIGEV_SIGNAL, which the thread manager then catches and does the
 * appropriate nonsense.  However, everything is written out here so as to not get lost.
 */
#define SIGEV_SIGNAL	0	/* notify via signal */
#define SIGEV_NONE	1	/* other notification: meaningless */
#define SIGEV_THREAD	2	/* deliver via thread creation */

#define SIGEV_MAX_SIZE	64
#define SIGEV_PAD_SIZE	((SIGEV_MAX_SIZE/sizeof(int)) - 4)

typedef struct sigevent {
	sigval_t sigev_value;
	int sigev_signo;
	int sigev_notify;
	union {
		int _pad[SIGEV_PAD_SIZE];

		struct {
			void (*_function)(sigval_t);
			void *_attribute;	/* really pthread_attr_t */
		} _sigev_thread;
	} _sigev_un;
} sigevent_t;

#define sigev_notify_function	_sigev_un._sigev_thread._function
#define sigev_notify_attributes	_sigev_un._sigev_thread._attribute

#ifdef __KERNEL__
#include <linux/string.h>

static inline void
copy_siginfo (siginfo_t *to, siginfo_t *from)
{
	if (from->si_code < 0)
		memcpy(to, from, sizeof(siginfo_t));
	else
		/* _sigprof is currently the largest know union member */
		memcpy(to, from, 4*sizeof(int) + sizeof(from->_sifields._sigprof));
}

extern int copy_siginfo_to_user(siginfo_t *to, siginfo_t *from);
extern int copy_siginfo_from_user(siginfo_t *to, siginfo_t *from);

#endif /* __KERNEL__ */

#endif /* _ASM_IA64_SIGINFO_H */
