#ifndef _ASM_S390X_S390_H
#define _ASM_S390X_S390_H

#include <linux/config.h>
#include <linux/socket.h>
#include <linux/nfs_fs.h>
#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/export.h>

#ifdef CONFIG_S390_SUPPORT

/* Macro that masks the high order bit of an 32 bit pointer and converts it*/
/*       to a 64 bit pointer */
#define A(__x) ((unsigned long)((__x) & 0x7FFFFFFFUL))
#define AA(__x)				\
	((unsigned long)(__x))

/* Now 32bit compatibility types */
typedef unsigned int           __kernel_size_t32;
typedef int                    __kernel_ssize_t32;
typedef int                    __kernel_ptrdiff_t32;
typedef int                    __kernel_time_t32;
typedef int                    __kernel_clock_t32;
typedef int                    __kernel_pid_t32;
typedef unsigned short         __kernel_ipc_pid_t32;
typedef unsigned short         __kernel_uid_t32;
typedef unsigned short         __kernel_gid_t32;
typedef unsigned short         __kernel_dev_t32;
typedef unsigned int           __kernel_ino_t32;
typedef unsigned short         __kernel_mode_t32;
typedef unsigned short         __kernel_umode_t32;
typedef short                  __kernel_nlink_t32;
typedef int                    __kernel_daddr_t32;
typedef int                    __kernel_off_t32;
typedef unsigned int           __kernel_caddr_t32;
typedef long                   __kernel_loff_t32;
typedef __kernel_fsid_t        __kernel_fsid_t32;  

struct ipc_kludge_32 {
        __u32   msgp;                           /* pointer              */
        __s32   msgtyp;
};

#define F_GETLK64       12
#define F_SETLK64       13
#define F_SETLKW64      14    

struct flock32 {
        short l_type;
        short l_whence;
        __kernel_off_t32 l_start;
        __kernel_off_t32 l_len;
        __kernel_pid_t32 l_pid;
        short __unused;
}; 

struct stat32 {
	unsigned short	st_dev;
	unsigned short	__pad1;
	__u32		st_ino;
	unsigned short	st_mode;
	unsigned short	st_nlink;
	unsigned short	st_uid;
	unsigned short	st_gid;
	unsigned short	st_rdev;
	unsigned short	__pad2;
	__u32		st_size;
	__u32		st_blksize;
	__u32		st_blocks;
	__u32		st_atime;
	__u32		__unused1;
	__u32		st_mtime;
	__u32		__unused2;
	__u32		st_ctime;
	__u32		__unused3;
	__u32		__unused4;
	__u32		__unused5;
};

struct statfs32 {
	__s32			f_type;
	__s32			f_bsize;
	__s32			f_blocks;
	__s32			f_bfree;
	__s32			f_bavail;
	__s32			f_files;
	__s32			f_ffree;
	__kernel_fsid_t		f_fsid;
	__s32			f_namelen;  
	__s32			f_spare[6];
};

typedef __u32 old_sigset_t32;       /* at least 32 bits */ 

struct old_sigaction32 {
       __u32			sa_handler;	/* Really a pointer, but need to deal with 32 bits */
       old_sigset_t32		sa_mask;	/* A 32 bit mask */
       __u32			sa_flags;
       __u32			sa_restorer;	/* Another 32 bit pointer */
};
 
#define _SIGCONTEXT_NSIG_WORDS32    2 
typedef struct {
        __u32   sig[_SIGCONTEXT_NSIG_WORDS32];
} sigset_t32;  

typedef union sigval32 {
        int     sival_int;
        __u32   sival_ptr;
} sigval_t32;
                 
typedef struct siginfo32 {
	int	si_signo;
	int	si_errno;
	int	si_code;

	union {
		int _pad[((128/sizeof(int)) - 3)];

		/* kill() */
		struct {
			pid_t	_pid;	/* sender's pid */
			uid_t	_uid;	/* sender's uid */
		} _kill;

		/* POSIX.1b timers */
		struct {
			unsigned int	_timer1;
			unsigned int	_timer2;
                
		} _timer;

		/* POSIX.1b signals */
		struct {
			pid_t			_pid;	/* sender's pid */
			uid_t			_uid;	/* sender's uid */
			sigval_t32		_sigval;
		} _rt;

		/* SIGCHLD */
		struct {
			pid_t			_pid;	/* which child */
			uid_t			_uid;	/* sender's uid */
			int			_status;/* exit code */
			__kernel_clock_t32	_utime;
			__kernel_clock_t32	_stime;
		} _sigchld;

		/* SIGILL, SIGFPE, SIGSEGV, SIGBUS */
		struct {
			__u32	_addr;	/* faulting insn/memory ref. - pointer */
		} _sigfault;
                          
		/* SIGPOLL */
		struct {
			int	_band;	/* POLL_IN, POLL_OUT, POLL_MSG */
			int	_fd;
		} _sigpoll;
	} _sifields;
} siginfo_t32;  

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
#define si_band		_sifields._sigpoll._band
#define si_fd		_sifields._sigpoll._fd    

/* asm/sigcontext.h */
typedef union
{
	__u64   d;
	__u32   f; 
} freg_t32;

typedef struct
{
	unsigned int	fpc;
	freg_t32	fprs[__NUM_FPRS];              
} _s390_fp_regs32;

typedef struct 
{
        __u32   mask;
        __u32	addr;
} _psw_t32 __attribute__ ((aligned(8)));

typedef struct
{
	_psw_t32	psw;
	__u32		gprs[__NUM_GPRS];
	__u32		acrs[__NUM_ACRS];
} _s390_regs_common32;

typedef struct
{
	_s390_regs_common32 regs;
	_s390_fp_regs32     fpregs;
} _sigregs32;

#define _SIGCONTEXT_NSIG32	64
#define _SIGCONTEXT_NSIG_BPW32	32
#define __SIGNAL_FRAMESIZE32	96
#define _SIGMASK_COPY_SIZE32	(sizeof(u32)*2)

struct sigcontext32
{
	__u32	oldmask[_SIGCONTEXT_NSIG_WORDS32];
	__u32	sregs;				/* pointer */
};

/* asm/signal.h */
struct sigaction32 {
	__u32		sa_handler;		/* pointer */
	__u32		sa_flags;
        __u32		sa_restorer;		/* pointer */
	sigset_t32	sa_mask;        /* mask last for extensibility */
};

typedef struct {
	__u32			ss_sp;		/* pointer */
	int			ss_flags;
	__kernel_size_t32	ss_size;
} stack_t32;

/* asm/ucontext.h */
struct ucontext32 {
	__u32			uc_flags;
	__u32			uc_link;	/* pointer */	
	stack_t32		uc_stack;
	_sigregs32		uc_mcontext;
	sigset_t32		uc_sigmask;	/* mask last for extensibility */
};

#endif /* !CONFIG_S390_SUPPORT */
 
#endif /* _ASM_S390X_S390_H */
