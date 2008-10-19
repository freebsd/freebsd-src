/*-
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 1994-1996 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _AMD64_LINUX_H_
#define	_AMD64_LINUX_H_

#include <amd64/linux32/linux32_syscall.h>

/*
 * debugging support
 */
extern u_char linux_debug_map[];
#define	ldebug(name)	isclr(linux_debug_map, LINUX_SYS_linux_ ## name)
#define	ARGS(nm, fmt)	"linux(%ld): "#nm"("fmt")\n", (long)td->td_proc->p_pid
#define	LMSG(fmt)	"linux(%ld): "fmt"\n", (long)td->td_proc->p_pid

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_LINUX);
#endif

#define	LINUX32_USRSTACK	((1ul << 32) - PAGE_SIZE)
/* XXX 16 = sizeof(linux32_ps_strings) */
#define	LINUX32_PS_STRINGS	(LINUX32_USRSTACK - 16)
#define	LINUX32_MAXDSIZ		(512 * 1024 * 1024)	/* 512MB */
#define	LINUX32_MAXSSIZ		(64 * 1024 * 1024)	/* 64MB */
#define	LINUX32_MAXVMEM		0			/* Unlimited */

#define	PTRIN(v)	(void *)(uintptr_t)(v)
#define	PTROUT(v)	(l_uintptr_t)(uintptr_t)(v)

/*
 * Provide a separate set of types for the Linux types.
 */
typedef int		l_int;
typedef int32_t		l_long;
typedef int64_t		l_longlong;
typedef short		l_short;
typedef unsigned int	l_uint;
typedef uint32_t	l_ulong;
typedef uint64_t	l_ulonglong;
typedef unsigned short	l_ushort;

typedef l_ulong		l_uintptr_t;
typedef l_long		l_clock_t;
typedef l_int		l_daddr_t;
typedef l_ushort	l_dev_t;
typedef l_uint		l_gid_t;
typedef l_ushort	l_gid16_t;
typedef l_ulong		l_ino_t;
typedef l_int		l_key_t;
typedef l_longlong	l_loff_t;
typedef l_ushort	l_mode_t;
typedef l_long		l_off_t;
typedef l_int		l_pid_t;
typedef l_uint		l_size_t;
typedef l_long		l_suseconds_t;
typedef l_long		l_time_t;
typedef l_uint		l_uid_t;
typedef l_ushort	l_uid16_t;
typedef l_int		l_timer_t;
typedef l_int		l_mqd_t;

typedef struct {
	l_int		val[2];
} __packed l_fsid_t;

typedef struct {
	l_time_t	tv_sec;
	l_suseconds_t	tv_usec;
} __packed l_timeval;

#define	l_fd_set	fd_set

/*
 * Miscellaneous
 */
#define	LINUX_NAME_MAX		255
#define	LINUX_MAX_UTSNAME	65

#define	LINUX_CTL_MAXNAME	10

struct l___sysctl_args
{
	l_uintptr_t	name;
	l_int		nlen;
	l_uintptr_t	oldval;
	l_uintptr_t	oldlenp;
	l_uintptr_t	newval;
	l_size_t	newlen;
	l_ulong		__spare[4];
} __packed;

/* Scheduling policies */
#define	LINUX_SCHED_OTHER	0
#define	LINUX_SCHED_FIFO	1
#define	LINUX_SCHED_RR		2

/* Resource limits */
#define	LINUX_RLIMIT_CPU	0
#define	LINUX_RLIMIT_FSIZE	1
#define	LINUX_RLIMIT_DATA	2
#define	LINUX_RLIMIT_STACK	3
#define	LINUX_RLIMIT_CORE	4
#define	LINUX_RLIMIT_RSS	5
#define	LINUX_RLIMIT_NPROC	6
#define	LINUX_RLIMIT_NOFILE	7
#define	LINUX_RLIMIT_MEMLOCK	8
#define	LINUX_RLIMIT_AS		9	/* Address space limit */

#define	LINUX_RLIM_NLIMITS	10

struct l_rlimit {
	l_ulong rlim_cur;
	l_ulong rlim_max;
} __packed;

