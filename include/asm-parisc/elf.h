#ifndef __ASMPARISC_ELF_H
#define __ASMPARISC_ELF_H

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>

#define EM_PARISC 15

/*
 * The following definitions are those for 32-bit ELF binaries on a 32-bit kernel
 * and for 64-bit binaries on a 64-bit kernel.  To run 32-bit binaries on a 64-bit
 * kernel, arch/parisc64/kernel/binfmt_elf32.c defines these macros appropriately
 * and then #includes binfmt_elf.c, which then includes this file.
 */
#ifndef ELF_CLASS

/*
 * This is used to ensure we don't load something for the wrong architecture.
 *
 * Note that this header file is used by default in fs/binfmt_elf.c. So
 * the following macros are for the default case. However, for the 64
 * bit kernel we also support 32 bit parisc binaries. To do that
 * arch/parisc64/kernel/binfmt_elf32.c defines its own set of these
 * macros, and then it includes fs/binfmt_elf.c to provide an alternate
 * elf binary handler for 32 bit binaries (on the 64 bit kernel).
 */
#ifdef __LP64__
#define ELF_CLASS       ELFCLASS64
#else
#define ELF_CLASS	ELFCLASS32
#endif

typedef unsigned long elf_greg_t;

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.

   For the moment, we have only optimizations for the Intel generations,
   but that could change... */

#define ELF_PLATFORM  ("PARISC\0" /*+((boot_cpu_data.x86-3)*5) */)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2) \
	current->personality = PER_LINUX
#endif

/*
 * Fill in general registers in a core dump.  This saves pretty
 * much the same registers as hp-ux, although in a different order.
 * Registers marked # below are not currently saved in pt_regs, so
 * we use their current values here.
 *
 * 	gr0..gr31
 * 	sr0..sr7
 * 	iaoq0..iaoq1
 * 	iasq0..iasq1
 * 	cr11 (sar)
 * 	cr19 (iir)
 * 	cr20 (isr)
 * 	cr21 (ior)
 *  #	cr22 (ipsw)
 *  #	cr0 (recovery counter)
 *  #	cr24..cr31 (temporary registers)
 *  #	cr8,9,12,13 (protection IDs)
 *  #	cr10 (scr/ccr)
 *  #	cr15 (ext int enable mask)
 *
 */

#define ELF_CORE_COPY_REGS(dst, pt)	\
	memset(dst, 0, sizeof(dst));	/* don't leak any "random" bits */ \
	memcpy(dst + 0, pt->gr, 32 * sizeof(elf_greg_t)); \
	memcpy(dst + 32, pt->sr, 8 * sizeof(elf_greg_t)); \
	memcpy(dst + 40, pt->iaoq, 2 * sizeof(elf_greg_t)); \
	memcpy(dst + 42, pt->iasq, 2 * sizeof(elf_greg_t)); \
	dst[44] = pt->sar;   dst[45] = pt->iir; \
	dst[46] = pt->isr;   dst[47] = pt->ior; \
	dst[48] = mfctl(22); dst[49] = mfctl(0); \
	dst[50] = mfctl(24); dst[51] = mfctl(25); \
	dst[52] = mfctl(26); dst[53] = mfctl(27); \
	dst[54] = mfctl(28); dst[55] = mfctl(29); \
	dst[56] = mfctl(30); dst[57] = mfctl(31); \
	dst[58] = mfctl( 8); dst[59] = mfctl( 9); \
	dst[60] = mfctl(12); dst[61] = mfctl(13); \
	dst[62] = mfctl(10); dst[63] = mfctl(15);

#endif /* ! ELF_CLASS */

#define ELF_NGREG 80	/* We only need 64 at present, but leave space
			   for expansion. */
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

#define ELF_NFPREG 32
typedef double elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

struct pt_regs;	/* forward declaration... */


#define elf_check_arch(x) ((x)->e_machine == EM_PARISC && (x)->e_ident[EI_CLASS] == ELF_CLASS)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_PARISC

/* %r23 is set by ld.so to a pointer to a function which might be 
   registered using atexit.  This provides a mean for the dynamic
   linker to call DT_FINI functions for shared libraries that have
   been loaded before the code runs.

   So that we can use the same startup file with static executables,
   we start programs with a value of 0 to indicate that there is no
   such function.  */
#define ELF_PLAT_INIT(_r, load_addr)       _r->gr[23] = 0

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.

   (2 * TASK_SIZE / 3) turns into something undefined when run through a
   32 bit preprocessor and in some cases results in the kernel trying to map
   ld.so to the kernel virtual base. Use a sane value instead. /Jes 
  */

#define ELF_ET_DYN_BASE         (TASK_UNMAPPED_BASE + 0x01000000)

/* This yields a mask that user programs can use to figure out what
   instruction set this CPU supports.  This could be done in user space,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP	0
/* (boot_cpu_data.x86_capability) */

#endif
