/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef _KERNEL

#ifdef SMP

#ifndef LOCORE

#include <sys/bus.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/pcb.h>

/* global symbols in mpboot.S */
extern char			mptramp_start[];
extern char			mptramp_end[];
extern u_int32_t		mptramp_pagetables;

/* global data in mp_machdep.c */
extern int			mp_naps;
extern int			boot_cpu_id;
extern struct pcb		stoppcbs[];
extern int			cpu_apic_ids[];
#ifdef COUNT_IPIS
extern u_long *ipi_invltlb_counts[MAXCPU];
extern u_long *ipi_invlrng_counts[MAXCPU];
extern u_long *ipi_invlpg_counts[MAXCPU];
extern u_long *ipi_invlcache_counts[MAXCPU];
extern u_long *ipi_rendezvous_counts[MAXCPU];
extern u_long *ipi_lazypmap_counts[MAXCPU];
#endif

/* IPI handlers */
inthand_t
	IDTVEC(invltlb),	/* TLB shootdowns - global */
	IDTVEC(invlpg),		/* TLB shootdowns - 1 page */
	IDTVEC(invlrng),	/* TLB shootdowns - page range */
	IDTVEC(invlcache),	/* Write back and invalidate cache */
	IDTVEC(ipi_intr_bitmap_handler), /* Bitmap based IPIs */ 
	IDTVEC(cpustop),	/* CPU stops & waits to be restarted */
	IDTVEC(cpususpend),	/* CPU suspends & waits to be resumed */
	IDTVEC(rendezvous);	/* handle CPU rendezvous */

/* functions in mp_machdep.c */
void	cpu_add(u_int apic_id, char boot_cpu);
void	cpustop_handler(void);
void	cpususpend_handler(void);
void	init_secondary(void);
int	ipi_nmi_handler(void);
void	ipi_selected(cpumask_t cpus, u_int ipi);
void	ipi_all_but_self(u_int ipi);
void 	ipi_bitmap_handler(struct trapframe frame);
u_int	mp_bootaddress(u_int);
int	mp_grab_cpu_hlt(void);
void	smp_cache_flush(void);
void	smp_invlpg(vm_offset_t addr);
void	smp_masked_invlpg(cpumask_t mask, vm_offset_t addr);
void	smp_invlpg_range(vm_offset_t startva, vm_offset_t endva);
void	smp_masked_invlpg_range(cpumask_t mask, vm_offset_t startva,
	    vm_offset_t endva);
void	smp_invltlb(void);
void	smp_masked_invltlb(cpumask_t mask);

#endif /* !LOCORE */
#endif /* SMP */

#endif /* _KERNEL */
#endif /* _MACHINE_SMP_H_ */
