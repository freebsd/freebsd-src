/**************************************************************************
**
**  $Id: pci_intel.c,v 1.1 94/09/05 22:38:38 wolf Exp $
**
**  Device driver for INTEL PCI chipsets.
**
**  386bsd / FreeBSD
**
**-------------------------------------------------------------------------
**
**  Written for 386bsd and FreeBSD by
**	wolf@dentaro.gun.de	Wolfgang Stanglmeier
**	se@mi.Uni-Koeln.de	Stefan Esser
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
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
**-------------------------------------------------------------------------
**
**  $Log:	pci_intel.c,v $
 * Revision 1.1  94/09/05  22:38:38  wolf
 * Initial revision
 * 
 * Revision 1.1  94/09/04  16:05:20  wolf
 * Initial revision
 * 
***************************************************************************
*/


/*==========================================================
**
**      Include files
**
**==========================================================
*/

#include <sys/types.h>

#include <i386/pci/pci.h>
#include <i386/pci/pcibios.h>
#include <i386/pci/pci_device.h>

static	int	probe1(pcici_t config_id);
static	int	return0(int unit);
static	int	intel_attach(pcici_t config_id, pcidi_t type);
static	int	intel_82424zx_attach(pcici_t config_id, pcidi_t type);
static	int	intel_82434lx_attach(pcici_t config_id, pcidi_t type);
extern	void	printf();
static	char	confread(pcici_t config_id, int port);

struct condmsg {
    unsigned char	port;
    unsigned char	mask;
    unsigned char	value;
    char	flags;
    char       *text;
};

#define M_EQ 0	/* mask and return true if equal */
#define M_NE 1	/* mask and return true if not equal */
#define TRUE 2	/* don't read config, always true */


struct	pci_driver intel82378_device = {
	probe1,
	intel_attach,
	0x04848086,
	0,
	"intel 82378IB pci-isa bridge",
	return0
};

struct	pci_driver intel82424_device = {
	probe1,
	intel_82424zx_attach,
	0x04838086,
	0,
	"intel 82424ZX cache dram controller",
	return0
};

struct	pci_driver intel82375_device = {
	probe1,
	intel_attach,
	0x04828086,
	0,
	"intel 82375EB pci-eisa bridge",
	return0
};

struct	pci_driver intel82434_device = {
	probe1,
	intel_82434lx_attach,
	0x04a38086,
	0,
	"intel 82434LX pci cache memory controller",
	return0
};

struct condmsg conf82424zx[] =
{
    { 0x00, 0x00, 0x00, TRUE, "\tCPU: " },
    { 0x50, 0xe0, 0x00, M_EQ, "486DX" },
    { 0x50, 0xe0, 0x20, M_EQ, "486SX" },
    { 0x50, 0xe0, 0x40, M_EQ, "486DX2 or 486DX4" },
    { 0x50, 0xe0, 0x80, M_EQ, "Overdrive (writeback)" },

    { 0x00, 0x00, 0x00, TRUE, ", bus=" },
    { 0x50, 0x03, 0x00, M_EQ, "25MHz" },
    { 0x50, 0x03, 0x01, M_EQ, "33MHz" },
    { 0x53, 0x01, 0x01, TRUE, ", CPU->Memory posting "},
    { 0x53, 0x01, 0x00, M_EQ, "OFF" },
    { 0x53, 0x01, 0x01, M_EQ, "ON" },

    { 0x56, 0x30, 0x00, M_NE, "\t\nWarning:" },
    { 0x56, 0x20, 0x00, M_NE, " NO cache parity!" },
    { 0x56, 0x10, 0x00, M_NE, " NO DRAM parity!" },
    { 0x55, 0x04, 0x04, M_EQ, "\nWarning: refresh OFF! " },

    { 0x00, 0x00, 0x00, TRUE, "\t\nCache: " },
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

    { 0x00, 0x00, 0x00, TRUE, "\t\nDRAM:" },
    { 0x55, 0x43, 0x00, M_NE, " page mode" },
    { 0x55, 0x02, 0x02, M_EQ, " code fetch" },
    { 0x55, 0x43, 0x43, M_EQ, "," },
    { 0x55, 0x43, 0x42, M_EQ, " and" },
    { 0x55, 0x40, 0x40, M_EQ, " read" },
    { 0x55, 0x03, 0x03, M_EQ, " and" },
    { 0x55, 0x43, 0x41, M_EQ, " and" },
    { 0x55, 0x01, 0x01, M_EQ, " write" },
    { 0x55, 0x43, 0x00, M_NE, "," },

    { 0x00, 0x00, 0x00, TRUE, " memory clocks=" },
    { 0x55, 0x20, 0x00, M_EQ, "X-2-2-2" },
    { 0x55, 0x20, 0x20, M_EQ, "X-1-2-1" },

    { 0x00, 0x00, 0x00, TRUE, "\t\nPCI: CPU->PCI posting " },
    { 0x53, 0x02, 0x02, M_EQ, "ON" },
    { 0x53, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, TRUE, ", CPU->PCI burst mode " },
    { 0x54, 0x02, 0x02, M_EQ, "ON" },
    { 0x54, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, TRUE, ", PCI->Mem. posting " },
    { 0x54, 0x01, 0x01, M_EQ, "ON" },
    { 0x54, 0x01, 0x00, M_EQ, "OFF" },

