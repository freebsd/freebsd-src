/*
 * binfmt_elf32.c: Support 32-bit Sparc ELF binaries on Ultra.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 David S. Miller	(davem@redhat.com)
 * Copyright (C) 1995, 1996, 1997, 1998 Jakub Jelinek	(jj@ultra.linux.cz)
 */

#define ELF_ARCH		EM_SPARC
#define ELF_CLASS		ELFCLASS32
#define ELF_DATA		ELFDATA2MSB;

/* For the most part we present code dumps in the format
 * Solaris does.
 */
typedef unsigned int elf_greg_t;
#define ELF_NGREG 38
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

/* Format is:
 * 	G0 --> G7
 *	O0 --> O7
 *	L0 --> L7
 *	I0 --> I7
 *	PSR, PC, nPC, Y, WIM, TBR
 */
#include <asm/psrcompat.h>
#define ELF_CORE_COPY_REGS(__elf_regs, __pt_regs)	\
do {	unsigned int *dest = &(__elf_regs[0]);		\
	struct pt_regs *src = (__pt_regs);		\
	unsigned int *sp;				\
	int i;						\
	for(i = 0; i < 16; i++)				\
		dest[i] = (unsigned int) src->u_regs[i];\
	/* Don't try this at home kids... */		\
	set_fs(USER_DS);				\
	sp = (unsigned int *) (src->u_regs[14] &	\
		0x00000000fffffffc);			\
	for(i = 0; i < 16; i++)				\
		__get_user(dest[i+16], &sp[i]);		\
	set_fs(KERNEL_DS);				\
	dest[32] = tstate_to_psr(src->tstate);		\
	dest[33] = (unsigned int) src->tpc;		\
	dest[34] = (unsigned int) src->tnpc;		\
	dest[35] = src->y;				\
	dest[36] = dest[37] = 0; /* XXX */		\
} while (0);

typedef struct {
	union {
		unsigned int	pr_regs[32];
		unsigned long	pr_dregs[16];
	} pr_fr;
	unsigned int __unused;
	unsigned int	pr_fsr;
	unsigned char	pr_qcnt;
	unsigned char	pr_q_entrysize;
	unsigned char	pr_en;
	unsigned int	pr_q[64];
} elf_fpregset_t;

/* UltraSparc extensions.  Still unused, but will be eventually.  */
typedef struct {
	unsigned int pr_type;
	unsigned int pr_align;
	union {
		struct {
			union {
				unsigned int	pr_regs[32];
				unsigned long	pr_dregs[16];
				long double	pr_qregs[8];
			} pr_xfr;
		} pr_v8p;
		unsigned int	pr_xfsr;
		unsigned int	pr_fprs;
		unsigned int	pr_xg[8];
		unsigned int	pr_xo[8];
		unsigned long	pr_tstate;
		unsigned int	pr_filler[8];
	} pr_un;
} elf_xregset_t;

#define elf_check_arch(x)	(((x)->e_machine == EM_SPARC) || ((x)->e_machine == EM_SPARC32PLUS))

#define ELF_ET_DYN_BASE         0x08000000


#include <asm/processor.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/elfcore.h>

struct timeval32
{
	int tv_sec, tv_usec;
};

#define elf_prstatus elf_prstatus32
struct elf_prstatus32
{
	struct elf_siginfo pr_info;	/* Info associated with signal */
	short	pr_cursig;		/* Current signal */
	unsigned int pr_sigpend;	/* Set of pending signals */
	unsigned int pr_sighold;	/* Set of held signals */
	pid_t	pr_pid;
	pid_t	pr_ppid;
	pid_t	pr_pgrp;
	pid_t	pr_sid;
	struct timeval32 pr_utime;	/* User time */
	struct timeval32 pr_stime;	/* System time */
	struct timeval32 pr_cutime;	/* Cumulative user time */
	struct timeval32 pr_cstime;	/* Cumulative system time */
	elf_gregset_t pr_reg;	/* GP registers */
	int pr_fpvalid;		/* True if math co-processor being used.  */
};

#define elf_prpsinfo elf_prpsinfo32
struct elf_prpsinfo32
{
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* char for pr_state */
	char	pr_zomb;	/* zombie */
	char	pr_nice;	/* nice val */
	unsigned int pr_flag;	/* flags */
	u16	pr_uid;
	u16	pr_gid;
	pid_t	pr_pid, pr_ppid, pr_pgrp, pr_sid;
	/* Lots missing */
	char	pr_fname[16];	/* filename of executable */
	char	pr_psargs[ELF_PRARGSZ];	/* initial part of arg list */
};

#include <linux/highuid.h>

#undef NEW_TO_OLD_UID
#undef NEW_TO_OLD_GID
#define NEW_TO_OLD_UID(uid) ((uid) > 65535) ? (u16)overflowuid : (u16)(uid)
#define NEW_TO_OLD_GID(gid) ((gid) > 65535) ? (u16)overflowgid : (u16)(gid)

#define elf_addr_t	u32
#define elf_caddr_t	u32
#undef start_thread
#define start_thread start_thread32
#define init_elf_binfmt init_elf32_binfmt
#undef CONFIG_BINFMT_ELF
#ifdef CONFIG_BINFMT_ELF32
#define CONFIG_BINFMT_ELF CONFIG_BINFMT_ELF32
#endif
#undef CONFIG_BINFMT_ELF_MODULE
#ifdef CONFIG_BINFMT_ELF32_MODULE
#define CONFIG_BINFMT_ELF_MODULE CONFIG_BINFMT_ELF32_MODULE
#endif

MODULE_DESCRIPTION("Binary format loader for compatibility with 32bit SparcLinux binaries on the Ultra");
MODULE_AUTHOR("Eric Youngdale, David S. Miller, Jakub Jelinek");

#undef MODULE_DESCRIPTION
#undef MODULE_AUTHOR

#include "../../../fs/binfmt_elf.c"
