/*-
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *	ABP[3]940UW - Bus-Master PCI Ultra-Wide (253 CDB)
 *	ABP950UW    - Dual Channel Bus-Master PCI Ultra-Wide (253 CDB/Channel)
 *	ABP970UW    - Bus-Master PCI Ultra-Wide (253 CDB)
 *	ABP3940U2W  - Bus-Master PCI LVD/Ultra2-Wide (253 CDB)
 *	ABP3950U2W  - Bus-Master PCI LVD/Ultra2-Wide (253 CDB)
 *
 * Copyright (c) 1998, 1999, 2000 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/advansys/adw_pci.c,v 1.25.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <cam/cam.h>
#include <cam/scsi/scsi_all.h>

#include <dev/advansys/adwvar.h>
#include <dev/advansys/adwlib.h>
#include <dev/advansys/adwmcode.h>

#define ADW_PCI_IOBASE	PCIR_BAR(0)		/* I/O Address */
#define ADW_PCI_MEMBASE	PCIR_BAR(1)		/* Mem I/O Address */

#define	PCI_ID_ADVANSYS_3550		0x230010CD00000000ull
#define	PCI_ID_ADVANSYS_38C0800_REV1	0x250010CD00000000ull
#define	PCI_ID_ADVANSYS_38C1600_REV1	0x270010CD00000000ull
#define PCI_ID_ALL_MASK             	0xFFFFFFFFFFFFFFFFull
#define PCI_ID_DEV_VENDOR_MASK      	0xFFFFFFFF00000000ull

struct adw_pci_identity;
typedef int (adw_device_setup_t)(device_t, struct adw_pci_identity *,
				 struct adw_softc *adw);

struct adw_pci_identity {
	u_int64_t		 full_id;
	u_int64_t		 id_mask;
	char			*name;
	adw_device_setup_t	*setup;
	const struct adw_mcode	*mcode_data;
	const struct adw_eeprom	*default_eeprom;
};

static adw_device_setup_t adw_asc3550_setup;
static adw_device_setup_t adw_asc38C0800_setup;
#ifdef NOTYET
static adw_device_setup_t adw_asc38C1600_setup;
#endif

struct adw_pci_identity adw_pci_ident_table[] =
{
	/* asc3550 based controllers */
	{
		PCI_ID_ADVANSYS_3550,
		PCI_ID_DEV_VENDOR_MASK,
		"AdvanSys 3550 Ultra SCSI Adapter",
		adw_asc3550_setup,
		&adw_asc3550_mcode_data,
		&adw_asc3550_default_eeprom
	},
	/* asc38C0800 based controllers */
	{
		PCI_ID_ADVANSYS_38C0800_REV1,
		PCI_ID_DEV_VENDOR_MASK,
		"AdvanSys 38C0800 Ultra2 SCSI Adapter",
		adw_asc38C0800_setup,
		&adw_asc38C0800_mcode_data,
		&adw_asc38C0800_default_eeprom
	},
#ifdef NOTYET
	/* XXX Disabled until I have hardware to test with */
	/* asc38C1600 based controllers */
	{
		PCI_ID_ADVANSYS_38C1600_REV1,
		PCI_ID_DEV_VENDOR_MASK,
		"AdvanSys 38C1600 Ultra160 SCSI Adapter",
		adw_asc38C1600_setup,
		NULL, /* None provided by vendor thus far */
		NULL  /* None provided by vendor thus far */
	}
#endif
};

static const int adw_num_pci_devs =
	sizeof(adw_pci_ident_table) / sizeof(*adw_pci_ident_table);

#define ADW_PCI_MAX_DMA_ADDR    (0xFFFFFFFFUL)
#define ADW_PCI_MAX_DMA_COUNT   (0xFFFFFFFFUL)

static int adw_pci_probe(device_t dev);
static int adw_pci_attach(device_t dev);

static device_method_t adw_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		adw_pci_probe),
	DEVMETHOD(device_attach,	adw_pci_attach),
	{ 0, 0 }
};

static driver_t adw_pci_driver = {
        "adw",
        adw_pci_methods,
        sizeof(struct adw_softc)
}; 

static devclass_t adw_devclass;

