/**************************************************************************
**
**  $Id: pcisupport.c,v 1.8 1995/02/02 13:12:18 davidg Exp $
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
** Copyright (c) 1994 Stefan Esser.  All rights reserved.
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


/*==========================================================
**
**      Include files
**
**==========================================================
*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

extern	void	printf();

extern	int	bootverbose;


/*---------------------------------------------------------
**
**	Intel chipsets for 486 / Pentium processor
**
**---------------------------------------------------------
*/

static	char*	chipset_probe (pcici_t tag, pcidi_t type);
static	void	chipset_attach(pcici_t tag, int unit);
static	u_long	chipset_count;

struct	pci_device chipset_device = {
	"chip",
	chipset_probe,
	chipset_attach,
	&chipset_count
};

DATA_SET (pcidevice_set, chipset_device);

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

static	char*	chipset_probe (pcici_t tag, pcidi_t type)
{
	switch (type) {
	case 0x04848086:
		return ("Intel 82378IB PCI-ISA bridge");
	case 0x04838086:
		return ("Intel 82424ZX cache DRAM controller");
	case 0x04828086:
		return ("Intel 82375EB PCI-EISA bridge");
	case 0x04a38086:
		return ("Intel 82434LX PCI cache memory controller");
	};
	return ((char*)0);
}

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

    { 0x56, 0x30, 0x00, M_NE, "\n\tWarning:" },
    { 0x56, 0x20, 0x00, M_NE, " NO cache parity!" },
    { 0x56, 0x10, 0x00, M_NE, " NO DRAM parity!" },
    { 0x55, 0x04, 0x04, M_EQ, "\n\tWarning: refresh OFF! " },

    { 0x00, 0x00, 0x00, TRUE, "\n\tCache: " },
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

    { 0x00, 0x00, 0x00, TRUE, "\n\tDRAM:" },
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

    { 0x00, 0x00, 0x00, TRUE, "\n\tCPU->PCI: posting " },
    { 0x53, 0x02, 0x00, M_NE, "ON" },
    { 0x53, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, TRUE, ", burst mode " },
    { 0x54, 0x02, 0x00, M_NE, "ON" },
    { 0x54, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, TRUE, "\n\tPCI->Memory: posting " },
    { 0x54, 0x01, 0x00, M_NE, "ON" },
    { 0x54, 0x01, 0x00, M_EQ, "OFF" },

    { 0x00, 0x00, 0x00, TRUE, "\n" },

/* end marker */
    { 0 }
};

struct condmsg conf82434lx[] =
{
    { 0x00, 0x00, 0x00, TRUE, "\tCPU: " },
    { 0x50, 0xe3, 0x82, M_EQ, "Pentium, 60MHz" },
    { 0x50, 0xe3, 0x83, M_EQ, "Pentium, 66MHz" },
    { 0x50, 0xe3, 0xa2, M_EQ, "Pentium, 90MHz" },
    { 0x50, 0xe3, 0xa3, M_EQ, "Pentium, 100MHz" },
    { 0x50, 0xc2, 0x82, M_NE, "(unknown)" },
    { 0x50, 0x04, 0x00, M_EQ, " (primary cache OFF)" },

    { 0x53, 0x01, 0x01, TRUE, ", CPU->Memory posting "},
    { 0x53, 0x01, 0x01, M_NE, "OFF" },
    { 0x53, 0x01, 0x01, M_EQ, "ON" },

    { 0x53, 0x04, 0x00, M_NE, ", read around write"},

    { 0x71, 0xc0, 0x00, M_NE, "\n\tWarning: NO cache parity!" },
    { 0x57, 0x20, 0x00, M_NE, "\n\tWarning: NO DRAM parity!" },
    { 0x55, 0x01, 0x01, M_EQ, "\n\tWarning: refresh OFF! " },

    { 0x00, 0x00, 0x00, TRUE, "\n\tCache: " },
    { 0x52, 0x01, 0x00, M_EQ, "None" },
    { 0x52, 0x81, 0x01, M_EQ, "" },
    { 0x52, 0xc1, 0x81, M_EQ, "256KB" },
    { 0x52, 0xc1, 0xc1, M_EQ, "512KB" },
    { 0x52, 0x03, 0x01, M_EQ, " writethrough" },
    { 0x52, 0x03, 0x03, M_EQ, " writeback" },

    { 0x52, 0x01, 0x01, M_EQ, ", cache clocks=" },
    { 0x52, 0x20, 0x00, M_EQ, "3-2-2-2/4-2-2-2" },
    { 0x52, 0x20, 0x00, M_NE, "3-1-1-1" },