struct l_rusage {
	l_timeval ru_utime;
	l_timeval ru_stime;
	l_long	ru_maxrss;
	l_long	ru_ixrss;
	l_long	ru_idrss;
	l_long	ru_isrss;
	l_long	ru_minflt;
	l_long	ru_majflt;
	l_long	ru_nswap;
	l_long	ru_inblock;
	l_long	ru_oublock;
	l_long	ru_msgsnd;
	l_long	ru_msgrcv;
	l_long	ru_nsignals;
	l_long	ru_nvcsw;
	l_long	ru_nivcsw;
} __packed;

/* mmap options */
#define	LINUX_MAP_SHARED	0x0001
#define	LINUX_MAP_PRIVATE	0x0002
#define	LINUX_MAP_FIXED		0x0010
#define	LINUX_MAP_ANON		0x0020
#define	LINUX_MAP_GROWSDOWN	0x0100

struct l_mmap_argv {
	l_uintptr_t	addr;
	l_size_t	len;
	l_int		prot;
	l_int		flags;
	l_int		fd;
	l_off_t		pgoff;
} __packed;

/*
 * stat family of syscalls
 */
struct l_timespec {
	l_time_t	tv_sec;
	l_long		tv_nsec;
} __packed;

struct l_newstat {
	l_ushort	st_dev;
	l_ushort	__pad1;
	l_ulong		st_ino;
	l_ushort	st_mode;
	l_ushort	st_nlink;
	l_ushort	st_uid;
	l_ushort	st_gid;
	l_ushort	st_rdev;
	l_ushort	__pad2;
	l_ulong		st_size;
	l_ulong		st_blksize;
	l_ulong		st_blocks;
	struct l_timespec	st_atimespec;
	struct l_timespec	st_mtimespec;
	struct l_timespec	st_ctimespec;
	l_ulong		__unused4;
	l_ulong		__unused5;
} __packed;

struct l_stat {
	l_ushort	st_dev;
	l_ulong		st_ino;
	l_ushort	st_mode;
	l_ushort	st_nlink;
	l_ushort	st_uid;
	l_ushort	st_gid;
	l_ushort	st_rdev;
	l_long		st_size;
	struct l_timespec	st_atimespec;
	struct l_timespec	st_mtimespec;
	struct l_timespec	st_ctimespec;
	l_long		st_blksize;
	l_long		st_blocks;
	l_ulong		st_flags;
	l_ulong		st_gen;
};

struct l_stat64 {
	l_ushort	st_dev;
	u_char		__pad0[10];
	l_ulong		__st_ino;
	l_uint		st_mode;
	l_uint		st_nlink;
	l_ulong		st_uid;
	l_ulong		st_gid;
	l_ushort	st_rdev;
	u_char		__pad3[10];
	l_longlong	st_size;
	l_ulong		st_blksize;
	l_ulong		st_blocks;
	l_ulong		__pad4;
	struct l_timespec	st_atimespec;
	struct l_timespec	st_mtimespec;
	struct l_timespec	st_ctimespec;
	l_ulonglong	st_ino;
} __packed;

struct l_statfs64 { 
        l_int           f_type; 
        l_int           f_bsize; 
        uint64_t        f_blocks; 
        uint64_t        f_bfree; 
        uint64_t        f_bavail; 
        uint64_t        f_files; 
        uint64_t        f_ffree; 
        l_fsid_t        f_fsid;
        l_int           f_namelen;
        l_int           f_spare[6];
} __packed;

struct l_new_utsname {
	char	sysname[LINUX_MAX_UTSNAME];
	char	nodename[LINUX_MAX_UTSNAME];
	char	release[LINUX_MAX_UTSNAME];
	char	version[LINUX_MAX_UTSNAME];
	char	machine[LINUX_MAX_UTSNAME];
	char	domainname[LINUX_MAX_UTSNAME];
} __packed;

/*
 * Signalling
 */
