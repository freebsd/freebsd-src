/*
 * iSeries_dma.h
 * Copyright (C) 2001  Mike Corrigan IBM Corporation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef _ISERIES_DMA_H
#define _ISERIES_DMA_H

#include <asm/types.h>
#ifndef __LINUX_SPINLOCK_H
#include <linux/spinlock.h>
#endif

// NUM_TCE_LEVELS defines the largest contiguous block
// of dma (tce) space we can get.  NUM_TCE_LEVELS = 10 
// allows up to 2**9 pages (512 * 4096) = 2 MB
#define NUM_TCE_LEVELS 10

#define NO_TCE ((dma_addr_t)-1)

// Tces come in two formats, one for the virtual bus and a different
// format for PCI
#define TCE_VB  0
#define TCE_PCI 1


union Tce {
   	u64	wholeTce;
	struct {
		u64	cacheBits	:6;	/* Cache hash bits - not used */
		u64	rsvd		:6;	
		u64	rpn		:40;	/* Absolute page number */
		u64	valid		:1;	/* Tce is valid (vb only) */
		u64	allIo		:1;	/* Tce is valid for all lps (vb only) */
		u64	lpIndex		:8;	/* LpIndex for user of TCE (vb only) */
		u64	pciWrite	:1;	/* Write allowed (pci only) */
		u64	readWrite	:1;	/* Read allowed (pci), Write allowed
						   (vb) */
	} tceBits;
};

struct Bitmap {
	unsigned long	numBits;
	unsigned long	numBytes;
	unsigned char * map;
};

struct MultiLevelBitmap {
	unsigned long 	maxLevel;
	struct Bitmap 	level[NUM_TCE_LEVELS];
};

struct TceTable {
	u64	busNumber;
	u64	size;
	u64	startOffset;
	u64	index;
	spinlock_t	lock;
	struct MultiLevelBitmap mlbm;
};

struct HvTceTableManagerCB {
	u64	busNumber;		/* Bus number for this tce table */
	u64	start;			/* Will be NULL for secondary */
	u64	totalSize;		/* Size (in pages) of whole table */
	u64	startOffset;		/* Index into real tce table of the
					   start of our section */
	u64	size;			/* Size (in pages) of our section */
	u64	index;			/* Index of this tce table (token?) */
	u16	maxTceTableIndex;	/* Max number of tables for partition */
	u8	virtualBusFlag;		/* Flag to indicate virtual bus */
	u8	rsvd[5];
};

extern struct TceTable virtBusTceTable;	/* Tce table for virtual bus */

extern struct TceTable * build_tce_table( struct HvTceTableManagerCB *,
					  struct TceTable *);
extern void              create_virtual_bus_tce_table( void );

extern void		 create_pci_bus_tce_table( unsigned busNumber );

#endif // _ISERIES_DMA_H
