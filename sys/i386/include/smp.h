/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/sys/i386/include/smp.h,v 1.76 2003/04/02 23:53:29 peter Exp $
 *
 */

#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#ifdef _KERNEL

#if defined(SMP) && defined(I386_CPU) && !defined(COMPILING_LINT)
#error SMP not supported with I386_CPU
#endif
#if defined(SMP) && !defined(APIC_IO)
# error APIC_IO required for SMP, add "options APIC_IO" to your config file.
#endif /* SMP && !APIC_IO */
#if defined(SMP) && defined(CPU_DISABLE_CMPXCHG) && !defined(COMPILING_LINT)
#error SMP not supported with CPU_DISABLE_CMPXCHG
#endif

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


#include <sys/bus.h>	/* XXX */
#include <machine/apic.h>
#include <machine/frame.h>
#include <i386/isa/icu.h>
#include <i386/isa/intr_machdep.h>

/*
 * Interprocessor interrupts for SMP.
 */
#define	IPI_INVLTLB		XINVLTLB_OFFSET
#define	IPI_INVLPG		XINVLPG_OFFSET
#define	IPI_INVLRNG		XINVLRNG_OFFSET
#define	IPI_LAZYPMAP		XLAZYPMAP_OFFSET
#define	IPI_RENDEZVOUS		XRENDEZVOUS_OFFSET
#define	IPI_AST			XCPUAST_OFFSET
#define	IPI_STOP		XCPUSTOP_OFFSET
#define	IPI_HARDCLOCK		XHARDCLOCK_OFFSET
#define	IPI_STATCLOCK		XSTATCLOCK_OFFSET

/* global data in mpboot.s */
extern int			bootMP_size;

/* functions in mpboot.s */
void	bootMP(void);

/* global data in mp_machdep.c */
extern int			bsp_apic_ready;
extern int			mp_naps;
extern int			mp_nbusses;
extern int			mp_napics;
extern int			mp_picmode;
extern int			boot_cpu_id;
extern vm_offset_t		cpu_apic_address;
extern vm_offset_t		io_apic_address[];
extern u_int32_t		cpu_apic_versions[];
extern u_int32_t		*io_apic_versions;
extern int			cpu_num_to_apic_id[];
extern int			io_num_to_apic_id[];
extern int			apic_id_to_logical[];
#define APIC_INTMAPSIZE 32
struct apic_intmapinfo {
  	int ioapic;
	int int_pin;
	volatile void *apic_address;
	int redirindex;
};
extern struct apic_intmapinfo	int_to_apicintpin[];
extern struct pcb		stoppcbs[];

/* functions in mp_machdep.c */
void	i386_mp_probe(void);
u_int	mp_bootaddress(u_int);
u_int	isa_apic_mask(u_int);
int	isa_apic_irq(int);
int	pci_apic_irq(int, int, int);
int	apic_irq(int, int);
int	next_apic_irq(int);
int	undirect_isa_irq(int);
int	undirect_pci_irq(int);
int	apic_bus_type(int);
int	apic_src_bus_id(int, int);
int	apic_src_bus_irq(int, int);
int	apic_int_type(int, int);
int	apic_trigger(int, int);
int	apic_polarity(int, int);
int	mp_grab_cpu_hlt(void);
void	assign_apic_irq(int apic, int intpin, int irq);
void	revoke_apic_irq(int irq);
void	bsp_apic_configure(void);
void	init_secondary(void);
void	forward_statclock(void);
void	forwarded_statclock(struct clockframe frame);
void	forward_hardclock(void);
void	forwarded_hardclock(struct clockframe frame);
void	ipi_selected(u_int cpus, u_int ipi);
void	ipi_all(u_int ipi);
void	ipi_all_but_self(u_int ipi);
void	ipi_self(u_int ipi);
#ifdef	APIC_INTR_REORDER
void	set_lapic_isrloc(int, int);
#endif /* APIC_INTR_REORDER */
void	smp_invlpg(vm_offset_t addr);
void	smp_masked_invlpg(u_int mask, vm_offset_t addr);
void	smp_invlpg_range(vm_offset_t startva, vm_offset_t endva);
void	smp_masked_invlpg_range(u_int mask, vm_offset_t startva,
	    vm_offset_t endva);
void	smp_invltlb(void);
void	smp_masked_invltlb(u_int mask);

/* global data in mpapic.c */
extern volatile lapic_t		lapic;
extern volatile ioapic_t	**ioapic;

/* functions in mpapic.c */
void	apic_dump(char*);
void	apic_initialize(void);
void	imen_dump(void);
int	apic_ipi(int, int, int);
int	selected_apic_ipi(u_int, int, int);
int	io_apic_setup(int);
void	io_apic_setup_intpin(int, int);
void	io_apic_set_id(int, int);
int	io_apic_get_id(int);
int	ext_int_setup(int, int);

void	set_apic_timer(int);
int	read_apic_timer(void);
void	u_sleep(int);
u_int	io_apic_read(int, int);
void	io_apic_write(int, int, u_int);

#endif /* !LOCORE */
#endif /* SMP && !APIC_IO */

#endif /* _KERNEL */
#endif /* _MACHINE_SMP_H_ */
