/*-
 * Copyright (c) 1999 Jonathan Lemon
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
 *	$Id: ida_pci.c,v 1.2 1999/07/03 20:17:02 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/devicestat.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/ida/idavar.h>

#define IDA_PCI_MAX_DMA_ADDR	0xFFFFFFFF
#define IDA_PCI_MAX_DMA_COUNT	0xFFFFFFFF

#define IDA_PCI_MEMADDR		(PCIR_MAPS + 4)		/* Mem I/O Address */

#define IDA_DEVICEID_SMART	0xAE100E11

static struct {
        u_long	board;
        char    *desc;
} board_id[] = {
	{ 0x4030,	"Compaq SMART-2/P array controller" },
	{ 0x4031,	"Compaq SMART-2SL array controller" },
	{ 0x4032,	"Compaq Smart Array 3200 controller" },
	{ 0x4033,	"Compaq Smart Array 3100ES controller" },
	{ 0x4034,	"Compaq Smart Array 221 controller" },

	{ 0,		"" },
};

static int ida_pci_probe(device_t dev);
static int ida_pci_attach(device_t dev);

static device_method_t ida_pci_methods[] = {
	DEVMETHOD(device_probe,		ida_pci_probe),
	DEVMETHOD(device_attach,	ida_pci_attach),

	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	{ 0, 0 }
};

static driver_t ida_pci_driver = {
	"ida",
	ida_pci_methods,
	sizeof(struct ida_softc)
};

static devclass_t ida_devclass;

static int
ida_pci_probe(device_t dev)
{
	u_long board;
	int i;

	if (pci_get_devid(dev) == IDA_DEVICEID_SMART) {
		board = pci_get_subdevice(dev);
		for (i = 0; board_id[i].board; i++) {
			if (board_id[i].board == board) {
				device_set_desc(dev, board_id[i].desc);
				return (0);
			}
		}
		/*
		 * It's an unknown Compaq SMART device, but assume we
		 * can support it.
		 */
		device_set_desc(dev, "Unknown Compaq Smart Array controller");
		return (0);
	}
	return (ENXIO);
}

static int
ida_pci_attach(device_t dev)
{
	struct ida_softc *ida;
	u_int command;
	int error, rid;

	command = pci_read_config(dev, PCIR_COMMAND, 1);

	/*
	 * for multiple card types, need to re-determine which type is
	 * being attached here
	 */

	/*
	 * it appears that this board only does MEMIO access.
	 */
	if ((command & PCIM_CMD_MEMEN) == 0) {
                device_printf(dev, "Only memory mapped I/O is supported\n");
		return (ENXIO);
	}

	ida = (struct ida_softc *)device_get_softc(dev);
	ida->dev = dev;

	ida->regs_res_type = SYS_RES_MEMORY;
	ida->regs_res_id = IDA_PCI_MEMADDR;
	ida->regs = bus_alloc_resource(dev, ida->regs_res_type,
	    &ida->regs_res_id, 0, ~0, 1, RF_ACTIVE);
	if (ida->regs == NULL) {
		device_printf(dev, "can't allocate register resources\n");
		return (ENOMEM);
	}

	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/0,
	    /*boundary*/0, /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
	    /*highaddr*/BUS_SPACE_MAXADDR, /*filter*/NULL, /*filterarg*/NULL,
	    /*maxsize*/MAXBSIZE, /*nsegments*/IDA_NSEG,
	    /*maxsegsize*/BUS_SPACE_MAXSIZE_32BIT, /*flags*/BUS_DMA_ALLOCNOW,
	    &ida->parent_dmat);
	if (error != 0) {
		device_printf(dev, "can't allocate DMA tag\n");
		ida_free(ida);
		return (ENOMEM);
	}

	rid = 0;
        ida->irq_res_type = SYS_RES_IRQ;
	ida->irq = bus_alloc_resource(dev, ida->irq_res_type, &rid,
	    0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
        if (ida->irq == NULL) {
                ida_free(ida);
                return (ENOMEM);
        }
	error = bus_setup_intr(dev, ida->irq, INTR_TYPE_BIO,
	    ida_intr, ida, &ida->ih);
	if (error) {
		device_printf(dev, "can't setup interrupt\n");
		ida_free(ida);
		return (ENOMEM);
	}

	error = ida_init(ida);
	if (error) {
                ida_free(ida);
                return (error);
        }
	ida_attach(ida);
	ida->flags = IDA_ATTACHED; 

	return (0);
}

DRIVER_MODULE(ida, pci, ida_pci_driver, ida_devclass, 0, 0);
