/* $NetBSD: cpu.h,v 1.2 2001/02/23 21:23:52 reinoud Exp $ */
/* $FreeBSD$ */

#ifndef MACHINE_CPU_H
#define MACHINE_CPU_H

#include <machine/armreg.h>
#include <machine/frame.h>

void	cpu_halt(void);
void	swi_vm(void *);

#ifdef _KERNEL
static __inline uint64_t
get_cyclecount(void)
{
/* This '#if' asks the question 'Does CP15/SCC include performance counters?' */
#if defined(CPU_ARM1136) || defined(CPU_ARM1176) \
 || defined(CPU_MV_PJ4B) \
 || defined(CPU_CORTEXA) || defined(CPU_KRAIT)
	uint32_t ccnt;
	uint64_t ccnt64;

	/*
	 * Read PMCCNTR. Curses! Its only 32 bits.
	 * TODO: Fix this by catching overflow with interrupt?
	 */
/* The ARMv6 vs ARMv7 divide is going to need a better way of
 * distinguishing between them.
 */
#if defined(CPU_ARM1136) || defined(CPU_ARM1176)
	/* ARMv6 - Earlier model SCCs */
	__asm __volatile("mrc p15, 0, %0, c15, c12, 1": "=r" (ccnt));
#else
	/* ARMv7 - Later model SCCs */
	__asm __volatile("mrc p15, 0, %0, c9, c13, 0": "=r" (ccnt));
#endif
	ccnt64 = (uint64_t)ccnt;
	return (ccnt64);
#else /* No performance counters, so use binuptime(9). This is slooooow */
	struct bintime bt;

	binuptime(&bt);
	return ((uint64_t)bt.sec << 56 | bt.frac >> 8);
#endif
}
#endif

#define TRAPF_USERMODE(frame)	((frame->tf_spsr & PSR_MODE) == PSR_USR32_MODE)

#define TRAPF_PC(tfp)		((tfp)->tf_pc)

#define cpu_getstack(td)	((td)->td_frame->tf_usr_sp)
#define cpu_setstack(td, sp)	((td)->td_frame->tf_usr_sp = (sp))
#define cpu_spinwait()		/* nothing */

#define ARM_NVEC		8
#define ARM_VEC_ALL		0xffffffff

extern vm_offset_t vector_page;

/*
 * Params passed into initarm. If you change the size of this you will
 * need to update locore.S to allocate more memory on the stack before
 * it calls initarm.
 */
struct arm_boot_params {
	register_t	abp_size;	/* Size of this structure */
	register_t	abp_r0;		/* r0 from the boot loader */
	register_t	abp_r1;		/* r1 from the boot loader */
	register_t	abp_r2;		/* r2 from the boot loader */
	register_t	abp_r3;		/* r3 from the boot loader */
	vm_offset_t	abp_physaddr;	/* The kernel physical address */
	vm_offset_t	abp_pagetable;	/* The early page table */
};

void	arm_vector_init(vm_offset_t, int);
void	fork_trampoline(void);
void	identify_arm_cpu(void);
void	*initarm(struct arm_boot_params *);

extern char btext[];
extern char etext[];
int badaddr_read(void *, size_t, void *);
#endif /* !MACHINE_CPU_H */
