/*-
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

#ifndef _I386_LINUX_LINUX_H_
#define	_I386_LINUX_LINUX_H_

#include <sys/signal.h> /* for sigval union */

#include <i386/linux/linux_syscall.h>

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_LINUX);
#endif

/*
 * Miscellaneous
 */
#define	LINUX_NAME_MAX		255
#define	LINUX_MAX_UTSNAME	65

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
#define	LINUX_RLIMIT_AS		9       /* address space limit */

#define	LINUX_RLIM_NLIMITS	10

/* mmap options */
#define	LINUX_MAP_SHARED	0x0001
#define	LINUX_MAP_PRIVATE	0x0002
#define	LINUX_MAP_FIXED		0x0010
#define	LINUX_MAP_ANON		0x0020
#define	LINUX_MAP_GROWSDOWN	0x0100

typedef char *	linux_caddr_t;
typedef long	linux_clock_t;
typedef u_short	linux_dev_t;
typedef u_short	linux_gid_t;
typedef u_long	linux_ino_t;
typedef int	linux_key_t;	/* XXX */
typedef u_short	linux_mode_t;
typedef u_short	linux_nlink_t;
typedef long	linux_off_t;
typedef int	linux_pid_t;
typedef u_int	linux_size_t;
typedef long	linux_time_t;
typedef u_short	linux_uid_t;

typedef struct {
	long	val[2];
} linux_fsid_t;

struct linux_new_utsname {
	char	sysname[LINUX_MAX_UTSNAME];
	char	nodename[LINUX_MAX_UTSNAME];
	char	release[LINUX_MAX_UTSNAME];
	char	version[LINUX_MAX_UTSNAME];
	char	machine[LINUX_MAX_UTSNAME];
	char	domainname[LINUX_MAX_UTSNAME];
};

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
#define	LINUX_SIGUNUSED		31

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
#define LINUX_MINSIGSTKSZ	2048
#define LINUX_SS_ONSTACK_BC	0 	/* backwards compat SS_ONSTACK */
#define LINUX_SS_ONSTACK	1
#define LINUX_SS_DISABLE	2


int linux_to_bsd_sigaltstack(int lsa);
int bsd_to_linux_sigaltstack(int bsa);


typedef void	(*linux_handler_t)(int);
typedef u_long	linux_osigset_t;

typedef struct {
	u_int	__bits[LINUX_NSIG_WORDS];
} linux_sigset_t;

typedef struct {
	linux_handler_t lsa_handler;
	linux_osigset_t	lsa_mask;
	u_long	lsa_flags;
	void	(*lsa_restorer)(void);
} linux_osigaction_t;

typedef struct {
	linux_handler_t lsa_handler;
	u_long	lsa_flags;
	void	(*lsa_restorer)(void);
	linux_sigset_t	lsa_mask;
} linux_sigaction_t;

typedef struct {
	void	*ss_sp;
	int	ss_flags;
	linux_size_t ss_size;
} linux_stack_t;

/* The Linux sigcontext, pretty much a standard 386 trapframe. */
struct linux_sigcontext {
	int	sc_gs;
	int	sc_fs;
	int     sc_es;
	int     sc_ds;
	int     sc_edi;
	int     sc_esi;
	int     sc_ebp;
	int	sc_esp;
	int     sc_ebx;
	int     sc_edx;
	int     sc_ecx;
	int     sc_eax;
	int     sc_trapno;
	int     sc_err;
	int     sc_eip;
	int     sc_cs;
	int     sc_eflags;
	int     sc_esp_at_signal;
	int     sc_ss;
	int	sc_387;
	int	sc_mask;
	int	sc_cr2;
};

struct linux_ucontext {
	unsigned long     	uc_flags;
	void  			*uc_link;
	linux_stack_t		uc_stack;
	struct linux_sigcontext uc_mcontext;
        linux_sigset_t		uc_sigmask;   
};


#define LINUX_SI_MAX_SIZE     128
#define LINUX_SI_PAD_SIZE     ((LINUX_SI_MAX_SIZE/sizeof(int)) - 3)

typedef struct siginfo {
	int lsi_signo;
	int lsi_errno;
	int lsi_code;

	union {
		int _pad[LINUX_SI_PAD_SIZE];
		struct {
			linux_pid_t _pid;
			linux_uid_t _uid;
		} _kill;

		struct {
			unsigned int _timer1;
			unsigned int _timer2;
		} _timer;
		
		struct {
			linux_pid_t _pid;             /* sender's pid */
			linux_uid_t _uid;             /* sender's uid */
			union sigval _sigval;
		} _rt;

		struct {
			linux_pid_t _pid;             /* which child */
			linux_uid_t _uid;             /* sender's uid */
			int _status;            /* exit code */
			linux_clock_t _utime;
			linux_clock_t _stime;
		} _sigchld;

		struct {
			void *_addr; /* faulting insn/memory ref. */
		} _sigfault;

		struct {
			int _band;      /* POLL_IN, POLL_OUT, POLL_MSG */
			int _fd;
		} _sigpoll;
	} _sifields;
} linux_siginfo_t;

#define lsi_pid          _sifields._kill._pid
#define lsi_uid          _sifields._kill._uid
#define lsi_status       _sifields._sigchld._status
#define lsi_utime        _sifields._sigchld._utime
#define lsi_stime        _sifields._sigchld._stime
#define lsi_value        _sifields._rt._sigval
#define lsi_int          _sifields._rt._sigval.sival_int
#define lsi_ptr          _sifields._rt._sigval.sival_ptr
#define lsi_addr         _sifields._sigfault._addr
#define lsi_band         _sifields._sigpoll._band
#define lsi_fd           _sifields._sigpoll._fd

