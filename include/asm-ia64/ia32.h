#ifndef _ASM_IA64_IA32_H
#define _ASM_IA64_IA32_H

#include <linux/config.h>

#ifdef CONFIG_IA32_SUPPORT

#include <linux/param.h>

/*
 * 32 bit structures for IA32 support.
 */

/* 32bit compatibility types */
typedef unsigned int	__kernel_size_t32;
typedef int		__kernel_ssize_t32;
typedef int		__kernel_ptrdiff_t32;
typedef int		__kernel_time_t32;
typedef int		__kernel_clock_t32;
typedef int		__kernel_pid_t32;
typedef unsigned short	__kernel_ipc_pid_t32;
typedef unsigned short	__kernel_uid_t32;
typedef unsigned int	__kernel_uid32_t32;
typedef unsigned short	__kernel_gid_t32;
typedef unsigned int	__kernel_gid32_t32;
typedef unsigned short	__kernel_dev_t32;
typedef unsigned int	__kernel_ino_t32;
typedef unsigned short	__kernel_mode_t32;
typedef unsigned short	__kernel_umode_t32;
typedef short		__kernel_nlink_t32;
typedef int		__kernel_daddr_t32;
typedef int		__kernel_off_t32;
typedef unsigned int	__kernel_caddr_t32;
typedef long		__kernel_loff_t32;
typedef __kernel_fsid_t	__kernel_fsid_t32;

#define IA32_PAGE_SHIFT		12	/* 4KB pages */
#define IA32_PAGE_SIZE		(1UL << IA32_PAGE_SHIFT)
#define IA32_PAGE_MASK		(~(IA32_PAGE_SIZE - 1))
#define IA32_PAGE_ALIGN(addr)	(((addr) + IA32_PAGE_SIZE - 1) & IA32_PAGE_MASK)
#define IA32_CLOCKS_PER_SEC	100	/* Cast in stone for IA32 Linux */
#define IA32_TICK(tick)		((unsigned long long)(tick) * IA32_CLOCKS_PER_SEC / CLOCKS_PER_SEC)

struct timespec32 {
	int	tv_sec;
	int	tv_nsec;
};

/* fcntl.h */
struct flock32 {
       short l_type;
       short l_whence;
       __kernel_off_t32 l_start;
       __kernel_off_t32 l_len;
       __kernel_pid_t32 l_pid;
};

#define F_GETLK64	12
#define F_SETLK64	13
#define F_SETLKW64	14

/* sigcontext.h */
/*
 * As documented in the iBCS2 standard..
 *
 * The first part of "struct _fpstate" is just the
 * normal i387 hardware setup, the extra "status"
 * word is used to save the coprocessor status word
 * before entering the handler.
 */
struct _fpreg_ia32 {
       unsigned short significand[4];
       unsigned short exponent;
};

struct _fpxreg_ia32 {
        unsigned short significand[4];
        unsigned short exponent;
        unsigned short padding[3];
};

struct _xmmreg_ia32 {
        unsigned int element[4];
};


struct _fpstate_ia32 {
       unsigned int    cw,
		       sw,
		       tag,
		       ipoff,
		       cssel,
		       dataoff,
		       datasel;
       struct _fpreg_ia32      _st[8];
       unsigned short  status;
       unsigned short  magic;          /* 0xffff = regular FPU data only */

       /* FXSR FPU environment */
       unsigned int         _fxsr_env[6];   /* FXSR FPU env is ignored */
       unsigned int         mxcsr;
       unsigned int         reserved;
       struct _fpxreg_ia32  _fxsr_st[8];    /* FXSR FPU reg data is ignored */
       struct _xmmreg_ia32  _xmm[8];
       unsigned int         padding[56];
};

struct sigcontext_ia32 {
       unsigned short gs, __gsh;
       unsigned short fs, __fsh;
       unsigned short es, __esh;
       unsigned short ds, __dsh;
       unsigned int edi;
       unsigned int esi;
       unsigned int ebp;
       unsigned int esp;
       unsigned int ebx;
       unsigned int edx;
       unsigned int ecx;
       unsigned int eax;
       unsigned int trapno;
       unsigned int err;
       unsigned int eip;
       unsigned short cs, __csh;
       unsigned int eflags;
       unsigned int esp_at_signal;
       unsigned short ss, __ssh;
       unsigned int fpstate;		/* really (struct _fpstate_ia32 *) */
       unsigned int oldmask;
       unsigned int cr2;
};

