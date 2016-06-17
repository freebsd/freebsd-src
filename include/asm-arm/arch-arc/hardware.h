/*
 *  linux/include/asm-arm/arch-arc/hardware.h
 *
 *  Copyright (C) 1996-1999 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This file contains the hardware definitions of the
 *  Acorn Archimedes/A5000 machines.
 *
 *  Modifications:
 *   04-04-1998	PJB/RMK	Merged arc and a5k versions
 */
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <linux/config.h>

#include <asm/arch/memory.h>

/*
 * What hardware must be present - these can be tested by the kernel
 * source.
 */
#define HAS_IOC
#define HAS_MEMC
#include <asm/hardware/memc.h>
#define HAS_VIDC

/* Hardware addresses of major areas.
 *  *_START is the physical address
 *  *_SIZE  is the size of the region
 *  *_BASE  is the virtual address
 */
#define IO_START		0x03000000
#define IO_SIZE			0x01000000
#define IO_BASE			0x03000000

/*
 * Screen mapping information
 */
#define SCREEN_START		0x02000000
#define SCREEN_END		0x02078000
#define SCREEN_BASE		0x02000000


#define EXPMASK_BASE		0x03360000
#define IOEB_BASE		0x03350000
#define VIDC_BASE		0x03400000
#define LATCHA_BASE		0x03250040
#define LATCHB_BASE		0x03250018
#define IOC_BASE		0x03200000
#define FLOPPYDMA_BASE		0x0302a000
#define PCIO_BASE		0x03010000

#define vidc_writel(val)	__raw_writel(val, VIDC_BASE)

#ifndef __ASSEMBLY__

/*
 * for use with inb/outb
 */
#ifdef CONFIG_ARCH_A5K
#define IOEB_VID_CTL		(IOEB_BASE + 0x48)
#define IOEB_PRESENT		(IOEB_BASE + 0x50)
#define IOEB_PSCLR		(IOEB_BASE + 0x58)
#define IOEB_MONTYPE		(IOEB_BASE + 0x70)
#endif

#define IO_EC_IOC_BASE		0x80090000
#define IO_EC_MEMC_BASE		0x80000000

#ifdef CONFIG_ARCH_ARC
/* A680 hardware */
#define WD1973_BASE		0x03290000
#define WD1973_LATCH		0x03350000
#define Z8530_BASE		0x032b0008
#define SCSI_BASE		0x03100000
#endif

#endif

#define	EXPMASK_STATUS		(EXPMASK_BASE + 0x00)
#define EXPMASK_ENABLE		(EXPMASK_BASE + 0x04)

#endif