    { 0x00, 0x00, 0x00, TRUE, "\n\tDRAM:" },
    { 0x57, 0x10, 0x00, M_EQ, " page mode" },

    { 0x00, 0x00, 0x00, TRUE, " memory clocks=" },
    { 0x57, 0xc0, 0x00, M_EQ, "X-4-4-4 (70ns)" },
    { 0x57, 0xc0, 0x40, M_EQ, "X-4-4-4/X-3-3-3 (60ns)" },
    { 0x57, 0xc0, 0x80, M_EQ, "???" },
    { 0x57, 0xc0, 0xc0, M_EQ, "X-3-3-3 (50ns)" },

    { 0x00, 0x00, 0x00, TRUE, "\n\tCPU->PCI: posting " },
    { 0x53, 0x02, 0x02, M_EQ, "ON" },
    { 0x53, 0x02, 0x00, M_EQ, "OFF" },
    { 0x00, 0x00, 0x00, TRUE, ", burst mode " },
    { 0x54, 0x02, 0x00, M_NE, "ON" },
    { 0x54, 0x02, 0x00, M_EQ, "OFF" },
    { 0x54, 0x04, 0x00, TRUE, ", PCI clocks=" },
    { 0x54, 0x04, 0x00, M_EQ, "2-2-2-2" },
    { 0x54, 0x04, 0x00, M_NE, "2-1-1-1" },
    { 0x00, 0x00, 0x00, TRUE, "\n\tPCI->Memory: posting " },
    { 0x54, 0x01, 0x00, M_NE, "ON" },
    { 0x54, 0x01, 0x00, M_EQ, "OFF" },

    { 0x00, 0x00, 0x00, TRUE, "\n" },

/* end marker */
    { 0 }
};

static char confread (pcici_t config_id, int port)
{
    unsigned long portw = port & ~3;
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
	    }
	}
	if (cond) printf ("%s", tbl->text);
	tbl++;
    }
}

void chipset_attach(pcici_t config_id, int unit)
{
    if (bootverbose) {
	switch (pci_conf_read (config_id, 0)) {

	case 0x04838086:
		writeconfig (config_id, conf82424zx);
		break;
	case 0x04a38086:
		writeconfig (config_id, conf82434lx);
		break;
	case 0x04848086:
	case 0x04828086:
		printf ("\t[40] %lx [50] %lx [54] %lx\n",
			pci_conf_read (config_id, 0x40),
			pci_conf_read (config_id, 0x50),
			pci_conf_read (config_id, 0x54));
		break;
	};
    }
}

/*---------------------------------------------------------
**
**	Catchall driver for VGA devices
**
**
**	By Garrett Wollman
**	<wollman@halloran-eldar.lcs.mit.edu>
**
**---------------------------------------------------------
*/

static	char*	vga_probe (pcici_t tag, pcidi_t type);
static	void	vga_attach(pcici_t tag, int unit);
static	u_long	vga_count;

struct	pci_device vga_device = {
	"vga",
	vga_probe,
	vga_attach,
	&vga_count
};

DATA_SET (pcidevice_set, vga_device);

static	char*	vga_probe (pcici_t tag, pcidi_t type)
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
			return "VGA-compatible display device";
		else
			return ("Display device");
	};
	return ((char*)0);
}

static	void	vga_attach(pcici_t tag, int unit)
{
/*
**	Breaks some systems.
**	The assigned adresses _have_ to be announced to the console driver.
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

static	char*	lkm_probe (pcici_t tag, pcidi_t type);
static	void	lkm_attach(pcici_t tag, int unit);
static	u_long	lkm_count;

struct	pci_device lkm_device = {
	"lkm",
	lkm_probe,
	lkm_attach,
	&lkm_count
};

DATA_SET (pcidevice_set, lkm_device);

static	char*	lkm_probe (pcici_t tag, pcidi_t type)
{
	/*
	**	Should try to load a matching driver.
	**	XXX Not yet!
	*/
	return ((char*)0);
}

static	void	lkm_attach(pcici_t tag, int unit)
{
}

/*---------------------------------------------------------
**
**	Devices to ignore
**
**---------------------------------------------------------
*/

static	char*	ign_probe (pcici_t tag, pcidi_t type);
static	void	ign_attach(pcici_t tag, int unit);
static	u_long	ign_count;

struct	pci_device ign_device = {
	NULL,
	ign_probe,
	ign_attach,
	&ign_count
};

DATA_SET (pcidevice_set, ign_device);

static	char*	ign_probe (pcici_t tag, pcidi_t type)
{
	switch (type) {

	case 0x10001042ul:	/* wd */
		return ("");

	};
	return ((char*)0);
}

static	void	ign_attach(pcici_t tag, int unit)
{
}
