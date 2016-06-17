/*
 *    Copyright 2000 MontaVista Software Inc.
 *    	http://www.mvista.com
 *	<mlocke@mvista.com>
 *
 *         Embedded Planet 405GP board
 *         http://www.embeddedplanet.com
 *
 * Please read the COPYING file for all license details.
 */

#ifdef __KERNEL__
#ifndef __ASM_EP405_H__
#define __ASM_EP405_H__

/* We have a 405GP core */
#include <platforms/ibm405gp.h>

#ifndef __ASSEMBLY__
typedef struct board_info {
	unsigned int	 bi_memsize;		/* DRAM installed, in bytes */
	unsigned char	 bi_enetaddr[6];	/* Local Ethernet MAC address */
	unsigned int	 bi_intfreq;		/* Processor speed, in Hz */
	unsigned int	 bi_busfreq;		/* PLB Bus speed, in Hz */
	unsigned int	 bi_pci_busfreq;	/* PCI Bus speed, in Hz */
	unsigned int	 bi_nvramsize;		/* Size of the NVRAM/RTC */
	unsigned int	 bi_opb_busfreq;	/* OPB Bus speed, in Hz */
	int		 bi_iic_fast[1];	/* Use fast i2c mode */
} bd_t;

/* Some 4xx parts use a different timebase frequency from the internal clock.
*/
#define bi_tbfreq bi_intfreq

extern u8 *ep405_bcsr;
extern u8 *ep405_nvram;

/* Map for the BCSR and NVRAM space */
#define EP405_BCSR_PADDR	((uint)0xf4000000)
#define EP405_BCSR_SIZE		((uint)16)
#define EP405_NVRAM_PADDR	((uint)0xf4200000)

/* serial defines */
#define BASE_BAUD		399193

#define PPC4xx_MACHINE_NAME "Embedded Planet 405GP"

#endif /* !__ASSEMBLY__ */
#endif /* __ASM_EP405_H__ */
#endif /* __KERNEL__ */
