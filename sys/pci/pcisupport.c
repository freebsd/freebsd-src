/**************************************************************************
**
**  $Id: pcisupport.c,v 1.97 1999/04/17 19:48:45 dfr Exp $
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
#include "opt_smp.h"
#include "intpm.h"
#include "alpm.h"

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

static char*
generic_pci_bridge (pcici_t tag)
{
    char *descr, tmpbuf[120];
    unsigned classreg = pci_conf_read (tag, PCI_CLASS_REG);

    if ((classreg & PCI_CLASS_MASK) == PCI_CLASS_BRIDGE) {

	unsigned id = pci_conf_read (tag, PCI_ID_REG);

	switch (classreg >> 16 & 0xff) {
		case 0:	strcpy(tmpbuf, "Host to PCI"); break;
		case 1:	strcpy(tmpbuf, "PCI to ISA"); break;
		case 2:	strcpy(tmpbuf, "PCI to EISA"); break;
		case 4:	strcpy(tmpbuf, "PCI to PCI"); break;
		case 5:	strcpy(tmpbuf, "PCI to PCMCIA"); break;
		case 7:	strcpy(tmpbuf, "PCI to CardBus"); break;
		default: 
			snprintf(tmpbuf, sizeof(tmpbuf),
			    "PCI to 0x%x", classreg>>16 & 0xff); 
			break;
	}
	snprintf(tmpbuf+strlen(tmpbuf), sizeof(tmpbuf)-strlen(tmpbuf),
	    " bridge (vendor=%04x device=%04x)",
	    id & 0xffff, (id >> 16) & 0xffff);
	descr = malloc (strlen(tmpbuf) +1, M_DEVBUF, M_WAITOK);
	strcpy(descr, tmpbuf);
	return descr;
    }
    return 0;
}

/*
 * XXX Both fixbushigh_orion() and fixbushigh_i1225() are bogus in that way,
 * that they store the highest bus number to scan in this device's config 
 * data, though it is about PCI buses attached to the CPU independently!
 * The same goes for fixbushigh_450nx.
 */

static void
fixbushigh_orion(device_t dev)
{
	pci_set_secondarybus(dev, pci_read_config(dev, 0x4a, 1));
	pci_set_subordinatebus(dev, pci_read_config(dev, 0x4b, 1));
}

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


/*
 * This reads the PCI config space for the 82451NX MIOC in the 450NX
 * chipset to determine the PCI bus configuration.
 *
 * Assuming the BIOS has set up the MIOC properly, this will correctly
 * report the number of PCI busses in the system.
 *
 * A small problem is that the Host to PCI bridge control is in the MIOC,
 * while the host-pci bridges are separate PCI devices.  So it really
 * isn't easily possible to set up the subordinatebus mappings as the
 * 82454NX PCI expander bridges are probed, although that makes the
 * most sense.
 */
static void
fixbushigh_450nx(device_t dev)
{
	int subordinatebus;
	unsigned long devmap;

	/*
	 * Read the DEVMAP field, so we know which fields to check.
	 * If the Host-PCI bridge isn't marked as present by the BIOS,
	 * we have to assume it doesn't exist.
	 * If this doesn't find all the PCI busses, complain to the
	 * BIOS vendor.  There is nothing more we can do.
	 */
	devmap = pci_read_config(dev, 0xd6, 2) & 0x3c;
	if (!devmap)
		panic("450NX MIOC: No host to PCI bridges marked present.\n");
	/*
	 * Since the buses are configured in order, we just have to
	 * find the highest bus, and use those numbers.
	 */
	if (devmap & 0x20) {			/* B1 */
		subordinatebus = pci_read_config(dev, 0xd5, 1);
	} else if (devmap & 0x10) {		/* A1 */
		subordinatebus = pci_read_config(dev, 0xd4, 1);
	} else if (devmap & 0x8) {		/* B0 */
		subordinatebus = pci_read_config(dev, 0xd2, 1);
	} else /* if (devmap & 0x4) */ {	/* A0 */
		subordinatebus = pci_read_config(dev, 0xd1, 1);
	}
	if (subordinatebus == 255) {
		printf("fixbushigh_450nx: bogus highest PCI bus %d",
		       subordinatebus);
#ifdef NBUS
		subordinatebus = NBUS - 2;
#else
		subordinatebus = 10;
#endif
		printf(", reduced to %d\n", subordinatebus);
	}
		
	if (bootverbose)
		printf("fixbushigh_450nx: subordinatebus is %d\n",
			subordinatebus);

	pci_set_secondarybus(dev, subordinatebus);
	pci_set_subordinatebus(dev, subordinatebus);
}

