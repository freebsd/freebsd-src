/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/boot/i386/libi386/biospci.c,v 1.2 1999/08/28 00:40:13 peter Exp $
 */

/*
 * PnP enumerator using the PCI BIOS.
 */

#include <stand.h>
#include <string.h>
#include <machine/stdarg.h>
#include <bootstrap.h>
#include <isapnp.h>
#include <btxv86.h>

/*
 * Stupid PCI BIOS interface doesn't let you simply enumerate everything
 * that's there, instead you have to ask it if it has something.
 *
 * So we have to scan by class code, subclass code and sometimes programming
 * interface.
 */

struct pci_progif 
{
    int		pi_code;
    char	*pi_name;
};

static struct pci_progif progif_null[] = {
    {0x0,	NULL},
    {-1,	NULL}
};

static struct pci_progif progif_display[] = {
    {0x0,	"VGA"},
    {0x1,	"8514"},
    {-1,	NULL}
};

static struct pci_progif progif_ide[] = {
    {0x00,	NULL},
    {0x01,	NULL},
    {0x02,	NULL},
    {0x03,	NULL},
    {0x04,	NULL},
    {0x05,	NULL},
    {0x06,	NULL},
    {0x07,	NULL},
    {0x08,	NULL},
    {0x09,	NULL},
    {0x0a,	NULL},
    {0x0b,	NULL},
    {0x0c,	NULL},
    {0x0d,	NULL},
    {0x0e,	NULL},
    {0x0f,	NULL},
    {0x80,	NULL},
    {0x81,	NULL},
    {0x82,	NULL},
    {0x83,	NULL},
    {0x84,	NULL},
    {0x85,	NULL},
    {0x86,	NULL},
    {0x87,	NULL},
    {0x88,	NULL},
    {0x89,	NULL},
    {0x8a,	NULL},
    {0x8b,	NULL},
    {0x8c,	NULL},
    {0x8d,	NULL},
    {0x8e,	NULL},
    {0x8f,	NULL},
    {-1,	NULL}
};

static struct pci_progif progif_serial[] = {
    {0x0,	"8250"},
    {0x1,	"16450"},
    {0x2,	"16550"},
    {-1,	NULL}
};

static struct pci_progif progif_parallel[] = {
    {0x0,	"Standard"},
    {0x1,	"Bidirectional"},
    {0x2,	"ECP"},
    {-1,	NULL}
};


struct pci_subclass 
{
    int			ps_subclass;
    char		*ps_name;
    struct pci_progif	*ps_progif;	/* if set, use for programming interface value(s) */
};

static struct pci_subclass subclass_old[] = {
    {0x0,	"Old non-VGA",		progif_null},
    {0x1,	"Old VGA",		progif_null},
    {-1,	NULL,			NULL}
};

static struct pci_subclass subclass_mass[] = {
    {0x0,	"SCSI",			progif_null},
    {0x1,	"IDE",			progif_ide},
    {0x2,	"Floppy disk",		progif_null},
    {0x3,	"IPI",			progif_null},
    {0x4,	"RAID",			progif_null},
    {0x80,	"mass storage",		progif_null},
    {-1,	NULL,			NULL}
};

static struct pci_subclass subclass_net[] = {
    {0x0,	"Ethernet",		progif_null},
    {0x1,	"Token ring",		progif_null},
    {0x2,	"FDDI",			progif_null},
    {0x3,	"ATM",			progif_null},
    {0x80,	"network",		progif_null},
    {-1,	NULL,			NULL}
};

static struct pci_subclass subclass_display[] = {
    {0x0,	NULL,			progif_display},
    {0x1,	"XGA",			progif_null},
    {0x80,	"other",		progif_null},
    {-1,	NULL,			NULL}
};

static struct pci_subclass subclass_comms[] = {
    {0x0,	"serial",		progif_serial},
    {0x1,	"parallel",		progif_parallel},
    {0x80,	"communications",	progif_null},
    {-1,	NULL,			NULL}
};

static struct pci_subclass subclass_serial[] = {
    {0x0,	"Firewire",		progif_null},
    {0x1,	"ACCESS.bus",		progif_null},
    {0x2,	"SSA",			progif_null},
    {0x3,	"USB",			progif_null},
    {0x4,	"Fibrechannel",		progif_null},
    {-1,	NULL,			NULL}
};

