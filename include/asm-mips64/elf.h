/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef _ASM_ELF_H
#define _ASM_ELF_H

#include <asm/ptrace.h>
#include <asm/user.h>

/* ELF header e_flags defines. */
/* MIPS architecture level. */
#define EF_MIPS_ARCH_1      0x00000000  /* -mips1 code.  */
#define EF_MIPS_ARCH_2      0x10000000  /* -mips2 code.  */
#define EF_MIPS_ARCH_3      0x20000000  /* -mips3 code.  */
#define EF_MIPS_ARCH_4      0x30000000  /* -mips4 code.  */
#define EF_MIPS_ARCH_5      0x40000000  /* -mips5 code.  */
#define EF_MIPS_ARCH_32     0x50000000  /* MIPS32 code.  */
#define EF_MIPS_ARCH_64     0x60000000  /* MIPS64 code.  */
/* The ABI of a file. */
#define EF_MIPS_ABI_O32     0x00001000  /* O32 ABI.  */
#define EF_MIPS_ABI_O64     0x00002000  /* O32 extended for 64 bit.  */

#ifndef ELF_ARCH
/* ELF register definitions */
#define ELF_NGREG	45
#define ELF_NFPREG	33

typedef unsigned long elf_greg_t;
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef double elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(hdr)						\
({									\
	int __res = 1;							\
	struct elfhdr *__h = (hdr);					\
									\
	if (__h->e_machine != EM_MIPS)					\
		__res = 0;						\
	if (__h->e_ident[EI_CLASS] == ELFCLASS32) 			\
		__res = 0;						\
									\
	__res;								\
})

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS64
#ifdef __MIPSEB__
#define ELF_DATA	ELFDATA2MSB
#elif __MIPSEL__
#define ELF_DATA	ELFDATA2LSB
#endif
#define ELF_ARCH	EM_MIPS
#endif /* !defined(ELF_ARCH) */

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	PAGE_SIZE

#define ELF_CORE_COPY_REGS(_dest,_regs)				\
	memcpy((char *) &_dest, (char *) _regs,			\
	       sizeof(struct pt_regs));

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  This could be done in userspace,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP       (0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.

   For the moment, we have only optimizations for the Intel generations,
   but that could change... */

#define ELF_PLATFORM  (NULL)

/*
 * See comments in asm-alpha/elf.h, this is the same thing
 * on the MIPS.
 */
#define ELF_PLAT_INIT(_r, load_addr)	do { \
	_r->regs[1] = _r->regs[2] = _r->regs[3] = _r->regs[4] = 0;	\
	_r->regs[5] = _r->regs[6] = _r->regs[7] = _r->regs[8] = 0;	\
	_r->regs[9] = _r->regs[10] = _r->regs[11] = _r->regs[12] = 0;	\
	_r->regs[13] = _r->regs[14] = _r->regs[15] = _r->regs[16] = 0;	\
	_r->regs[17] = _r->regs[18] = _r->regs[19] = _r->regs[20] = 0;	\
	_r->regs[21] = _r->regs[22] = _r->regs[23] = _r->regs[24] = 0;	\
	_r->regs[25] = _r->regs[26] = _r->regs[27] = _r->regs[28] = 0;	\
	_r->regs[30] = _r->regs[31] = 0;				\
} while (0)

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#ifndef ELF_ET_DYN_BASE
#define ELF_ET_DYN_BASE         (TASK_SIZE / 3 * 2)
#endif

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2)			\
do {	current->thread.mflags &= ~MF_ABI_MASK;		\
	if ((ex).e_ident[EI_CLASS] == ELFCLASS32) {	\
		if ((((ex).e_flags & EF_MIPS_ABI2) != 0) &&	\
		     ((ex).e_flags & EF_MIPS_ABI) == 0)		\
			current->thread.mflags |= MF_N32;	\
		else						\
			current->thread.mflags |= MF_O32;	\
	} else						\
		current->thread.mflags |= MF_N64;	\
	if (ibcs2)					\
		set_personality(PER_SVR4);		\
	else if (current->personality != PER_LINUX32)	\
		set_personality(PER_LINUX);		\
} while (0)
#endif

#endif /* _ASM_ELF_H */
