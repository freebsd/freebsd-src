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
#include <x86/apicvar.h>
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
extern void *dpcpu;
extern char *bootSTK;
extern int bootAP;
extern void *bootstacks[];
extern volatile u_int cpu_ipi_pending[];
extern volatile int aps_ready;
extern struct mtx ap_boot_mtx;
extern int cpu_logical;
extern int cpu_cores;
extern int pmap_pcid_enabled;
extern int invpcid_works;
extern u_int xhits_gbl[];
extern u_int xhits_pg[];
extern u_int xhits_rng[];
extern u_int ipi_global;
extern u_int ipi_page;
extern u_int ipi_range;
extern u_int ipi_range_size;

extern volatile int smp_tlb_wait;

struct cpu_info {
	int	cpu_present:1;
	int	cpu_bsp:1;
	int	cpu_disabled:1;
	int	cpu_hyperthread:1;
};
extern struct cpu_info cpu_info[];

#ifdef COUNT_IPIS
extern u_long *ipi_invltlb_counts[MAXCPU];
extern u_long *ipi_invlrng_counts[MAXCPU];
extern u_long *ipi_invlpg_counts[MAXCPU];
extern u_long *ipi_invlcache_counts[MAXCPU];
extern u_long *ipi_rendezvous_counts[MAXCPU];
#endif

/* IPI handlers */
inthand_t
	IDTVEC(invltlb),	/* TLB shootdowns - global */
	IDTVEC(invltlb_pcid),	/* TLB shootdowns - global, pcid */
	IDTVEC(invltlb_invpcid),/* TLB shootdowns - global, invpcid */
	IDTVEC(invlpg),		/* TLB shootdowns - 1 page */
	IDTVEC(invlrng),	/* TLB shootdowns - page range */
	IDTVEC(invlcache),	/* Write back and invalidate cache */
	IDTVEC(ipi_intr_bitmap_handler), /* Bitmap based IPIs */ 
	IDTVEC(cpustop),	/* CPU stops & waits to be restarted */
	IDTVEC(cpususpend),	/* CPU suspends & waits to be resumed */
	IDTVEC(justreturn),	/* interrupt CPU with minimum overhead */
	IDTVEC(rendezvous);	/* handle CPU rendezvous */

struct pmap;

/* functions in mp_machdep.c */
void	assign_cpu_ids(void);
void	cpu_add(u_int apic_id, char boot_cpu);
void	cpustop_handler(void);
void	cpususpend_handler(void);
void	init_secondary_tail(void);
void	invltlb_handler(void);
void	invltlb_pcid_handler(void);
void	invltlb_invpcid_handler(void);
void	invlpg_handler(void);
void	invlrng_handler(void);
void	invlcache_handler(void);
void	init_secondary(void);
void	ipi_startup(int apic_id, int vector);
void	ipi_all_but_self(u_int ipi);
void 	ipi_bitmap_handler(struct trapframe frame);
void	ipi_cpu(int cpu, u_int ipi);
int	ipi_nmi_handler(void);
void	ipi_selected(cpuset_t cpus, u_int ipi);
u_int	mp_bootaddress(u_int);
void	set_interrupt_apic_ids(void);
void	smp_cache_flush(void);
void	smp_masked_invlpg(cpuset_t mask, vm_offset_t addr);
void	smp_masked_invlpg_range(cpuset_t mask, vm_offset_t startva,
	    vm_offset_t endva);
void	smp_masked_invltlb(cpuset_t mask, struct pmap *pmap);
int	native_start_all_aps(void);
void	mem_range_AP_init(void);
void	topo_probe(void);
void	ipi_send_cpu(int cpu, u_int ipi);

#endif /* !LOCORE */
#endif /* SMP */

#endif /* _KERNEL */
#endif /* _MACHINE_SMP_H_ */
