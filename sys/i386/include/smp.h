/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: smp.h,v 1.8 1997/05/07 19:53:20 peter Exp $
 *
 */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef KERNEL

#if defined(SMP) && !defined(APIC_IO)
# error APIC_IO required for SMP, add "options APIC_IO" to your config file.
#endif /* SMP && NCPU */

#if defined(SMP) && !defined(NCPU)
# define NCPU			2
#endif /* SMP && NCPU */

#if defined(SMP) || defined(APIC_IO)

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

/* global data in mp_machdep.c */
extern int			mp_ncpus;
extern int			mp_naps;
extern int			mp_nbusses;
extern int			mp_napics;
extern int			mp_picmode;
extern int			mpenabled;
extern int			boot_cpu_id;
extern vm_offset_t		cpu_apic_address;
extern vm_offset_t		io_apic_address[];
extern u_int32_t		cpu_apic_versions[];
extern u_int32_t		io_apic_versions[];
extern int			cpu_num_to_apic_id[];
extern int			io_num_to_apic_id[];
extern int			apic_id_to_logical[];

/* functions in mp_machdep.c */
u_int	mp_bootaddress		__P((u_int));
int	mp_probe		__P((void));
void	mp_start		__P((void));
void	mp_announce		__P((void));
int	get_isa_apic_irq	__P((int));
u_int	get_isa_apic_mask	__P((u_int));
int	undirect_isa_irq	__P((int));
int	get_eisa_apic_irq	__P((int));
int	get_pci_apic_irq	__P((int, int, int));
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

/* global data in mpapic.c */
extern volatile u_int*		apic_base;

#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX
#else
extern volatile u_int*		io_apic_base;
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

/* in pmap.c FIXME: belongs in pmap.h??? */
void	pmap_bootstrap_apics	__P((void));
void	pmap_bootstrap2		__P((void));

#if 0
/* chicken and egg problem... */
static __inline unsigned
cpunumber(void)
{
	return (unsigned)ID_TO_CPU((apic_base[APIC_ID] & APIC_ID_MASK) >> 24);
}
#else
/*
 * we 'borrow' this info from apic.h
 * this will go away soon...
 */
static __inline unsigned
cpunumber(void)
{
	return (unsigned)(apic_id_to_logical[(apic_base[8] & 0x0f000000) >> 24]);
}
#endif /* 0 */

#endif /* SMP || APIC_IO */
#endif /* KERNEL */
#endif /* _MACHINE_SMP_H_ */