/* user.h */
/*
 * IA32 (Pentium III/4) FXSR, SSE support
 *
 * Provide support for the GDB 5.0+ PTRACE_{GET|SET}FPXREGS requests for
 * interacting with the FXSR-format floating point environment.  Floating
 * point data can be accessed in the regular format in the usual manner,
 * and both the standard and SIMD floating point data can be accessed via
 * the new ptrace requests.  In either case, changes to the FPU environment
 * will be reflected in the task's state as expected.
 */
struct ia32_user_i387_struct {
	int	cwd;
	int	swd;
	int	twd;
	int	fip;
	int	fcs;
	int	foo;
	int	fos;
	/* 8*10 bytes for each FP-reg = 80 bytes */
	struct _fpreg_ia32 	st_space[8];
};

struct ia32_user_fxsr_struct {
	unsigned short	cwd;
	unsigned short	swd;
	unsigned short	twd;
	unsigned short	fop;
	int	fip;
	int	fcs;
	int	foo;
	int	fos;
	int	mxcsr;
	int	reserved;
	int	st_space[32];	/* 8*16 bytes for each FP-reg = 128 bytes */
	int	xmm_space[32];	/* 8*16 bytes for each XMM-reg = 128 bytes */
	int	padding[56];
};

/* signal.h */
#define _IA32_NSIG	       64
#define _IA32_NSIG_BPW	       32
#define _IA32_NSIG_WORDS	       (_IA32_NSIG / _IA32_NSIG_BPW)

#define IA32_SET_SA_HANDLER(ka,handler,restorer)				\
				((ka)->sa.sa_handler = (__sighandler_t)		\
					(((unsigned long)(restorer) << 32)	\
					 | ((handler) & 0xffffffff)))
#define IA32_SA_HANDLER(ka)	((unsigned long) (ka)->sa.sa_handler & 0xffffffff)
#define IA32_SA_RESTORER(ka)	((unsigned long) (ka)->sa.sa_handler >> 32)

typedef struct {
       unsigned int sig[_IA32_NSIG_WORDS];
} sigset32_t;

struct sigaction32 {
       unsigned int sa_handler;		/* Really a pointer, but need to deal with 32 bits */
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
	unsigned int	  uc_link;
	stack_ia32_t	  uc_stack;
	struct sigcontext_ia32 uc_mcontext;
	sigset_t	  uc_sigmask;	/* mask last for extensibility */
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

struct stat64 {
	unsigned short	st_dev;
	unsigned char	__pad0[10];
	unsigned int	__st_ino;
	unsigned int	st_mode;
	unsigned int	st_nlink;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned short	st_rdev;
	unsigned char	__pad3[10];
	unsigned int	st_size_lo;
	unsigned int	st_size_hi;
	unsigned int	st_blksize;
	unsigned int	st_blocks;	/* Number 512-byte blocks allocated. */
	unsigned int	__pad4;		/* future possible st_blocks high bits */
	unsigned int	st_atime;
	unsigned int	__pad5;
	unsigned int	st_mtime;
	unsigned int	__pad6;
	unsigned int	st_ctime;
	unsigned int	__pad7;		/* will be high 32 bits of ctime someday */
	unsigned int	st_ino_lo;
	unsigned int	st_ino_hi;
};

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

struct linux32_dirent {
	u32	d_ino;
	u32	d_off;
	u16	d_reclen;
	char	d_name[256];
};

struct old_linux32_dirent {
	u32	d_ino;
	u32	d_offset;
	u16	d_namlen;
	char	d_name[1];
};

/*
 * IA-32 ELF specific definitions for IA-64.
 */

#define _ASM_IA64_ELF_H		/* Don't include elf.h */

#include <linux/sched.h>
#include <asm/processor.h>

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == EM_386)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_386

#define IA32_PAGE_OFFSET	0xc0000000
#define IA32_STACK_TOP		IA32_PAGE_OFFSET

/*
 * The system segments (GDT, TSS, LDT) have to be mapped below 4GB so the IA-32 engine can
 * access them.
 */
