#ifndef __ASM_SH_ELF_H
#define __ASM_SH_ELF_H

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>
#include <asm/user.h>

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof (struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct user_fpu_struct elf_fpregset_t;

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ( (x)->e_machine == EM_SH )

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#ifdef __LITTLE_ENDIAN__
#define ELF_DATA	ELFDATA2LSB
#else
#define ELF_DATA	ELFDATA2MSB
#endif
#define ELF_ARCH	EM_SH

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         (2 * TASK_SIZE / 3)


#define ELF_CORE_COPY_REGS(_dest,_regs)				\
	memcpy((char *) &_dest, (char *) _regs,			\
	       sizeof(struct pt_regs));

/* This yields a mask that user programs can use to figure out what
   instruction set this CPU supports.  This could be done in user space,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP	(0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.

   For the moment, we have only optimizations for the Intel generations,
   but that could change... */

#define ELF_PLATFORM  (NULL)

#define ELF_PLAT_INIT(_r, load_addr) \
  do { _r->regs[0]=0; _r->regs[1]=0; _r->regs[2]=0; _r->regs[3]=0; \
       _r->regs[4]=0; _r->regs[5]=0; _r->regs[6]=0; _r->regs[7]=0; \
       _r->regs[8]=0; _r->regs[9]=0; _r->regs[10]=0; _r->regs[11]=0; \
       _r->regs[12]=0; _r->regs[13]=0; _r->regs[14]=0; \
       _r->sr = SR_FD; } while (0)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2) set_personality(PER_LINUX_32BIT)
#endif

#endif /* __ASM_SH_ELF_H */
