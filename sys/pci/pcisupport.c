/**************************************************************************
**
**  $Id: pcisupport.c,v 1.39 1996/09/09 06:09:45 rgrimes Exp $
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

static void config_orion (pcici_t tag);

/*---------------------------------------------------------
**
**	Intel chipsets for 486 / Pentium processor
**
**---------------------------------------------------------
*/

static	char*	chipset_probe (pcici_t tag, pcidi_t type);
static	void	chipset_attach(pcici_t tag, int unit);
static	u_long	chipset_count;

static struct pci_device chipset_device = {
	"chip",
	chipset_probe,
	chipset_attach,
	&chipset_count,
	NULL
};

DATA_SET (pcidevice_set, chipset_device);

struct condmsg {
    unsigned char	port;
    unsigned char	mask;
    unsigned char	value;
    char		flags;
    const char		*text;
};

/* make sure formats expand to at least as many chars !!! */
#define PPB_DESCR "generic PCI bridge (vendor=%04x device=%04x subclass=%1.2d)"

static char*
generic_pci_bridge (pcici_t tag)
{
    char *descr;
    unsigned classreg = pci_conf_read (tag, PCI_CLASS_REG);

    if ((classreg & PCI_CLASS_MASK) == PCI_CLASS_BRIDGE) {

	unsigned id = pci_conf_read (tag, PCI_ID_REG);

	descr = malloc (sizeof PPB_DESCR +1, M_DEVBUF, M_WAITOK);
	if (descr) {
	    sprintf (descr, PPB_DESCR, id & 0xffff, (id >> 16) & 0xffff, 
			(classreg >> 16) & 0xff);
	}
	return descr;
    }
    return 0;
}