static void
fixbushigh_Ross(device_t dev)
{
	int secondarybus;

	/* just guessing the secondary bus register number ... */
	secondarybus = pci_read_config(dev, 0x45, 1);
	if (secondarybus != 0) {
		pci_set_secondarybus(dev, secondarybus + 1);
		pci_set_subordinatebus(dev, secondarybus + 1);
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
	case 0x84cb8086:
		return ("Intel 82454NX PCI Expander Bridge");
	case 0x00221014:
		return ("IBM 82351 PCI-PCI bridge");
	case 0x00011011:
		return ("DEC 21050 PCI-PCI bridge");
	case 0x124b8086:
		return ("Intel 82380FB mobile PCI to PCI bridge");
	
	/* VLSI -- vendor 0x1004 */
	case 0x01021004:
		return ("VLSI 82C534 Eagle II PCI Bus bridge");
	case 0x01031004:
		return ("VLSI 82C538 Eagle II PCI Docking bridge");

	/* XXX Here is MVP3, I got the datasheet but NO M/B to test it  */
	/* totally. Please let me know if anything wrong.            -F */
	case 0x85981106:
		return("VIA 82C598MVP (Apollo MVP3) PCI-PCI bridge");

	/* AcerLabs -- vendor 0x10b9 */
	/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
	/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x524710b9:
		return("AcerLabs M5247 PCI-PCI(AGP Supported) bridge");
	case 0x524310b9:/* 5243 seems like 5247, need more info to divide*/
		return("AcerLabs M5243 PCI-PCI bridge");

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
		return 0;
	}

	return ENXIO;
}

static int pcib_attach(device_t dev)
{
	struct pci_devinfo *dinfo;
	pcicfgregs *cfg;

	dinfo = device_get_ivars(dev);
	cfg = &dinfo->cfg;

	chipset_attach(dev, device_get_unit(dev));

	if (cfg->secondarybus) {
		device_add_child(dev, "pci", cfg->secondarybus, 0);
		return bus_generic_attach(dev);
	} else
		return 0;
}

static device_method_t pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcib_probe),
	DEVMETHOD(device_attach,	pcib_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

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

static driver_t pcib_driver = {
	"pcib",
	pcib_methods,
	DRIVER_TYPE_MISC,
	1,
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
	
	/* VLSI -- vendor 0x1004 */
	case 0x00061004:
		return ("VLSI 82C593 PCI to ISA bridge");

	/* VIA Technologies -- vendor 0x1106 
	 * Note that the old Apollo Master chipset is not in here, as VIA
	 * does not seem to have any docs on their website for it, and I do
	 * not have a Master board in my posession. -LC */

	case 0x05861106: /* south bridge section -- IDE is covered in ide_pci.c */
		return("VIA 82C586 PCI-ISA bridge");

	/* AcerLabs -- vendor 0x10b9 */
	/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
	/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x153310b9:
		return("AcerLabs M1533 portable PCI-ISA bridge");
	case 0x154310b9:
		return("AcerLabs M1543 desktop PCI-ISA bridge");

	/* SiS -- vendor 0x1039 */
	case 0x00081039:
		return ("SiS 85c503 PCI-ISA bridge");
	}

	if (pci_get_class(dev) == PCIC_BRIDGE
	    && (pci_get_subclass(dev) == PCIS_BRIDGE_ISA
		|| pci_get_subclass(dev) == PCIS_BRIDGE_EISA))
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
		device_set_desc_copy(dev, desc);

		/* In case of a generic EISA bridge */
		if (pci_get_subclass(dev) == PCIS_BRIDGE_EISA)
			is_eisa = 1;

		/* For PCI-EISA bridge, add both eisa and isa */
		/* Don't bother adding more than one EISA bus */
		if (is_eisa && !devclass_get_device(devclass_find("isa"), 0))
			device_add_child(dev, "eisa", -1, 0);

		/* Don't bother adding more than one ISA bus */
		if (!devclass_get_device(devclass_find("isa"), 0))
			device_add_child(dev, "isa", -1, 0);
		return 0;
	}

	return ENXIO;
}

static device_method_t isab_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		isab_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

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
	DRIVER_TYPE_MISC,
	1,
};

static devclass_t isab_devclass;

DRIVER_MODULE(isab, pci, isab_driver, isab_devclass, 0, 0);