#define	LINUX_SIGHUP		1
#define	LINUX_SIGINT		2
#define	LINUX_SIGQUIT		3
#define	LINUX_SIGILL		4
#define	LINUX_SIGTRAP		5
#define	LINUX_SIGABRT		6
#define	LINUX_SIGIOT		LINUX_SIGABRT
#define	LINUX_SIGBUS		7
#define	LINUX_SIGFPE		8
#define	LINUX_SIGKILL		9
#define	LINUX_SIGUSR1		10
#define	LINUX_SIGSEGV		11
#define	LINUX_SIGUSR2		12
#define	LINUX_SIGPIPE		13
#define	LINUX_SIGALRM		14
#define	LINUX_SIGTERM		15
#define	LINUX_SIGSTKFLT		16
#define	LINUX_SIGCHLD		17
#define	LINUX_SIGCONT		18
#define	LINUX_SIGSTOP		19
#define	LINUX_SIGTSTP		20
#define	LINUX_SIGTTIN		21
#define	LINUX_SIGTTOU		22
#define	LINUX_SIGURG		23
#define	LINUX_SIGXCPU		24
#define	LINUX_SIGXFSZ		25
#define	LINUX_SIGVTALRM		26
#define	LINUX_SIGPROF		27
#define	LINUX_SIGWINCH		28
#define	LINUX_SIGIO		29
#define	LINUX_SIGPOLL		LINUX_SIGIO
#define	LINUX_SIGPWR		30
#define	LINUX_SIGSYS		31

#define	LINUX_SIGTBLSZ		31
#define	LINUX_NSIG_WORDS	2
#define	LINUX_NBPW		32
#define	LINUX_NSIG		(LINUX_NBPW * LINUX_NSIG_WORDS)

/* sigaction flags */
#define	LINUX_SA_NOCLDSTOP	0x00000001
#define	LINUX_SA_NOCLDWAIT	0x00000002
#define	LINUX_SA_SIGINFO	0x00000004
#define	LINUX_SA_RESTORER	0x04000000
#define	LINUX_SA_ONSTACK	0x08000000
#define	LINUX_SA_RESTART	0x10000000
#define	LINUX_SA_INTERRUPT	0x20000000
#define	LINUX_SA_NOMASK		0x40000000
#define	LINUX_SA_ONESHOT	0x80000000

/* sigprocmask actions */
#define	LINUX_SIG_BLOCK		0
#define	LINUX_SIG_UNBLOCK	1
#define	LINUX_SIG_SETMASK	2

/* sigset_t macros */
#define	LINUX_SIGEMPTYSET(set)		(set).__bits[0] = (set).__bits[1] = 0
#define	LINUX_SIGISMEMBER(set, sig)	SIGISMEMBER(set, sig)
#define	LINUX_SIGADDSET(set, sig)	SIGADDSET(set, sig)

/* sigaltstack */
#define	LINUX_MINSIGSTKSZ	2048
#define	LINUX_SS_ONSTACK	1
#define	LINUX_SS_DISABLE	2

int linux_to_bsd_sigaltstack(int lsa);
int bsd_to_linux_sigaltstack(int bsa);

typedef l_uintptr_t l_handler_t;
typedef l_ulong	l_osigset_t;

typedef struct {
	l_uint	__bits[LINUX_NSIG_WORDS];
} __packed l_sigset_t;

typedef struct {
	l_handler_t	lsa_handler;
	l_osigset_t	lsa_mask;
	l_ulong		lsa_flags;
	l_uintptr_t	lsa_restorer;
} __packed l_osigaction_t;

typedef struct {
	l_handler_t	lsa_handler;
	l_ulong		lsa_flags;
	l_uintptr_t	lsa_restorer;
	l_sigset_t	lsa_mask;
} __packed l_sigaction_t;

typedef struct {
	l_uintptr_t	ss_sp;
	l_int		ss_flags;
	l_size_t	ss_size;
} __packed l_stack_t;