struct linux_fpreg {
	u_int16_t significand[4];
	u_int16_t exponent;
};

struct linux_fpxreg {
	u_int16_t significand[4];
	u_int16_t exponent;
	u_int16_t padding[3];
};

struct linux_xmmreg {
	u_int32_t element[4];
};

struct linux_fpstate {
	/* Regular FPU environment */
	u_int32_t		cw;
	u_int32_t		sw;
	u_int32_t		tag;
	u_int32_t		ipoff;
	u_int32_t		cssel;
	u_int32_t		dataoff;
	u_int32_t		datasel;
	struct linux_fpreg	_st[8];
	u_int16_t		status;
	u_int16_t		magic;  /* 0xffff = regular FPU data */

	/* FXSR FPU environment */
	u_int32_t		_fxsr_env[6]; /* env is ignored */
	u_int32_t		mxcsr;
	u_int32_t		reserved;
	struct linux_fpxreg	_fxsr_st[8];  /* reg data is ignored */
	struct linux_xmmreg	_xmm[8];
	u_int32_t		padding[56];
};

/*
 * We make the stack look like Linux expects it when calling a signal
 * handler, but use the BSD way of calling the handler and sigreturn().
 * This means that we need to pass the pointer to the handler too.
 * It is appended to the frame to not interfere with the rest of it.
 */
struct linux_sigframe {
	int	sf_sig;
	struct	linux_sigcontext sf_sc;
	struct  linux_fpstate fpstate;
	u_int	extramask[LINUX_NSIG_WORDS-1];
	linux_handler_t sf_handler;
};

struct linux_rt_sigframe {
	int			sf_sig;
	linux_siginfo_t 	*sf_siginfo;;
	struct linux_ucontext	*sf_ucontext;	
	linux_siginfo_t sf_si;
	struct linux_ucontext 	sf_sc;          
	linux_handler_t 	sf_handler;
};


extern int bsd_to_linux_signal[];
extern int linux_to_bsd_signal[];
extern struct sysentvec linux_sysvec;
extern struct sysentvec elf_linux_sysvec;
void bsd_to_linux_sigset(sigset_t *bss, linux_sigset_t *lss);

/*
 * Pluggable ioctl handlers
 */
struct linker_set;
struct linux_ioctl_args;
struct proc;

typedef int linux_ioctl_function_t(struct proc *, struct linux_ioctl_args *);

struct linux_ioctl_handler {
	linux_ioctl_function_t *func;
	int	low, high;
};

int	linux_ioctl_register_handler(struct linux_ioctl_handler *h);
int	linux_ioctl_register_handlers(struct linker_set *s);
int	linux_ioctl_unregister_handler(struct linux_ioctl_handler *h);
int	linux_ioctl_unregister_handlers(struct linker_set *s);

/*
 * open/fcntl flags
 */
#define	LINUX_O_RDONLY		00
#define	LINUX_O_WRONLY		01
#define	LINUX_O_RDWR		02
#define	LINUX_O_CREAT		0100
#define	LINUX_O_EXCL		0200
#define	LINUX_O_NOCTTY		0400
#define	LINUX_O_TRUNC		01000
#define	LINUX_O_APPEND		02000
#define	LINUX_O_NONBLOCK	04000
#define	LINUX_O_NDELAY		LINUX_O_NONBLOCK
#define	LINUX_O_SYNC		010000
#define	LINUX_FASYNC		020000

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

#define	LINUX_F_RDLCK		0
#define	LINUX_F_WRLCK		1
#define	LINUX_F_UNLCK		2

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

#define	LINUX_IP_TOS		1
#define	LINUX_IP_TTL		2
#define	LINUX_IP_HDRINCL	3
#define	LINUX_IP_OPTIONS	4

#define	LINUX_IP_MULTICAST_IF		32
#define	LINUX_IP_MULTICAST_TTL		33
#define	LINUX_IP_MULTICAST_LOOP		34
#define	LINUX_IP_ADD_MEMBERSHIP		35
#define	LINUX_IP_DROP_MEMBERSHIP	36

struct linux_sockaddr {
	u_short	sa_family;
	char	sa_data[14];
};

struct linux_ifmap {
	u_long	mem_start;
	u_long	mem_end;
	u_short	base_addr; 
	u_char	irq;
	u_char	dma;
	u_char	port;
};

#define	LINUX_IFHWADDRLEN	6
#define	LINUX_IFNAMSIZ		16

struct linux_ifreq {
	union {
		char	ifrn_name[LINUX_IFNAMSIZ];
	} ifr_ifrn;

	union {
		struct	linux_sockaddr ifru_addr;
		struct	linux_sockaddr ifru_dstaddr;
		struct	linux_sockaddr ifru_broadaddr;
		struct	linux_sockaddr ifru_netmask;
		struct	linux_sockaddr ifru_hwaddr;
		short	ifru_flags;
		int	ifru_metric;
		int	ifru_mtu;
		struct	linux_ifmap ifru_map;
		char	ifru_slave[LINUX_IFNAMSIZ]; /* Just fits the size */
		linux_caddr_t ifru_data;
	} ifr_ifru;
};

#define	ifr_name	ifr_ifrn.ifrn_name	/* interface name */
#define	ifr_hwaddr	ifr_ifru.ifru_hwaddr	/* MAC address */

#endif /* !_I386_LINUX_LINUX_H_ */