DRIVER_MODULE(adw, pci, adw_pci_driver, adw_devclass, 0, 0);
MODULE_DEPEND(adw, pci, 1, 1, 1);

static __inline u_int64_t
adw_compose_id(u_int device, u_int vendor, u_int subdevice, u_int subvendor)
{
	u_int64_t id;

	id = subvendor
	   | (subdevice << 16)
	   | ((u_int64_t)vendor << 32)
	   | ((u_int64_t)device << 48);

        return (id);
}

static struct adw_pci_identity *
adw_find_pci_device(device_t dev)
{
	u_int64_t  full_id;
	struct     adw_pci_identity *entry;
	u_int      i;

	full_id = adw_compose_id(pci_get_device(dev),
				 pci_get_vendor(dev),
				 pci_get_subdevice(dev),
				 pci_get_subvendor(dev));

	for (i = 0; i < adw_num_pci_devs; i++) {
		entry = &adw_pci_ident_table[i];
		if (entry->full_id == (full_id & entry->id_mask))
			return (entry);
	}
	return (NULL);
}

static int
adw_pci_probe(device_t dev)
{
	struct	adw_pci_identity *entry;

	entry = adw_find_pci_device(dev);
	if (entry != NULL) {
		device_set_desc(dev, entry->name);
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
adw_pci_attach(device_t dev)
{
	struct		adw_softc *adw;
	struct		adw_pci_identity *entry;
	u_int32_t	command;
	struct		resource *regs;
	int		regs_type;
	int		regs_id;
	int		error;
	int		zero;
 
	command = pci_read_config(dev, PCIR_COMMAND, /*bytes*/1);
	entry = adw_find_pci_device(dev);
	if (entry == NULL)
		return (ENXIO);
	regs = NULL;
	regs_type = 0;
	regs_id = 0;
#ifdef ADW_ALLOW_MEMIO
	if ((command & PCIM_CMD_MEMEN) != 0) {
		regs_type = SYS_RES_MEMORY;
		regs_id = ADW_PCI_MEMBASE;
		regs = bus_alloc_resource_any(dev, regs_type,
					      &regs_id, RF_ACTIVE);
	}
#endif
	if (regs == NULL && (command & PCIM_CMD_PORTEN) != 0) {
		regs_type = SYS_RES_IOPORT;
		regs_id = ADW_PCI_IOBASE;
		regs = bus_alloc_resource_any(dev, regs_type,
					      &regs_id, RF_ACTIVE);
	}

	if (regs == NULL) {
		device_printf(dev, "can't allocate register resources\n");
		return (ENOMEM);
	}

	adw = adw_alloc(dev, regs, regs_type, regs_id);
	if (adw == NULL)
		return(ENOMEM);

	/*
	 * Now that we have access to our registers, just verify that
	 * this really is an AdvanSys device.
	 */
	if (adw_find_signature(adw) == 0) {
		adw_free(adw);
		return (ENXIO);
	}

	adw_reset_chip(adw);

	error = entry->setup(dev, entry, adw);

	if (error != 0)
		return (error);

	/* Ensure busmastering is enabled */
	command |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, /*bytes*/1);

	/* Allocate a dmatag for our transfer DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	error = bus_dma_tag_create(
			/* parent	*/ NULL,
			/* alignment	*/ 1,
			/* boundary	*/ 0,
			/* lowaddr	*/ ADW_PCI_MAX_DMA_ADDR,
			/* highaddr	*/ BUS_SPACE_MAXADDR,
			/* filter	*/ NULL,
			/* filterarg	*/ NULL,
			/* maxsize	*/ BUS_SPACE_MAXSIZE_32BIT,
			/* nsegments	*/ ~0,
			/* maxsegsz	*/ ADW_PCI_MAX_DMA_COUNT,
			/* flags	*/ 0,
			/* lockfunc	*/ busdma_lock_mutex,
			/* lockarg	*/ &Giant,
			&adw->parent_dmat);

	adw->init_level++;
 
	if (error != 0) {
		printf("%s: Could not allocate DMA tag - error %d\n",
		       adw_name(adw), error);
		adw_free(adw);
		return (error);
	}

	adw->init_level++;

	error = adw_init(adw);
	if (error != 0) {
		adw_free(adw);
		return (error);
	}

	/*
	 * If the PCI Configuration Command Register "Parity Error Response
	 * Control" Bit was clear (0), then set the microcode variable
	 * 'control_flag' CONTROL_FLAG_IGNORE_PERR flag to tell the microcode
	 * to ignore DMA parity errors.
	 */
	if ((command & PCIM_CMD_PERRESPEN) == 0)
		adw_lram_write_16(adw, ADW_MC_CONTROL_FLAG,
				  adw_lram_read_16(adw, ADW_MC_CONTROL_FLAG)
				  | ADW_MC_CONTROL_IGN_PERR);

	zero = 0;
	adw->irq_res_type = SYS_RES_IRQ;
	adw->irq = bus_alloc_resource_any(dev, adw->irq_res_type, &zero,
					  RF_ACTIVE | RF_SHAREABLE);
	if (adw->irq == NULL) {
		adw_free(adw);
		return (ENOMEM);
	}

	error = adw_attach(adw);
	if (error != 0)
		adw_free(adw);
	return (error);
}