/* The Linux sigcontext, pretty much a standard 386 trapframe. */
struct l_sigcontext {
	l_int		sc_gs;
	l_int		sc_fs;
	l_int		sc_es;
	l_int		sc_ds;
	l_int		sc_edi;
	l_int		sc_esi;
	l_int		sc_ebp;
	l_int		sc_esp;
	l_int		sc_ebx;
	l_int		sc_edx;
	l_int		sc_ecx;
	l_int		sc_eax;
	l_int		sc_trapno;
	l_int		sc_err;
	l_int		sc_eip;
	l_int		sc_cs;
	l_int		sc_eflags;
	l_int		sc_esp_at_signal;
	l_int		sc_ss;
	l_int		sc_387;
	l_int		sc_mask;
	l_int		sc_cr2;
} __packed;

struct l_ucontext {
	l_ulong		uc_flags;
	l_uintptr_t	uc_link;
	l_stack_t	uc_stack;
	struct l_sigcontext	uc_mcontext;
	l_sigset_t	uc_sigmask;
} __packed;

#define	LINUX_SI_MAX_SIZE	128
#define	LINUX_SI_PAD_SIZE	((LINUX_SI_MAX_SIZE/sizeof(l_int)) - 3)

typedef union l_sigval {
	l_int		sival_int;
	l_uintptr_t	sival_ptr;
} l_sigval_t;

typedef struct l_siginfo {
	l_int		lsi_signo;
	l_int		lsi_errno;
	l_int		lsi_code;
	union {
		l_int	_pad[LINUX_SI_PAD_SIZE];

		struct {
			l_pid_t		_pid;
			l_uid_t		_uid;
		} __packed _kill;

		struct {
			l_timer_t	_tid;
			l_int		_overrun;
			char		_pad[sizeof(l_uid_t) - sizeof(l_int)];
			l_sigval_t	_sigval;
			l_int		_sys_private;
		} __packed _timer;

		struct {
			l_pid_t		_pid;		/* sender's pid */
			l_uid_t		_uid;		/* sender's uid */
			l_sigval_t	_sigval;
		} __packed _rt;

		struct {
			l_pid_t		_pid;		/* which child */
			l_uid_t		_uid;		/* sender's uid */
			l_int		_status;	/* exit code */
			l_clock_t	_utime;
			l_clock_t	_stime;
		} __packed _sigchld;

		struct {
			l_uintptr_t	_addr;	/* Faulting insn/memory ref. */
		} __packed _sigfault;

		struct {
			l_long		_band;	/* POLL_IN,POLL_OUT,POLL_MSG */
			l_int		_fd;
		} __packed _sigpoll;
	} _sifields;
} __packed l_siginfo_t;

#define	lsi_pid		_sifields._kill._pid
#define	lsi_uid		_sifields._kill._uid
#define	lsi_tid		_sifields._timer._tid
#define	lsi_overrun	_sifields._timer._overrun
#define	lsi_sys_private	_sifields._timer._sys_private
#define	lsi_status	_sifields._sigchld._status
#define	lsi_utime	_sifields._sigchld._utime
#define	lsi_stime	_sifields._sigchld._stime
#define	lsi_value	_sifields._rt._sigval
#define	lsi_int		_sifields._rt._sigval.sival_int
#define	lsi_ptr		_sifields._rt._sigval.sival_ptr
#define	lsi_addr	_sifields._sigfault._addr
#define	lsi_band	_sifields._sigpoll._band
#define	lsi_fd		_sifields._sigpoll._fd

struct l_fpreg {
	u_int16_t	significand[4];
	u_int16_t	exponent;
} __packed;

struct l_fpxreg {
	u_int16_t	significand[4];
	u_int16_t	exponent;
	u_int16_t	padding[3];
} __packed;

struct l_xmmreg {
	u_int32_t	element[4];
} __packed;

struct l_fpstate {
	/* Regular FPU environment */
	u_int32_t		cw;
	u_int32_t		sw;
	u_int32_t		tag;
	u_int32_t		ipoff;
	u_int32_t		cssel;
	u_int32_t		dataoff;
	u_int32_t		datasel;
	struct l_fpreg		_st[8];
	u_int16_t		status;
	u_int16_t		magic;		/* 0xffff = regular FPU data */

