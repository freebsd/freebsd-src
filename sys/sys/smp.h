/*
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

#if defined(SMP) && !defined(APIC_IO)
# error APIC_IO required for SMP, add "options APIC_IO" to your config file.
#endif /* SMP && !APIC_IO */

/* Number of CPUs. */
#if defined(SMP) && !defined(NCPU)
# define NCPU			2
#endif /* SMP && NCPU */

/* Number of IO APICs. */
#if defined(APIC_IO) && !defined(NAPIC)
# define NAPIC			2
#endif /* SMP && NAPIC */


#if defined(SMP) || defined(APIC_IO)

#ifndef LOCORE

/*
 * For sending values to POST displays.
 * XXX FIXME: where does this really belong, isa.h/isa.c perhaps?
 */
extern int current_postcode;  /** XXX currently in mp_machdep.c */
#define POSTCODE(X)	current_postcode = (X), \
			outb(0x80, current_postcode)
#define POSTCODE_LO(X)	current_postcode &= 0xf0, \
			current_postcode |= ((X) & 0x0f), \
			outb(0x80, current_postcode)
#define POSTCODE_HI(X)	current_postcode &= 0x0f, \
			current_postcode |= (((X) << 4) & 0xf0), \
			outb(0x80, current_postcode)


#include <machine/apic.h>

/* global data in mpboot.s */
extern int			bootMP_size;

/* functions in mpboot.s */
void	bootMP			__P((void));

/* global data in mplock.s */
extern u_int			mp_lock;
extern u_int			isr_lock;
#ifdef RECURSIVE_MPINTRLOCK
extern u_int			mpintr_lock;
#endif /*  RECURSIVE_MPINTRLOCK */

/* functions in mplock.s */
void	get_mplock		__P((void));
void	rel_mplock		__P((void));
int		try_mplock		__P((void));
#ifdef RECURSIVE_MPINTRLOCK
void	get_mpintrlock		__P((void));
void	rel_mpintrlock		__P((void));
int		try_mpintrlock		__P((void));
#endif /*  RECURSIVE_MPINTRLOCK */

/* global data in apic_vector.s */
extern volatile u_int		stopped_cpus;
extern volatile u_int		started_cpus;

extern volatile u_int		checkstate_probed_cpus;
extern volatile u_int		checkstate_need_ast;
extern volatile u_int		resched_cpus;
extern void (*cpustop_restartfunc) __P((void));

/* functions in apic_ipl.s */
void	apic_eoi		__P((void));
u_int	io_apic_read		__P((int, int));
void	io_apic_write		__P((int, int, u_int));

/* global data in mp_machdep.c */
extern int			bsp_apic_ready;
extern int			mp_ncpus;
extern int			mp_naps;
extern int			mp_nbusses;
extern int			mp_napics;
extern int			mp_picmode;
extern int			boot_cpu_id;
extern vm_offset_t		cpu_apic_address;
extern vm_offset_t		io_apic_address[];
extern u_int32_t		cpu_apic_versions[];
extern u_int32_t		io_apic_versions[];
extern int			cpu_num_to_apic_id[];
extern int			io_num_to_apic_id[];
extern int			apic_id_to_logical[];
#define APIC_INTMAPSIZE 24
struct apic_intmapinfo {
  	int ioapic;
	int int_pin;
	volatile void *apic_address;
	int redirindex;
};
extern struct apic_intmapinfo	int_to_apicintpin[];
extern u_int			all_cpus;
extern struct pcb		stoppcbs[];

/* functions in mp_machdep.c */
u_int	mp_bootaddress		__P((u_int));
int	mp_probe		__P((void));
void	mp_start		__P((void));
void	mp_announce		__P((void));
u_int	isa_apic_mask		__P((u_int));
int	isa_apic_irq		__P((int));
int	pci_apic_irq		__P((int, int, int));
int	apic_irq		__P((int, int));
int	next_apic_irq		__P((int));
int	undirect_isa_irq	__P((int));
int	undirect_pci_irq	__P((int));
int	apic_bus_type		__P((int));
int	apic_src_bus_id		__P((int, int));
int	apic_src_bus_irq	__P((int, int));
int	apic_int_type		__P((int, int));
int	apic_trigger		__P((int, int));
int	apic_polarity		__P((int, int));
void	assign_apic_irq		__P((int apic, int intpin, int irq));
void	revoke_apic_irq		__P((int irq));
void	bsp_apic_configure	__P((void));
void	init_secondary		__P((void));
void	smp_invltlb		__P((void));
int	stop_cpus		__P((u_int));
int	restart_cpus		__P((u_int));
#ifdef BETTER_CLOCK 
void	forward_statclock	__P((int pscnt));
void	forward_hardclock	__P((int pscnt));
#endif /* BETTER_CLOCK */
void	forward_signal		__P((struct proc *));
void	forward_roundrobin	__P((void));
#ifdef	APIC_INTR_REORDER
void	set_lapic_isrloc	__P((int, int));
#endif /* APIC_INTR_REORDER */
void	smp_rendezvous_action	__P((void));
void	smp_rendezvous		__P((void (*)(void *), 
				     void (*)(void *),
				     void (*)(void *),
				     void *arg));

/* global data in mpapic.c */
extern volatile lapic_t		lapic;
extern volatile ioapic_t	*ioapic[];

/* functions in mpapic.c */
void	apic_dump		__P((char*));
void	apic_initialize		__P((void));
void	imen_dump		__P((void));
int	apic_ipi		__P((int, int, int));
int	selected_apic_ipi	__P((u_int, int, int));
int	io_apic_setup		__P((int));
void	io_apic_set_id		__P((int, int));
int	io_apic_get_id		__P((int));
int	ext_int_setup		__P((int, int));

#if defined(READY)
void	clr_io_apic_mask24	__P((int, u_int32_t));
void	set_io_apic_mask24	__P((int, u_int32_t));
#endif /* READY */

void	set_apic_timer		__P((int));
int	read_apic_timer		__P((void));
void	u_sleep			__P((int));

/* global data in init_smp.c */
extern int			invltlb_ok;
extern int			smp_active;
extern int			smp_started;
extern volatile int		smp_idle_loops;

#endif /* !LOCORE */
#else	/* !SMP && !APIC_IO */

/*
 * Create dummy MP lock empties
 */

static __inline void
get_mplock(void)
{
}

static __inline void
rel_mplock(void)
{
}

#endif

#endif /* _KERNEL */
#endif /* _MACHINE_SMP_H_ */
