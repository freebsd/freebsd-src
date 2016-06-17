/*
 * c 2001 PPC 64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <asm/types.h>
#include <asm/page.h>
#include <stddef.h>
#include <linux/config.h>
#include <linux/threads.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#include <asm/iSeries/ItLpPaca.h>
#include <asm/naca.h>
#include <asm/paca.h>

struct naca_struct *naca;
struct systemcfg *systemcfg;

/* The Paca is an array with one entry per processor.  Each contains an 
 * ItLpPaca, which contains the information shared between the 
 * hypervisor and Linux.  Each also contains an ItLpRegSave area which
 * is used by the hypervisor to save registers.
 * On systems with hardware multi-threading, there are two threads
 * per processor.  The Paca array must contain an entry for each thread.
 * The VPD Areas will give a max logical processors = 2 * max physical
 * processors.  The processor VPD array needs one entry per physical
 * processor (not thread).
 */
#define PACAINITDATA(number,start,lpq,asrr,asrv) \
{                                                                          \
        xLpPacaPtr: &paca[number].xLpPaca,                                 \
        xLpRegSavePtr: &paca[number].xRegSav,                              \
        xPacaIndex: (number),           /* Paca Index        */             \
        default_decr: 0x00ff0000,       /* Initial Decr      */             \
        xStab_data: {                                                       \
                real: (asrr),           /* Real pointer to segment table */ \
                virt: (asrv),           /* Virt pointer to segment table */ \
                next_round_robin: 1     /* Round robin index */             \
        },                                                                  \
        lpQueuePtr: (lpq),              /* &xItLpQueue,                  */ \
        /* xRtas: {                                                            \
                lock: SPIN_LOCK_UNLOCKED                                    \
        }, */                                                                  \
        xProcStart: (start),            /* Processor start */               \
        xLpPaca: {                                                          \
                xDesc: 0xd397d781,      /* "LpPa"          */               \
                xSize: sizeof(struct ItLpPaca),                             \
                xFPRegsInUse: 1,                                            \
                xDynProcStatus: 2,                                          \
		xDecrVal: 0x00ff0000,					    \
                xEndOfQuantum: 0xffffffffffffffff                           \
        },                                                                  \
        xRegSav: {                                                          \
                xDesc: 0xd397d9e2,      /* "LpRS"          */               \
                xSize: sizeof(struct ItLpRegSave)                           \
        },                                                                  \
        exception_sp:                                                       \
                (&paca[number].exception_stack[0]) - EXC_FRAME_SIZE,       \
}

struct paca_struct paca[MAX_PACAS] __page_aligned = {
#ifdef CONFIG_PPC_ISERIES
	PACAINITDATA( 0, 1, &xItLpQueue, 0, STAB0_VIRT_ADDR),
#else
	PACAINITDATA( 0, 1, 0, STAB0_PHYS_ADDR, STAB0_VIRT_ADDR),
#endif
	PACAINITDATA( 1, 0, 0, 0, 0),
	PACAINITDATA( 2, 0, 0, 0, 0),
	PACAINITDATA( 3, 0, 0, 0, 0),
	PACAINITDATA( 4, 0, 0, 0, 0),
	PACAINITDATA( 5, 0, 0, 0, 0),
	PACAINITDATA( 6, 0, 0, 0, 0),
	PACAINITDATA( 7, 0, 0, 0, 0),
	PACAINITDATA( 8, 0, 0, 0, 0),
	PACAINITDATA( 9, 0, 0, 0, 0),
	PACAINITDATA(10, 0, 0, 0, 0),
	PACAINITDATA(11, 0, 0, 0, 0),
	PACAINITDATA(12, 0, 0, 0, 0),
	PACAINITDATA(13, 0, 0, 0, 0),
	PACAINITDATA(14, 0, 0, 0, 0),
	PACAINITDATA(15, 0, 0, 0, 0),
	PACAINITDATA(16, 0, 0, 0, 0),
	PACAINITDATA(17, 0, 0, 0, 0),
	PACAINITDATA(18, 0, 0, 0, 0),
	PACAINITDATA(19, 0, 0, 0, 0),
	PACAINITDATA(20, 0, 0, 0, 0),
	PACAINITDATA(21, 0, 0, 0, 0),
	PACAINITDATA(22, 0, 0, 0, 0),
	PACAINITDATA(23, 0, 0, 0, 0),
	PACAINITDATA(24, 0, 0, 0, 0),
	PACAINITDATA(25, 0, 0, 0, 0),
	PACAINITDATA(26, 0, 0, 0, 0),
	PACAINITDATA(27, 0, 0, 0, 0),
	PACAINITDATA(28, 0, 0, 0, 0),
	PACAINITDATA(29, 0, 0, 0, 0),
	PACAINITDATA(30, 0, 0, 0, 0),
	PACAINITDATA(31, 0, 0, 0, 0),
	PACAINITDATA(32, 0, 0, 0, 0),
	PACAINITDATA(33, 0, 0, 0, 0),
	PACAINITDATA(34, 0, 0, 0, 0),
	PACAINITDATA(35, 0, 0, 0, 0),
	PACAINITDATA(36, 0, 0, 0, 0),
	PACAINITDATA(37, 0, 0, 0, 0),
	PACAINITDATA(38, 0, 0, 0, 0),
	PACAINITDATA(39, 0, 0, 0, 0),
	PACAINITDATA(40, 0, 0, 0, 0),
	PACAINITDATA(41, 0, 0, 0, 0),
	PACAINITDATA(42, 0, 0, 0, 0),
	PACAINITDATA(43, 0, 0, 0, 0),
	PACAINITDATA(44, 0, 0, 0, 0),
	PACAINITDATA(45, 0, 0, 0, 0),
	PACAINITDATA(46, 0, 0, 0, 0),
	PACAINITDATA(47, 0, 0, 0, 0)
};