	/* FXSR FPU environment */
	u_int32_t		_fxsr_env[6];	/* env is ignored. */
	u_int32_t		mxcsr;
	u_int32_t		reserved;
	struct l_fpxreg		_fxsr_st[8];	/* reg data is ignored. */
	struct l_xmmreg		_xmm[8];
	u_int32_t		padding[56];
} __packed;

/*
 * We make the stack look like Linux expects it when calling a signal
 * handler, but use the BSD way of calling the handler and sigreturn().
 * This means that we need to pass the pointer to the handler too.
 * It is appended to the frame to not interfere with the rest of it.
 */
struct l_sigframe {
	l_int			sf_sig;
	struct l_sigcontext	sf_sc;
	struct l_fpstate	sf_fpstate;
	l_uint			sf_extramask[LINUX_NSIG_WORDS-1];
	l_handler_t		sf_handler;
} __packed;

struct l_rt_sigframe {
	l_int			sf_sig;
	l_uintptr_t 		sf_siginfo;
	l_uintptr_t		sf_ucontext;
	l_siginfo_t		sf_si;
	struct l_ucontext 	sf_sc;
	l_handler_t 		sf_handler;
} __packed;

extern int bsd_to_linux_signal[];
extern int linux_to_bsd_signal[];
extern struct sysentvec elf_linux_sysvec;

/*
 * Pluggable ioctl handlers
 */
struct linux_ioctl_args;
struct thread;

typedef int linux_ioctl_function_t(struct thread *, struct linux_ioctl_args *);

struct linux_ioctl_handler {
	linux_ioctl_function_t *func;
	int	low, high;
};

int	linux_ioctl_register_handler(struct linux_ioctl_handler *h);
int	linux_ioctl_unregister_handler(struct linux_ioctl_handler *h);

/*
 * open/fcntl flags
 */
#define	LINUX_O_RDONLY		00000000
#define	LINUX_O_WRONLY		00000001
#define	LINUX_O_RDWR		00000002
#define	LINUX_O_ACCMODE		00000003
#define	LINUX_O_CREAT		00000100
#define	LINUX_O_EXCL		00000200
#define	LINUX_O_NOCTTY		00000400
#define	LINUX_O_TRUNC		00001000
#define	LINUX_O_APPEND		00002000
#define	LINUX_O_NONBLOCK	00004000
#define	LINUX_O_NDELAY		LINUX_O_NONBLOCK
#define	LINUX_O_SYNC		00010000
#define	LINUX_FASYNC		00020000
#define	LINUX_O_DIRECT		00040000	/* Direct disk access hint */
#define	LINUX_O_LARGEFILE	00100000
#define	LINUX_O_DIRECTORY	00200000	/* Must be a directory */
#define	LINUX_O_NOFOLLOW	00400000	/* Do not follow links */
#define	LINUX_O_NOATIME		01000000

#define	LINUX_F_DUPFD		0
#define	LINUX_F_GETFD		1
#define	LINUX_F_SETFD		2
#define	LINUX_F_GETFL		3
#define	LINUX_F_SETFL		4
#define	LINUX_F_GETLK		5
#define	LINUX_F_SETLK		6
#define	LINUX_F_SETLKW		7
#define	LINUX_F_SETOWN		8
#define	LINUX_F_GETOWN		9

#define	LINUX_F_GETLK64		12
#define	LINUX_F_SETLK64		13
#define	LINUX_F_SETLKW64	14

#define	LINUX_F_RDLCK		0
#define	LINUX_F_WRLCK		1
#define	LINUX_F_UNLCK		2

/*
 * mount flags
 */
#define	LINUX_MS_RDONLY		0x0001
#define	LINUX_MS_NOSUID		0x0002
#define	LINUX_MS_NODEV		0x0004
#define	LINUX_MS_NOEXEC		0x0008
#define	LINUX_MS_REMOUNT	0x0020

/*
 * SystemV IPC defines
 */
#define	LINUX_SEMOP		1
#define	LINUX_SEMGET		2
#define	LINUX_SEMCTL		3
#define	LINUX_MSGSND		11
#define	LINUX_MSGRCV		12
#define	LINUX_MSGGET		13
#define	LINUX_MSGCTL		14
#define	LINUX_SHMAT		21
#define	LINUX_SHMDT		22
#define	LINUX_SHMGET		23
#define	LINUX_SHMCTL		24

