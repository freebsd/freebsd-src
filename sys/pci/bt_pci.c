/*
 * Product specific probe and attach routines for:
 *      Buslogic BT946, BT948, BT956, BT958 SCSI controllers
 *
 * Copyright (c) 1995, 1997, 1998 Justin T. Gibbs
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
 * $FreeBSD$
 */

#include "pci.h"
#if NPCI > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <dev/buslogic/btreg.h>

#define BT_PCI_IOADDR	PCIR_MAPS
#define BT_PCI_MEMADDR	PCIR_MAPS + 4

#define PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER	0x1040104Bul
#define PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC	0x0140104Bul
#define PCI_DEVICE_ID_BUSLOGIC_FLASHPOINT	0x8130104Bul

static int btpcideterminebusspace(pcici_t config_id, bus_space_tag_t* tagp,
				  bus_space_handle_t* bshp);
static const char* bt_pci_probe(pcici_t tag, pcidi_t type);
static void bt_pci_attach(pcici_t config_id, int unit);

static struct  pci_device bt_pci_driver = {
	"bt",
        bt_pci_probe,
        bt_pci_attach,
        &bt_unit,
	NULL
};

DATA_SET (pcidevice_set, bt_pci_driver);

static int
btpcideterminebusspace(pcici_t config_id, bus_space_tag_t* tagp,
		       bus_space_handle_t* bshp)
{
	vm_offset_t	vaddr;
	vm_offset_t	paddr;
	u_int16_t	io_port;
	int		command;

	vaddr = 0;
	paddr = 0;
	command = pci_cfgread(config_id, PCIR_COMMAND, /*bytes*/1);
	/* XXX Memory Mapped I/O seems to cause problems */
#if 0
	if ((command & PCIM_CMD_MEMEN) == 0
	 || (pci_map_mem(config_id, BT_PCI_MEMADDR, &vaddr, &paddr)) == 0)
#endif
		if ((command & PCIM_CMD_PORTEN) == 0
		 || (pci_map_port(config_id, BT_PCI_IOADDR, &io_port)) == 0)
			return (-1);

	if (vaddr != 0) {
		*tagp = I386_BUS_SPACE_MEM;
		*bshp = vaddr;
	} else {
		*tagp = I386_BUS_SPACE_IO;
		*bshp = io_port;
	}

	return (0);
}

static const char*
bt_pci_probe (pcici_t config_id, pcidi_t type)
{
	switch(type) {
		case PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER:
		case PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC:
		{
			struct bt_softc   *bt;
			bus_space_tag_t	   tag;
		        bus_space_handle_t bsh;
			pci_info_data_t pci_info;
			int error;

			if (btpcideterminebusspace(config_id, &tag, &bsh) != 0)
				break;

			bt = bt_alloc(BT_TEMP_UNIT, tag, bsh);
			if (bt == NULL)
				break;

			/*
			 * Determine if an ISA compatible I/O port has been
			 * enabled.  If so, record the port so it will not
			 * be probed by our ISA probe.  If the PCI I/O port
			 * was not set to the compatibility port, disable it.
			 */
			error = bt_cmd(bt, BOP_INQUIRE_PCI_INFO,
				       /*param*/NULL, /*paramlen*/0,
				       (u_int8_t*)&pci_info, sizeof(pci_info),
				       DEFAULT_CMD_TIMEOUT);
			if (error == 0
			 && pci_info.io_port < BIO_DISABLED) {
				bt_mark_probed_bio(pci_info.io_port);
				if (bsh != bt_iop_from_bio(pci_info.io_port)) {
					u_int8_t new_addr;

					new_addr = BIO_DISABLED;
					bt_cmd(bt, BOP_MODIFY_IO_ADDR,
					       /*param*/&new_addr,
					       /*paramlen*/1, /*reply_buf*/NULL,
					       /*reply_len*/0,
					       DEFAULT_CMD_TIMEOUT);
				}
			}
			bt_free(bt);
			return ("Buslogic Multi-Master SCSI Host Adapter");
			break;
		}
		default:
			break;
	}

	return (NULL);
}

static void
bt_pci_attach(pcici_t config_id, int unit)
{
	struct bt_softc   *bt;
	bus_space_tag_t	   tag;
        bus_space_handle_t bsh;
	int		   opri;

	if (btpcideterminebusspace(config_id, &tag, &bsh) != 0)
		return;

	if ((bt = bt_alloc(unit, tag, bsh)) == NULL)
		return;  /* XXX PCI code should take return status */

	/* Allocate a dmatag for our CCB DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignemnt*/1, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
			       /*nsegments*/BUS_SPACE_UNRESTRICTED,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &bt->parent_dmat) != 0) {
		bt_free(bt);
		return;
	}

	if ((pci_map_int(config_id, bt_intr, (void *)bt, &cam_imask)) == 0) {
		bt_free(bt);
		return;
	}
	/*
	 * Protect ourself from spurrious interrupts during
	 * intialization and attach.  We should really rely
	 * on interrupts during attach, but we don't have
	 * access to our interrupts during ISA probes, so until
	 * that changes, we mask our interrupts during attach
	 * too.
	 */
	opri = splcam();

	if (bt_probe(bt) || bt_fetch_adapter_info(bt) || bt_init(bt)) {
		bt_free(bt);
		splx(opri);
		return; /* XXX PCI code should take return status */
	}

	bt_attach(bt);

	splx(opri);
	return;
}

#endif /* NPCI > 0 */
