#ifndef _ASM_X86_64_IA32_H
#define _ASM_X86_64_IA32_H

#include <linux/config.h>

#ifdef CONFIG_IA32_EMULATION

/*
 * 32 bit structures for IA32 support.
 */

/* 32bit compatibility types */
typedef unsigned int	       __kernel_size_t32;
typedef int		       __kernel_ssize_t32;
typedef int		       __kernel_ptrdiff_t32;
typedef int		       __kernel_time_t32;
typedef int		       __kernel_clock_t32;
typedef int		       __kernel_pid_t32;
typedef unsigned short	       __kernel_ipc_pid_t32;
typedef unsigned short	       __kernel_uid_t32;
typedef unsigned 	       __kernel_uid32_t32;
typedef unsigned short	       __kernel_gid_t32;
typedef unsigned 	       __kernel_gid32_t32;
typedef unsigned short	       __kernel_dev_t32;
typedef unsigned int	       __kernel_ino_t32;
typedef unsigned short	       __kernel_mode_t32;
typedef unsigned short	       __kernel_umode_t32;
typedef short		       __kernel_nlink_t32;
typedef int		       __kernel_daddr_t32;
typedef int		       __kernel_off_t32;
typedef unsigned int	       __kernel_caddr_t32;
typedef long		       __kernel_loff_t32;
typedef __kernel_fsid_t	       __kernel_fsid_t32;


/* fcntl.h */
struct flock32 {
       short l_type;
       short l_whence;
       __kernel_off_t32 l_start;
       __kernel_off_t32 l_len;
       __kernel_pid_t32 l_pid;
};


struct ia32_flock64 {
	short  l_type;
	short  l_whence;
	loff_t l_start;  /* unnatural alignment */
	loff_t l_len;
	pid_t  l_pid;
} __attribute__((packed));

#define F_GETLK64	12	/*  using 'struct flock64' */
#define F_SETLK64	13
#define F_SETLKW64	14

#include <asm/sigcontext32.h>

/* signal.h */
#define _IA32_NSIG	       64
#define _IA32_NSIG_BPW	       32
#define _IA32_NSIG_WORDS	       (_IA32_NSIG / _IA32_NSIG_BPW)

typedef struct {
       unsigned int sig[_IA32_NSIG_WORDS];
} sigset32_t;

struct sigaction32 {
       unsigned int  sa_handler;	/* Really a pointer, but need to deal 
					     with 32 bits */
       unsigned int sa_flags;
       unsigned int sa_restorer;	/* Another 32 bit pointer */
       sigset32_t sa_mask;		/* A 32 bit mask */
};

typedef unsigned int old_sigset32_t;	/* at least 32 bits */

struct old_sigaction32 {
       unsigned int  sa_handler;	/* Really a pointer, but need to deal 
					     with 32 bits */
       old_sigset32_t sa_mask;		/* A 32 bit mask */
       unsigned int sa_flags;
       unsigned int sa_restorer;	/* Another 32 bit pointer */
};

typedef struct sigaltstack_ia32 {
	unsigned int	ss_sp;
	int		ss_flags;
	unsigned int	ss_size;
} stack_ia32_t;

struct ucontext_ia32 {
	unsigned int	  uc_flags;
	unsigned int 	  uc_link;
	stack_ia32_t	  uc_stack;
	struct sigcontext_ia32 uc_mcontext;
	sigset32_t	  uc_sigmask;	/* mask last for extensibility */
};

struct stat32 {
       unsigned short st_dev;
       unsigned short __pad1;
       unsigned int st_ino;
       unsigned short st_mode;
       unsigned short st_nlink;
       unsigned short st_uid;
       unsigned short st_gid;
       unsigned short st_rdev;
       unsigned short __pad2;
       unsigned int  st_size;
       unsigned int  st_blksize;
       unsigned int  st_blocks;
       unsigned int  st_atime;
       unsigned int  __unused1;
       unsigned int  st_mtime;
       unsigned int  __unused2;
       unsigned int  st_ctime;
       unsigned int  __unused3;
       unsigned int  __unused4;
       unsigned int  __unused5;
};


/* This matches struct stat64 in glibc2.2, hence the absolutely
 * insane amounts of padding around dev_t's.
 */
struct stat64 {
	unsigned long long	st_dev;
	unsigned char		__pad0[4];

#define STAT64_HAS_BROKEN_ST_INO	1
	unsigned int		__st_ino;

	unsigned int		st_mode;
	unsigned int		st_nlink;

	unsigned int		st_uid;
	unsigned int		st_gid;

	unsigned long long	st_rdev;
	unsigned char		__pad3[4];

	long long		st_size;
	unsigned int		st_blksize;

	long long		st_blocks;/* Number 512-byte blocks allocated. */

	unsigned long long	st_atime;
	unsigned long long	st_mtime;
	unsigned long long	st_ctime;

	unsigned long long	st_ino;
} __attribute__((packed));


struct statfs32 {
       int f_type;
       int f_bsize;
       int f_blocks;
       int f_bfree;
       int f_bavail;
       int f_files;
       int f_ffree;
       __kernel_fsid_t32 f_fsid;
       int f_namelen;  /* SunOS ignores this field. */
       int f_spare[6];
};

typedef union sigval32 {
	int sival_int;
	unsigned int sival_ptr;
} sigval_t32;

typedef struct siginfo32 {
	int si_signo;
	int si_errno;
	int si_code;

	union {
		int _pad[((128/sizeof(int)) - 3)];

		/* kill() */
		struct {
			unsigned int _pid;	/* sender's pid */
			unsigned int _uid;	/* sender's uid */
		} _kill;

		/* POSIX.1b timers */
		struct {
			unsigned int _timer1;
			unsigned int _timer2;
		} _timer;

		/* POSIX.1b signals */
		struct {
			unsigned int _pid;	/* sender's pid */
			unsigned int _uid;	/* sender's uid */
			sigval_t32 _sigval;
		} _rt;

		/* SIGCHLD */
		struct {
			unsigned int _pid;	/* which child */
			unsigned int _uid;	/* sender's uid */
			int _status;		/* exit code */
			__kernel_clock_t32 _utime;
			__kernel_clock_t32 _stime;
		} _sigchld;

		/* SIGILL, SIGFPE, SIGSEGV, SIGBUS */
		struct {
			unsigned int _addr;	/* faulting insn/memory ref. */
		} _sigfault;

		/* SIGPOLL */
		struct {
			int _band;	/* POLL_IN, POLL_OUT, POLL_MSG */
			int _fd;
		} _sigpoll;
	} _sifields;
} siginfo_t32;


struct ustat32 {
	__u32	f_tfree;
	__kernel_ino_t32		f_tinode;
	char			f_fname[6];
	char			f_fpack[6];
};

struct iovec32 { 
	unsigned int iov_base; 
	int iov_len; 
};

struct timespec32 {
	int 	tv_sec;
	int	tv_nsec;
};

#endif /* !CONFIG_IA32_SUPPORT */
 
#endif 