#define	LINUX_IPC_RMID		0
#define	LINUX_IPC_SET		1
#define	LINUX_IPC_STAT		2
#define	LINUX_IPC_INFO		3

#define	LINUX_SHM_LOCK		11
#define	LINUX_SHM_UNLOCK	12
#define	LINUX_SHM_STAT		13
#define	LINUX_SHM_INFO		14

#define	LINUX_SHM_RDONLY	0x1000
#define	LINUX_SHM_RND		0x2000
#define	LINUX_SHM_REMAP		0x4000

/* semctl commands */
#define	LINUX_GETPID		11
#define	LINUX_GETVAL		12
#define	LINUX_GETALL		13
#define	LINUX_GETNCNT		14
#define	LINUX_GETZCNT		15
#define	LINUX_SETVAL		16
#define	LINUX_SETALL		17
#define	LINUX_SEM_STAT		18
#define	LINUX_SEM_INFO		19

union l_semun {
	l_int		val;
	l_uintptr_t	buf;
	l_uintptr_t	array;
	l_uintptr_t	__buf;
	l_uintptr_t	__pad;
} __packed;

/*
 * Socket defines
 */
#define	LINUX_SOCKET 		1
#define	LINUX_BIND		2
#define	LINUX_CONNECT 		3
#define	LINUX_LISTEN 		4
#define	LINUX_ACCEPT 		5
#define	LINUX_GETSOCKNAME	6
#define	LINUX_GETPEERNAME	7
#define	LINUX_SOCKETPAIR	8
#define	LINUX_SEND		9
#define	LINUX_RECV		10
#define	LINUX_SENDTO 		11
#define	LINUX_RECVFROM 		12
#define	LINUX_SHUTDOWN 		13
#define	LINUX_SETSOCKOPT	14
#define	LINUX_GETSOCKOPT	15
#define	LINUX_SENDMSG		16
#define	LINUX_RECVMSG		17

#define	LINUX_AF_UNSPEC		0
#define	LINUX_AF_UNIX		1
#define	LINUX_AF_INET		2
#define	LINUX_AF_AX25		3
#define	LINUX_AF_IPX		4
#define	LINUX_AF_APPLETALK	5
#define	LINUX_AF_INET6		10

#define	LINUX_SOL_SOCKET	1
#define	LINUX_SOL_IP		0
#define	LINUX_SOL_IPX		256
#define	LINUX_SOL_AX25		257
#define	LINUX_SOL_TCP		6
#define	LINUX_SOL_UDP		17

#define	LINUX_SO_DEBUG		1
#define	LINUX_SO_REUSEADDR	2
#define	LINUX_SO_TYPE		3
#define	LINUX_SO_ERROR		4
#define	LINUX_SO_DONTROUTE	5
#define	LINUX_SO_BROADCAST	6
#define	LINUX_SO_SNDBUF		7
#define	LINUX_SO_RCVBUF		8
#define	LINUX_SO_KEEPALIVE	9
#define	LINUX_SO_OOBINLINE	10
#define	LINUX_SO_NO_CHECK	11
#define	LINUX_SO_PRIORITY	12
#define	LINUX_SO_LINGER		13
#define	LINUX_SO_PEERCRED	17
#define	LINUX_SO_RCVLOWAT	18
#define	LINUX_SO_SNDLOWAT	19
#define	LINUX_SO_RCVTIMEO	20
#define	LINUX_SO_SNDTIMEO	21
#define	LINUX_SO_TIMESTAMP	29
#define	LINUX_SO_ACCEPTCONN	30

#define	LINUX_IP_TOS		1
#define	LINUX_IP_TTL		2
#define	LINUX_IP_HDRINCL	3
#define	LINUX_IP_OPTIONS	4

#define	LINUX_IP_MULTICAST_IF		32
#define	LINUX_IP_MULTICAST_TTL		33
#define	LINUX_IP_MULTICAST_LOOP		34
#define	LINUX_IP_ADD_MEMBERSHIP		35
#define	LINUX_IP_DROP_MEMBERSHIP	36