static char*
chipset_probe (pcici_t tag, pcidi_t type)
{
	unsigned	rev;
	char		*descr;

	switch (type) {
	case 0x04868086:
		return ("Intel 82425EX PCI system controller");
	case 0x04848086:
		rev = (unsigned) pci_conf_read (tag, PCI_CLASS_REG) & 0xff;
		if (rev == 3)
		    return ("Intel 82378ZB PCI-ISA bridge");
		return ("Intel 82378IB PCI-ISA bridge");
	case 0x04838086:
		return ("Intel 82424ZX (Saturn) cache DRAM controller");
	case 0x04828086:
		return ("Intel 82375EB PCI-EISA bridge");
	case 0x04961039:
		return ("SiS 85c496");
	case 0x04a38086:
		rev = (unsigned) pci_conf_read (tag, PCI_CLASS_REG) & 0xff;
		if (rev == 16 || rev == 17)
		    return ("Intel 82434NX (Neptune) PCI cache memory controller");
		return ("Intel 82434LX (Mercury) PCI cache memory controller");
	case 0x122d8086:
		return ("Intel 82437FX PCI cache memory controller");
	case 0x122e8086:
		return ("Intel 82371FB PCI-ISA bridge");
	case 0x12308086:
		return ("Intel 82371FB IDE interface");
	case 0x12508086:
		return ("Intel 82439");
	case 0x04061039:
		return ("SiS 85c501");
	case 0x00081039:
		return ("SiS 85c503");
	case 0x06011039:
		return ("SiS 85c601");
	case 0x70008086:
		return ("Intel 82371SB PCI-ISA bridge");
	case 0x70108086:
		return ("Intel 82371SB IDE interface");
	case 0x12378086:
		return ("Intel 82440FX (Natoma) PCI and memory controller");
	case 0x84c48086:
		return ("Intel 82450KX (Orion) PCI memory controller");
	case 0x84c58086:
		return ("Intel 82454GX (Orion) host to PCI bridge");
	case 0x00221014:
		return ("IBM 82351 PCI-PCI bridge");
	case 0x00011011:
		return ("DEC 21050 PCI-PCI bridge");
	};

	if (descr = generic_pci_bridge(tag))
		return descr;

	return NULL;
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

static char confread (pcici_t config_id, int port)
{
    unsigned long portw = port & ~3;
    unsigned long ports = (port - portw) << 3;

    unsigned long l = pci_conf_read (config_id, portw);
    return (l >> ports);
}

static void
writeconfig (pcici_t config_id, const struct condmsg *tbl)
{
    while (tbl->flags != M_XX) {
	const char *text = 0;

	if (tbl->flags == M_TR) {
	    text = tbl->text;
	} else {
	    unsigned char v = (unsigned char) confread(config_id, tbl->port);
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
dumpconfigspace (pcici_t tag)
{
    int reg;
    printf ("configuration space registers:");
    for (reg = 0; reg < 0x100; reg+=4) {
	if ((reg & 0x0f) == 0) 
	    printf ("\n%02x:\t", reg);
	printf ("%08x ", pci_conf_read (tag, reg));
    }
    printf ("\n");
}
#endif /* DUMPCONFIGSPACE */

#endif /* PCI_QUIET */

extern unsigned pciroots;

static void
config_orion (pcici_t tag)
{
    unsigned busno = (pci_conf_read (tag, 0x48) >> 16) & 0xff;

    if (busno > 0) {
	pciroots++;
    }
}

static void
chipset_attach (pcici_t config_id, int unit)
{
	switch (pci_conf_read (config_id, PCI_ID_REG)) {

	case 0x84c48086: /* Intel Orion */
		config_orion (config_id);
		break;
	}
#ifndef PCI_QUIET
	if (!bootverbose)
		return;

	switch (pci_conf_read (config_id, PCI_ID_REG)) {
	case 0x04868086:
		writeconfig (config_id, conf82425ex);
		break;
	case 0x04838086:
		writeconfig (config_id, conf82424zx);
		break;
	case 0x04a38086:
		writeconfig (config_id, conf82434lx);
		break;
	case 0x04848086:
		writeconfig (config_id, conf82378);
		break;
	case 0x122d8086:
		writeconfig (config_id, conf82437fx);
		break;
	case 0x122e8086:
		writeconfig (config_id, conf82371fb);
		break;
	case 0x12308086:
		writeconfig (config_id, conf82371fb2);
		break;
#if 0
	case 0x00011011: /* DEC 21050 */
	case 0x00221014: /* IBM xxx */
		writeconfig (config_id, conf_pci2pci);
		break;
#endif
	};
#endif /* PCI_QUIET */
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

static	char*	vga_probe  (pcici_t tag, pcidi_t type);
static	void	vga_attach (pcici_t tag, int unit);
static	u_long	vga_count;

static struct pci_device vga_device = {
	"vga",
	vga_probe,
	vga_attach,
	&vga_count,
	NULL
};

DATA_SET (pcidevice_set, vga_device);

static char* vga_probe (pcici_t tag, pcidi_t type)
{
	int data = pci_conf_read(tag, PCI_CLASS_REG);

	switch (data & PCI_CLASS_MASK) {

	case PCI_CLASS_PREHISTORIC:
		if ((data & PCI_SUBCLASS_MASK)
			!= PCI_SUBCLASS_PREHISTORIC_VGA)
			break;

	case PCI_CLASS_DISPLAY:
		if ((data & PCI_SUBCLASS_MASK)
		    == PCI_SUBCLASS_DISPLAY_VGA)
			return ("VGA-compatible display device");
		else
			return ("Display device");
	};
	return ((char*)0);
}

static void vga_attach (pcici_t tag, int unit)
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
}

/*---------------------------------------------------------
**
**	Hook for loadable pci drivers
**
**---------------------------------------------------------
*/

static	char*	lkm_probe  (pcici_t tag, pcidi_t type);
static	void	lkm_attach (pcici_t tag, int unit);
static	u_long	lkm_count;

static struct pci_device lkm_device = {
	"lkm",
	lkm_probe,
	lkm_attach,
	&lkm_count,
	NULL
};

DATA_SET (pcidevice_set, lkm_device);

static char*
lkm_probe (pcici_t tag, pcidi_t type)
{
	/*
	**	Not yet!
	**	(Should try to load a matching driver)
	*/
	return ((char*)0);
}

static void
lkm_attach (pcici_t tag, int unit)
{}

/*---------------------------------------------------------
**
**	Devices to ignore
**
**---------------------------------------------------------
*/

static	char*	ign_probe  (pcici_t tag, pcidi_t type);
static	void	ign_attach (pcici_t tag, int unit);
static	u_long	ign_count;

static struct pci_device ign_device = {
	NULL,
	ign_probe,
	ign_attach,
	&ign_count,
	NULL
};

DATA_SET (pcidevice_set, ign_device);

static char*
ign_probe (pcici_t tag, pcidi_t type)
{
	switch (type) {

	case 0x10001042ul:	/* wd */
		return ("");
/*		return ("SMC FDC 37c665");*/
	};
	return ((char*)0);
}

static void
ign_attach (pcici_t tag, int unit)
{}
