/*-
 * Copyright (c) 2012, Gavin Atkinson <gavin@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#if defined(__i386__) || defined(__amd64__) || defined(__powerpc__)
#include <machine/intr_machdep.h>
#endif

#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include "pcib_if.h"
#include "pci_if.h"


static struct resource_spec hotplug_res_spec_msi[] = {
	{ SYS_RES_IRQ,		1,		RF_ACTIVE },
	{ -1,			0,		0 }
};


static void
pci_slot_status_print(device_t pcib)
{
	struct pci_devinfo *dinfo;
	int pos;

	dinfo = device_get_ivars(pcib);
	pos = dinfo->cfg.pcie.pcie_location;
	device_printf(pcib, "... LINK_STA=0x%b\n",
	    pci_read_config(pcib, pos + PCIER_LINK_STA, 2),
	    "\020"
	    "\001<b0>"
	    "\002<b1>"
	    "\003<b3>"
	    "\004<b3>"
	    "\005<b4>"
	    "\006<b5>"
	    "\007<b6>"
	    "\010<b7>"
	    "\011<b8>"
	    "\012<b9>"
	    "\013Undef"
	    "\014LinkTrain"
	    "\015SlotClkConfig"
	    "\016DLLLinkActive"
	    "\017LinkBWManStat"
	    "\020LinkAutonBwStat"
	    );
//	device_printf(pcib, "... SLOT_CAP=0x%x\n",
//	    pci_read_config(pcib, pos+PCIER_SLOT_CAP, 4));
	device_printf(pcib, "... SLOT_CTL=0x%b\n",
	    pci_read_config(pcib, pos + PCIER_SLOT_CTL, 2),
	    "\020"
	    "\001AttnButtPressEn"
	    "\002PowerFaultDetEn"
	    "\003MRLSensChgEn"
	    "\004PresDetChgEn"
	    "\005CmdCompIntEn"
	    "\006HotPlugIntEn"
	    "\007AttnIndCtl1"
	    "\010AttnIndCtl2"
	    "\011PwrIndCtl1"
	    "\012PwrIndCtl2"
	    "\013PwrCtrlrCtl"
	    "\014ElecMechIntCtl"
	    "\015DLLStatChEn"
	    "\016<b13>"
	    "\017<b14>"
	    "\020<b15>"
	    );
	device_printf(pcib, "... SLOT_STA=0x%b\n",
	    pci_read_config(pcib, pos + PCIER_SLOT_STA, 2),
	    "\020"
	    "\001AttnButtPress"
	    "\002PowerFaultDet"
	    "\003MRLSensChg"
	    "\004PresDetChg"
	    "\005CmdComplete"
	    "\006MRLSensState"
	    "\007PresDetState"
	    "\010ElecMechIntState"
	    "\011DLLState"
	    "\012<b9>"
	    "\013<b10>"
	    "\014<b11>"
	    "\015<b12>"
	    "\016<b13>"
	    "\017<b14>"
	    "\020<b15>"
	    );
}

static void
pci_hotplug_intr_task(void *arg, int npending)
{
	device_t dev = arg;
	device_t pcib = device_get_parent(dev);
	device_t *devlistp;
	struct pci_devinfo *dinfo;
	int busno, devcnt, domain, i, pos;
	int linksta, slotsta;

	dinfo = device_get_ivars(pcib);
	pos = dinfo->cfg.pcie.pcie_location;

//	mtx_lock(&dinfo->cfg.hp.hp_mtx);

	linksta = pci_read_config(pcib, pos + PCIER_LINK_STA, 2);
	slotsta = pci_read_config(pcib, pos + PCIER_SLOT_STA, 2);
	pci_slot_status_print(pcib);
/* XXXGA: HACK AHEAD */
	if (slotsta & PCIEM_SLOT_STA_DLLSC) {
		if ((linksta & PCIEM_LINK_STA_DL_ACTIVE) && dinfo->cfg.hp.hp_cnt == 0) {
			dinfo->cfg.hp.hp_cnt=1;
			/* delay really for DLLSC */
			DELAY(100000);	/* section 6.7.3.3 */
			printf("Hotplug: Attaching children\n");
			mtx_lock(&Giant);
			domain = pcib_get_domain(dev);
			busno = pcib_get_bus(dev);
			pci_add_children(dev, domain, busno,
			    sizeof(struct pci_devinfo));
			(void)bus_generic_attach(dev);
			mtx_unlock(&Giant);
		} else if (((linksta & PCIEM_LINK_STA_DL_ACTIVE) == 0) && dinfo->cfg.hp.hp_cnt == 1) {
			printf("Hotplug: Detaching children\n");
			mtx_lock(&Giant);
			/* XXXGA error checking */
			(void)bus_generic_detach(dev);
			device_get_children(dev, &devlistp, &devcnt);
			for (i = 0; i < devcnt; i++)
				device_delete_child(dev, devlistp[i]);
			free(devlistp, M_TEMP);
			mtx_unlock(&Giant);
			dinfo->cfg.hp.hp_cnt=0;
		} else
			printf("Hotplug: Ignoring\n");
	}