static struct pci_class
{
    int			pc_class;
    char		*pc_name;
    struct pci_subclass	*pc_subclass;
} pci_classes[] = {
    {0x0,	"device",	subclass_old},
    {0x1,	"controller",	subclass_mass},
    {0x2,	"controller",	subclass_net},
    {0x3,	"display",	subclass_display},
    {0x7,	"controller",	subclass_comms},
    {0xc,	"controller",	subclass_serial},
    {-1,	NULL,		NULL}
};


static void	biospci_enumerate(void);
static void	biospci_addinfo(int devid, struct pci_class *pc, struct pci_subclass *psc, struct pci_progif *ppi);

static int	biospci_version;
static int	biospci_hwcap;

struct pnphandler biospcihandler =
{
    "PCI BIOS",
    biospci_enumerate
};

static void
biospci_enumerate(void)
{
    int			index, locator, devid;
    struct pci_class	*pc;
    struct pci_subclass *psc;
    struct pci_progif	*ppi;

    /* Find the PCI BIOS */
    v86.ctl = V86_FLAGS;
    v86.addr = 0x1a;
    v86.eax = 0xb101;
    v86.edi = 0x0;
    v86int();

    /* Check for OK response */
    if ((v86.efl & 1) || ((v86.eax & 0xff00) != 0) || (v86.edx != 0x20494350))
	return;

    biospci_version = v86.ebx & 0xffff;
    biospci_hwcap = v86.eax & 0xff;
#if 0
    printf("PCI BIOS %d.%d%s%s\n", 
	   bcd2bin((biospci_version >> 8) & 0xf), bcd2bin(biospci_version & 0xf),
	   (biospci_hwcap & 1) ? " config1" : "", (biospci_hwcap & 2) ? " config2" : "");
#endif
    /* Iterate over known classes */
    for (pc = pci_classes; pc->pc_class >= 0; pc++) {
	/* Iterate over subclasses */
	for (psc = pc->pc_subclass; psc->ps_subclass >= 0; psc++) {
	    /* Iterate over programming interfaces */
	    for (ppi = psc->ps_progif; ppi->pi_code >= 0; ppi++) {

		/* Scan for matches */
		for (index = 0; ; index++) {

		    /* Look for a match */
		    v86.ctl = V86_FLAGS;
		    v86.addr = 0x1a;
		    v86.eax = 0xb103;
		    v86.ecx = (pc->pc_class << 16) + (psc->ps_subclass << 8) + ppi->pi_code;
		    v86.esi = index;
		    v86int();
		    /* error/end of matches */
		    if ((v86.efl & 1) || (v86.eax & 0xff00))
			break;

		    /* Got something */
		    locator = v86.ebx;
		    
		    /* Read the device identifier from the nominated device */
		    v86.ctl = V86_FLAGS;
		    v86.addr = 0x1a;
		    v86.eax = 0xb10a;
		    v86.ebx = locator;
		    v86.edi = 0x0;
		    v86int();
		    /* error */
		    if ((v86.efl & 1) || (v86.eax & 0xff00))
			break;
		    
		    /* We have the device ID, create a PnP object and save everything */
		    devid = v86.ecx;
		    biospci_addinfo(devid, pc, psc, ppi);
		}
	    }
	}
    }
}

static void
biospci_addinfo(int devid, struct pci_class *pc, struct pci_subclass *psc, struct pci_progif *ppi) 
{
    struct pnpinfo	*pi;
    char		desc[80];
    
    
    /* build the description */
    desc[0] = 0;
    if (ppi->pi_name != NULL) {
	strcat(desc, ppi->pi_name);
	strcat(desc, " ");
    }
    if (psc->ps_name != NULL) {
	strcat(desc, psc->ps_name);
	strcat(desc, " ");
    }
    if (pc->pc_name != NULL)
	strcat(desc, pc->pc_name);

    pi = pnp_allocinfo();
    pi->pi_desc = strdup(desc);
    sprintf(desc,"0x%08x", devid);
    pnp_addident(pi, desc);
    pnp_addinfo(pi);
}