struct l_sockaddr {
	l_ushort	sa_family;
	char		sa_data[14];
} __packed;

struct l_ifmap {
	l_ulong		mem_start;
	l_ulong		mem_end;
	l_ushort	base_addr;
	u_char		irq;
	u_char		dma;
	u_char		port;
} __packed;

#define	LINUX_IFHWADDRLEN	6
#define	LINUX_IFNAMSIZ		16

struct l_ifreq {
	union {
		char	ifrn_name[LINUX_IFNAMSIZ];
	} ifr_ifrn;

	union {
		struct l_sockaddr	ifru_addr;
		struct l_sockaddr	ifru_dstaddr;
		struct l_sockaddr	ifru_broadaddr;
		struct l_sockaddr	ifru_netmask;
		struct l_sockaddr	ifru_hwaddr;
		l_short		ifru_flags[1];
		l_int		ifru_metric;
		l_int		ifru_mtu;
		struct l_ifmap	ifru_map;
		char		ifru_slave[LINUX_IFNAMSIZ];
		l_uintptr_t	ifru_data;
	} ifr_ifru;
} __packed;

#define	ifr_name	ifr_ifrn.ifrn_name	/* Interface name */
#define	ifr_hwaddr	ifr_ifru.ifru_hwaddr	/* MAC address */

struct l_ifconf {
	int	ifc_len;
	union {
		l_uintptr_t	ifcu_buf;
		l_uintptr_t	ifcu_req;
	} ifc_ifcu;
} __packed;

#define	ifc_buf		ifc_ifcu.ifcu_buf
#define	ifc_req		ifc_ifcu.ifcu_req

/*
 * poll()
 */
#define	LINUX_POLLIN		0x0001
#define	LINUX_POLLPRI		0x0002
#define	LINUX_POLLOUT		0x0004
#define	LINUX_POLLERR		0x0008
#define	LINUX_POLLHUP		0x0010
#define	LINUX_POLLNVAL		0x0020
#define	LINUX_POLLRDNORM	0x0040
#define	LINUX_POLLRDBAND	0x0080
#define	LINUX_POLLWRNORM	0x0100
#define	LINUX_POLLWRBAND	0x0200
#define	LINUX_POLLMSG		0x0400

struct l_pollfd {
	l_int		fd;
	l_short		events;
	l_short		revents;
} __packed;

struct l_user_desc {
	l_uint		entry_number;
	l_uint		base_addr;
	l_uint		limit;
	l_uint		seg_32bit:1;
	l_uint		contents:2;
	l_uint		read_exec_only:1;
	l_uint		limit_in_pages:1;
	l_uint		seg_not_present:1;
	l_uint		useable:1;
};

#define	LINUX_LOWERWORD	0x0000ffff

/*
 * Macros which does the same thing as those in Linux include/asm-um/ldt-i386.h.
 * These convert Linux user space descriptor to machine one.
 */
#define	LINUX_LDT_entry_a(info)					\
	((((info)->base_addr & LINUX_LOWERWORD) << 16) |	\
	((info)->limit & LINUX_LOWERWORD))

#define	LINUX_ENTRY_B_READ_EXEC_ONLY	9
#define	LINUX_ENTRY_B_CONTENTS		10
#define	LINUX_ENTRY_B_SEG_NOT_PRESENT	15
#define	LINUX_ENTRY_B_BASE_ADDR		16
#define	LINUX_ENTRY_B_USEABLE		20
#define	LINUX_ENTRY_B_SEG32BIT		22
#define	LINUX_ENTRY_B_LIMIT		23

