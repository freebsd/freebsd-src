/*
 * FreeBSD, PCI product support functions
 *
 * Copyright (c) 1995-2001 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ahc_pci.c,v 1.53 2003/05/03 23:27:57 gibbs Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/aic7xxx/aic7xxx_osm.h>

#define	AHC_PCI_IOADDR  PCIR_MAPS	/* I/O Address */
#define	AHC_PCI_MEMADDR (PCIR_MAPS + 4) /* Mem I/O Address */

static int ahc_pci_probe(device_t dev);
static int ahc_pci_attach(device_t dev);

static device_method_t ahc_pci_device_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ahc_pci_probe),
	DEVMETHOD(device_attach,	ahc_pci_attach),
	DEVMETHOD(device_detach,	ahc_detach),
	{ 0, 0 }
};

static driver_t ahc_pci_driver = {
	"ahc",
	ahc_pci_device_methods,
	sizeof(struct ahc_softc)
};

DRIVER_MODULE(ahc_pci, pci, ahc_pci_driver, ahc_devclass, 0, 0);
DRIVER_MODULE(ahc_pci, cardbus, ahc_pci_driver, ahc_devclass, 0, 0);
MODULE_DEPEND(ahc_pci, ahc, 1, 1, 1);
MODULE_VERSION(ahc_pci, 1);

static int
ahc_pci_probe(device_t dev)
{
	struct	ahc_pci_identity *entry;

	entry = ahc_find_pci_device(dev);
	if (entry != NULL) {
		device_set_desc(dev, entry->name);
		return (0);
	}
	return (ENXIO);
}

static int
ahc_pci_attach(device_t dev)
{
	struct	 ahc_pci_identity *entry;
	struct	 ahc_softc *ahc;
	char	*name;
	int	 error;

	entry = ahc_find_pci_device(dev);
	if (entry == NULL)
		return (ENXIO);

	/*
	 * Allocate a softc for this card and
	 * set it up for attachment by our
	 * common detect routine.
	 */
	name = malloc(strlen(device_get_nameunit(dev)) + 1, M_DEVBUF, M_NOWAIT);
	if (name == NULL)
		return (ENOMEM);
	strcpy(name, device_get_nameunit(dev));
	ahc = ahc_alloc(dev, name);
	if (ahc == NULL)
		return (ENOMEM);

	ahc_set_unit(ahc, device_get_unit(dev));

	/*
	 * Should we bother disabling 39Bit addressing
	 * based on installed memory?
	 */
	if (sizeof(bus_addr_t) > 4)
                ahc->flags |= AHC_39BIT_ADDRESSING;

	/* Allocate a dmatag for our SCB DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
				   /*boundary*/0,
				   (ahc->flags & AHC_39BIT_ADDRESSING)
				   ? 0x7FFFFFFFFF
				   : BUS_SPACE_MAXADDR_32BIT,
				   /*highaddr*/BUS_SPACE_MAXADDR,
				   /*filter*/NULL, /*filterarg*/NULL,
				   /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
				   /*nsegments*/AHC_NSEG,
				   /*maxsegsz*/AHC_MAXTRANSFER_SIZE,
				   /*flags*/0,
				   /*lockfunc*/busdma_lock_mutex,
				   /*lockarg*/&Giant,
				   &ahc->parent_dmat);

	if (error != 0) {
		printf("ahc_pci_attach: Could not allocate DMA tag "
		       "- error %d\n", error);
		ahc_free(ahc);
		return (ENOMEM);
	}
	ahc->dev_softc = dev;
	error = ahc_pci_config(ahc, entry);
	if (error != 0) {
		ahc_free(ahc);
		return (error);
	}

	ahc_attach(ahc);
	return (0);
}