#define IA32_GDT_OFFSET		(IA32_PAGE_OFFSET)
#define IA32_TSS_OFFSET		(IA32_PAGE_OFFSET + PAGE_SIZE)
#define IA32_LDT_OFFSET		(IA32_PAGE_OFFSET + 2*PAGE_SIZE)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	IA32_PAGE_SIZE

/*
 * This is the location that an ET_DYN program is loaded if exec'ed.
 * Typical use of this is to invoke "./ld.so someprog" to test out a
 * new version of the loader.  We need to make sure that it is out of
 * the way of the program that it will "exec", and that there is
 * sufficient room for the brk.
 */
#define ELF_ET_DYN_BASE		(IA32_PAGE_OFFSET/3 + 0x1000000)

void ia64_elf32_init(struct pt_regs *regs);
#define ELF_PLAT_INIT(_r, load_addr)	ia64_elf32_init(_r)

#define elf_addr_t	u32
#define elf_caddr_t	u32

/* ELF register definitions.  This is needed for core dump support.  */

#define ELF_NGREG	128			/* XXX fix me */
#define ELF_NFPREG	128			/* XXX fix me */

typedef unsigned long elf_greg_t;
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct {
	unsigned long w0;
	unsigned long w1;
} elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

/* This macro yields a bitmask that programs can use to figure out
   what instruction set this CPU supports.  */
#define ELF_HWCAP	0

/* This macro yields a string that ld.so will use to load
   implementation specific libraries for optimization.  Not terribly
   relevant until we have real hardware to play with... */
#define ELF_PLATFORM	0

#ifdef __KERNEL__
# define SET_PERSONALITY(EX,IBCS2)				\
	(current->personality = (IBCS2) ? PER_SVR4 : PER_LINUX)
#endif

#define IA32_EFLAG	0x200

/*
 * IA-32 ELF specific definitions for IA-64.
 */

#define __USER_CS      0x23
#define __USER_DS      0x2B

#define FIRST_TSS_ENTRY 6
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))

#define IA32_SEGSEL_RPL		(0x3 << 0)
#define IA32_SEGSEL_TI		(0x1 << 2)
#define IA32_SEGSEL_INDEX_SHIFT	3

#define IA32_SEG_BASE		16
#define IA32_SEG_TYPE		40
#define IA32_SEG_SYS		44
#define IA32_SEG_DPL		45
#define IA32_SEG_P		47
#define IA32_SEG_HIGH_LIMIT	48
#define IA32_SEG_AVL		52
#define IA32_SEG_DB		54
#define IA32_SEG_G		55
#define IA32_SEG_HIGH_BASE	56

#define IA32_SEG_DESCRIPTOR(base, limit, segtype, nonsysseg, dpl, segpresent, avl, segdb, gran)	\
	       (((limit) & 0xffff)								\
		| (((unsigned long) (base) & 0xffffff) << IA32_SEG_BASE)			\
		| ((unsigned long) (segtype) << IA32_SEG_TYPE)					\
		| ((unsigned long) (nonsysseg) << IA32_SEG_SYS)					\
		| ((unsigned long) (dpl) << IA32_SEG_DPL)					\
		| ((unsigned long) (segpresent) << IA32_SEG_P)					\
		| ((((unsigned long) (limit) >> 16) & 0xf) << IA32_SEG_HIGH_LIMIT)		\
		| ((unsigned long) (avl) << IA32_SEG_AVL)					\
		| ((unsigned long) (segdb) << IA32_SEG_DB)					\
		| ((unsigned long) (gran) << IA32_SEG_G)					\
		| ((((unsigned long) (base) >> 24) & 0xff) << IA32_SEG_HIGH_BASE))

#define SEG_LIM		32
#define SEG_TYPE	52
#define SEG_SYS		56
#define SEG_DPL		57
#define SEG_P		59
#define SEG_AVL		60
#define SEG_DB		62
#define SEG_G		63