static const char*
chip_match(device_t dev)
{
	unsigned	rev;

	switch (pci_get_devid(dev)) {
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
		return ("Intel 824?? host to PCI bridge");
	case 0x71908086:
		return ("Intel 82443BX host to PCI bridge");
	case 0x71918086:
		return ("Intel 82443BX host to AGP bridge");
	case 0x71928086:
		return ("Intel 82443BX host to PCI bridge (AGP disabled)");
	case 0x84c48086:
		fixbushigh_orion(dev);
		return ("Intel 82454KX/GX (Orion) host to PCI bridge");
	case 0x84ca8086:
		fixbushigh_450nx(dev);
		return ("Intel 82451NX Memory and I/O Controller");
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
	case 0x12348086:
		return ("Intel 82371MX mobile PCI I/O IDE accelerator (MPIIX)");
	case 0x12358086:
		return ("Intel 82437MX mobile PCI cache memory controller");
	case 0x12508086:
		return ("Intel 82439");
	case 0x70308086:
		return ("Intel 82437VX PCI cache memory controller");
	case 0x71008086:
		return ("Intel 82439TX System Controller (MTXC)");

	case 0x71138086:
		return ("Intel 82371AB Power management controller");
	case 0x12378086:
		fixwsc_natoma(dev);
		return ("Intel 82440FX (Natoma) PCI and memory controller");
	case 0x84c58086:
		return ("Intel 82453KX/GX (Orion) PCI memory controller");
	/* SiS -- vendor 0x1039 */
	case 0x04961039:
		return ("SiS 85c496");
	case 0x04061039:
		return ("SiS 85c501");
	case 0x06011039:
		return ("SiS 85c601");
	
	/* VLSI -- vendor 0x1004 */
	case 0x00051004:
		return ("VLSI 82C592 Host to PCI bridge");
	case 0x01011004:
		return ("VLSI 82C532 Eagle II Peripheral Controller");
	case 0x01041004:
		return ("VLSI 82C535 Eagle II System Controller");
	case 0x01051004:
		return ("VLSI 82C147 IrDA Controller");

	/* VIA Technologies -- vendor 0x1106 
	 * Note that the old Apollo Master chipset is not in here, as VIA
	 * does not seem to have any docs on their website for it, and I do
	 * not have a Master board in my posession. -LC */

	case 0x05851106:
		return("VIA 82C585 (Apollo VP1/VPX) system controller");
	case 0x05951106:
	case 0x15951106:
		return("VIA 82C595 (Apollo VP2) system controller");
	case 0x05971106:
		return("VIA 82C597 (Apollo VP3) system controller");
	/* XXX Here is MVP3, I got the datasheet but NO M/B to test it  */
	/* totally. Please let me know if anything wrong.            -F */
	/* XXX need info on the MVP3 -- any takers? */
	case 0x05981106:
		return("VIA 82C598MVP (Apollo MVP3) host bridge");
	case 0x30401106:
		return("VIA 82C586B ACPI interface");
#if 0
	/* XXX New info added-in */
        case 0x05711106:
		return("VIA 82C586B IDE controller");
	case 0x30381106:
		return("VIA 82C586B USB controller");
#endif

	/* NEC -- vendor 0x1033 */
	case 0x00011033:
		return ("NEC 0001 PCI to PC-98 C-bus bridge");
	case 0x00021033:
		return ("NEC 0002 PCI to PC-98 local bus bridge");
	case 0x00161033:
		return ("NEC 0016 PCI to PC-98 local bus bridge");
	case 0x002c1033:
		return ("NEC 002C PCI to PC-98 C-bus bridge");
	case 0x003b1033:
		return ("NEC 003B PCI to PC-98 C-bus bridge");

	/* AcerLabs -- vendor 0x10b9 */
	/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
	/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x154110b9:
		return("AcerLabs M1541 (Aladdin-V) PCI host bridge");
	};

	if (pci_get_class(dev) == PCIC_BRIDGE
	    && pci_get_subclass(dev) != PCIS_BRIDGE_PCI
	    && pci_get_subclass(dev) != PCIS_BRIDGE_ISA
	    && pci_get_subclass(dev) != PCIS_BRIDGE_EISA)
		return pci_bridge_type(dev);

	return NULL;
}

static int chip_probe(device_t dev)
{
	const char *desc;

	desc = chip_match(dev);
	if (desc) {
		device_set_desc_copy(dev, desc);
		return 0;
	}

	return ENXIO;
}

static int chip_attach(device_t dev)
{
	chipset_attach(dev, device_get_unit(dev));

	return 0;
}

static device_method_t chip_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		chip_probe),
	DEVMETHOD(device_attach,	chip_attach),

	{ 0, 0 }
};

static driver_t chip_driver = {
	"chip",
	chip_methods,
	DRIVER_TYPE_MISC,
	1,
};

static devclass_t chip_devclass;

DRIVER_MODULE(chip, pci, chip_driver, chip_devclass, 0, 0);

