/*
 * Copyright (c) 1996, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *	$FreeBSD$
 */

/*
 * Cyclades Y PCI serial interface driver
 */

#include "pci.h"
#if NPCI > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <pci/pcivar.h>

#include <pci/cy_pcireg.h>

static char *cy_probe		__P((pcici_t, pcidi_t));
static void cy_attach		__P((pcici_t, int));

extern int cyattach_common(void *, int); /* Not exactly correct */
extern void cyintr(int);

static u_long cy_count;

static struct pci_device cy_device = {
        "cy",
        cy_probe,
        cy_attach,
        &cy_count,
        NULL
};
DATA_SET(pcidevice_set, cy_device);

static char *
cy_probe(config_id, device_id)
	pcici_t config_id;
	pcidi_t device_id;
{
	if ((device_id & 0xffff) == CY_VENDORID_CYCLADES &&
	    ((device_id >> 16) == CY_DEVICEID_CYCLOM_Y_1 ||
	     (device_id >> 16) == CY_DEVICEID_CYCLOM_Y_2))
		return ("Cyclades Cyclom-Y Serial Adapter");
	return NULL;
}

static void
cy_attach(config_id, unit)
	pcici_t config_id;
	int unit;
{
	vm_offset_t paddr;
	void *vaddr;
	u_int32_t ioport;
	int adapter;

	ioport = (u_int32_t) pci_conf_read(config_id, CY_PCI_BASE_ADDR1) & ~0x3;
	paddr = pci_conf_read(config_id, CY_PCI_BASE_ADDR2) & ~0xf;
#if 0
	if (!pci_map_mem(config_id, CY_PCI_BASE_ADDR2, &vaddr, &paddr)) {
		printf("cy%d: couldn't map shared memory\n", unit);
		return;
	};
#endif
	vaddr = pmap_mapdev(paddr, 0x4000);

	adapter = cyattach_common(vaddr, 1);
	if (adapter < 0) {
		/*
		 * No ports found. Release resources and punt.
		 */
		printf("cy%d: no ports found!", unit);
		goto fail;
	}

	/*
	 * Allocate our interrupt.
	 * XXX	Using the ISA interrupt handler directly is a bit of a violation
	 *	since it doesn't actually take the same argument. For PCI, the
	 *	argument is a void * token, but for ISA it is a unit. Since
	 *	there is no overlap in PCI/ISA unit numbers for this driver, and
	 *	since the ISA driver must handle the interrupt anyway, we use
	 *	the unit number as the token even for PCI.
	 */
	if (!pci_map_int(config_id, (pci_inthand_t *)cyintr, (void *)adapter, &tty_imask)) {
		printf("cy%d: couldn't map interrupt\n", unit);
		goto fail;
	}
	/*
	 * Enable the "local" interrupt input to generate a
	 * PCI interrupt.
	 */
	outw(ioport + CY_PLX_ICS, inw(CY_PLX_ICS) |
	    CY_PLX_ICS_IENABLE | CY_PLX_ICS_LOCAL_IENABLE);

	return;

fail:
	/* XXX should release any allocated virtual memory */
	return;
}

#endif /* NPCI > 0 */