    { 0x00, 0x00, 0x00, TRUE, "\n" },

/* end marker */
    { 0 }
};

struct condmsg conf82434lx[] =
{
    { 0x00, 0x00, 0x00, TRUE, "\tCPU: " },
    { 0x50, 0xe0, 0x80, M_EQ, "Pentium" },
    { 0x50, 0xe0, 0x80, M_NE, "???" },
    { 0x50, 0x02, 0x00, M_EQ, ", ???MHz" },
    { 0x50, 0x03, 0x02, M_EQ, ", 60MHz" },
    { 0x50, 0x03, 0x03, M_EQ, ", 66MHz" },
    { 0x50, 0x04, 0x00, M_EQ, " (primary cache OFF)" },

    { 0x53, 0x01, 0x01, TRUE, ", CPU->Memory posting "},
    { 0x53, 0x01, 0x00, M_EQ, "OFF" },
    { 0x53, 0x01, 0x01, M_NE, "ON" },

    { 0x53, 0x04, 0x00, M_NE, ", read around write"},

    { 0x71, 0xc0, 0x00, M_NE, "\t\nWarning: NO cache parity!" },
    { 0x57, 0x20, 0x00, M_NE, "\t\nWarning: NO DRAM parity!" },
    { 0x55, 0x01, 0x01, M_EQ, "\t\nWarning: refresh OFF! " },

    { 0x00, 0x00, 0x00, TRUE, "\t\nCache: " },
    { 0x52, 0x01, 0x00, M_EQ, "None" },
    { 0x52, 0x81, 0x01, M_EQ, "" },
    { 0x52, 0xc1, 0x81, M_EQ, "256KB" },
    { 0x52, 0xc1, 0xc1, M_EQ, "512KB" },
    { 0x52, 0x03, 0x01, M_EQ, " writethrough" },
    { 0x52, 0x03, 0x03, M_EQ, " writeback" },

    { 0x52, 0x01, 0x01, M_EQ, ", cache clocks=" },
    { 0x52, 0x20, 0x00, M_EQ, "3-2-2-2/4-2-2-2" },
    { 0x52, 0x20, 0x00, M_NE, "3-1-1-1" },

    { 0x00, 0x00, 0x00, TRUE, "\t\nDRAM:" },
    { 0x57, 0x10, 0x00, M_EQ, " page mode" },

    { 0x00, 0x00, 0x00, TRUE, " memory clocks=" },
    { 0x57, 0xc0, 0x00, M_EQ, "X-4-4-4 (70ns)" },
    { 0x57, 0xc0, 0x40, M_EQ, "X-4-4-4/X-3-3-3 (60ns)" },
    { 0x57, 0xc0, 0x80, M_EQ, "???" },
    { 0x57, 0xc0, 0xc0, M_EQ, "X-3-3-3 (50ns)" },

    { 0x00, 0x00, 0x00, TRUE, "\t\nPCI: CPU->PCI posting " },
    { 0x53, 0x02, 0x02, M_EQ, "ON" },
    { 0x53, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, TRUE, ", CPU->PCI burst mode " },
    { 0x54, 0x02, 0x00, M_NE, "ON" },
    { 0x54, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, TRUE, ", PCI->Memory posting " },
    { 0x54, 0x01, 0x00, M_NE, "ON" },
    { 0x54, 0x01, 0x00, M_EQ, "OFF" },
    { 0x54, 0x04, 0x00, TRUE, ", PCI clocks=" },
    { 0x54, 0x04, 0x00, M_EQ, "2-2-2-2" },
    { 0x54, 0x04, 0x00, M_NE, "2-1-1-1" },

    { 0x00, 0x00, 0x00, TRUE, "\n" },

/* end marker */
    { 0 }
};

int return0(int unit)
{
	return (0);
}

int probe1(pcici_t config_id)
{
	return (1);
}

static char confread (pcici_t config_id, int port)
{
    unsigned long portw = port & -3;
    unsigned long ports = (port - portw) << 3;

    unsigned long l = pci_conf_read (config_id, portw);
    return (l >> ports);
}

static void writeconfig(pcici_t config_id, struct condmsg *tbl)
{
    while (tbl->text) {
	int cond = 0;
	if (tbl->flags == TRUE) {
	    cond = 1;
	} else {
	    unsigned char v = (unsigned char) confread(config_id, tbl->port);
	    switch (tbl->flags) {
    case M_EQ:
		if ((v & tbl->mask) == tbl->value) cond = 1;
		break;
    case M_NE:
		if ((v & tbl->mask) != tbl->value) cond = 1;
		break;
    default:
	    }
	}
	if (cond) printf ("%s", tbl->text);
	tbl++;
    }
}

int	intel_attach(pcici_t config_id, pcidi_t type)
{
	printf ("        [40] %x [50] %x [54] %x\n",
		pci_conf_read (config_id, 0x40),
		pci_conf_read (config_id, 0x50),
		pci_conf_read (config_id, 0x54));
	return(0);
}

int intel_82424zx_attach(pcici_t config_id, pcidi_t type)
{
	writeconfig (config_id, conf82424zx);
	return (0);
}

int	intel_82434lx_attach(pcici_t config_id, pcidi_t type)
{
	writeconfig (config_id, conf82424zx);
	return (0);
}

