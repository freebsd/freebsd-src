/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: smp.h,v 1.12 1997/06/25 20:59:15 fsmp Exp $
 *
 */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef KERNEL

#if defined(SMP) && !defined(APIC_IO)
# error APIC_IO required for SMP, add "options APIC_IO" to your config file.
#endif /* SMP && !APIC_IO */

#if defined(SMP) && !defined(NCPU)
# define NCPU			2
#endif /* SMP && NCPU */

#if defined(SMP) || defined(APIC_IO)

/*
 * For sending values to POST displays.
 */
#define POSTCODE(X)		outb(0x80, (X))

#include <machine/apic.h>

/* global data in mpboot.s */
extern int			bootMP_size;

/* functions in mpboot.s */
void	bootMP			__P((void));

/* global data in mplock.s */
extern u_int			mp_lock;

/* functions in mplock.s */
void	get_mplock		__P((void));
void	rel_mplock		__P((void));
void	try_mplock		__P((void));

/* global data in apic_vector.s */
extern volatile u_int		stopped_cpus;
extern volatile u_int		started_cpus;

/* global data in mp_machdep.c */
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
extern u_int			SMP_prvpt[];
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
void	configure_local_apic	__P((void));
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
void	apic_initialize		__P((int));
int	apic_ipi		__P((int, int, int));
int	selected_apic_ipi	__P((u_int, int, int));
int	io_apic_setup		__P((int));
int	ext_int_setup		__P((int, int));
void	write_io_apic_mask24	__P((int, u_int32_t));

#if defined(READY)
void	clr_io_apic_mask24	__P((int, u_int32_t));
void	set_io_apic_mask24	__P((int, u_int32_t));
#endif /* READY */

void	set_apic_timer		__P((int));
int	read_apic_timer		__P((void));
void	u_sleep			__P((int));

/* global data in init_smp.c */
extern int			smp_active;
extern int			invltlb_ok;

/* 'private' global data in locore.s */
extern volatile u_int		cpuid;
extern volatile u_int		cpu_lockid;
extern volatile u_int		other_cpus;

#endif /* SMP || APIC_IO */
#endif /* KERNEL */
#endif /* _MACHINE_SMP_H_ */
