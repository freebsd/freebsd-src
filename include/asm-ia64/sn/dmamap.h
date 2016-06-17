/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_DMAMAP_H
#define _ASM_IA64_SN_DMAMAP_H

#include <asm/sn/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Definitions for allocating, freeing, and using DMA maps
 */

/*
 * DMA map types
 */
#define	DMA_SCSI	0
#define	DMA_A24VME	1		/* Challenge/Onyx only 	*/
#define	DMA_A32VME	2		/* Challenge/Onyx only 	*/
#define	DMA_A64VME	3		/* SN0/Racer */

#define	DMA_EISA	4

#define	DMA_PCI32	5		/* SN0/Racer 	*/
#define	DMA_PCI64	6		/* SN0/Racer 	*/

/*
 * DMA map structure as returned by dma_mapalloc()
 */
typedef struct dmamap {
	int		dma_type;	/* Map type (see above) */
	int		dma_adap;	/* I/O adapter */
	int		dma_index;	/* Beginning map register to use */
	int		dma_size;	/* Number of map registers to use */
	paddr_t		dma_addr;	/* Corresponding bus addr for A24/A32 */
	unsigned long	dma_virtaddr;	/* Beginning virtual address that is mapped */
} dmamap_t;

#ifdef __cplusplus
}
#endif

/* standard flags values for pio_map routines,
 * including {xtalk,pciio}_dmamap calls.
 * NOTE: try to keep these in step with PIOMAP flags.
 */
#define DMAMAP_FIXED	0x1
#define DMAMAP_NOSLEEP	0x2
#define	DMAMAP_INPLACE	0x4

#define	DMAMAP_FLAGS	0x7

#endif /* _ASM_IA64_SN_DMAMAP_H */
