/*
 * ItLpPaca.h
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

//=============================================================================
//                                   
//	This control block contains the data that is shared between the 
//	hypervisor (PLIC) and the OS.
//    
//
//----------------------------------------------------------------------------
#ifndef  _PPC_TYPES_H
#include <asm/types.h>
#endif

#ifndef _ITLPPACA_H
#define _ITLPPACA_H


struct ItLpPaca
{
//=============================================================================
// CACHE_LINE_1 0x0000 - 0x007F Contains read-only data
// NOTE: The xDynXyz fields are fields that will be dynamically changed by 
// PLIC when preparing to bring a processor online or when dispatching a 
// virtual processor!
//=============================================================================
	u32	xDesc;			// Eye catcher 0xD397D781	x00-x03
	u16	xSize;			// Size of this struct		x04-x05
	u16	xRsvd1_0;		// Reserved			x06-x07
	u16	xRsvd1_1:14;		// Reserved			x08-x09
	u8	xSharedProc:1;		// Shared processor indicator	...
	u8	xSecondaryThread:1;	// Secondary thread indicator	...
	volatile u8 xDynProcStatus:8;	// Dynamic Status of this proc	x0A-x0A
	u8	xSecondaryThreadCnt;	// Secondary thread count	x0B-x0B
	volatile u16 xDynHvPhysicalProcIndex;// Dynamic HV Physical Proc Index0C-x0D
	volatile u16 xDynHvLogicalProcIndex;// Dynamic HV Logical Proc Indexx0E-x0F
	u32	xDecrVal;   		// Value for Decr programming 	x10-x13
	u32	xPMCVal;       		// Value for PMC regs         	x14-x17
	volatile u32 xDynHwNodeId;	// Dynamic Hardware Node id	x18-x1B
	volatile u32 xDynHwProcId;	// Dynamic Hardware Proc Id	x1C-x1F
	volatile u32 xDynPIR;		// Dynamic ProcIdReg value	x20-x23
	u32	xDseiData;           	// DSEI data                  	x24-x27
	u64	xSPRG3;               	// SPRG3 value                	x28-x2F
	u8	xRsvd1_3[80];		// Reserved			x30-x7F
   
//=============================================================================
// CACHE_LINE_2 0x0080 - 0x00FF Contains local read-write data
//=============================================================================
	// This Dword contains a byte for each type of interrupt that can occur.  
	// The IPI is a count while the others are just a binary 1 or 0.
	union {
		u64	xAnyInt;
		struct {
			u16	xRsvd;		// Reserved - cleared by #mpasmbl
			u8	xXirrInt;	// Indicates xXirrValue is valid or Immed IO
			u8	xIpiCnt;	// IPI Count
			u8	xDecrInt;	// DECR interrupt occurred
			u8	xPdcInt;	// PDC interrupt occurred
			u8	xQuantumInt;	// Interrupt quantum reached
			u8	xOldPlicDeferredExtInt;	// Old PLIC has a deferred XIRR pending
		} xFields;
	} xIntDword;

	// Whenever any fields in this Dword are set then PLIC will defer the 
	// processing of external interrupts.  Note that PLIC will store the 
	// XIRR directly into the xXirrValue field so that another XIRR will 
	// not be presented until this one clears.  The layout of the low 
	// 4-bytes of this Dword is upto SLIC - PLIC just checks whether the 
	// entire Dword is zero or not.  A non-zero value in the low order 
	// 2-bytes will result in SLIC being granted the highest thread 
	// priority upon return.  A 0 will return to SLIC as medium priority.
	u64	xPlicDeferIntsArea;	// Entire Dword

	// Used to pass the real SRR0/1 from PLIC to SLIC as well as to 
	// pass the target SRR0/1 from SLIC to PLIC on a SetAsrAndRfid.
	u64     xSavedSrr0;             // Saved SRR0                   x10-x17
	u64     xSavedSrr1;             // Saved SRR1                   x18-x1F

	// Used to pass parms from the OS to PLIC for SetAsrAndRfid
	u64     xSavedGpr3;             // Saved GPR3                   x20-x27
	u64     xSavedGpr4;             // Saved GPR4                   x28-x2F
	u64     xSavedGpr5;             // Saved GPR5                   x30-x37

	u8	xRsvd2_1;		// Reserved			x38-x38
	u8      xCpuCtlsTaskAttributes; // Task attributes for cpuctls  x39-x39
	u8      xFPRegsInUse;           // FP regs in use               x3A-x3A
	u8      xPMCRegsInUse;          // PMC regs in use              x3B-x3B
	volatile u32  xSavedDecr;	// Saved Decr Value             x3C-x3F
	volatile u64  xEmulatedTimeBase;// Emulated TB for this thread  x40-x47
	volatile u64  xCurPLICLatency;	// Unaccounted PLIC latency     x48-x4F
	u64     xTotPLICLatency;        // Accumulated PLIC latency     x50-x57   
	u64     xWaitStateCycles;       // Wait cycles for this proc    x58-x5F
	u64     xEndOfQuantum;          // TB at end of quantum         x60-x67
	u64     xPDCSavedSPRG1;         // Saved SPRG1 for PMC int      x68-x6F
	u64     xPDCSavedSRR0;          // Saved SRR0 for PMC int       x70-x77
	volatile u32 xVirtualDecr;	// Virtual DECR for shared procsx78-x7B
	u16     xSLBCount;              // # of SLBs to maintain        x7C-x7D
	u8      xIdle;                  // Indicate OS is idle          x7E
	u8      xRsvd2_2;               // Reserved                     x7F


//=============================================================================
// CACHE_LINE_3 0x0100 - 0x007F: This line is shared with other processors
//=============================================================================
	// This is the xYieldCount.  An "odd" value (low bit on) means that 
	// the processor is yielded (either because of an OS yield or a PLIC 
	// preempt).  An even value implies that the processor is currently 
	// executing.
	// NOTE: This value will ALWAYS be zero for dedicated processors and 
	// will NEVER be zero for shared processors (ie, initialized to a 1).
	volatile u32 xYieldCount;	// PLIC increments each dispatchx00-x03
	u8	xRsvd3_0[124];		// Reserved                     x04-x7F         

//=============================================================================
// CACHE_LINE_4-5 0x0100 - 0x01FF Contains PMC interrupt data
//=============================================================================
	u8      xPmcSaveArea[256];	// PMC interrupt Area           x00-xFF


};
#endif // _ITLPPACA_H
