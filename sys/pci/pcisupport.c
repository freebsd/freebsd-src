/**************************************************************************
**
** $FreeBSD$
**
**  Device driver for DEC/INTEL PCI chipsets.
**
**  FreeBSD
**
**-------------------------------------------------------------------------
**
**  Written for FreeBSD by
**	wolf@cologne.de 	Wolfgang Stanglmeier
**	se@mi.Uni-Koeln.de	Stefan Esser
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994,1995 Stefan Esser.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
***************************************************************************
*/

#include "opt_bus.h"
#include "opt_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/pmap.h>

#include "pcib_if.h"

/*---------------------------------------------------------
**
**	Intel chipsets for 486 / Pentium processor
**
**---------------------------------------------------------
*/

static	void	chipset_attach(device_t dev, int unit);

struct condmsg {
    unsigned char	port;
    unsigned char	mask;
    unsigned char	value;
    char		flags;
    const char		*text;
};


static void
fixbushigh_i1225(device_t dev)
{
	int sublementarybus;

	sublementarybus = pci_read_config(dev, 0x41, 1);
	if (sublementarybus != 0xff) {
		pci_set_secondarybus(dev, sublementarybus + 1);
		pci_set_subordinatebus(dev, sublementarybus + 1);
	}
}

static void
fixwsc_natoma(device_t dev)
{
	int pmccfg;

	pmccfg = pci_read_config(dev, 0x50, 2);
#if defined(SMP)
	if (pmccfg & 0x8000) {
		printf("Correcting Natoma config for SMP\n");
		pmccfg &= ~0x8000;
		pci_write_config(dev, 0x50, 2, pmccfg);
	}
#else
	if ((pmccfg & 0x8000) == 0) {
		printf("Correcting Natoma config for non-SMP\n");
		pmccfg |= 0x8000;
		pci_write_config(dev, 0x50, 2, pmccfg);
	}
#endif
}
		
#ifndef PCI_QUIET

#define	M_XX 0	/* end of list */
#define M_EQ 1  /* mask and return true if equal */
#define M_NE 2  /* mask and return true if not equal */
#define M_TR 3  /* don't read config, always true */
#define M_EN 4	/* mask and print "enabled" if true, "disabled" if false */
#define M_NN 5	/* opposite sense of M_EN */

static const struct condmsg conf82425ex[] =
{
    { 0x00, 0x00, 0x00, M_TR, "\tClock " },
    { 0x50, 0x06, 0x00, M_EQ, "25" },
    { 0x50, 0x06, 0x02, M_EQ, "33" },
    { 0x50, 0x04, 0x04, M_EQ, "??", },
    { 0x00, 0x00, 0x00, M_TR, "MHz, L1 Cache " },
    { 0x50, 0x01, 0x00, M_EQ, "Disabled\n" },
    { 0x50, 0x09, 0x01, M_EQ, "Write-through\n" },
    { 0x50, 0x09, 0x09, M_EQ, "Write-back\n" },

    { 0x00, 0x00, 0x00, M_TR, "\tL2 Cache " },
    { 0x52, 0x07, 0x00, M_EQ, "Disabled" },
    { 0x52, 0x0f, 0x01, M_EQ, "64KB Write-through" },
    { 0x52, 0x0f, 0x02, M_EQ, "128KB Write-through" },
    { 0x52, 0x0f, 0x03, M_EQ, "256KB Write-through" },
    { 0x52, 0x0f, 0x04, M_EQ, "512KB Write-through" },
    { 0x52, 0x0f, 0x01, M_EQ, "64KB Write-back" },
    { 0x52, 0x0f, 0x02, M_EQ, "128KB Write-back" },
    { 0x52, 0x0f, 0x03, M_EQ, "256KB Write-back" },
    { 0x52, 0x0f, 0x04, M_EQ, "512KB Write-back" },
    { 0x53, 0x01, 0x00, M_EQ, ", 3-" },
    { 0x53, 0x01, 0x01, M_EQ, ", 2-" },
    { 0x53, 0x06, 0x00, M_EQ, "3-3-3" },
    { 0x53, 0x06, 0x02, M_EQ, "2-2-2" },
    { 0x53, 0x06, 0x04, M_EQ, "1-1-1" },
    { 0x53, 0x06, 0x06, M_EQ, "?-?-?" },
    { 0x53, 0x18, 0x00, M_EQ, "/4-2-2-2\n" },
    { 0x53, 0x18, 0x08, M_EQ, "/3-2-2-2\n" },
    { 0x53, 0x18, 0x10, M_EQ, "/?-?-?-?\n" },
    { 0x53, 0x18, 0x18, M_EQ, "/2-1-1-1\n" },

    { 0x56, 0x00, 0x00, M_TR, "\tDRAM: " },
    { 0x56, 0x02, 0x02, M_EQ, "Fast Code Read, " },
    { 0x56, 0x04, 0x04, M_EQ, "Fast Data Read, " },
    { 0x56, 0x08, 0x08, M_EQ, "Fast Write, " },
    { 0x57, 0x20, 0x20, M_EQ, "Pipelined CAS" },
    { 0x57, 0x2e, 0x00, M_NE, "\n\t" },
    { 0x57, 0x00, 0x00, M_TR, "Timing: RAS: " },
    { 0x57, 0x07, 0x00, M_EQ, "4" },
    { 0x57, 0x07, 0x01, M_EQ, "3" },
    { 0x57, 0x07, 0x02, M_EQ, "2" },
    { 0x57, 0x07, 0x04, M_EQ, "1.5" },
    { 0x57, 0x07, 0x05, M_EQ, "1" },
    { 0x57, 0x00, 0x00, M_TR, " Clocks, CAS Read: " },
    { 0x57, 0x18, 0x00, M_EQ, "3/1", },
    { 0x57, 0x18, 0x00, M_EQ, "2/1", },
    { 0x57, 0x18, 0x00, M_EQ, "1.5/0.5", },
    { 0x57, 0x18, 0x00, M_EQ, "1/1", },
    { 0x57, 0x00, 0x00, M_TR, ", CAS Write: " },
    { 0x57, 0x20, 0x00, M_EQ, "2/1", },
    { 0x57, 0x20, 0x20, M_EQ, "1/1", },
    { 0x57, 0x00, 0x00, M_TR, "\n" },

    { 0x40, 0x01, 0x01, M_EQ, "\tCPU-to-PCI Byte Merging\n" },
    { 0x40, 0x02, 0x02, M_EQ, "\tCPU-to-PCI Bursting\n" },
    { 0x40, 0x04, 0x04, M_EQ, "\tPCI Posted Writes\n" },
    { 0x40, 0x20, 0x00, M_EQ, "\tDRAM Parity Disabled\n" },

    { 0x48, 0x03, 0x01, M_EQ, "\tPCI IDE controller: Primary (1F0h-1F7h,3F6h,3F7h)" },
    { 0x48, 0x03, 0x02, M_EQ, "\tPCI IDE controller: Secondary (170h-177h,376h,377h)" },
    { 0x4d, 0x01, 0x01, M_EQ, "\tRTC (70-77h)\n" },
    { 0x4d, 0x02, 0x02, M_EQ, "\tKeyboard (60,62,64,66h)\n" },
    { 0x4d, 0x08, 0x08, M_EQ, "\tIRQ12/M Mouse Function\n" },

/* end marker */
    { 0 }
};

