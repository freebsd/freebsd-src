/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: smp.h,v 1.29 1997/08/24 20:33:24 fsmp Exp $
 *
 */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef KERNEL

#if defined(SMP) && !defined(APIC_IO)
# error APIC_IO required for SMP, add "options APIC_IO" to your config file.
#endif /* SMP && !APIC_IO */

/* Number of CPUs. */
#if defined(SMP) && !defined(NCPU)
# define NCPU			2
#endif /* SMP && NCPU */

/* Number of IO APICs. */
#if defined(APIC_IO) && !defined(NAPIC)
# define NAPIC			1
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

/* functions in mplock.s */
void	get_mplock		__P((void));
void	rel_mplock		__P((void));
void	try_mplock		__P((void));

/* global data in apic_vector.s */
extern u_int			ivectors[];
extern volatile u_int		stopped_cpus;
extern volatile u_int		started_cpus;

/* global data in apic_ipl.s */
extern u_int			vec[];
extern u_int			Xintr8254;
extern u_int			mask8254;

/* functions in apic_ipl.s */
void	vec8254			__P((void));
void	INTREN			__P((u_int));
void	INTRDIS			__P((u_int));
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
extern u_int			all_cpus;
extern u_char			SMP_ioapic[];

/* functions in mp_machdep.c */
u_int	mp_bootaddress		__P((u_int));
int	mp_probe		__P((void));
void	mp_start		__P((void));
void	mp_announce		__P((void));
u_int	isa_apic_mask		__P((u_int));
int	isa_apic_pin		__P((int));
int	pci_apic_pin		__P((int, int, int));
int	undirect_isa_irq	__P((int));
int	undirect_pci_irq	__P((int));
int	apic_bus_type		__P((int));
int	apic_src_bus_id		__P((int, int));
int	apic_src_bus_irq	__P((int, int));
int	apic_int_type		__P((int, int));
int	apic_trigger		__P((int, int));
int	apic_polarity		__P((int, int));
void	bsp_apic_configure	__P((void));
void	init_secondary		__P((void));
void	smp_invltlb		__P((void));
int	stop_cpus		__P((u_int));
int	restart_cpus		__P((u_int));

/* global data in mpapic.c */
extern volatile lapic_t		lapic;

#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX
#else
extern volatile ioapic_t	*ioapic[];
#endif /* MULTIPLE_IOAPICS */

/* functions in mpapic.c */
void	apic_dump		__P((char*));
void	apic_initialize		__P((void));
void	imen_dump		__P((void));
int	apic_ipi		__P((int, int, int));
int	selected_apic_ipi	__P((u_int, int, int));
int	io_apic_setup		__P((int));
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
extern volatile int		smp_idle_loops;

/* 'private' global data in locore.s */
extern volatile u_int		cpuid;
extern volatile u_int		cpu_lockid;
extern volatile u_int		other_cpus;

#endif /* !LOCORE */
#endif /* SMP || APIC_IO */
#endif /* KERNEL */
#endif /* _MACHINE_SMP_H_ */
