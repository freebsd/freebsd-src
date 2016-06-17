/* 
 * Copyright 2001 Mike Corrigan, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <asm/types.h>
#include <asm/page.h>
#include <stddef.h>
#include <linux/threads.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/naca.h>
#include <asm/abs_addr.h>
#include <asm/bitops.h>
#include <asm/iSeries/ItLpNaca.h>
#include <asm/iSeries/ItLpPaca.h>
#include <asm/iSeries/ItLpRegSave.h>
#include <asm/paca.h>
#include <asm/iSeries/HvReleaseData.h>
#include <asm/iSeries/LparMap.h>
#include <asm/iSeries/ItVpdAreas.h>
#include <asm/iSeries/ItIplParmsReal.h>
#include <asm/iSeries/ItExtVpdPanel.h>
#include <asm/iSeries/ItLpQueue.h>
#include <asm/iSeries/IoHriProcessorVpd.h>
#include <asm/iSeries/ItSpCommArea.h>

extern char _start_boltedStacks[];

/* The LparMap data is now located at offset 0x6000 in head.S
 * It was put there so that the HvReleaseData could address it
 * with a 32-bit offset as required by the iSeries hypervisor
 *
 * The Naca has a pointer to the ItVpdAreas.  The hypervisor finds
 * the Naca via the HvReleaseData area.  The HvReleaseData has the
 * offset into the Naca of the pointer to the ItVpdAreas.
 */

extern struct ItVpdAreas itVpdAreas;

/* The LpQueue is used to pass event data from the hypervisor to
 * the partition.  This is where I/O interrupt events are communicated.
 * The ItLpQueue must be initialized (even though only to all zeros)
 * If it were uninitialized (in .bss) it would get zeroed after the
 * kernel gets control.  The hypervisor will have filled in some fields
 * before the kernel gets control.  By initializing it we keep it out
 * of the .bss
 */

struct ItLpQueue xItLpQueue = {};


/* The HvReleaseData is the root of the information shared between 
 * the hypervisor and Linux.  
 */

struct HvReleaseData hvReleaseData = {
	0xc8a5d9c4,	/* desc = "HvRD" ebcdic */
	sizeof(struct HvReleaseData),
	offsetof(struct naca_struct, xItVpdAreas),
	(struct naca_struct *)(KERNELBASE+0x4000), /* 64-bit Naca address */
	0x6000,		/* offset of LparMap within loadarea (see head.S) */
	0,
	1,		/* tags inactive       */
	0,		/* 64 bit              */
	0,		/* shared processors   */
	0,		/* HMT allowed         */
	6,		/* TEMP: This allows non-GA driver */
	4,		/* We are v5r2m0               */
	3,		/* Min supported PLIC = v5r1m0 */
	3,		/* Min usable PLIC   = v5r1m0 */
	{ 0xd3, 0x89, 0x95, 0xa4, /* "Linux 2.4   "*/
	  0xa7, 0x40, 0xf2, 0x4b,
	  0xf4, 0x4b, 0xf6, 0xf4 },
	{0}  
};

extern void SystemReset_Iseries(void);
extern void MachineCheck_Iseries(void);
extern void DataAccess_Iseries(void);
extern void InstructionAccess_Iseries(void);
extern void HardwareInterrupt_Iseries(void);
extern void Alignment_Iseries(void);
extern void ProgramCheck_Iseries(void);
extern void FPUnavailable_Iseries(void);
extern void Decrementer_Iseries(void);
extern void Trap_0a_Iseries(void);
extern void Trap_0b_Iseries(void);
extern void SystemCall_Iseries(void);
extern void SingleStep_Iseries(void);
extern void Trap_0e_Iseries(void);
extern void PerformanceMonitor_Iseries(void);
extern void DataAccessSLB_Iseries(void);
extern void InstructionAccessSLB_Iseries(void);
	
struct ItLpNaca itLpNaca = {
	0xd397d581,	/* desc = "LpNa" ebcdic */
	0x0400,		/* size of ItLpNaca     */
	0x0300, 19,	/* offset to int array, # ents */
	0, 0, 0,	/* Part # of primary, serv, me */
	0, 0x100,	/* # of LP queues, offset */
	0, 0, 0,	/* Piranha stuff */
	{ 0,0,0,0,0 },	/* reserved */
	0,0,0,0,0,0,0,	/* stuff    */
	{ 0,0,0,0,0 },	/* reserved */
	0,		/* reserved */
	0,		/* VRM index of PLIC */
	0, 0,		/* min supported, compat SLIC */
	0,		/* 64-bit addr of load area   */
	0,		/* chunks for load area  */
	0, 0,		/* PASE mask, seg table  */
	{ 0 },		/* 64 reserved bytes  */
	{ 0 }, 		/* 128 reserved bytes */
	{ 0 }, 		/* Old LP Queue       */
	{ 0 }, 		/* 384 reserved bytes */
	{
		(u64)SystemReset_Iseries,	/* 0x100 System Reset */
		(u64)MachineCheck_Iseries,	/* 0x200 Machine Check */
		(u64)DataAccess_Iseries,	/* 0x300 Data Access */
		(u64)InstructionAccess_Iseries,	/* 0x400 Instruction Access */
		(u64)HardwareInterrupt_Iseries,	/* 0x500 External */
		(u64)Alignment_Iseries,		/* 0x600 Alignment */
		(u64)ProgramCheck_Iseries,	/* 0x700 Program Check */
		(u64)FPUnavailable_Iseries,	/* 0x800 FP Unavailable */
		(u64)Decrementer_Iseries,	/* 0x900 Decrementer */
		(u64)Trap_0a_Iseries,		/* 0xa00 Trap 0A */
		(u64)Trap_0b_Iseries,		/* 0xb00 Trap 0B */
		(u64)SystemCall_Iseries,	/* 0xc00 System Call */
		(u64)SingleStep_Iseries,	/* 0xd00 Single Step */
		(u64)Trap_0e_Iseries,		/* 0xe00 Trap 0E */
		(u64)PerformanceMonitor_Iseries,/* 0xf00 Performance Monitor */
		0,				/* int 0x1000 */
		0,				/* int 0x1010 */
		0,				/* int 0x1020 CPU ctls */
		(u64)HardwareInterrupt_Iseries, /* SC Ret Hdlr */
		(u64)DataAccessSLB_Iseries,	/* 0x380 D-SLB */
		(u64)InstructionAccessSLB_Iseries /* 0x480 I-SLB */
	}
};

