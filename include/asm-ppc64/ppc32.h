#ifndef _PPC64_PPC32_H
#define _PPC64_PPC32_H

#include <asm/siginfo.h>
#include <asm/signal.h>

/*
 * Data types and macros for providing 32b PowerPC support.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __KERNEL_STRICT_NAMES
#include <linux/types.h>
typedef __kernel_fsid_t __kernel_fsid_t32;
#endif

/* Use this to get at 32-bit user passed pointers. */
/* Things to consider: the low-level assembly stub does
   srl x, 0, x for first four arguments, so if you have
   pointer to something in the first four arguments, just
   declare it as a pointer, not u32. On the other side, 
   arguments from 5th onwards should be declared as u32
   for pointers, and need AA() around each usage.
   A() macro should be used for places where you e.g.
   have some internal variable u32 and just want to get
   rid of a compiler warning. AA() has to be used in
   places where you want to convert a function argument
   to 32bit pointer or when you e.g. access pt_regs
   structure and want to consider 32bit registers only.
   -
 */
#define A(__x) ((unsigned long)(__x))
#define AA(__x)				\
({	unsigned long __ret;		\
	__asm__ ("clrldi	%0, %0, 32"	\
		 : "=r" (__ret)		\
		 : "0" (__x));		\
	__ret;				\
})

/* These are here to support 32-bit syscalls on a 64-bit kernel. */
typedef unsigned int	__kernel_size_t32;
typedef int		__kernel_ssize_t32;
typedef int		__kernel_ptrdiff_t32;
typedef int		__kernel_time_t32;
typedef int		__kernel_clock_t32;
typedef int		__kernel_pid_t32;
typedef unsigned short	__kernel_ipc_pid_t32;
typedef unsigned int	__kernel_uid_t32;
typedef unsigned int	__kernel_gid_t32;
typedef unsigned int	__kernel_dev_t32;
typedef unsigned int	__kernel_ino_t32;
typedef unsigned int	__kernel_mode_t32;
typedef unsigned int	__kernel_umode_t32;
typedef short		__kernel_nlink_t32;
typedef int		__kernel_daddr_t32;
typedef int		__kernel_off_t32;
typedef unsigned int	__kernel_caddr_t32;
typedef int		__kernel_loff_t32;
typedef int		__kernel_key_t32;

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

struct ustat32 {
	__kernel_daddr_t32      f_tfree;
	__kernel_ino_t32        f_tinode;
	char                    f_fname[6];
	char                    f_fpack[6];
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
		int _pad[SI_PAD_SIZE32];

		/* kill() */
		struct {
			__kernel_pid_t32 _pid;		/* sender's pid */
			__kernel_uid_t32 _uid;		/* sender's uid */
		} _kill;

		/* POSIX.1b timers */
		struct {
			unsigned int _timer1;
			unsigned int _timer2;
		} _timer;

		/* POSIX.1b signals */
		struct {
			__kernel_pid_t32 _pid;		/* sender's pid */
			__kernel_uid_t32 _uid;		/* sender's uid */
			sigval_t32 _sigval;
		} _rt;

		/* SIGCHLD */
		struct {
			__kernel_pid_t32 _pid;		/* which child */
			__kernel_uid_t32 _uid;		/* sender's uid */
			int _status;			/* exit code */
			__kernel_clock_t32 _utime;
			__kernel_clock_t32 _stime;
		} _sigchld;

		/* SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGEMT */
		struct {
			unsigned int _addr; /* faulting insn/memory ref. */
		} _sigfault;

		/* SIGPOLL */
		struct {
			int _band;	/* POLL_IN, POLL_OUT, POLL_MSG */
			int _fd;
		} _sigpoll;
	} _sifields;
} siginfo_t32;

#define __old_sigset_t32	old_sigset_t32
#define __old_sigaction32	old_sigaction32