static int
adw_generic_setup(device_t dev, struct adw_pci_identity *entry,
		  struct adw_softc *adw)
{
	adw->channel = pci_get_function(dev) == 1 ? 'B' : 'A';
	adw->chip = ADW_CHIP_NONE;
	adw->features = ADW_FENONE;
	adw->flags = ADW_FNONE;
	adw->mcode_data = entry->mcode_data;
	adw->default_eeprom = entry->default_eeprom;
	return (0);
}

static int
adw_asc3550_setup(device_t dev, struct adw_pci_identity *entry,
		  struct adw_softc *adw)
{
	int error;

	error = adw_generic_setup(dev, entry, adw);
	if (error != 0)
		return (error);
	adw->chip = ADW_CHIP_ASC3550;
	adw->features = ADW_ASC3550_FE;
	adw->memsize = ADW_3550_MEMSIZE;
	/*
	 * For ASC-3550, setting the START_CTL_EMFU [3:2] bits
	 * sets a FIFO threshold of 128 bytes. This register is
	 * only accessible to the host.
	 */
	adw_outb(adw, ADW_DMA_CFG0,
		 ADW_DMA_CFG0_START_CTL_EM_FU|ADW_DMA_CFG0_READ_CMD_MRM);
	adw_outb(adw, ADW_MEM_CFG,
		 adw_inb(adw, ADW_MEM_CFG) | ADW_MEM_CFG_RAM_SZ_8KB);
	return (0);
}

static int
adw_asc38C0800_setup(device_t dev, struct adw_pci_identity *entry,
		     struct adw_softc *adw)
{
	int error;

	error = adw_generic_setup(dev, entry, adw);
	if (error != 0)
		return (error);
	/*
	 * For ASC-38C0800, set FIFO_THRESH_80B [6:4] bits and
	 * START_CTL_TH [3:2] bits for the default FIFO threshold.
	 *
	 * Note: ASC-38C0800 FIFO threshold has been changed to 256 bytes.
	 *
	 * For DMA Errata #4 set the BC_THRESH_ENB bit.
	 */
	adw_outb(adw, ADW_DMA_CFG0,
		 ADW_DMA_CFG0_BC_THRESH_ENB|ADW_DMA_CFG0_FIFO_THRESH_80B
		|ADW_DMA_CFG0_START_CTL_TH|ADW_DMA_CFG0_READ_CMD_MRM);
	adw_outb(adw, ADW_MEM_CFG,
		 adw_inb(adw, ADW_MEM_CFG) | ADW_MEM_CFG_RAM_SZ_16KB);
	adw->chip = ADW_CHIP_ASC38C0800;
	adw->features = ADW_ASC38C0800_FE;
	adw->memsize = ADW_38C0800_MEMSIZE;
	return (error);
}

#ifdef NOTYET
static int
adw_asc38C1600_setup(device_t dev, struct adw_pci_identity *entry,
		     struct adw_softc *adw)
{
	int error;

	error = adw_generic_setup(dev, entry, adw);
	if (error != 0)
		return (error);
	adw->chip = ADW_CHIP_ASC38C1600;
	adw->features = ADW_ASC38C1600_FE;
	adw->memsize = ADW_38C1600_MEMSIZE;
	return (error);
}
#endif