struct ItIplParmsReal xItIplParmsReal = {};

struct ItExtVpdPanel xItExtVpdPanel = {};

#define maxPhysicalProcessors 32

struct IoHriProcessorVpd xIoHriProcessorVpd[maxPhysicalProcessors] = {
	{
		xInstCacheOperandSize: 32,
		xDataCacheOperandSize: 32,
		xProcFreq:     50000000,
		xTimeBaseFreq: 50000000,
		xPVR: 0x3600
	}
};
	

u64    xMsVpd[3400] = {};		/* Space for Main Store Vpd 27,200 bytes */

u64    xRecoveryLogBuffer[32] = {};	/* Space for Recovery Log Buffer */

struct SpCommArea xSpCommArea = {
	0xE2D7C3C2,
	1,
	{0},
	0, 0, 0, 0, {0}
};

struct ItVpdAreas itVpdAreas = {
	0xc9a3e5c1,	/* "ItVA" */
	sizeof( struct ItVpdAreas ),
	0, 0,
	26,		/* # VPD array entries */
	10,		/* # DMA array entries */
	MAX_PROCESSORS*2, maxPhysicalProcessors,	/* Max logical, physical procs */
	offsetof(struct ItVpdAreas,xPlicDmaToks),/* offset to DMA toks */
	offsetof(struct ItVpdAreas,xSlicVpdAdrs),/* offset to VPD addrs */
	offsetof(struct ItVpdAreas,xPlicDmaLens),/* offset to DMA lens */
	offsetof(struct ItVpdAreas,xSlicVpdLens),/* offset to VPD lens */
	0,		/* max slot labels */
	1,		/* max LP queues */
	{0}, {0},	/* reserved */
	{0},		/* DMA lengths */
	{0},		/* DMA tokens */
	{		/* VPD lengths */
	        0,0,0,		        /*  0 - 2 */
		sizeof(xItExtVpdPanel), /*       3 Extended VPD   */
		sizeof(struct paca_struct),	/*       4 length of Paca  */
		0,			/*       5 */
		sizeof(struct ItIplParmsReal),/* 6 length of IPL parms */
		26992,			/*	 7 length of MS VPD */
		0,			/*       8 */
		sizeof(struct ItLpNaca),/*       9 length of LP Naca */
		0, 			/*	10 */
		256,			/*	11 length of Recovery Log Buf */
		sizeof(struct SpCommArea), /*   12 length of SP Comm Area */
		0,0,0,			/* 13 - 15 */
		sizeof(struct IoHriProcessorVpd),/* 16 length of Proc Vpd */
		0,0,0,0,0,0,		/* 17 - 22  */
		sizeof(struct ItLpQueue),/*     23 length of Lp Queue */
		0,0			/* 24 - 25 */
		},
	{			/* VPD addresses */
		0,0,0,  		/*	 0 -  2 */
		&xItExtVpdPanel,        /*       3 Extended VPD */
		&paca[0],		/*       4 first Paca */
		0,			/*       5 */
		&xItIplParmsReal,	/*	 6 IPL parms */
		&xMsVpd,		/*	 7 MS Vpd */
		0,			/*       8 */
		&itLpNaca,		/*       9 LpNaca */
		0,			/*	10 */
		&xRecoveryLogBuffer,	/*	11 Recovery Log Buffer */
		&xSpCommArea,		/*	12 SP Comm Area */
		0,0,0,			/* 13 - 15 */
		&xIoHriProcessorVpd,	/*      16 Proc Vpd */
		0,0,0,0,0,0,		/* 17 - 22 */
		&xItLpQueue,		/*      23 Lp Queue */
		0,0
	}
};

struct msChunks msChunks = {0, 0, 0, 0, NULL};

/* Depending on whether this is called from iSeries or pSeries setup
 * code, the location of the msChunks struct may or may not have
 * to be reloc'd, so we force the caller to do that for us by passing
 * in a pointer to the structure.
 */
unsigned long
msChunks_alloc(unsigned long mem, unsigned long num_chunks, unsigned long chunk_size)
{
	unsigned long offset = reloc_offset();
	struct msChunks *_msChunks = PTRRELOC(&msChunks);

	_msChunks->num_chunks  = num_chunks;
	_msChunks->chunk_size  = chunk_size;
	_msChunks->chunk_shift = __ilog2(chunk_size);
	_msChunks->chunk_mask  = (1UL<<_msChunks->chunk_shift)-1;

	mem = _ALIGN(mem, sizeof(msChunks_entry));
	_msChunks->abs = (msChunks_entry *)(mem + offset);
	mem += num_chunks * sizeof(msChunks_entry);

	return mem;
}
