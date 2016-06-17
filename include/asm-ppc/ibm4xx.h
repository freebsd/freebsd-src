/*
 *
 *    Copyright (c) 1999 Grant Erickson <grant@lcse.umn.edu>
 *
 *    Module name: ibm4xx.h
 *
 *    Description:
 *	A generic include file which pulls in appropriate include files
 *      for specific board types based on configuration settings.
 *
 */

#ifdef __KERNEL__
#ifndef __ASM_IBM4XX_H__
#define __ASM_IBM4XX_H__

#include <linux/config.h>
#include <asm/types.h>

#ifdef CONFIG_4xx

#ifndef __ASSEMBLY__
/*
 * The "residual" board information structure the boot loader passes
 * into the kernel.
 */
extern unsigned char __res[];

/* Device Control Registers */

#define stringify(s)	tostring(s)
#define tostring(s)	#s

#define mfdcr_or_dflt(rn,default_rval) \
	({unsigned int rval;						\
	if (rn == 0)							\
		rval = default_rval;					\
	else								\
		asm volatile("mfdcr %0," stringify(rn) : "=r" (rval));	\
	rval;})

/* R/W of indirect DCRs make use of standard naming conventions for DCRs */

#define mfdcri(base, reg)			\
({						\
     mtdcr(base##_CFGADDR, base##_##reg);	\
     mfdcr(base##_CFGDATA);			\
})

#define mtdcri(base, reg, data)			\
do {						\
     mtdcr(base##_CFGADDR, base##_##reg);	\
     mtdcr(base##_CFGDATA, data);		\
} while (0)
#endif /* __ASSEMBLY__ */

#endif /* CONFIG_4xx */

#ifdef CONFIG_40x

#if defined(CONFIG_CPCI405)
#include <platforms/cpci405.h>
#endif

#if defined(CONFIG_EP405)
#include <platforms/ep405.h>
#endif

#if defined(CONFIG_OAK)
#include <platforms/oak.h>
#endif

#if defined(CONFIG_REDWOOD_5)
#include <platforms/redwood5.h>
#endif

#if defined(CONFIG_REDWOOD_6)
#include <platforms/redwood6.h>
#endif

#if defined(CONFIG_WALNUT)
#include <platforms/walnut.h>
#endif

#ifndef PPC4xx_MACHINE_NAME
#define PPC4xx_MACHINE_NAME	"Unidentified 4xx class"
#endif

/* IO_BASE is for PCI I/O.
 * ISA not supported, just here to resolve copilation.
 */

#ifndef _IO_BASE
#define _IO_BASE	0xe8000000	/* The PCI address window */
#define _ISA_MEM_BASE	0
#define PCI_DRAM_OFFSET	0
#endif

#elif CONFIG_44x

#if defined(CONFIG_EBONY)
#include <platforms/ebony.h>
#endif

#if defined(CONFIG_OCOTEA)
#include <platforms/ocotea.h>
#endif

#endif /* CONFIG_40x */

#ifndef __ASSEMBLY__
#if defined(EMAC_NUMS) && EMAC_NUMS > 0
/*
 * Per EMAC map of PHY ids which should be probed by emac_probe.
 * Different EMACs can have overlapping maps.
 *
 * Note, this map uses inverse logic for bits:
 *  0 - id should be probed
 *  1 - id should be ignored
 *
 * Default value of 0x00000000 - will result in usual
 * auto-detection logic.
 *
 */
extern u32 emac_phy_map[EMAC_NUMS];
#endif
#endif /* __ASSEMBLY__ */

#endif /* __ASM_IBM4XX_H__ */
#endif /* __KERNEL__ */