static const struct condmsg conf82424zx[] =
{
    { 0x00, 0x00, 0x00, M_TR, "\tCPU: " },
    { 0x50, 0xe0, 0x00, M_EQ, "486DX" },
    { 0x50, 0xe0, 0x20, M_EQ, "486SX" },
    { 0x50, 0xe0, 0x40, M_EQ, "486DX2 or 486DX4" },
    { 0x50, 0xe0, 0x80, M_EQ, "Overdrive (writeback)" },

    { 0x00, 0x00, 0x00, M_TR, ", bus=" },
    { 0x50, 0x03, 0x00, M_EQ, "25MHz" },
    { 0x50, 0x03, 0x01, M_EQ, "33MHz" },
    { 0x53, 0x01, 0x01, M_TR, ", CPU->Memory posting "},
    { 0x53, 0x01, 0x00, M_EQ, "OFF" },
    { 0x53, 0x01, 0x01, M_EQ, "ON" },

    { 0x56, 0x30, 0x00, M_NE, "\n\tWarning:" },
    { 0x56, 0x20, 0x00, M_NE, " NO cache parity!" },
    { 0x56, 0x10, 0x00, M_NE, " NO DRAM parity!" },
    { 0x55, 0x04, 0x04, M_EQ, "\n\tWarning: refresh OFF! " },

    { 0x00, 0x00, 0x00, M_TR, "\n\tCache: " },
    { 0x52, 0x01, 0x00, M_EQ, "None" },
    { 0x52, 0xc1, 0x01, M_EQ, "64KB" },
    { 0x52, 0xc1, 0x41, M_EQ, "128KB" },
    { 0x52, 0xc1, 0x81, M_EQ, "256KB" },
    { 0x52, 0xc1, 0xc1, M_EQ, "512KB" },
    { 0x52, 0x03, 0x01, M_EQ, " writethrough" },
    { 0x52, 0x03, 0x03, M_EQ, " writeback" },

    { 0x52, 0x01, 0x01, M_EQ, ", cache clocks=" },
    { 0x52, 0x05, 0x01, M_EQ, "3-1-1-1" },
    { 0x52, 0x05, 0x05, M_EQ, "2-1-1-1" },

    { 0x00, 0x00, 0x00, M_TR, "\n\tDRAM:" },
    { 0x55, 0x43, 0x00, M_NE, " page mode" },
    { 0x55, 0x02, 0x02, M_EQ, " code fetch" },
    { 0x55, 0x43, 0x43, M_EQ, "," },
    { 0x55, 0x43, 0x42, M_EQ, " and" },
    { 0x55, 0x40, 0x40, M_EQ, " read" },
    { 0x55, 0x03, 0x03, M_EQ, " and" },
    { 0x55, 0x43, 0x41, M_EQ, " and" },
    { 0x55, 0x01, 0x01, M_EQ, " write" },
    { 0x55, 0x43, 0x00, M_NE, "," },

    { 0x00, 0x00, 0x00, M_TR, " memory clocks=" },
    { 0x55, 0x20, 0x00, M_EQ, "X-2-2-2" },
    { 0x55, 0x20, 0x20, M_EQ, "X-1-2-1" },

    { 0x00, 0x00, 0x00, M_TR, "\n\tCPU->PCI: posting " },
    { 0x53, 0x02, 0x00, M_NE, "ON" },
    { 0x53, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, M_TR, ", burst mode " },
    { 0x54, 0x02, 0x00, M_NE, "ON" },
    { 0x54, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, M_TR, "\n\tPCI->Memory: posting " },
    { 0x54, 0x01, 0x00, M_NE, "ON" },
    { 0x54, 0x01, 0x00, M_EQ, "OFF" },

    { 0x00, 0x00, 0x00, M_TR, "\n" },

/* end marker */
    { 0 }
};

static const struct condmsg conf82434lx[] =
{
    { 0x00, 0x00, 0x00, M_TR, "\tCPU: " },
    { 0x50, 0xe3, 0x82, M_EQ, "Pentium, 60MHz" },
    { 0x50, 0xe3, 0x83, M_EQ, "Pentium, 66MHz" },
    { 0x50, 0xe3, 0xa2, M_EQ, "Pentium, 90MHz" },
    { 0x50, 0xe3, 0xa3, M_EQ, "Pentium, 100MHz" },
    { 0x50, 0xc2, 0x82, M_NE, "(unknown)" },
    { 0x50, 0x04, 0x00, M_EQ, " (primary cache OFF)" },

    { 0x53, 0x01, 0x01, M_TR, ", CPU->Memory posting "},
    { 0x53, 0x01, 0x01, M_NE, "OFF" },
    { 0x53, 0x01, 0x01, M_EQ, "ON" },

    { 0x53, 0x08, 0x00, M_NE, ", read around write"},

    { 0x70, 0x04, 0x00, M_EQ, "\n\tWarning: Cache parity disabled!" },
    { 0x57, 0x20, 0x00, M_NE, "\n\tWarning: DRAM parity mask!" },
    { 0x57, 0x01, 0x00, M_EQ, "\n\tWarning: refresh OFF! " },

    { 0x00, 0x00, 0x00, M_TR, "\n\tCache: " },
    { 0x52, 0x01, 0x00, M_EQ, "None" },
    { 0x52, 0x81, 0x01, M_EQ, "" },
    { 0x52, 0xc1, 0x81, M_EQ, "256KB" },
    { 0x52, 0xc1, 0xc1, M_EQ, "512KB" },
    { 0x52, 0x03, 0x01, M_EQ, " writethrough" },
    { 0x52, 0x03, 0x03, M_EQ, " writeback" },

    { 0x52, 0x01, 0x01, M_EQ, ", cache clocks=" },
    { 0x52, 0x21, 0x01, M_EQ, "3-2-2-2/4-2-2-2" },
    { 0x52, 0x21, 0x21, M_EQ, "3-1-1-1" },

    { 0x52, 0x01, 0x01, M_EQ, "\n\tCache flags: " },
    { 0x52, 0x11, 0x11, M_EQ, " cache-all" },
    { 0x52, 0x09, 0x09, M_EQ, " byte-control" },
    { 0x52, 0x05, 0x05, M_EQ, " powersaver" },

    { 0x00, 0x00, 0x00, M_TR, "\n\tDRAM:" },
    { 0x57, 0x10, 0x00, M_EQ, " page mode" },

    { 0x00, 0x00, 0x00, M_TR, " memory clocks=" },
    { 0x57, 0xc0, 0x00, M_EQ, "X-4-4-4 (70ns)" },
    { 0x57, 0xc0, 0x40, M_EQ, "X-4-4-4/X-3-3-3 (60ns)" },
    { 0x57, 0xc0, 0x80, M_EQ, "???" },
    { 0x57, 0xc0, 0xc0, M_EQ, "X-3-3-3 (50ns)" },
    { 0x58, 0x02, 0x02, M_EQ, ", RAS-wait" },
    { 0x58, 0x01, 0x01, M_EQ, ", CAS-wait" },

    { 0x00, 0x00, 0x00, M_TR, "\n\tCPU->PCI: posting " },
    { 0x53, 0x02, 0x02, M_EQ, "ON" },
    { 0x53, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, M_TR, ", burst mode " },
    { 0x54, 0x02, 0x00, M_NE, "ON" },
    { 0x54, 0x02, 0x00, M_EQ, "OFF" },
    { 0x54, 0x04, 0x00, M_TR, ", PCI clocks=" },
    { 0x54, 0x04, 0x00, M_EQ, "2-2-2-2" },
    { 0x54, 0x04, 0x00, M_NE, "2-1-1-1" },
    { 0x00, 0x00, 0x00, M_TR, "\n\tPCI->Memory: posting " },
    { 0x54, 0x01, 0x00, M_NE, "ON" },
    { 0x54, 0x01, 0x00, M_EQ, "OFF" },

    { 0x57, 0x01, 0x01, M_EQ, "\n\tRefresh:" },
    { 0x57, 0x03, 0x03, M_EQ, " CAS#/RAS#(Hidden)" },
    { 0x57, 0x03, 0x01, M_EQ, " RAS#Only" },
    { 0x57, 0x05, 0x05, M_EQ, " BurstOf4" },

    { 0x00, 0x00, 0x00, M_TR, "\n" },

/* end marker */
    { 0 }
};

static const struct condmsg conf82378[] =
{
    { 0x00, 0x00, 0x00, M_TR, "\tBus Modes:" },
    { 0x41, 0x04, 0x04, M_EQ, " Bus Park," },
    { 0x41, 0x02, 0x02, M_EQ, " Bus Lock," },
    { 0x41, 0x02, 0x00, M_EQ, " Resource Lock," },
    { 0x41, 0x01, 0x01, M_EQ, " GAT" },
    { 0x4d, 0x20, 0x20, M_EQ, "\n\tCoprocessor errors enabled" },
    { 0x4d, 0x10, 0x10, M_EQ, "\n\tMouse function enabled" },

    { 0x4e, 0x30, 0x10, M_EQ, "\n\tIDE controller: Primary (1F0h-1F7h,3F6h,3F7h)" },
    { 0x4e, 0x30, 0x30, M_EQ, "\n\tIDE controller: Secondary (170h-177h,376h,377h)" },
    { 0x4e, 0x28, 0x08, M_EQ, "\n\tFloppy controller: 3F0h,3F1h " },
    { 0x4e, 0x24, 0x04, M_EQ, "\n\tFloppy controller: 3F2h-3F7h " },
    { 0x4e, 0x28, 0x28, M_EQ, "\n\tFloppy controller: 370h,371h " },
    { 0x4e, 0x24, 0x24, M_EQ, "\n\tFloppy controller: 372h-377h " },
    { 0x4e, 0x02, 0x02, M_EQ, "\n\tKeyboard controller: 60h,62h,64h,66h" },
    { 0x4e, 0x01, 0x01, M_EQ, "\n\tRTC: 70h-77h" },

    { 0x4f, 0x80, 0x80, M_EQ, "\n\tConfiguration RAM: 0C00h,0800h-08FFh" },
    { 0x4f, 0x40, 0x40, M_EQ, "\n\tPort 92: enabled" },
    { 0x4f, 0x03, 0x00, M_EQ, "\n\tSerial Port A: COM1 (3F8h-3FFh)" },
    { 0x4f, 0x03, 0x01, M_EQ, "\n\tSerial Port A: COM2 (2F8h-2FFh)" },
    { 0x4f, 0x0c, 0x00, M_EQ, "\n\tSerial Port B: COM1 (3F8h-3FFh)" },
    { 0x4f, 0x0c, 0x04, M_EQ, "\n\tSerial Port B: COM2 (2F8h-2FFh)" },
    { 0x4f, 0x30, 0x00, M_EQ, "\n\tParallel Port: LPT1 (3BCh-3BFh)" },
    { 0x4f, 0x30, 0x04, M_EQ, "\n\tParallel Port: LPT2 (378h-37Fh)" },
    { 0x4f, 0x30, 0x20, M_EQ, "\n\tParallel Port: LPT3 (278h-27Fh)" },
    { 0x00, 0x00, 0x00, M_TR, "\n" },

/* end marker */
    { 0 }
};

