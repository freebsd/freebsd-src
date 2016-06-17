#ifndef __I386_LITHIUM_H
#define __I386_LITHIUM_H

#include <linux/config.h>

/*
 * Lithium is the I/O ASIC on the SGI 320 and 540 Visual Workstations
 */

#define	LI_PCI_A_PHYS		0xfc000000	/* Enet is dev 3 */
#define	LI_PCI_B_PHYS		0xfd000000	/* PIIX4 is here */

/* see set_fixmap() and asm/fixmap.h */
#define LI_PCIA_VADDR   (fix_to_virt(FIX_LI_PCIA))
#define LI_PCIB_VADDR   (fix_to_virt(FIX_LI_PCIB))

/* Not a standard PCI? (not in linux/pci.h) */
#define	LI_PCI_BUSNUM	0x44			/* lo8: primary, hi8: sub */
#define LI_PCI_INTEN    0x46

#ifdef CONFIG_X86_VISWS_APIC
/* More special purpose macros... */
static __inline void li_pcia_write16(unsigned long reg, unsigned short v)
{
	*((volatile unsigned short *)(LI_PCIA_VADDR+reg))=v;
}

static __inline unsigned short li_pcia_read16(unsigned long reg)
{
	 return *((volatile unsigned short *)(LI_PCIA_VADDR+reg));
}

static __inline void li_pcib_write16(unsigned long reg, unsigned short v)
{
	*((volatile unsigned short *)(LI_PCIB_VADDR+reg))=v;
}

static __inline unsigned short li_pcib_read16(unsigned long reg)
{
	return *((volatile unsigned short *)(LI_PCIB_VADDR+reg));
}
#endif

#endif

