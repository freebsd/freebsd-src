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

#ifndef _ALPHA_LINUX_LINUX_H_
#define	_ALPHA_LINUX_LINUX_H_

#include <alpha/linux/linux_syscall.h>

/*
 * debugging support
 */
extern u_char linux_debug_map[]; 
#define ldebug(name)	isclr(linux_debug_map, LINUX_SYS_linux_ ## name)
#define ARGS(nm, fmt)	"Linux-emul(%ld): "#nm"("fmt")\n", (long)p->p_pid 
#define LMSG(fmt)	"Linux-emul(%ld): "fmt"\n", (long)p->p_pid 

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
#define	LINUX_RLIMIT_AS		7       /* address space limit */
#define	LINUX_RLIMIT_NPROC	8
#define	LINUX_RLIMIT_NOFILE	6
#define	LINUX_RLIMIT_MEMLOCK	9

#define	LINUX_RLIM_NLIMITS	10

/* mmap options */
#define	LINUX_MAP_SHARED	0x0001
#define	LINUX_MAP_PRIVATE	0x0002
#define	LINUX_MAP_ANON		0x0010
#define	LINUX_MAP_FIXED		0x0100
#define	LINUX_MAP_GROWSDOWN	0x1000

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
/*typedef u_int	linux_size_t; */
typedef long	linux_time_t;
typedef u_short	linux_uid_t;

typedef struct {
	int	val[2];
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
#define	LINUX_SIGTBLSZ		31
#define	LINUX_SIGUNUSED		LINUX_SIGTBLSZ

#define	LINUX_NSIG		64

/* sigaction flags */
#define	LINUX_SA_ONSTACK	0x00000001
#define	LINUX_SA_RESTART	0x00000002
#define	LINUX_SA_NOCLDSTOP	0x00000004
#define	LINUX_SA_NODEFER	0x00000008
#define	LINUX_SA_RESETHAND	0x00000010
#define	LINUX_SA_NOCLDWAIT	0x00000020
#define	LINUX_SA_SIGINFO	0x00000040
#define	LINUX_SA_RESTORER	0x04000000
#define	LINUX_SA_INTERRUPT	0x20000000
#define	LINUX_SA_NOMASK		LINUX_SA_NODEFER
#define	LINUX_SA_ONESHOT	LINUX_SA_RESETHAND

/* sigprocmask actions */
#define	LINUX_SIG_BLOCK		0
#define	LINUX_SIG_UNBLOCK	1
#define	LINUX_SIG_SETMASK	2

/* sigset_t macros */
#define	LINUX_SIGEMPTYSET(set)		(set).__bits[0] = (set).__bits[1] = 0
#define	LINUX_SIGISMEMBER(set, sig)	SIGISMEMBER(set, sig)
#define	LINUX_SIGADDSET(set, sig)	SIGADDSET(set, sig)

#define	LINUX_MINSIGSTKSZ	4096

typedef void	(*linux_handler_t)(int);
typedef u_long	linux_osigset_t;

typedef struct {
	u_int	__bits[2];
/*	u_long	__bits[1];*/
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

#if 0
typedef struct {
	void	*ss_sp;
	int	ss_flags;
	linux_size_t ss_size;
} linux_stack_t;
#endif

/*
 * The Linux sigcontext
 */
struct linux_sigcontext {
	long	sc_onstack;
	long	sc_mask;
	long	sc_pc;
	long	sc_ps;
	long	sc_regs[32];
	long	sc_ownedfp;
	long	sc_fpregs[32];
	u_long	sc_fpcr;
	u_long	sc_fp_control;
	u_long	sc_reserved1, sc_reserved2;
	u_long	sc_ssize;
	char *	sc_sbase;
	u_long	sc_traparg_a0;
	u_long	sc_traparg_a1;
	u_long	sc_traparg_a2;
	u_long	sc_fp_trap_pc;
	u_long	sc_fp_trigger_sum;
	u_long	sc_fp_trigger_inst;
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
	linux_handler_t sf_handler;
};

/*
 * Pluggable ioctl handlers
 */
struct linux_ioctl_args;
struct proc;

typedef int linux_ioctl_function_t(struct proc *, struct linux_ioctl_args *);

struct linux_ioctl_handler {
	linux_ioctl_function_t *func;
	int	low, high;
};

int	linux_ioctl_register_handler(struct linux_ioctl_handler *h);
int	linux_ioctl_unregister_handler(struct linux_ioctl_handler *h);

/*
 * open/fcntl flags
 */
#define	LINUX_O_RDONLY		00
#define	LINUX_O_WRONLY		01
#define	LINUX_O_RDWR		02
#define	LINUX_O_NONBLOCK	04
#define	LINUX_O_APPEND		010
#define	LINUX_O_CREAT		01000
#define	LINUX_O_TRUNC		02000
#define	LINUX_O_EXCL		04000
#define	LINUX_O_NOCTTY		010000
#define	LINUX_O_NDELAY		LINUX_O_NONBLOCK
#define	LINUX_O_SYNC		040000

#define	LINUX_FASYNC		020000

/* fcntl flags */
#define	LINUX_F_DUPFD		0
#define	LINUX_F_GETFD		1
#define	LINUX_F_SETFD		2
#define	LINUX_F_GETFL		3
#define	LINUX_F_SETFL		4
#define	LINUX_F_SETOWN		5
#define	LINUX_F_GETOWN		6
#define	LINUX_F_GETLK		7
#define	LINUX_F_SETLK		8
#define	LINUX_F_SETLKW		9
#define	LINUX_F_SETSIG		10
#define	LINUX_F_GETSIG		11

#define	LINUX_F_RDLCK		1
#define	LINUX_F_WRLCK		2
#define	LINUX_F_UNLCK		8

/*
 * mount flags
 */
#define LINUX_MS_RDONLY         0x0001
#define LINUX_MS_NOSUID         0x0002
#define LINUX_MS_NODEV          0x0004
#define LINUX_MS_NOEXEC         0x0008
#define LINUX_MS_REMOUNT        0x0020
        
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
#define	LINUX_SOL_TCP		6
#define	LINUX_SOL_UDP		17
#define	LINUX_SOL_IPX		256
#define	LINUX_SOL_AX25		257

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
		char	ifrn_name[LINUX_IFNAMSIZ];    /* if name, e.g. "en0" */
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
		/* linux_caddr_t ifru_data; */
		caddr_t	ifru_data;
	} ifr_ifru;
};

#define	ifr_name	ifr_ifrn.ifrn_name	/* interface name */
#define	ifr_hwaddr	ifr_ifru.ifru_hwaddr	/* MAC address */


extern char linux_sigcode[];
extern int linux_szsigcode;
/*extern const char linux_emul_path[];*/

extern struct sysent linux_sysent[LINUX_SYS_MAXSYSCALL];
extern struct sysentvec linux_sysvec;
extern struct sysentvec elf_linux_sysvec;

/* dummy struct definitions */
struct image_params;
struct trapframe;

#endif /* !_ALPHA_LINUX_LINUX_H_ */
