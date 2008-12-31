/* $NetBSD: cpu.h,v 1.2 2001/02/23 21:23:52 reinoud Exp $ */
/* $FreeBSD: src/sys/arm/include/cpu.h,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $ */

#ifndef MACHINE_CPU_H
#define MACHINE_CPU_H

#include <machine/armreg.h>

void    cpu_halt(void);
void    swi_vm(void *);

#ifdef _KERNEL
static __inline uint64_t
get_cyclecount(void)
{
	struct bintime bt;

	binuptime(&bt);
	return (bt.frac ^ bt.sec);
			
}
#endif

#define TRAPF_USERMODE(frame)	((frame->tf_spsr & PSR_MODE) == PSR_USR32_MODE)

#define TRAPF_PC(tfp)		((tfp)->tf_pc)

#define cpu_getstack(td)        ((td)->td_frame->tf_usr_sp)
#define cpu_setstack(td, sp)    ((td)->td_frame->tf_usr_sp = (sp))
#define cpu_spinwait()		/* nothing */

#define ARM_NVEC		8
#define ARM_VEC_ALL		0xffffffff

extern vm_offset_t vector_page;

void	arm_vector_init(vm_offset_t, int);
void	fork_trampoline(void);
void	identify_arm_cpu(void);
void	*initarm(void *, void *);

extern char btext[];
extern char etext[];
int badaddr_read (void *, size_t, void *);
#endif /* !MACHINE_CPU_H */