static const struct condmsg conf82437fx[] = 
{
    /* PCON -- PCI Control Register */
    { 0x00, 0x00, 0x00, M_TR, "\tCPU Inactivity timer: " },
    { 0x50, 0xe0, 0xe0, M_EQ, "8" },
    { 0x50, 0xe0, 0xd0, M_EQ, "7" },
    { 0x50, 0xe0, 0xc0, M_EQ, "6" },
    { 0x50, 0xe0, 0xb0, M_EQ, "5" },
    { 0x50, 0xe0, 0xa0, M_EQ, "4" },
    { 0x50, 0xe0, 0x90, M_EQ, "3" },
    { 0x50, 0xe0, 0x80, M_EQ, "2" },
    { 0x50, 0xe0, 0x00, M_EQ, "1" },
    { 0x00, 0x00, 0x00, M_TR, " clocks\n\tPeer Concurrency: " },
    { 0x50, 0x08, 0x08, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n\tCPU-to-PCI Write Bursting: " },
    { 0x50, 0x04, 0x00, M_NN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n\tPCI Streaming: " },
    { 0x50, 0x02, 0x00, M_NN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n\tBus Concurrency: " },
    { 0x50, 0x01, 0x00, M_NN, 0 },

    /* CC -- Cache Control Regsiter */
    { 0x00, 0x00, 0x00, M_TR, "\n\tCache:" },
    { 0x52, 0xc0, 0x80, M_EQ, " 512K" },
    { 0x52, 0xc0, 0x40, M_EQ, " 256K" },
    { 0x52, 0xc0, 0x00, M_EQ, " NO" },
    { 0x52, 0x30, 0x00, M_EQ, " pipelined-burst" },
    { 0x52, 0x30, 0x10, M_EQ, " burst" },
    { 0x52, 0x30, 0x20, M_EQ, " asynchronous" },
    { 0x52, 0x30, 0x30, M_EQ, " dual-bank pipelined-burst" },
    { 0x00, 0x00, 0x00, M_TR, " secondary; L1 " },
    { 0x52, 0x01, 0x00, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n" },

    /* DRAMC -- DRAM Control Register */
    { 0x57, 0x07, 0x00, M_EQ, "Warning: refresh OFF!\n" },
    { 0x00, 0x00, 0x00, M_TR, "\tDRAM:" },
    { 0x57, 0xc0, 0x00, M_EQ, " no memory hole" },
    { 0x57, 0xc0, 0x40, M_EQ, " 512K-640K memory hole" },
    { 0x57, 0xc0, 0x80, M_EQ, " 15M-16M memory hole" },
    { 0x57, 0x07, 0x01, M_EQ, ", 50 MHz refresh" },
    { 0x57, 0x07, 0x02, M_EQ, ", 60 MHz refresh" },
    { 0x57, 0x07, 0x03, M_EQ, ", 66 MHz refresh" },

    /* DRAMT = DRAM Timing Register */
    { 0x00, 0x00, 0x00, M_TR, "\n\tRead burst timing: " },
    { 0x58, 0x60, 0x00, M_EQ, "x-4-4-4/x-4-4-4" },
    { 0x58, 0x60, 0x20, M_EQ, "x-3-3-3/x-4-4-4" },
    { 0x58, 0x60, 0x40, M_EQ, "x-2-2-2/x-3-3-3" },
    { 0x58, 0x60, 0x60, M_EQ, "???" },
    { 0x00, 0x00, 0x00, M_TR, "\n\tWrite burst timing: " },
    { 0x58, 0x18, 0x00, M_EQ, "x-4-4-4" },
    { 0x58, 0x18, 0x08, M_EQ, "x-3-3-3" },
    { 0x58, 0x18, 0x10, M_EQ, "x-2-2-2" },
    { 0x58, 0x18, 0x18, M_EQ, "???" },
    { 0x00, 0x00, 0x00, M_TR, "\n\tRAS-CAS delay: " },
    { 0x58, 0x04, 0x00, M_EQ, "3" },
    { 0x58, 0x04, 0x04, M_EQ, "2" },
    { 0x00, 0x00, 0x00, M_TR, " clocks\n" },

    /* end marker */
    { 0 }
};

static const struct condmsg conf82437vx[] = 
{
    /* PCON -- PCI Control Register */
    { 0x00, 0x00, 0x00, M_TR, "\n\tPCI Concurrency: " },
    { 0x50, 0x08, 0x08, M_EN, 0 },

    /* CC -- Cache Control Regsiter */
    { 0x00, 0x00, 0x00, M_TR, "\n\tCache:" },
    { 0x52, 0xc0, 0x80, M_EQ, " 512K" },
    { 0x52, 0xc0, 0x40, M_EQ, " 256K" },
    { 0x52, 0xc0, 0x00, M_EQ, " NO" },
    { 0x52, 0x30, 0x00, M_EQ, " pipelined-burst" },
    { 0x52, 0x30, 0x10, M_EQ, " burst" },
    { 0x52, 0x30, 0x20, M_EQ, " asynchronous" },
    { 0x52, 0x30, 0x30, M_EQ, " dual-bank pipelined-burst" },
    { 0x00, 0x00, 0x00, M_TR, " secondary; L1 " },
    { 0x52, 0x01, 0x00, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n" },

    /* DRAMC -- DRAM Control Register */
    { 0x57, 0x07, 0x00, M_EQ, "Warning: refresh OFF!\n" },
    { 0x00, 0x00, 0x00, M_TR, "\tDRAM:" },
    { 0x57, 0xc0, 0x00, M_EQ, " no memory hole" },
    { 0x57, 0xc0, 0x40, M_EQ, " 512K-640K memory hole" },
    { 0x57, 0xc0, 0x80, M_EQ, " 15M-16M memory hole" },
    { 0x57, 0x07, 0x01, M_EQ, ", 50 MHz refresh" },
    { 0x57, 0x07, 0x02, M_EQ, ", 60 MHz refresh" },
    { 0x57, 0x07, 0x03, M_EQ, ", 66 MHz refresh" },

    /* DRAMT = DRAM Timing Register */
    { 0x00, 0x00, 0x00, M_TR, "\n\tRead burst timing: " },
    { 0x58, 0x60, 0x00, M_EQ, "x-4-4-4/x-4-4-4" },
    { 0x58, 0x60, 0x20, M_EQ, "x-3-3-3/x-4-4-4" },
    { 0x58, 0x60, 0x40, M_EQ, "x-2-2-2/x-3-3-3" },
    { 0x58, 0x60, 0x60, M_EQ, "???" },
    { 0x00, 0x00, 0x00, M_TR, "\n\tWrite burst timing: " },
    { 0x58, 0x18, 0x00, M_EQ, "x-4-4-4" },
    { 0x58, 0x18, 0x08, M_EQ, "x-3-3-3" },
    { 0x58, 0x18, 0x10, M_EQ, "x-2-2-2" },
    { 0x58, 0x18, 0x18, M_EQ, "???" },
    { 0x00, 0x00, 0x00, M_TR, "\n\tRAS-CAS delay: " },
    { 0x58, 0x04, 0x00, M_EQ, "3" },
    { 0x58, 0x04, 0x04, M_EQ, "2" },
    { 0x00, 0x00, 0x00, M_TR, " clocks\n" },

    /* end marker */
    { 0 }
};

static const struct condmsg conf82371fb[] =
{
    /* IORT -- ISA I/O Recovery Timer Register */
    { 0x00, 0x00, 0x00, M_TR, "\tI/O Recovery Timing: 8-bit " },
    { 0x4c, 0x40, 0x00, M_EQ, "3.5" },
    { 0x4c, 0x78, 0x48, M_EQ, "1" },
    { 0x4c, 0x78, 0x50, M_EQ, "2" },
    { 0x4c, 0x78, 0x58, M_EQ, "3" },
    { 0x4c, 0x78, 0x60, M_EQ, "4" },
    { 0x4c, 0x78, 0x68, M_EQ, "5" },
    { 0x4c, 0x78, 0x70, M_EQ, "6" },
    { 0x4c, 0x78, 0x78, M_EQ, "7" },
    { 0x4c, 0x78, 0x40, M_EQ, "8" },
    { 0x00, 0x00, 0x00, M_TR, " clocks, 16-bit " },
    { 0x4c, 0x04, 0x00, M_EQ, "3.5" },
    { 0x4c, 0x07, 0x05, M_EQ, "1" },
    { 0x4c, 0x07, 0x06, M_EQ, "2" },
    { 0x4c, 0x07, 0x07, M_EQ, "3" },
    { 0x4c, 0x07, 0x04, M_EQ, "4" },
    { 0x00, 0x00, 0x00, M_TR, " clocks\n" },

    /* XBCS -- X-Bus Chip Select Register */
    { 0x00, 0x00, 0x00, M_TR, "\tExtended BIOS: " },
    { 0x4e, 0x80, 0x80, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n\tLower BIOS: " },
    { 0x4e, 0x40, 0x40, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n\tCoprocessor IRQ13: " },
    { 0x4e, 0x20, 0x20, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n\tMouse IRQ12: " },
    { 0x4e, 0x10, 0x10, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n" },

    { 0x00, 0x00, 0x00, M_TR, "\tInterrupt Routing: " },
#define PIRQ(x, n) \
    { 0x00, 0x00, 0x00, M_TR, n ": " }, \
    { x, 0x80, 0x80, M_EQ, "disabled" }, \
    { x, 0xc0, 0x40, M_EQ, "[shared] " }, \
    { x, 0x8f, 0x03, M_EQ, "IRQ3" }, \
    { x, 0x8f, 0x04, M_EQ, "IRQ4" }, \
    { x, 0x8f, 0x05, M_EQ, "IRQ5" }, \
    { x, 0x8f, 0x06, M_EQ, "IRQ6" }, \
    { x, 0x8f, 0x07, M_EQ, "IRQ7" }, \
    { x, 0x8f, 0x09, M_EQ, "IRQ9" }, \
    { x, 0x8f, 0x0a, M_EQ, "IRQ10" }, \
    { x, 0x8f, 0x0b, M_EQ, "IRQ11" }, \
    { x, 0x8f, 0x0c, M_EQ, "IRQ12" }, \
    { x, 0x8f, 0x0e, M_EQ, "IRQ14" }, \
    { x, 0x8f, 0x0f, M_EQ, "IRQ15" }

    /* Interrupt routing */
    PIRQ(0x60, "A"),
    PIRQ(0x61, ", B"),
    PIRQ(0x62, ", C"),
    PIRQ(0x63, ", D"),
    PIRQ(0x70, "\n\t\tMB0"),
    PIRQ(0x71, ", MB1"),

    { 0x00, 0x00, 0x00, M_TR, "\n" },

#undef PIRQ

    /* XXX - do DMA routing, too? */
    { 0 }
};

static const struct condmsg conf82371fb2[] =
{
    /* IDETM -- IDE Timing Register */
    { 0x00, 0x00, 0x00, M_TR, "\tPrimary IDE: " },
    { 0x41, 0x80, 0x80, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n\tSecondary IDE: " },
    { 0x43, 0x80, 0x80, M_EN, 0 },
    { 0x00, 0x00, 0x00, M_TR, "\n" },

    /* end of list */
    { 0 }
};

static void
writeconfig (device_t dev, const struct condmsg *tbl)
{
    while (tbl->flags != M_XX) {
	const char *text = 0;

	if (tbl->flags == M_TR) {
	    text = tbl->text;
	} else {
	    unsigned char v = pci_read_config(dev, tbl->port, 1);
	    switch (tbl->flags) {
    case M_EQ:
		if ((v & tbl->mask) == tbl->value) text = tbl->text;
		break;
    case M_NE:
		if ((v & tbl->mask) != tbl->value) text = tbl->text;
		break;
    case M_EN:
		text = (v & tbl->mask) ? "enabled" : "disabled";
		break;
    case M_NN:
		text = (v & tbl->mask) ? "disabled" : "enabled";
	    }
	}
	if (text) printf ("%s", text);
	tbl++;
    }
}

#ifdef DUMPCONFIGSPACE
static void
dumpconfigspace (device_t dev)
{
    int reg;
    printf ("configuration space registers:");
    for (reg = 0; reg < 0x100; reg+=4) {
	if ((reg & 0x0f) == 0) 
	    printf ("\n%02x:\t", reg);
	printf ("%08x ", pci_read_config(dev, reg, 4));
    }
    printf ("\n");
}
#endif /* DUMPCONFIGSPACE */

#endif /* PCI_QUIET */


static void
chipset_attach (device_t dev, int unit)
{
#ifndef PCI_QUIET
	if (!bootverbose)
		return;

	switch (pci_get_devid(dev)) {
	case 0x04868086:
		writeconfig (dev, conf82425ex);
		break;
	case 0x04838086:
		writeconfig (dev, conf82424zx);
		break;
	case 0x04a38086:
		writeconfig (dev, conf82434lx);
		break;
	case 0x04848086:
		writeconfig (dev, conf82378);
		break;
	case 0x122d8086:
		writeconfig (dev, conf82437fx);
		break;
	case 0x70308086:
		writeconfig (dev, conf82437vx);
		break;
	case 0x70008086:
	case 0x122e8086:
		writeconfig (dev, conf82371fb);
		break;
	case 0x70108086:
	case 0x12308086:
		writeconfig (dev, conf82371fb2);
		break;
#if 0
	case 0x00011011: /* DEC 21050 */
	case 0x00221014: /* IBM xxx */
		writeconfig (dev, conf_pci2pci);
		break;
#endif
	};
#endif /* PCI_QUIET */
}

static const char *
pci_bridge_type(device_t dev)
{
    char *descr, tmpbuf[120];

    if (pci_get_class(dev) != PCIC_BRIDGE)
	    return NULL;

    switch (pci_get_subclass(dev)) {
    case PCIS_BRIDGE_HOST:	strcpy(tmpbuf, "Host to PCI"); break;
    case PCIS_BRIDGE_ISA:	strcpy(tmpbuf, "PCI to ISA"); break;
    case PCIS_BRIDGE_EISA:	strcpy(tmpbuf, "PCI to EISA"); break;
    case PCIS_BRIDGE_MCA:	strcpy(tmpbuf, "PCI to MCA"); break;
    case PCIS_BRIDGE_PCI:	strcpy(tmpbuf, "PCI to PCI"); break;
    case PCIS_BRIDGE_PCMCIA:	strcpy(tmpbuf, "PCI to PCMCIA"); break;
    case PCIS_BRIDGE_NUBUS:	strcpy(tmpbuf, "PCI to NUBUS"); break;
    case PCIS_BRIDGE_CARDBUS:	strcpy(tmpbuf, "PCI to CardBus"); break;
    case PCIS_BRIDGE_OTHER:	strcpy(tmpbuf, "PCI to Other"); break;
    default: 
	    snprintf(tmpbuf, sizeof(tmpbuf),
		     "PCI to 0x%x", pci_get_subclass(dev)); 
	    break;
    }
    snprintf(tmpbuf+strlen(tmpbuf), sizeof(tmpbuf)-strlen(tmpbuf),
	     " bridge (vendor=%04x device=%04x)",
	     pci_get_vendor(dev), pci_get_device(dev));
    descr = malloc (strlen(tmpbuf) +1, M_DEVBUF, M_WAITOK);
    strcpy(descr, tmpbuf);
    return descr;
}

static const char*
pcib_match(device_t dev)
{
	switch (pci_get_devid(dev)) {
	/* Intel -- vendor 0x8086 */
	case 0x71818086:
		return ("Intel 82443LX (440 LX) PCI-PCI (AGP) bridge");
	case 0x71918086:
		return ("Intel 82443BX (440 BX) PCI-PCI (AGP) bridge");
	case 0x71A18086:
		return ("Intel 82443GX (440 GX) PCI-PCI (AGP) bridge");
	case 0x11318086:
		return ("Intel 82815 (i815 GMCH) PCI-PCI (AGP) bridge");
	case 0x84cb8086:
		return ("Intel 82454NX PCI Expander Bridge");
	case 0x124b8086:
		return ("Intel 82380FB mobile PCI to PCI bridge");
	case 0x24188086:
		return ("Intel 82801AA (ICH) Hub to PCI bridge");
	case 0x24288086:
		return ("Intel 82801AB (ICH0) Hub to PCI bridge");
	case 0x244E8086:
		return ("Intel 82801BA (ICH2) Hub to PCI bridge");
	
	/* VLSI -- vendor 0x1004 */
	case 0x01021004:
		return ("VLSI 82C534 Eagle II PCI Bus bridge");
	case 0x01031004:
		return ("VLSI 82C538 Eagle II PCI Docking bridge");

	/* VIA Technologies -- vendor 0x1106 */
	case 0x85981106:
		return ("VIA 82C598MVP/82C694X (Apollo MVP3/Pro133A) PCI-PCI (AGP) bridge");
	case 0x83911106:
		return ("VIA 8371 (KX133) PCI-PCI (AGP) bridge");
	case 0x83051106:
		return ("VIA 8363 (KT133) PCI-PCI (AGP) bridge");

	/* AcerLabs -- vendor 0x10b9 */
	/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
	/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x524710b9:
		return ("AcerLabs M5247 PCI-PCI(AGP Supported) bridge");
	case 0x524310b9:/* 5243 seems like 5247, need more info to divide*/
		return ("AcerLabs M5243 PCI-PCI bridge");

	/* AMD -- vendor 0x1022 */
	case 0x70071022:
		return ("AMD-751 PCI-PCI (AGP) bridge");

	/* DEC -- vendor 0x1011 */
	case 0x00011011:
		return ("DEC 21050 PCI-PCI bridge");
	case 0x00211011:
		return ("DEC 21052 PCI-PCI bridge");
	case 0x00221011:
		return ("DEC 21150 PCI-PCI bridge");
	case 0x00241011:
		return ("DEC 21152 PCI-PCI bridge");
	case 0x00251011:
		return ("DEC 21153 PCI-PCI bridge");
	case 0x00261011:
		return ("DEC 21154 PCI-PCI bridge");

	/* Others */
	case 0x00221014:
		return ("IBM 82351 PCI-PCI bridge");
	/* UMC United Microelectronics 0x1060 */
	case 0x88811060:
		return ("UMC UM8881 HB4 486 PCI Chipset");
	};

	if (pci_get_class(dev) == PCIC_BRIDGE
	    && pci_get_subclass(dev) == PCIS_BRIDGE_PCI)
		return pci_bridge_type(dev);

	return NULL;
}

static int pcib_probe(device_t dev)
{
	const char *desc;

	desc = pcib_match(dev);
	if (desc) {
		device_set_desc_copy(dev, desc);
		return -10000;
	}

	return ENXIO;
}

static int pcib_attach(device_t dev)
{
	u_int8_t secondary;
	device_t child;

	chipset_attach(dev, device_get_unit(dev));

	secondary = pci_get_secondarybus(dev);
	if (secondary) {
		child = device_add_child(dev, "pci", -1);
		*(int*) device_get_softc(dev) = secondary;
		return bus_generic_attach(dev);
	} else
		return 0;
}

static int
pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	switch (which) {
	case PCIB_IVAR_BUS:
		*result = *(int*) device_get_softc(dev);
		return 0;
	}
	return ENOENT;
}

static int
pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	switch (which) {
	case PCIB_IVAR_BUS:
		*(int*) device_get_softc(dev) = value;
		return 0;
	}
	return ENOENT;
}

static int
pcib_maxslots(device_t dev)
{
	return 31;
}

static u_int32_t
pcib_read_config(device_t dev, int b, int s, int f,
		 int reg, int width)
{
	/*
	 * Pass through to the next ppb up the chain (i.e. our
	 * grandparent).
	 */
	return PCIB_READ_CONFIG(device_get_parent(device_get_parent(dev)),
				b, s, f, reg, width);
}

static void
pcib_write_config(device_t dev, int b, int s, int f,
		  int reg, u_int32_t val, int width)
{
	/*
	 * Pass through to the next ppb up the chain (i.e. our
	 * grandparent).
	 */
	PCIB_WRITE_CONFIG(device_get_parent(device_get_parent(dev)),
			  b, s, f, reg, val, width);
}

static device_method_t pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcib_probe),
	DEVMETHOD(device_attach,	pcib_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	pcib_maxslots),
	DEVMETHOD(pcib_read_config,	pcib_read_config),
	DEVMETHOD(pcib_write_config,	pcib_write_config),

	{ 0, 0 }
};

static driver_t pcib_driver = {
	"pcib",
	pcib_methods,
	sizeof(int),
};

static devclass_t pcib_devclass;

DRIVER_MODULE(pcib, pci, pcib_driver, pcib_devclass, 0, 0);

static const char *
eisab_match(device_t dev)
{
	switch (pci_get_devid(dev)) {
	case 0x04828086:
		/* Recognize this specifically, it has PCI-HOST class (!) */
		return ("Intel 82375EB PCI-EISA bridge");
	}
	if (pci_get_class(dev) == PCIC_BRIDGE
	    && pci_get_subclass(dev) == PCIS_BRIDGE_EISA)
		return pci_bridge_type(dev);

	return NULL;
}

static const char *
isab_match(device_t dev)
{
	unsigned	rev;

	switch (pci_get_devid(dev)) {
	case 0x04848086:
		rev = pci_get_revid(dev);
		if (rev == 3)
		    return ("Intel 82378ZB PCI to ISA bridge");
		return ("Intel 82378IB PCI to ISA bridge");
	case 0x122e8086:
		return ("Intel 82371FB PCI to ISA bridge");
	case 0x70008086:
		return ("Intel 82371SB PCI to ISA bridge");
	case 0x71108086:
		return ("Intel 82371AB PCI to ISA bridge");
	case 0x71988086:
		return ("Intel 82443MX PCI to X-bus bridge");
	case 0x24108086:
		return ("Intel 82801AA (ICH) PCI to LPC bridge");
	case 0x24208086:
		return ("Intel 82801AB (ICH0) PCI to LPC bridge");
	case 0x24408086:
		return ("Intel 82801BA (ICH2) PCI to LPC bridge");
	
	/* VLSI -- vendor 0x1004 */
	case 0x00061004:
		return ("VLSI 82C593 PCI to ISA bridge");

	/* VIA Technologies -- vendor 0x1106 */
	case 0x05861106: /* south bridge section */
		return ("VIA 82C586 PCI-ISA bridge");
	case 0x05961106:
		return ("VIA 82C596 PCI-ISA bridge");
	case 0x06861106:
		return ("VIA 82C686 PCI-ISA bridge");

	/* AcerLabs -- vendor 0x10b9 */
	/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
	/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x153310b9:
		return ("AcerLabs M1533 portable PCI-ISA bridge");
	case 0x154310b9:
		return ("AcerLabs M1543 desktop PCI-ISA bridge");

	/* SiS -- vendor 0x1039 */
	case 0x00081039:
		return ("SiS 85c503 PCI-ISA bridge");

	/* Cyrix -- vendor 0x1078 */
	case 0x00001078:
		return ("Cyrix Cx5510 PCI-ISA bridge");
	case 0x01001078:
		return ("Cyrix Cx5530 PCI-ISA bridge");

	/* OPTi -- vendor 0x1045 */
	case 0xc7001045:
		return ("OPTi 82C700 (FireStar) PCI-ISA bridge");

	/* NEC -- vendor 0x1033 */
	/* The "C-bus" is 16-bits bus on PC98. */
	case 0x00011033:
		return ("NEC 0001 PCI to PC-98 C-bus bridge");
	case 0x002c1033:
		return ("NEC 002C PCI to PC-98 C-bus bridge");
	case 0x003b1033:
		return ("NEC 003B PCI to PC-98 C-bus bridge");
	/* UMC United Microelectronics 0x1060 */
	case 0x886a1060:
		return ("UMC UM8886 ISA Bridge with EIDE");

	/* Cypress -- vendor 0x1080 */
	case 0xc6931080:
		if (pci_get_class(dev) == PCIC_BRIDGE
		    && pci_get_subclass(dev) == PCIS_BRIDGE_ISA)
			return ("Cypress 82C693 PCI-ISA bridge");
		break;

	/* ServerWorks -- vendor 0x1166 */
	case 0x02001166:
		return ("ServerWorks IB6566 PCI to ISA bridge");
	}

	if (pci_get_class(dev) == PCIC_BRIDGE
	    && pci_get_subclass(dev) == PCIS_BRIDGE_ISA)
		return pci_bridge_type(dev);

	return NULL;
}

static int
isab_probe(device_t dev)
{
	const char *desc;
	int	is_eisa;

	is_eisa = 0;
	desc = eisab_match(dev);
	if (desc)
		is_eisa = 1;
	else
		desc = isab_match(dev);
	if (desc) {
		/*
		 * For a PCI-EISA bridge, add both eisa and isa.
		 * Only add one instance of eisa or isa for now.
		 */
		device_set_desc_copy(dev, desc);
		if (is_eisa && !devclass_get_device(devclass_find("eisa"), 0))
			device_add_child(dev, "eisa", -1);

		if (!devclass_get_device(devclass_find("isa"), 0))
			device_add_child(dev, "isa", -1);
		return 0;
	}
	return ENXIO;
}

static int
isab_attach(device_t dev)
{
	chipset_attach(dev, device_get_unit(dev));
	return bus_generic_attach(dev);
}

static device_method_t isab_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		isab_probe),
	DEVMETHOD(device_attach,	isab_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t isab_driver = {
	"isab",
	isab_methods,
	1,
};

static devclass_t isab_devclass;

DRIVER_MODULE(isab, pci, isab_driver, isab_devclass, 0, 0);

const char *
pci_usb_match(device_t dev)
{
	switch (pci_get_devid(dev)) {

	/* Intel -- vendor 0x8086 */
	case 0x70208086:
		return ("Intel 82371SB (PIIX3) USB controller");
	case 0x71128086:
		return ("Intel 82371AB/EB (PIIX4) USB controller");
	case 0x719a8086:
		return ("Intel 82443MX USB controller");
	case 0x24128086:
		return ("Intel 82801AA (ICH) USB controller");
	case 0x24228086:
		return ("Intel 82801AB (ICH0) USB controller");
	case 0x24428086:
		return ("Intel 82801BA (ICH2) USB controller #1");
	case 0x24448086:
		return ("Intel 82801BA (ICH2) USB controller #2");

	/* VIA Technologies -- vendor 0x1106 (0x1107 on the Apollo Master) */
	case 0x30381106:
		return ("VIA 83C572 USB controller");

	/* AcerLabs -- vendor 0x10b9 */
	case 0x523710b9:
		return ("AcerLabs M5237 (Aladdin-V) USB controller");

	/* OPTi -- vendor 0x1045 */
	case 0xc8611045:
		return ("OPTi 82C861 (FireLink) USB controller");

	/* NEC -- vendor 0x1033 */
	case 0x00351033:
		return ("NEC uPD 9210 USB controller");

	/* CMD Tech -- vendor 0x1095 */
	case 0x06701095:
		return ("CMD Tech 670 (USB0670) USB controller");
	case 0x06731095:
		return ("CMD Tech 673 (USB0673) USB controller");
	}

	if (pci_get_class(dev) == PCIC_SERIALBUS
	    && pci_get_subclass(dev) == PCIS_SERIALBUS_USB) {
		if (pci_get_progif(dev) == 0x00 /* UHCI */ ) {
			return ("UHCI USB controller");
		} else if (pci_get_progif(dev) == 0x10 /* OHCI */ ) {
			return ("OHCI USB controller");
		} else {
			return ("USB controller");
		}
	}
	return NULL;
}

const char *
pci_ata_match(device_t dev)
{

	switch (pci_get_devid(dev)) {

	/* Intel -- vendor 0x8086 */
	case 0x12308086:
		return ("Intel PIIX ATA controller");
	case 0x70108086:
		return ("Intel PIIX3 ATA controller");
	case 0x71118086:
		return ("Intel PIIX4 ATA controller");
	case 0x12348086:
		return ("Intel 82371MX mobile PCI ATA accelerator (MPIIX)");

	/* Promise -- vendor 0x105a */
	case 0x4d33105a:
		return ("Promise Ultra/33 ATA controller");
	case 0x4d38105a:
		return ("Promise Ultra/66 ATA controller");

	/* AcerLabs -- vendor 0x10b9 */
	case 0x522910b9:
		return ("AcerLabs Aladdin ATA controller");

	/* VIA Technologies -- vendor 0x1106 (0x1107 on the Apollo Master) */
	case 0x05711106:
		switch (pci_read_config(dev, 0x08, 1)) {
		case 1:
			return ("VIA 85C586 ATA controller");
		case 6:
			return ("VIA 85C586 ATA controller");
		}
		/* FALL THROUGH */
	case 0x15711106:
		return ("VIA Apollo ATA controller");

	/* CMD Tech -- vendor 0x1095 */
	case 0x06401095:
		return ("CMD 640 ATA controller");
	case 0x06461095:
		return ("CMD 646 ATA controller");

	/* Cypress -- vendor 0x1080 */
	case 0xc6931080:
		return ("Cypress 82C693 ATA controller");

	/* Cyrix -- vendor 0x1078 */
	case 0x01021078:
		return ("Cyrix 5530 ATA controller");

	/* SiS -- vendor 0x1039 */
	case 0x55131039:
		return ("SiS 5591 ATA controller");

	/* Highpoint tech -- vendor 0x1103 */
	case 0x00041103:
		return ("HighPoint HPT366 ATA controller");

	/* OPTi -- vendor 0x1045 */
	case 0xd5681045:
		return ("OPTi 82C700 (FireStar) ATA controller(generic mode):");
	}

	if (pci_get_class(dev) == PCIC_STORAGE &&
	    pci_get_subclass(dev) == PCIS_STORAGE_IDE)
		return ("Unknown PCI ATA controller");

	return NULL;
}

const char*
pci_chip_match(device_t dev)
{
	unsigned	rev;

	switch (pci_get_devid(dev)) {
	/* Intel -- vendor 0x8086 */
	case 0x00088086:
		/* Silently ignore this one! What is it, anyway ??? */
		return ("");
	case 0x71108086:
		/*
		 * On my laptop (Tecra 8000DVD), this device has a
		 * bogus subclass 0x80 so make sure that it doesn't
		 * match the generic 'chip' driver by accident.
		 */
		return NULL;
	case 0x12258086:
		fixbushigh_i1225(dev);
		return ("Intel 82454KX/GX (Orion) host to PCI bridge");
	case 0x71808086:
		return ("Intel 82443LX (440 LX) host to PCI bridge");
	case 0x71908086:
		return ("Intel 82443BX (440 BX) host to PCI bridge");
	case 0x71928086:
		return ("Intel 82443BX host to PCI bridge (AGP disabled)");
	case 0x71948086:
		return ("Intel 82443MX (440 MX) host to PCI bridge");
 	case 0x71a08086:
 		return ("Intel 82443GX host to PCI bridge");
 	case 0x71a18086:
 		return ("Intel 82443GX host to AGP bridge");
 	case 0x71a28086:
 		return ("Intel 82443GX host to PCI bridge (AGP disabled)");
	case 0x84c48086:
		return ("Intel 82454KX/GX (Orion) host to PCI bridge");
	case 0x84ca8086:
		return ("Intel 82451NX Memory and I/O controller");
	case 0x04868086:
		return ("Intel 82425EX PCI system controller");
	case 0x04838086:
		return ("Intel 82424ZX (Saturn) cache DRAM controller");
	case 0x04a38086:
		rev = pci_get_revid(dev);
		if (rev == 16 || rev == 17)
		    return ("Intel 82434NX (Neptune) PCI cache memory controller");
		return ("Intel 82434LX (Mercury) PCI cache memory controller");
	case 0x122d8086:
		return ("Intel 82437FX PCI cache memory controller");
	case 0x12358086:
		return ("Intel 82437MX mobile PCI cache memory controller");
	case 0x12508086:
		return ("Intel 82439HX PCI cache memory controller");
	case 0x70308086:
		return ("Intel 82437VX PCI cache memory controller");
	case 0x71008086:
		return ("Intel 82439TX System controller (MTXC)");
	case 0x71138086:
		return ("Intel 82371AB Power management controller");
	case 0x12378086:
		fixwsc_natoma(dev);
		return ("Intel 82440FX (Natoma) PCI and memory controller");
	case 0x84c58086:
		return ("Intel 82453KX/GX (Orion) PCI memory controller");
	case 0x71208086:
		return ("Intel 82810 (i810 GMCH) Host To Hub bridge");
	case 0x71228086:
		return ("Intel 82810-DC100 (i810-DC100 GMCH) Host To Hub bridge");
	case 0x71248086:
		return ("Intel 82810E (i810E GMCH) Host To Hub bridge");
	case 0x11308086:
		return ("Intel 82815 (i815 GMCH) Host To Hub bridge");
	case 0x24158086:
		return ("Intel 82801AA (ICH) AC'97 Audio Controller");
	case 0x24258086:
		return ("Intel 82801AB (ICH0) AC'97 Audio Controller");
	case 0x24458086:
		return ("Intel 82801BA (ICH2) AC'97 Audio Controller");
	case 0x24468086:
		return ("Intel 82801BA (ICH2) AC'97 Modem Controller");
	case 0x71958086:
		return ("Intel 82443MX AC'97 Audio Controller");
	case 0x719b8086:
		return ("Intel 82443MX SMBus and power management controller");
	case 0x24438086:
		return ("Intel 82801BA (ICH2) SMBus controller");

	/* Sony -- vendor 0x104d */
	case 0x8009104d:
		return ("Sony CXD1947A FireWire Host Controller");

	/* SiS -- vendor 0x1039 */
	case 0x04961039:
		return ("SiS 85c496 PCI/VL Bridge");
	case 0x04061039:
		return ("SiS 85c501");
	case 0x06011039:
		return ("SiS 85c601");
	case 0x55911039:
		return ("SiS 5591 host to PCI bridge");
	case 0x00011039:
		return ("SiS 5591 host to AGP bridge");
	
	/* VLSI -- vendor 0x1004 */
	case 0x00051004:
		return ("VLSI 82C592 Host to PCI bridge");
	case 0x01011004:
		return ("VLSI 82C532 Eagle II Peripheral controller");
	case 0x01041004:
		return ("VLSI 82C535 Eagle II System controller");
	case 0x01051004:
		return ("VLSI 82C147 IrDA controller");

	/* VIA Technologies -- vendor 0x1106 (0x1107 on the Apollo Master) */
	case 0x15761107:
		return ("VIA 82C570 (Apollo Master) system controller");
	case 0x05851106:
		return ("VIA 82C585 (Apollo VP1/VPX) system controller");
	case 0x05951106:
	case 0x15951106:
		return ("VIA 82C595 (Apollo VP2) system controller");
	case 0x05971106:
		return ("VIA 82C597 (Apollo VP3) system controller");
	case 0x05981106:
		return ("VIA 82C598 (Apollo MVP3) host bridge");
	case 0x06911106:
		return ("VIA 82C691 (Apollo Pro) host bridge");
	case 0x06931106:
		return ("VIA 82C693 (Apollo Pro+) host bridge");
	case 0x03911106:
 		return ("VIA 8371 (KX133) host to PCI bridge");
	case 0x30401106:
		return ("VIA 82C586B ACPI interface");
	case 0x30501106:
		return ("VIA 82C596B ACPI interface");
	case 0x30571106:
		return ("VIA 82C686 ACPI interface");
	case 0x30581106:
		return ("VIA 82C686 AC97 Audio");
	case 0x30681106:
		return ("VIA 82C686 AC97 Modem");

	/* AMD -- vendor 0x1022 */
	case 0x70061022:
		return ("AMD-751 host to PCI bridge");

	/* NEC -- vendor 0x1033 */
	case 0x00021033:
		return ("NEC 0002 PCI to PC-98 local bus bridge");
	case 0x00161033:
		return ("NEC 0016 PCI to PC-98 local bus bridge");

	/* AcerLabs -- vendor 0x10b9 */
	/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
	/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x154110b9:
		return ("AcerLabs M1541 (Aladdin-V) PCI host bridge");
	case 0x710110b9:
		return ("AcerLabs M15x3 Power Management Unit");

	/* OPTi -- vendor 0x1045 */
	case 0xc7011045:
		return ("OPTi 82C700 host to PCI bridge");
	case 0xc8221045:
		return ("OPTi 82C822 host to PCI Bridge");

	/* Texas Instruments -- vendor 0x104c */
	case 0xac1c104c:
		return ("Texas Instruments PCI1225 CardBus controller");
	case 0xac50104c:
		return ("Texas Instruments PCI1410 CardBus controller");
	case 0xac51104c:
		return ("Texas Instruments PCI1420 CardBus controller");
	case 0xac1b104c:
		return ("Texas Instruments PCI1450 CardBus controller");
	case 0xac52104c:
		return ("Texas Instruments PCI1451 CardBus controller");

	/* NeoMagic -- vendor 0x10c8 */
	case 0x800510c8:
		return ("NeoMagic MagicMedia 256AX Audio controller");
	case 0x800610c8:
		return ("NeoMagic MagicMedia 256ZX Audio controller");

	/* ESS Technology Inc -- vendor 0x125d */
	case 0x1969125d:
		return ("ESS Technology Solo-1 Audio controller");
	case 0x1978125d:
		return ("ESS Technology Maestro 2E Audio controller");

	/* Aureal Inc.-- vendor 0x12eb */
	case 0x000112eb:
		return ("Aureal Vortex AU8820 Audio controller");
	case 0x000212eb:
		return ("Aureal Vortex AU8830 Audio controller");

	/* Lucent -- Vendor 0x11c1 */
	case 0x044011c1:
	case 0x044811c1:
		return ("Lucent K56Flex DSVD LTModem (Win Modem, unsupported)");

	/* CCUBE -- Vendor 0x123f */
	case 0x8888123f:
		return ("Cinemaster C 3.0 DVD Decoder");

	/* Toshiba -- vendor 0x1179 */
	case 0x07011179:
		return ("Toshiba Fast Infra Red controller");

	/* Compaq -- vendor 0x0e11 */
	case 0xa0f70e11:
		return ("Compaq PCI Hotplug controller");

	/* NEC -- vendor 0x1033 */

	/* PCI to C-bus bridge */
	/* The following chipsets are PCI to PC98 C-bus bridge.
	 * The C-bus is the 16-bits bus on PC98 and it should be probed as
	 * PCI to ISA bridge.  Because class of the C-bus is not defined,
	 * C-bus bridges are recognized as "other bridge."  To make C-bus
	 * bridge be recognized as ISA bridge, this function returns NULL.
	 */
	case 0x00011033:
	case 0x002c1033:
	case 0x003b1033:
		return NULL;
	};

	if (pci_get_class(dev) == PCIC_BRIDGE &&
	    pci_get_subclass(dev) != PCIS_BRIDGE_PCI &&
	    pci_get_subclass(dev) != PCIS_BRIDGE_ISA &&
	    pci_get_subclass(dev) != PCIS_BRIDGE_EISA)
		return pci_bridge_type(dev);

	return NULL;
}

/*---------------------------------------------------------
**
**	Catchall driver for VGA devices
**
**	By Garrett Wollman
**	<wollman@halloran-eldar.lcs.mit.edu>
**
**---------------------------------------------------------
*/

const char* pci_vga_match(device_t dev)
{
	u_int id = pci_get_devid(dev);
	const char *vendor, *chip, *type;

	vendor = chip = type = 0;
	switch (id & 0xffff) {
	case 0x003d:
		vendor = "Real 3D";
		switch (id >> 16) {
		case 0x00d1:
			chip = "i740"; break;
		}
		break;
	case 0x10c8:
		vendor = "NeoMagic";
		switch (id >> 16) {
		case 0x0003:
			chip = "MagicGraph 128ZV"; break;
		case 0x0004:
			chip = "MagicGraph 128XD"; break;
		case 0x0005:
			chip = "MagicMedia 256AV"; break;
		case 0x0006:
			chip = "MagicMedia 256ZX"; break;
		}
		break;
	case 0x121a:
		vendor = "3Dfx";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x0001:
			chip = "Voodoo"; break;
		case 0x0002:
			chip = "Voodoo 2"; break;
  		case 0x0003:
			chip = "Voodoo Banshee"; break;
		case 0x0005:
			chip = "Voodoo 3"; break;
		}
		break;
	case 0x102b:
		vendor = "Matrox";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x0518:
			chip = "MGA 2085PX"; break;
		case 0x0519:
			chip = "MGA Millennium 2064W"; break;
		case 0x051a:
			chip = "MGA 1024SG/1064SG/1164SG"; break;
		case 0x051b:
			chip = "MGA Millennium II 2164W"; break;
		case 0x051f:
			chip = "MGA Millennium II 2164WA-B AG"; break;
		case 0x0520:
			chip = "MGA G200"; break;
		case 0x0521:
			chip = "MGA G200 AGP"; break;
		case 0x0525:
			chip = "MGA G400 AGP"; break;
		case 0x0d10:
			chip = "MGA Impression"; break;
		case 0x1000:
			chip = "MGA G100"; break;
		case 0x1001:
			chip = "MGA G100 AGP"; break;

		}
		break;
	case 0x1002:
		vendor = "ATI";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x4158:
			chip = "Mach32"; break;
		case 0x4354:
			chip = "Mach64-CT"; break;
		case 0x4358:
			chip = "Mach64-CX"; break;
		case 0x4554:
			chip = "Mach64-ET"; break;
		case 0x4654:
		case 0x5654:
			chip = "Mach64-VT"; break;
		case 0x4742:
			chip = "Mach64-GB"; break;
		case 0x4744:
			chip = "Mach64-GD"; break;
		case 0x4749:
			chip = "Mach64-GI"; break;
		case 0x474d:
			chip = "Mach64-GM"; break;
		case 0x474e:
			chip = "Mach64-GN"; break;
		case 0x474f:
			chip = "Mach64-GO"; break;
		case 0x4750:
			chip = "Mach64-GP"; break;
		case 0x4751:
			chip = "Mach64-GQ"; break;
		case 0x4752:
			chip = "Mach64-GR"; break;
		case 0x4753:
			chip = "Mach64-GS"; break;
		case 0x4754:
			chip = "Mach64-GT"; break;
		case 0x4755:
			chip = "Mach64-GU"; break;
		case 0x4756:
			chip = "Mach64-GV"; break;
		case 0x4757:
			chip = "Mach64-GW"; break;
		case 0x4758:
			chip = "Mach64-GX"; break;
		case 0x4c42:
			chip = "Mach64-LB"; break;
		case 0x4c46:
			chip = "Rage128-LF Mobility"; break;
		case 0x4c4d:
			chip = "Mobility-1"; break;
		case 0x475a:
			chip = "Mach64-GZ"; break;
		case 0x5245:
			chip = "Rage128-RE"; break;
		case 0x5246:
			chip = "Rage128-RF"; break;
		case 0x524b:
			chip = "Rage128-RK"; break;
		case 0x524c:
			chip = "Rage128-RL"; break;
		}
		break;
	case 0x1005:
		vendor = "Avance Logic";
		switch (id >> 16) {
		case 0x2301:
			chip = "ALG2301"; break;
		case 0x2302:
			chip = "ALG2302"; break;
		}
		break;
	case 0x100c:
		vendor = "Tseng Labs";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x3202:
		case 0x3205:
		case 0x3206:
		case 0x3207:
			chip = "ET4000 W32P"; break;
		case 0x3208:
			chip = "ET6000/ET6100"; break;
		case 0x4702:
			chip = "ET6300"; break;
		}
		break;
	case 0x100e:
		vendor = "Weitek";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x9001:
			chip = "P9000"; break;
		case 0x9100:
			chip = "P9100"; break;
		}
		break;
	case 0x1013:
		vendor = "Cirrus Logic";
		switch (id >> 16) {
		case 0x0038:
			chip = "GD7548"; break;
		case 0x0040:
			chip = "GD7555"; break;
		case 0x004c:
			chip = "GD7556"; break;
		case 0x00a0:
			chip = "GD5430"; break;
		case 0x00a4:
		case 0x00a8:
			chip = "GD5434"; break;
		case 0x00ac:
			chip = "GD5436"; break;
		case 0x00b8:
			chip = "GD5446"; break;
		case 0x00bc:
			chip = "GD5480"; break;
		case 0x00d0:
			chip = "GD5462"; break;
		case 0x00d4:
		case 0x00d5:
			chip = "GD5464"; break;
		case 0x00d6:
			chip = "GD5465"; break;
		case 0x1200:
			chip = "GD7542"; break;
		case 0x1202:
			chip = "GD7543"; break;
		case 0x1204:
			chip = "GD7541"; break;
		}
		break;
	case 0x1023:
		vendor = "Trident";
		break;		/* let default deal with it */
	case 0x102c:
		vendor = "Chips & Technologies";
		switch (id >> 16) {
		case 0x00b8:
			chip = "64310"; break;
		case 0x00d8:
			chip = "65545"; break;
		case 0x00dc:
			chip = "65548"; break;
		case 0x00c0:
			chip = "69000"; break;
		case 0x00e0:
			chip = "65550"; break;
		case 0x00e4:
			chip = "65554"; break;
		case 0x00e5:
			chip = "65555"; break;
		case 0x00f4:
			chip = "68554"; break;
                }
		break;
	case 0x1033:
		vendor = "NEC";
		switch (id >> 16) {
		case 0x0009:
			type = "PCI to PC-98 Core Graph bridge";
			break;
		}
		break;
	case 0x1039:
		vendor = "SiS";
		switch (id >> 16) {
		case 0x0001:
			chip = "86c201"; break;
		case 0x0002:
			chip = "86c202"; break;
		case 0x0205:
			chip = "86c205"; break;
		case 0x0215:
			chip = "86c215"; break;
		case 0x0225:
			chip = "86c225"; break;
		case 0x0200:
			chip = "5597/98"; break;
		case 0x6326:
			chip = "6326"; break;
		case 0x6306:
			chip = "530/620"; break;
		}
		break;
	case 0x105d:
		vendor = "Number Nine";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x2309:
			chip = "Imagine 128"; break;
		case 0x2339:
			chip = "Imagine 128 II"; break;
		}
		break;
	case 0x1142:
		vendor = "Alliance";
		switch (id >> 16) {
		case 0x3210:
			chip = "PM6410"; break;
		case 0x6422:
			chip = "PM6422"; break;
		case 0x6424:
			chip = "PMAT24"; break;
		}
		break;
	case 0x1163:
		vendor = "Rendition Verite";
		switch (id >> 16) {
		case 0x0001:
			chip = "V1000"; break;
		case 0x2000:
			chip = "V2000"; break;
		}
		break;
	case 0x1236:
		vendor = "Sigma Designs";
		if ((id >> 16) == 0x6401)
			chip = "REALmagic64/GX";
		break;
	case 0x5333:
		vendor = "S3";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x8811:
			chip = "Trio"; break;
		case 0x8812:
			chip = "Aurora 64"; break;
		case 0x8814:
			chip = "Trio 64UV+"; break;
		case 0x8901:
			chip = "Trio 64V2/DX/GX"; break;
		case 0x8902:
			chip = "Plato"; break;
		case 0x8904:
			chip = "Trio3D"; break;
		case 0x8880:
			chip = "868"; break;
		case 0x88b0:
			chip = "928"; break;
		case 0x88c0:
		case 0x88c1:
			chip = "864"; break;
		case 0x88d0:
		case 0x88d1:
			chip = "964"; break;
		case 0x88f0:
			chip = "968"; break;
		case 0x5631:
			chip = "ViRGE"; break;
		case 0x883d:
			chip = "ViRGE VX"; break;
		case 0x8a01:
			chip = "ViRGE DX/GX"; break;
		case 0x8a10:
			chip = "ViRGE GX2"; break;
		case 0x8a13:
			chip = "Trio3D/2X"; break;
		case 0x8a20:
		case 0x8a21:
			chip = "Savage3D"; break;
		case 0x8a22:
			chip = "Savage 4"; break;
		case 0x8c01:
			chip = "ViRGE MX"; break;
		case 0x8c03:
			chip = "ViRGE MX+"; break;
		}
		break;
	case 0xedd8:
		vendor = "ARK Logic";
		switch (id >> 16) {
		case 0xa091:
			chip = "1000PV"; break;
		case 0xa099:
			chip = "2000PV"; break;
		case 0xa0a1:
			chip = "2000MT"; break;
		case 0xa0a9:
			chip = "2000MI"; break;
		}
		break;
	case 0x3d3d:
		vendor = "3D Labs";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x0001:
			chip = "300SX"; break;
		case 0x0002:
			chip = "500TX"; break;
		case 0x0003:
			chip = "Delta"; break;
		case 0x0004:
			chip = "PerMedia"; break;
		}
		break;
	case 0x10de:
		vendor = "NVidia";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x0008:
			chip = "NV1"; break;
		case 0x0020:
			chip = "Riva TNT"; break;	
		case 0x0028:
			chip = "Riva TNT2"; break;
		case 0x0029:
			chip = "Riva Ultra TNT2"; break;
		case 0x002c:
			chip = "Riva Vanta TNT2"; break;
		case 0x002d:
			chip = "Riva Ultra Vanta TNT2"; break;
		case 0x00a0:
			chip = "Riva Integrated TNT2"; break;
		case 0x0100:
			chip = "GeForce 256"; break;
		case 0x0101:
			chip = "GeForce DDR"; break;
		case 0x0103:
			chip = "Quadro"; break;
		case 0x0150:
		case 0x0151:
		case 0x0152:
			chip = "GeForce2 GTS"; break;
		case 0x0153:
			chip = "Quadro2"; break;
		}
		break;
	case 0x12d2:
		vendor = "NVidia/SGS-Thomson";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x0018:
			chip = "Riva128"; break;	
		}
		break;
	case 0x104a:
		vendor = "SGS-Thomson";
		switch (id >> 16) {
		case 0x0008:
			chip = "STG2000"; break;
		}
		break;
	case 0x8086:
		vendor = "Intel";
		switch (id >> 16) {
		case 0x7121:
			chip = "82810 (i810 GMCH)"; break;
		case 0x7123:
			chip = "82810-DC100 (i810-DC100 GMCH)"; break;
		case 0x7125:
			chip = "82810E (i810E GMCH)"; break;
		case 0x7800:
			chip = "i740 AGP"; break;
		}
		break;
	case 0x10ea:
		vendor = "Intergraphics";
		switch (id >> 16) {
		case 0x1680:
			chip = "IGA-1680"; break;
		case 0x1682:
			chip = "IGA-1682"; break;
		}
		break;
	}

	if (vendor && chip) {
		char *buf;
		int len;

		if (type == 0)
			type = "SVGA controller";

		len = strlen(vendor) + strlen(chip) + strlen(type) + 4;
		MALLOC(buf, char *, len, M_TEMP, M_NOWAIT);
		if (buf)
			sprintf(buf, "%s %s %s", vendor, chip, type);
		return buf;
	}

	switch (pci_get_class(dev)) {

	case PCIC_OLD:
		if (pci_get_subclass(dev) != PCIS_OLD_VGA)
			return 0;
		if (type == 0)
			type = "VGA-compatible display device";
		break;

	case PCIC_DISPLAY:
		if (type == 0) {
			if (pci_get_subclass(dev) == PCIS_DISPLAY_VGA)
				type = "VGA-compatible display device";
			else {
				/*
				 * If it isn't a vga display device,
				 * don't pretend we found one.
				 */
				return 0;
			}
		}
		break;

	default:
		return 0;
	};
	/*
	 * If we got here, we know for sure it's some sort of display
	 * device, but we weren't able to identify it specifically.
	 * At a minimum we can return the type, but we'd like to
	 * identify the vendor and chip ID if at all possible.
	 * (Some of the checks above intentionally don't bother for
	 * vendors where we know the chip ID is the same as the
	 * model number.)
	 */
	if (vendor) {
		char *buf;
		int len;

		len = strlen(vendor) + strlen(type) + 2 + 6 + 4 + 1;
		MALLOC(buf, char *, len, M_TEMP, M_NOWAIT);
		if (buf)
			sprintf(buf, "%s model %04x %s", vendor, id >> 16, type);
		return buf;
	}
	return type;
}

/*---------------------------------------------------------
**
**	Devices to ignore
**
**---------------------------------------------------------
*/

static const char *
ign_match(device_t dev)
{
	switch (pci_get_devid(dev)) {

	case 0x10001042ul:	/* wd */
		return ("SMC FDC 37c665");
	};

	return NULL;
}

static int
ign_probe(device_t dev)
{
	const char *s;

	s = ign_match(dev);
	if (s) {
		device_set_desc(dev, s);
		device_quiet(dev);
		return -10000;
	}
	return ENXIO;
}

static int
ign_attach(device_t dev)
{
	return 0;
}

static device_method_t ign_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ign_probe),
	DEVMETHOD(device_attach,	ign_attach),

	{ 0, 0 }
};

static driver_t ign_driver = {
	"ign",
	ign_methods,
	1,
};

static devclass_t ign_devclass;

DRIVER_MODULE(ign, pci, ign_driver, ign_devclass, 0, 0);