/* Unscramble an IA-32 segment descriptor into the IA-64 format.  */
#define IA32_SEG_UNSCRAMBLE(sd)									 \
	(   (((sd) >> IA32_SEG_BASE) & 0xffffff) | ((((sd) >> IA32_SEG_HIGH_BASE) & 0xff) << 24) \
	 | ((((sd) & 0xffff) | ((((sd) >> IA32_SEG_HIGH_LIMIT) & 0xf) << 16)) << SEG_LIM)	 \
	 | ((((sd) >> IA32_SEG_TYPE) & 0xf) << SEG_TYPE)					 \
	 | ((((sd) >> IA32_SEG_SYS) & 0x1) << SEG_SYS)						 \
	 | ((((sd) >> IA32_SEG_DPL) & 0x3) << SEG_DPL)						 \
	 | ((((sd) >> IA32_SEG_P) & 0x1) << SEG_P)						 \
	 | ((((sd) >> IA32_SEG_AVL) & 0x1) << SEG_AVL)						 \
	 | ((((sd) >> IA32_SEG_DB) & 0x1) << SEG_DB)						 \
	 | ((((sd) >> IA32_SEG_G) & 0x1) << SEG_G))

#define IA32_IOBASE	0x2000000000000000 /* Virtual address for I/O space */

#define IA32_CR0	0x80000001	/* Enable PG and PE bits */
#define IA32_CR4	0x600		/* MMXEX and FXSR on */

/*
 *  IA32 floating point control registers starting values
 */

#define IA32_FSR_DEFAULT	0x55550000		/* set all tag bits */
#define IA32_FCR_DEFAULT	0x17800000037fUL	/* extended precision, all masks */

#define IA32_PTRACE_GETREGS	12
#define IA32_PTRACE_SETREGS	13
#define IA32_PTRACE_GETFPREGS	14
#define IA32_PTRACE_SETFPREGS	15
#define IA32_PTRACE_GETFPXREGS	18
#define IA32_PTRACE_SETFPXREGS	19

#define ia32_start_thread(regs,new_ip,new_sp) do {				\
	set_fs(USER_DS);							\
	ia64_psr(regs)->cpl = 3;	/* set user mode */			\
	ia64_psr(regs)->ri = 0;		/* clear return slot number */		\
	ia64_psr(regs)->is = 1;		/* IA-32 instruction set */		\
	regs->cr_iip = new_ip;							\
	regs->ar_rsc = 0xc;		/* enforced lazy mode, priv. level 3 */	\
	regs->ar_rnat = 0;							\
	regs->loadrs = 0;							\
	regs->r12 = new_sp;							\
} while (0)

/*
 * Local Descriptor Table (LDT) related declarations.
 */

#define IA32_LDT_ENTRIES	8192		/* Maximum number of LDT entries supported. */
#define IA32_LDT_ENTRY_SIZE	8		/* The size of each LDT entry. */

struct ia32_modify_ldt_ldt_s {
	unsigned int entry_number;
	unsigned int base_addr;
	unsigned int limit;
	unsigned int seg_32bit:1;
	unsigned int contents:2;
	unsigned int read_exec_only:1;
	unsigned int limit_in_pages:1;
	unsigned int seg_not_present:1;
	unsigned int useable:1;
};

extern void ia32_gdt_init (void);
extern int ia32_setup_frame1 (int sig, struct k_sigaction *ka, siginfo_t *info,
			       sigset_t *set, struct pt_regs *regs);
extern void ia32_init_addr_space (struct pt_regs *regs);
extern int ia32_setup_arg_pages (struct linux_binprm *bprm);
extern int ia32_exception (struct pt_regs *regs, unsigned long isr);
extern int ia32_intercept (struct pt_regs *regs, unsigned long isr);
extern unsigned long ia32_do_mmap (struct file *, unsigned long, unsigned long, int, int, loff_t);
extern void ia32_load_segment_descriptors (struct task_struct *task);

#define ia32f2ia64f(dst,src) \
	do { \
	register double f6 asm ("f6"); \
	asm volatile ("ldfe f6=[%2];; stf.spill [%1]=f6" : "=f"(f6): "r"(dst), "r"(src) : "memory"); \
	} while(0)

#define ia64f2ia32f(dst,src) \
	do { \
	register double f6 asm ("f6"); \
	asm volatile ("ldf.fill f6=[%2];; stfe [%1]=f6" : "=f"(f6): "r"(dst),  "r"(src) : "memory"); \
	} while(0)

#endif /* !CONFIG_IA32_SUPPORT */

#endif /* _ASM_IA64_IA32_H */