//	mtx_unlock(&dinfo->cfg.hp.hp_mtx);
}

static int
pci_hotplug_intr(void *arg)
{
	device_t dev = arg;
	device_t pcib = device_get_parent(dev);
	struct pci_devinfo *dinfo;

	device_printf(dev, "Received interrupt!\n");
//	pci_slot_status_print(pcib);
	dinfo = device_get_ivars(pcib);
	taskqueue_enqueue_fast(taskqueue_fast, &dinfo->cfg.hp.hp_inttask);

	return (FILTER_HANDLED);
}

void
pci_hotplug_init(device_t dev)
{
	device_t pcib = device_get_parent(dev);
	struct pci_devinfo *dinfo;
	int error, flags, irq, msic, pos;

	dinfo = device_get_ivars(pcib);
	pos = dinfo->cfg.pcie.pcie_location;
	device_printf(dev, "dinfo=%p, pos=0x%x\n", dinfo, pos);
	if (pos != 0) {
		device_printf(dev, "Hotplug?\n");
		flags = pci_read_config(pcib, pos + PCIER_FLAGS, 2);
		device_printf(dev, "... FLAGS = 0x%x\n", flags);
		if (flags & PCIEM_FLAGS_SLOT) {
			mtx_init(&dinfo->cfg.hp.hp_mtx,
			    device_get_nameunit(dev), "pciehp", MTX_DEF);
			device_printf(dev, "... is slot!\n");
/* XXX GAV: Check for SLOT_CAP_HPC here */
			pci_slot_status_print(pcib);
			irq = (flags & PCIEM_FLAGS_IRQ) >> 9;
			device_printf(dev, "IRQ = %d\n", irq);

			device_printf(dev, "MSI count self %d parent %d\n", pci_msi_count(dev), pci_msi_count(pcib));
			device_printf(dev, "MSI-X count self %d parent %d\n", pci_msix_count(dev), pci_msix_count(pcib));

			msic = pci_msi_count(pcib);
			if (msic == 1) {
				if (pci_alloc_msi(pcib, &msic) == 0) {
					if (msic == 1) {
						device_printf(dev, "Using %d MSI messages\n",
						    msic);
						dinfo->cfg.pcie.pcie_irq_spec = hotplug_res_spec_msi;
					} else {
						device_printf(dev, "Error: %d MSI messages\n",
						    msic);
						pci_release_msi(dev);
					}
				}
			}
/* XXX GAV: Am currently ignoring "irq" */
			error = bus_alloc_resources(pcib, dinfo->cfg.pcie.pcie_irq_spec, dinfo->cfg.pcie.pcie_res_irq);
			if (error) {
				device_printf(dev, "couldn't allocate IRQ resources, %d\n", error);
			} else {
				error = bus_setup_intr(pcib, dinfo->cfg.pcie.pcie_res_irq[0],
				    INTR_TYPE_AV | INTR_MPSAFE, pci_hotplug_intr, NULL, dev,
				    &dinfo->cfg.pcie.pcie_intrhand[0]);
				if (error) {
					device_printf(dev, "couldn't set up IRQ resources, %d\n", error);
				}
			}
			TASK_INIT(&dinfo->cfg.hp.hp_inttask, 0, pci_hotplug_intr_task, dev);
			/* XXXGA 6.7.3.1 don't enable things the slot doesn't support */
			flags = pci_read_config(pcib, pos + PCIER_SLOT_CTL, 2);
			flags |= PCIEM_SLOT_CTL_PDCE | PCIEM_SLOT_CTL_MRLSCE | PCIEM_SLOT_CTL_HPIE | PCIEM_SLOT_CTL_DLLSCE;
			pci_write_config(pcib, pos + PCIER_SLOT_CTL, flags, 2);
			device_printf(dev, "Enabled interrupts\n");
			pci_slot_status_print(pcib);
		}
	}
}