#define	LINUX_LDT_entry_b(info)							\
	(((info)->base_addr & 0xff000000) |					\
	((info)->limit & 0xf0000) |						\
	((info)->contents << LINUX_ENTRY_B_CONTENTS) |				\
	(((info)->seg_not_present == 0) << LINUX_ENTRY_B_SEG_NOT_PRESENT) |	\
	(((info)->base_addr & 0x00ff0000) >> LINUX_ENTRY_B_BASE_ADDR) |		\
	(((info)->read_exec_only == 0) << LINUX_ENTRY_B_READ_EXEC_ONLY) |	\
	((info)->seg_32bit << LINUX_ENTRY_B_SEG32BIT) |				\
	((info)->useable << LINUX_ENTRY_B_USEABLE) |				\
	((info)->limit_in_pages << LINUX_ENTRY_B_LIMIT) | 0x7000)

#define	LINUX_LDT_empty(info)		\
	((info)->base_addr == 0 &&	\
	(info)->limit == 0 &&		\
	(info)->contents == 0 &&	\
	(info)->seg_not_present == 1 &&	\
	(info)->read_exec_only == 1 &&	\
	(info)->seg_32bit == 0 &&	\
	(info)->limit_in_pages == 0 &&	\
	(info)->useable == 0)

/*
 * Macros for converting segments.
 * They do the same as those in arch/i386/kernel/process.c in Linux.
 */
#define	LINUX_GET_BASE(desc)				\
	((((desc)->a >> 16) & LINUX_LOWERWORD) |	\
	(((desc)->b << 16) & 0x00ff0000) |		\
	((desc)->b & 0xff000000))

#define	LINUX_GET_LIMIT(desc)			\
	(((desc)->a & LINUX_LOWERWORD) |	\
	((desc)->b & 0xf0000))

#define	LINUX_GET_32BIT(desc)		\
	(((desc)->b >> LINUX_ENTRY_B_SEG32BIT) & 1)
#define	LINUX_GET_CONTENTS(desc)	\
	(((desc)->b >> LINUX_ENTRY_B_CONTENTS) & 3)
#define	LINUX_GET_WRITABLE(desc)	\
	(((desc)->b >> LINUX_ENTRY_B_READ_EXEC_ONLY) & 1)
#define	LINUX_GET_LIMIT_PAGES(desc)	\
	(((desc)->b >> LINUX_ENTRY_B_LIMIT) & 1)
#define	LINUX_GET_PRESENT(desc)		\
	(((desc)->b >> LINUX_ENTRY_B_SEG_NOT_PRESENT) & 1)
#define	LINUX_GET_USEABLE(desc)		\
	(((desc)->b >> LINUX_ENTRY_B_USEABLE) & 1)

#define	LINUX_CLOCK_REALTIME		0
#define	LINUX_CLOCK_MONOTONIC		1
#define	LINUX_CLOCK_PROCESS_CPUTIME_ID	2
#define	LINUX_CLOCK_THREAD_CPUTIME_ID	3
#define	LINUX_CLOCK_REALTIME_HR		4
#define	LINUX_CLOCK_MONOTONIC_HR	5

#define	LINUX_CLONE_VM			0x00000100
#define	LINUX_CLONE_FS			0x00000200
#define	LINUX_CLONE_FILES		0x00000400
#define	LINUX_CLONE_SIGHAND		0x00000800
#define	LINUX_CLONE_PID			0x00001000	/* No longer exist in Linux */
#define	LINUX_CLONE_VFORK		0x00004000
#define	LINUX_CLONE_PARENT		0x00008000
#define	LINUX_CLONE_THREAD		0x00010000
#define	LINUX_CLONE_SETTLS		0x00080000
#define	LINUX_CLONE_PARENT_SETTID	0x00100000
#define	LINUX_CLONE_CHILD_CLEARTID	0x00200000
#define	LINUX_CLONE_CHILD_SETTID	0x01000000

#define	LINUX_THREADING_FLAGS					\
	(LINUX_CLONE_VM | LINUX_CLONE_FS | LINUX_CLONE_FILES |	\
	LINUX_CLONE_SIGHAND | LINUX_CLONE_THREAD)

/* robust futexes */
struct linux_robust_list {
	l_uintptr_t			next;
};

struct linux_robust_list_head {
	struct linux_robust_list	list;
	l_ulong				futex_offset;
	l_uintptr_t			pending_list;
};

#endif /* !_AMD64_LINUX_H_ */
