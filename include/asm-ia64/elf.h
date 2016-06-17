#ifndef _ASM_IA64_ELF_H
#define _ASM_IA64_ELF_H

/*
 * ELF archtecture specific definitions.
 *
 * Copyright (C) 1998, 1999, 2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <asm/fpu.h>
#include <asm/page.h>

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == EM_IA_64)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS64
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_IA_64

#define USE_ELF_CORE_DUMP

/* Least-significant four bits of ELF header's e_flags are OS-specific.  The bits are
   interpreted as follows by Linux: */
#define EF_IA_64_LINUX_EXECUTABLE_STACK	0x1	/* is stack (& heap) executable by default? */

/* always align to 64KB to allow for future page sizes of up to 64KB: */
#define ELF_EXEC_PAGESIZE	PAGE_SIZE

/*
 * This is the location that an ET_DYN program is loaded if exec'ed.
 * Typical use of this is to invoke "./ld.so someprog" to test out a
 * new version of the loader.  We need to make sure that it is out of
 * the way of the program that it will "exec", and that there is
 * sufficient room for the brk.
 */
#define ELF_ET_DYN_BASE		(TASK_UNMAPPED_BASE + 0x800000000)


/*
 * We use (abuse?) this macro to insert the (empty) vm_area that is
 * used to map the register backing store.  I don't see any better
 * place to do this, but we should discuss this with Linus once we can
 * talk to him...
 */
extern void ia64_init_addr_space (void);
#define ELF_PLAT_INIT(_r, load_addr)	ia64_init_addr_space()

/* ELF register definitions.  This is needed for core dump support.  */

/*
 * elf_gregset_t contains the application-level state in the following order:
 *	r0-r31
 *	NaT bits (for r0-r31; bit N == 1 iff rN is a NaT)
 *	predicate registers (p0-p63)
 *	b0-b7
 *	ip cfm psr
 *	ar.rsc ar.bsp ar.bspstore ar.rnat
 *	ar.ccv ar.unat ar.fpsr ar.pfs ar.lc ar.ec ar.csd ar.ssd
 */
#define ELF_NGREG	128	/* we really need just 72 but let's leave some headroom... */
#define ELF_NFPREG	128	/* f0 and f1 could be omitted, but so what... */

typedef unsigned long elf_greg_t;
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct ia64_fpreg elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

struct pt_regs;	/* forward declaration... */
extern void ia64_elf_core_copy_regs (struct pt_regs *src, elf_gregset_t dst);
#define ELF_CORE_COPY_REGS(_dest,_regs)	ia64_elf_core_copy_regs(_regs, _dest);

/* This macro yields a bitmask that programs can use to figure out
   what instruction set this CPU supports.  */
#define ELF_HWCAP 	0

/* This macro yields a string that ld.so will use to load
   implementation specific libraries for optimization.  Not terribly
   relevant until we have real hardware to play with... */
#define ELF_PLATFORM	0

#ifdef __KERNEL__
struct elf64_hdr;
extern void ia64_set_personality (struct elf64_hdr *elf_ex, int ibcs2_interpreter);
#define SET_PERSONALITY(ex, ibcs2)	ia64_set_personality(&(ex), ibcs2)
#endif

#endif /* _ASM_IA64_ELF_H */
