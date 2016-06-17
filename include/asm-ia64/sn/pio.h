/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_PIO_H
#define _ASM_IA64_SN_PIO_H

#include <linux/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/driver.h>

/*
 * pioaddr_t	- The kernel virtual address that a PIO can be done upon.
 *		  Should probably be (volatile void*) but EVEREST would do PIO
 *		  to long mostly, just cast for other sizes.
 */

typedef volatile ulong*	pioaddr_t;

/*
 * iopaddr_t	- the physical io space relative address (e.g. VME A16S 0x0800).
 * iosapce_t	- specifies the io address space to be mapped/accessed.
 * piomap_t	- the handle returned by pio_alloc() and used with all the pio
 *		  access functions.
 */


typedef struct piomap {
	uint		pio_bus;
	uint		pio_adap;
	int		pio_flag;
	int		pio_reg;
	char		pio_name[7];	/* to identify the mapped device */
	struct piomap	*pio_next;	/* dlist to link active piomap's */
	struct piomap	*pio_prev;	/* for debug and error reporting */
	iopaddr_t	pio_iopmask;	/* valid iop address bit mask */
	iobush_t	pio_bushandle;	/* bus-level handle */
} piomap_t;

#define pio_type	pio_iospace.ios_type
#define pio_iopaddr	pio_iospace.ios_iopaddr
#define pio_size	pio_iospace.ios_size
#define pio_vaddr	pio_iospace.ios_vaddr

/* Macro to get/set PIO error function */
#define	pio_seterrf(p,f)	(p)->pio_errfunc = (f)
#define	pio_geterrf(p)		(p)->pio_errfunc


/*
 * piomap_t type defines
 */

#define PIOMAP_NTYPES	7

#define PIOMAP_A16N	VME_A16NP
#define PIOMAP_A16S	VME_A16S
#define PIOMAP_A24N	VME_A24NP
#define PIOMAP_A24S	VME_A24S
#define PIOMAP_A32N	VME_A32NP
#define PIOMAP_A32S	VME_A32S
#define PIOMAP_A64	6

#define PIOMAP_EISA_IO	0
#define PIOMAP_EISA_MEM	1

#define PIOMAP_PCI_IO	0
#define PIOMAP_PCI_MEM	1
#define PIOMAP_PCI_CFG	2
#define PIOMAP_PCI_ID	3

/* IBUS piomap types */
#define PIOMAP_FCI	0

/* dang gio piomap types */

#define	PIOMAP_GIO32	0
#define	PIOMAP_GIO64	1

#define ET_MEM         	0
#define ET_IO          	1
#define LAN_RAM         2
#define LAN_IO          3

#define PIOREG_NULL	(-1)

/* standard flags values for pio_map routines,
 * including {xtalk,pciio}_piomap calls.
 * NOTE: try to keep these in step with DMAMAP flags.
 */
#define PIOMAP_UNFIXED	0x0
#define PIOMAP_FIXED	0x1
#define PIOMAP_NOSLEEP	0x2
#define	PIOMAP_INPLACE	0x4

#define	PIOMAP_FLAGS	0x7

#endif	/* _ASM_IA64_SN_PIO_H */