typedef unsigned int __old_sigset_t32;
struct __old_sigaction32 {
	unsigned int		sa_handler;
	__old_sigset_t32  	sa_mask;
	unsigned int    	sa_flags;
	unsigned int		sa_restorer;     /* not used by Linux/SPARC yet */
};



#define _PPC32_NSIG	       64
#define _PPC32_NSIG_BPW	       32
#define _PPC32_NSIG_WORDS	       (_PPC32_NSIG / _PPC32_NSIG_BPW)

typedef struct {
       unsigned int sig[_PPC32_NSIG_WORDS];
} sigset32_t;

struct sigaction32 {
       unsigned int  sa_handler;	/* Really a pointer, but need to deal with 32 bits */
       unsigned int sa_flags;
       unsigned int sa_restorer;	/* Another 32 bit pointer */
       sigset32_t sa_mask;		/* A 32 bit mask */
};

typedef struct sigaltstack_32 {
	unsigned int ss_sp;
	int ss_flags;
	__kernel_size_t32 ss_size;
} stack_32_t;

struct flock32 {
	short l_type;
	short l_whence;
	__kernel_off_t32 l_start;
	__kernel_off_t32 l_len;
	__kernel_pid_t32 l_pid;
	short __unused;
};

struct stat32 {
	__kernel_dev_t32   st_dev; /* 2 */
	/* __kernel_dev_t32 __pad1; */ /* 2 */
	__kernel_ino_t32   st_ino; /* 4  */
	__kernel_mode_t32  st_mode; /* 2  */
	short   	   st_nlink; /* 2 */
	__kernel_uid_t32   st_uid; /* 2 */
	__kernel_gid_t32   st_gid; /* 2 */
	__kernel_dev_t32   st_rdev; /* 2 */
	/* __kernel_dev_t32 __pad2; */ /* 2 */
	__kernel_off_t32   st_size; /* 4 */
	__kernel_off_t32   st_blksize; /* 4 */
	__kernel_off_t32   st_blocks; /* 4 */
	__kernel_time_t32  st_atime; /* 4 */
	unsigned int       __unused1; /* 4 */
	__kernel_time_t32  st_mtime; /* 4 */
	unsigned int       __unused2; /* 4 */
	__kernel_time_t32  st_ctime; /* 4 */
	unsigned int       __unused3; /* 4 */
	unsigned int  __unused4[2]; /* 2*4 */
};

struct __old_kernel_stat32
{
	unsigned short st_dev;
	unsigned short st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned short st_rdev;
	unsigned int   st_size;
	unsigned int   st_atime;
	unsigned int   st_mtime;
	unsigned int   st_ctime;
};

struct sigcontext32 {
	unsigned int	_unused[4];
	int		signal;
	unsigned int	handler;
	unsigned int	oldmask;
	u32 regs;  /* 4 byte pointer to the pt_regs32 structure. */
};

struct ucontext32 { 
	unsigned int	  uc_flags;
	unsigned int 	  uc_link;
	stack_32_t	  uc_stack;
	struct sigcontext32 uc_mcontext;
	sigset_t	  uc_sigmask;	/* mask last for extensibility */
};

struct ipc_kludge_32 {
	unsigned int msgp;
	int msgtyp;
};

struct ipc_perm32 {
        __kernel_key_t32  key;
        __kernel_uid_t32  uid;
        __kernel_gid_t32  gid;
        __kernel_uid_t32  cuid;
        __kernel_gid_t32  cgid;
        __kernel_mode_t32 mode; 
        unsigned short  seq;
};

struct ipc64_perm32 {
	__kernel_key_t32 key;
	__kernel_uid_t32 uid;
	__kernel_gid_t32 gid;
	__kernel_uid_t32 cuid;
	__kernel_gid_t32 cgid;
	__kernel_mode_t32 mode;
	unsigned int	seq;
	unsigned int	__pad1;
	unsigned long	__unused1;
	unsigned long	__unused2;
};

#endif  /* _PPC64_PPC32_H */
