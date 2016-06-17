#include <linux/config.h>
#ifndef __I386_COBALT_H
#define __I386_COBALT_H

/*
 * Cobalt is the system ASIC on the SGI 320 and 540 Visual Workstations
 */ 

#define	CO_CPU_PHYS		0xc2000000
#define	CO_APIC_PHYS		0xc4000000

/* see set_fixmap() and asm/fixmap.h */
#define	CO_CPU_VADDR		(fix_to_virt(FIX_CO_CPU))
#define	CO_APIC_VADDR		(fix_to_virt(FIX_CO_APIC))

/* Cobalt CPU registers -- relative to CO_CPU_VADDR, use co_cpu_*() */
#define	CO_CPU_REV		0x08
#define	CO_CPU_CTRL		0x10
#define	CO_CPU_STAT		0x20
#define	CO_CPU_TIMEVAL		0x30

/* CO_CPU_CTRL bits */
#define	CO_CTRL_TIMERUN		0x04	/* 0 == disabled */
#define	CO_CTRL_TIMEMASK	0x08	/* 0 == unmasked */

/* CO_CPU_STATUS bits */
#define	CO_STAT_TIMEINTR	0x02	/* (r) 1 == int pend, (w) 0 == clear */

/* CO_CPU_TIMEVAL value */
#define	CO_TIME_HZ		100000000 /* Cobalt core rate */

/* Cobalt APIC registers -- relative to CO_APIC_VADDR, use co_apic_*() */
#define	CO_APIC_HI(n)		(((n) * 0x10) + 4)
#define	CO_APIC_LO(n)		((n) * 0x10)
#define	CO_APIC_ID		0x0ffc

/* CO_APIC_ID bits */
#define	CO_APIC_ENABLE		0x00000100

/* CO_APIC_LO bits */
#define	CO_APIC_LEVEL		0x08000		/* 0 = edge */

/*
 * Where things are physically wired to Cobalt
 * #defines with no board _<type>_<rev>_ are common to all (thus far)
 */
#define CO_APIC_0_5_IDE0	5
#define	CO_APIC_0_5_SERIAL	13	 /* XXX not really...h/w bug! */
#define CO_APIC_0_5_PARLL	4
#define CO_APIC_0_5_FLOPPY	6

#define	CO_APIC_0_6_IDE0	4
#define	CO_APIC_0_6_USB	7	/* PIIX4 USB */

#define	CO_APIC_1_2_IDE0	4

#define CO_APIC_0_5_IDE1	2
#define CO_APIC_0_6_IDE1	2

/* XXX */
#define	CO_APIC_IDE0	CO_APIC_0_5_IDE0
#define	CO_APIC_IDE1	CO_APIC_0_5_IDE1
#define	CO_APIC_SERIAL	CO_APIC_0_5_SERIAL
/* XXX */

#define CO_APIC_ENET	3	/* Lithium PCI Bridge A, Device 3 */
#define	CO_APIC_8259	12	/* serial, floppy, par-l-l, audio */

#define	CO_APIC_VIDOUT0	16
#define	CO_APIC_VIDOUT1	17
#define	CO_APIC_VIDIN0	18
#define	CO_APIC_VIDIN1	19

#define CO_APIC_CPU	28	/* Timer and Cache interrupt */

/*
 * This is the "irq" arg to request_irq(), just a unique cookie.
 */
#define	CO_IRQ_TIMER	0
#define CO_IRQ_ENET	3
#define CO_IRQ_SERIAL	4
#define CO_IRQ_FLOPPY	6	/* Same as drivers/block/floppy.c:FLOPPY_IRQ */
#define	CO_IRQ_PARLL	7
#define	CO_IRQ_POWER	9
#define CO_IRQ_IDE	14
#define	CO_IRQ_8259	12

#ifdef CONFIG_X86_VISWS_APIC
static __inline void co_cpu_write(unsigned long reg, unsigned long v)
{
	*((volatile unsigned long *)(CO_CPU_VADDR+reg))=v;
}

static __inline unsigned long co_cpu_read(unsigned long reg)
{
	return *((volatile unsigned long *)(CO_CPU_VADDR+reg));
}            
             
static __inline void co_apic_write(unsigned long reg, unsigned long v)
{
	*((volatile unsigned long *)(CO_APIC_VADDR+reg))=v;
}            
             
static __inline unsigned long co_apic_read(unsigned long reg)
{
	return *((volatile unsigned long *)(CO_APIC_VADDR+reg));
}
#endif

extern char visws_board_type;

#define	VISWS_320	0
#define	VISWS_540	1

extern char visws_board_rev;

#endif
