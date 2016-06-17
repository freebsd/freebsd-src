#ifndef _NACA_H
#define _NACA_H

/* 
 * c 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/types.h>
#include <asm/systemcfg.h>

struct naca_struct {
	/*==================================================================
	 * Cache line 1: 0x0000 - 0x007F
	 * Kernel only data - undefined for user space
	 *==================================================================
	 */
	void *xItVpdAreas;              /* VPD Data                  0x00 */
	void *xRamDisk;                 /* iSeries ramdisk           0x08 */
	u64   xRamDiskSize;		/* In pages                  0x10 */
	struct paca_struct *paca;	/* Ptr to an array of pacas  0x18 */
	u64 debug_switch;		/* Debug print control       0x20 */
	u64 banner;                     /* Ptr to banner string      0x28 */
	u64 log;                        /* Ptr to log buffer         0x30 */
	u64 serialPortAddr;		/* Phy addr of serial port   0x38 */
	u64 interrupt_controller;	/* Type of int controller    0x40 */ 
	u64 slb_size;			/* SLB size in entries       0x48 */
	u64 pftSize;			/* Log 2 of page table size  0x50 */
	void *systemcfg;		/* Pointer to systemcfg data 0x58 */
	u32 dCacheL1LogLineSize;	/* L1 d-cache line size Log2 0x60 */
	u32 dCacheL1LinesPerPage;	/* L1 d-cache lines / page   0x64 */
	u32 iCacheL1LogLineSize;	/* L1 i-cache line size Log2 0x68 */
	u32 iCacheL1LinesPerPage;	/* L1 i-cache lines / page   0x6c */
	u64 smt_snooze_delay;           /* Delay (in usec) before    0x70 */
					/* entering ST mode               */
	u8  smt_state;                  /* 0 = SMT off               0x78 */
					/* 1 = SMT on                     */
					/* 2 = SMT dynamic                */
	u8  resv0[7];                   /* Reserved           0x79 - 0x7F */
};

extern struct naca_struct *naca;

#endif /* _NACA_H */