int
ahc_pci_map_registers(struct ahc_softc *ahc)
{
	struct	resource *regs;
	u_int	command;
	int	regs_type;
	int	regs_id;
	int	allow_memio;

	command = ahc_pci_read_config(ahc->dev_softc, PCIR_COMMAND, /*bytes*/1);
	regs = NULL;
	regs_type = 0;
	regs_id = 0;

	/* Retrieve the per-device 'allow_memio' hint */
	if (resource_int_value(device_get_name(ahc->dev_softc),
			       device_get_unit(ahc->dev_softc),
			       "allow_memio", &allow_memio) != 0) {
		if (bootverbose)
			device_printf(ahc->dev_softc, "Defaulting to MEMIO ");
#ifdef AHC_ALLOW_MEMIO
		if (bootverbose)
			printf("on\n");
		allow_memio = 1;
#else
		if (bootverbose)
			printf("off\n");
		allow_memio = 0;
#endif
	}

	if ((allow_memio != 0) && (command & PCIM_CMD_MEMEN) != 0) {

		regs_type = SYS_RES_MEMORY;
		regs_id = AHC_PCI_MEMADDR;
		regs = bus_alloc_resource(ahc->dev_softc, regs_type,
					  &regs_id, 0, ~0, 1, RF_ACTIVE);
		if (regs != NULL) {
			ahc->tag = rman_get_bustag(regs);
			ahc->bsh = rman_get_bushandle(regs);

			/*
			 * Do a quick test to see if memory mapped
			 * I/O is functioning correctly.
			 */
			if (ahc_pci_test_register_access(ahc) != 0) {
				device_printf(ahc->dev_softc,
				       "PCI Device %d:%d:%d failed memory "
				       "mapped test.  Using PIO.\n",
				       ahc_get_pci_bus(ahc->dev_softc),
				       ahc_get_pci_slot(ahc->dev_softc),
				       ahc_get_pci_function(ahc->dev_softc));
				bus_release_resource(ahc->dev_softc, regs_type,
						     regs_id, regs);
				regs = NULL;
			} else {
				command &= ~PCIM_CMD_PORTEN;
				ahc_pci_write_config(ahc->dev_softc,
						     PCIR_COMMAND,
						     command, /*bytes*/1);
			}
		}
	}

	if (regs == NULL && (command & PCIM_CMD_PORTEN) != 0) {
		regs_type = SYS_RES_IOPORT;
		regs_id = AHC_PCI_IOADDR;
		regs = bus_alloc_resource(ahc->dev_softc, regs_type,
					  &regs_id, 0, ~0, 1, RF_ACTIVE);
		if (regs != NULL) {
			ahc->tag = rman_get_bustag(regs);
			ahc->bsh = rman_get_bushandle(regs);
			command &= ~PCIM_CMD_MEMEN;
			ahc_pci_write_config(ahc->dev_softc, PCIR_COMMAND,
					     command, /*bytes*/1);
		}
	}
	if (regs == NULL) {
		device_printf(ahc->dev_softc,
			      "can't allocate register resources\n");
		return (ENOMEM);
	}
	ahc->platform_data->regs_res_type = regs_type;
	ahc->platform_data->regs_res_id = regs_id;
	ahc->platform_data->regs = regs;
	return (0);
}

int
ahc_pci_map_int(struct ahc_softc *ahc)
{
	int zero;

	zero = 0;
	ahc->platform_data->irq =
	    bus_alloc_resource(ahc->dev_softc, SYS_RES_IRQ, &zero,
			       0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (ahc->platform_data->irq == NULL) {
		device_printf(ahc->dev_softc,
			      "bus_alloc_resource() failed to allocate IRQ\n");
		return (ENOMEM);
	}
	ahc->platform_data->irq_res_type = SYS_RES_IRQ;
	return (ahc_map_int(ahc));
}

void
ahc_power_state_change(struct ahc_softc *ahc, ahc_power_state new_state)
{
	uint32_t cap;
	u_int cap_offset;

	/*
	 * Traverse the capability list looking for
	 * the power management capability.
	 */
	cap = 0;
	cap_offset = ahc_pci_read_config(ahc->dev_softc,
					 PCIR_CAP_PTR, /*bytes*/1);
	while (cap_offset != 0) {

		cap = ahc_pci_read_config(ahc->dev_softc,
					  cap_offset, /*bytes*/4);
		if ((cap & 0xFF) == 1
		 && ((cap >> 16) & 0x3) > 0) {
			uint32_t pm_control;

			pm_control = ahc_pci_read_config(ahc->dev_softc,
							 cap_offset + 4,
							 /*bytes*/2);
			pm_control &= ~0x3;
			pm_control |= new_state;
			ahc_pci_write_config(ahc->dev_softc,
					     cap_offset + 4,
					     pm_control, /*bytes*/2);
			break;
		}
		cap_offset = (cap >> 8) & 0xFF;
	}
}