/*---------------------------------------------------------
**
**	Catchall driver for VGA devices
**
**	By Garrett Wollman
**	<wollman@halloran-eldar.lcs.mit.edu>
**
**---------------------------------------------------------
*/

static const char* vga_match(device_t dev)
{
	/* int data = pci_conf_read(tag, PCI_CLASS_REG); */
	u_int id = pci_get_devid(dev);
	const char *vendor, *chip, *type;

	return 0;		/* XXX interferes with syscons */

	vendor = chip = type = 0;
	switch (id) {
	case 0x10c8:
		vendor = "NeoMagic";
		switch (id >> 16) {
		case 0x0004:
			chip = "NM2160 laptop";	break;
		}
		break;
	case 0x102b:
		vendor = "Matrox";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x0518:
			chip = "MGA 2085PX"; break;
		case 0x0519:
			chip = "MGA 2064W"; break;
		case 0x051a:
			chip = "MGA 1024SG/1064SG/1164SG"; break;
		case 0x051b:
			chip = "MGA 2164W"; break;
		}
		break;

	case 0x1002:
		vendor = "ATI";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x4158:
			chip = "Mach32"; break;
		case 0x4758:
			chip = "Mach64-GX"; break;
		case 0x4358:
			chip = "Mach64-CX"; break;
		case 0x4354:
			chip = "Mach64-CT"; break;
		case 0x4554:
			chip = "Mach64-ET"; break;
		case 0x5654:
			chip = "Mach64-VT"; break;
		case 0x4754:
			chip = "Mach64-GT"; break;
		}
		break;
	case 0x1005:
		vendor = "Avance Logic";
		switch (id >> 16) {
		case 0x2301:
			chip = "ALG2301"; break;
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
			chip = "ET6000"; break;
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
		case 0x00a0:
			chip = "GD5430"; break;
		case 0x00a4:
		case 0x00a8:
			chip = "GD5434"; break;
		case 0x00ac:
			chip = "GD5436"; break;
		case 0x00b8:
			chip = "GD5446"; break;
		case 0x00d0:
			chip = "GD5462"; break;
		case 0x00d4:
			chip = "GD5464"; break;
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
		if ((id >> 16) == 0x00d8)
			chip = "65545";
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
		}
		break;
	case 0x105d:
		vendor = "Number Nine";
		type = "graphics accelerator";
		switch (id >> 16) {
		case 0x2309:
		case 0x2339:
			chip = "Imagine 128"; break;
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
	case 0x1236:
		vendor = "Sigma Designs";
		if ((id >> 16) == 0x6401)
			chip = "64GX";
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
		case 0x8901:
			chip = "Trio 64"; break;
		case 0x8902:
			chip = "Plato"; break;
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
	}

	if (vendor && chip) {
		char *buf;
		int len;

		if (type == 0) {
			type = "SVGA controller";
		}

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
			if (pci_get_subclass(dev) != PCIS_DISPLAY_VGA)
				type = "VGA-compatible display device";
			else {
				/*
				 * If it isn't a vga display device,
				 * don't pretend we found one.
				 */
				type = "Display device";
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

static int vga_probe(device_t dev)
{
	const char *desc;

	desc = vga_match(dev);
	if (desc) {
		device_set_desc(dev, desc);
		return 0;
	}

	return ENXIO;
}

static int vga_attach(device_t dev)
{
/*
**	If the assigned addresses are remapped,
**	the console driver has to be informed about the new address.
*/
#if 0
	vm_offset_t va;
	vm_offset_t pa;
	int reg;
	for (reg = PCI_MAP_REG_START; reg < PCI_MAP_REG_END; reg += 4)
		(void) pci_map_mem (tag, reg, &va, &pa);
#endif
	return 0;
}

static device_method_t vga_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vga_probe),
	DEVMETHOD(device_attach,	vga_attach),

	{ 0, 0 }
};

static driver_t vga_driver = {
	"vga",
	vga_methods,
	DRIVER_TYPE_MISC,
	1,
};

static devclass_t vga_devclass;

DRIVER_MODULE(vga, pci, vga_driver, vga_devclass, 0, 0);

/*---------------------------------------------------------
**
**	Devices to ignore
**
**---------------------------------------------------------
*/

static int
ign_probe (device_t dev)
{
	switch (pci_get_devid(dev)) {

	case 0x10001042ul:	/* wd */
		return 0;
/*		return ("SMC FDC 37c665");*/
	};
	return ENXIO;
}

static int
ign_attach (device_t dev)
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
	DRIVER_TYPE_MISC,
	1,
};

static devclass_t ign_devclass;

DRIVER_MODULE(ign, pci, ign_driver, ign_devclass, 0, 0);
