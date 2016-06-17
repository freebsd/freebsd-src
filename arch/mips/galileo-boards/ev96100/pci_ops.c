/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Galileo EV96100 board specific pci support.
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * This file was derived from Carsten Langgaard's
 * arch/mips/mips-boards/generic/pci.c
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>

#include <asm/delay.h>
#include <asm/gt64120/gt64120.h>
#include <asm/pci_channel.h>

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1

#define GT_PCI_MEM_BASE    0x12000000
#define GT_PCI_MEM_SIZE    0x02000000
#define GT_PCI_IO_BASE     0x10000000
#define GT_PCI_IO_SIZE     0x02000000

static struct resource pci_io_resource = {
	"io pci IO space",
	0x10000000,
	0x10000000 + 0x02000000,
	IORESOURCE_IO
};

static struct resource pci_mem_resource = {
	"ext pci memory space",
	0x12000000,
	0x12000000 + 0x02000000,
	IORESOURCE_MEM
};

extern struct pci_ops gt64120_pci_ops;

struct pci_channel mips_pci_channels[] = {
	{ &gt64120_pci_ops, &pci_io_resource, &pci_mem_resource, 1, 0xff },
	{ NULL, NULL, NULL, NULL, NULL}
};
