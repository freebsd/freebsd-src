/*
 * Copyright (c) 2000, 2001 Richard Hodges and Matriplex, inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Matriplex, inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 *
 * This driver is derived from the Nicstar driver by Mark Tinguely, and
 * some of the original driver still exists here.  Those portions are...
 *   Copyright (c) 1996, 1997, 1998, 1999 Mark Tinguely
 *   All rights reserved.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <sys/bus.h>
#include <sys/conf.h>

#include <sys/module.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>

#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/idt/idtreg.h>
#include <dev/idt/idtvar.h>

#define	IDT_VID		0x111d
#define	IDT_NICSTAR_DID	0x0001

struct pci_type {
	u_int16_t	pci_vid;
	u_int16_t	pci_did;
	char *		pci_name;
} pci_devs[] = {
	{ IDT_VID, IDT_NICSTAR_DID, "IDT IDT77201/211 NICStAR ATM Adapter" },
	{ 0, 0, NULL }
};

uma_zone_t	idt_nif_zone;
uma_zone_t	idt_vcc_zone;

static int	idt_probe	(device_t);
static int	idt_attach	(device_t);
static int	idt_detach	(device_t);
static int	idt_shutdown	(device_t);
static void	idt_free	(device_t);
static int	idt_modevent	(module_t, int, void *);

static int
idt_probe(device_t dev)
{
	struct pci_type *t = pci_devs;

	while(t->pci_name != NULL) {
		if ((pci_get_vendor(dev) == t->pci_vid) &&
		    (pci_get_device(dev) == t->pci_did)) {
			device_set_desc(dev, t->pci_name);
			return(0);
		}
		t++;
	}

        return(ENXIO);
}

/******************************************************************************
 *
 *  Attach device
 *
 * Date first: 11/14/2000  last: 06/10/2001
 */

static int
idt_attach(device_t dev)
{
	struct idt_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	error = 0;

	pci_enable_busmaster(dev);

	/* count = 2 (times 32 PCI clocks) */
	pci_write_config(dev, PCIR_LATTIMER, 0x20, 1);

	/* Map IDT registers */
	sc->mem_rid = 0x14;
	sc->mem_type = SYS_RES_MEMORY;
	sc->mem = bus_alloc_resource(dev, sc->mem_type, &sc->mem_rid,
				     0, ~0, 1, RF_ACTIVE);
	if (sc->mem == NULL) {
		device_printf(dev, "could not map registers.\n");
		error = ENXIO;
		goto fail;
	}
	sc->bustag = rman_get_bustag(sc->mem);
	sc->bushandle = rman_get_bushandle(sc->mem);

	/* Map interrupt */
	sc->irq_rid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid,
				     0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq == NULL) {
		device_printf(dev, "could not map interrupt.\n");
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET, nicstar_intr,
			       sc, &sc->irq_ih);
	if (error) {
		device_printf(dev, "could not setup irq.\n");
		error = ENXIO;
		goto fail;
	}

	sc->virt_baseaddr = (vm_offset_t)rman_get_virtual(sc->mem);
	sc->cmd_reg = sc->virt_baseaddr + REGCMD;	/* old reg */
	sc->stat_reg = sc->virt_baseaddr + REGSTAT;	/* old reg */
	sc->reg_cmd = (u_long *)(sc->virt_baseaddr + REGCMD);
	sc->reg_stat = (u_long *)(sc->virt_baseaddr + REGSTAT);
	sc->reg_cfg = (u_long *)(sc->virt_baseaddr + REGCFG);
	sc->reg_data = (u_long *)(sc->virt_baseaddr + 0);
	sc->reg_tsqh = (u_long *)(sc->virt_baseaddr + REGTSQH);
	sc->reg_gp = (u_long *)(sc->virt_baseaddr + REGGP);
	sc->pci_rev = pci_get_revid(dev);
	sc->timer_wrap = 0;

	callout_handle_init(&sc->ch);

	phys_init(sc);		/* initialize the hardware */
	nicstar_init(sc);	/* allocate and initialize */
	
	error = idt_harp_init(sc);
	if (error)
		goto fail;

	return (0);
fail:
	idt_free(dev);
	return (error);
}

/******************************************************************************
 *
 *  Detach device
 *
 * Date first: 11/14/2000  last: 11/14/2000
 */

static int
idt_detach(device_t dev)
{
	struct idt_softc *sc;
	int error;

	sc = device_get_softc(dev);
	error = 0;

        /*
         * De-Register this interface with ATM core services
         */
	error = atm_physif_deregister(&sc->iu_cmn);

	idt_device_stop(sc);	/* Stop the device */

	/*
	 * Lock out all device interupts.
	 */
	DEVICE_LOCK(&sc->iu_cmn);
	idt_free(dev);
	idt_release_mem(sc);
	DEVICE_UNLOCK(&sc->iu_cmn);

	return (error);
}

/******************************************************************************
 *
 *  Shutdown device
 *
 * Date first: 11/14/2000  last: 11/14/2000
 */

static int
idt_shutdown(device_t dev)
{

	struct idt_softc *sc;

	sc = device_get_softc(dev);

	idt_device_stop(sc);	/* Stop the device */

	return (0);
}

static void
idt_free (device_t dev)
{
	struct idt_softc *sc;

	sc = device_get_softc(dev);

	if (sc->irq_ih)
		bus_teardown_intr(dev, sc->irq, sc->irq_ih);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);
	if (sc->mem)
		bus_release_resource(dev, sc->mem_type, sc->mem_rid, sc->mem);

	return;
}

static int
idt_modevent (module_t mod, int type, void *data)
{
	int error;

	error = 0;

	switch (type) {
	case MOD_LOAD:
		idt_nif_zone = uma_zcreate("idt nif",
			sizeof(struct atm_nif),
			NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (idt_nif_zone == NULL)
			panic("hfa_modevent:uma_zcreate nif");
		uma_zone_set_max(idt_nif_zone, 20);

		idt_vcc_zone = uma_zcreate("idt vcc",
			sizeof(Idt_vcc),
			NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (idt_vcc_zone == NULL)
			panic("hfa_modevent: uma_zcreate vcc");
		uma_zone_set_max(idt_vcc_zone, 100);

		break;

	case MOD_UNLOAD:
		uma_zdestroy(idt_nif_zone);
		uma_zdestroy(idt_vcc_zone);

		break;
	default:
		break;
	}

	return (error);
}

static device_method_t idt_methods[] = {
	DEVMETHOD(device_probe,		idt_probe),
	DEVMETHOD(device_attach,	idt_attach),
	DEVMETHOD(device_detach,	idt_detach),
	DEVMETHOD(device_shutdown,	idt_shutdown),
	{0, 0}
};

static driver_t idt_driver = {
	"idt",
	idt_methods,
	sizeof(struct idt_softc)
};

static devclass_t idt_devclass;

DRIVER_MODULE(idt, pci, idt_driver, idt_devclass, idt_modevent, 0);
MODULE_VERSION(idt, 1);
