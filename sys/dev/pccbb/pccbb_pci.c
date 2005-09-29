/*-
 * Copyright (c) 2002-2004 M. Warner Losh.
 * Copyright (c) 2000-2001 Jonathan Chen.
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
 */

/*-
 * Copyright (c) 1998, 1999 and 2000
 *      HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by HAYAKAWA Koichi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Driver for PCI to CardBus Bridge chips
 *
 * References:
 *  TI Datasheets:
 *   http://www-s.ti.com/cgi-bin/sc/generic2.cgi?family=PCI+CARDBUS+CONTROLLERS
 *
 * Written by Jonathan Chen <jon@freebsd.org>
 * The author would like to acknowledge:
 *  * HAYAKAWA Koichi: Author of the NetBSD code for the same thing
 *  * Warner Losh: Newbus/newcard guru and author of the pccard side of things
 *  * YAMAMOTO Shigeru: Author of another FreeBSD cardbus driver
 *  * David Cross: Author of the initial ugly hack for a specific cardbus card
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/module.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/clock.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include <dev/exca/excareg.h>
#include <dev/exca/excavar.h>

#include <dev/pccbb/pccbbreg.h>
#include <dev/pccbb/pccbbvar.h>

#include "power_if.h"
#include "card_if.h"
#include "pcib_if.h"

#define	DPRINTF(x) do { if (cbb_debug) printf x; } while (0)
#define	DEVPRINTF(x) do { if (cbb_debug) device_printf x; } while (0)

#define	PCI_MASK_CONFIG(DEV,REG,MASK,SIZE)				\
	pci_write_config(DEV, REG, pci_read_config(DEV, REG, SIZE) MASK, SIZE)
#define	PCI_MASK2_CONFIG(DEV,REG,MASK1,MASK2,SIZE)			\
	pci_write_config(DEV, REG, (					\
		pci_read_config(DEV, REG, SIZE) MASK1) MASK2, SIZE)

static void cbb_chipinit(struct cbb_softc *sc);

static struct yenta_chipinfo {
	uint32_t yc_id;
	const	char *yc_name;
	int	yc_chiptype;
} yc_chipsets[] = {
	/* Texas Instruments chips */
	{PCIC_ID_TI1031, "TI1031 PCI-PC Card Bridge", CB_TI113X},
	{PCIC_ID_TI1130, "TI1130 PCI-CardBus Bridge", CB_TI113X},
	{PCIC_ID_TI1131, "TI1131 PCI-CardBus Bridge", CB_TI113X},

	{PCIC_ID_TI1210, "TI1210 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1211, "TI1211 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1220, "TI1220 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1221, "TI1221 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1225, "TI1225 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1250, "TI1250 PCI-CardBus Bridge", CB_TI125X},
	{PCIC_ID_TI1251, "TI1251 PCI-CardBus Bridge", CB_TI125X},
	{PCIC_ID_TI1251B,"TI1251B PCI-CardBus Bridge",CB_TI125X},
	{PCIC_ID_TI1260, "TI1260 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1260B,"TI1260B PCI-CardBus Bridge",CB_TI12XX},
	{PCIC_ID_TI1410, "TI1410 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1420, "TI1420 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1421, "TI1421 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1450, "TI1450 PCI-CardBus Bridge", CB_TI125X}, /*SIC!*/
	{PCIC_ID_TI1451, "TI1451 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1510, "TI1510 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI1520, "TI1520 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI4410, "TI4410 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI4450, "TI4450 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI4451, "TI4451 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI4510, "TI4510 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI6411, "TI6411 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI6420, "TI6420 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI6420SC, "TI6420 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI7410, "TI7410 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI7510, "TI7510 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI7610, "TI7610 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI7610M, "TI7610 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI7610SD, "TI7610 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_TI7610MS, "TI7610 PCI-CardBus Bridge", CB_TI12XX},

	/* ENE */
	{PCIC_ID_ENE_CB710, "ENE CB710 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_ENE_CB720, "ENE CB720 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_ENE_CB1211, "ENE CB1211 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_ENE_CB1225, "ENE CB1225 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_ENE_CB1410, "ENE CB1410 PCI-CardBus Bridge", CB_TI12XX},
	{PCIC_ID_ENE_CB1420, "ENE CB1420 PCI-CardBus Bridge", CB_TI12XX},

	/* Ricoh chips */
	{PCIC_ID_RICOH_RL5C465, "RF5C465 PCI-CardBus Bridge", CB_RF5C46X},
	{PCIC_ID_RICOH_RL5C466, "RF5C466 PCI-CardBus Bridge", CB_RF5C46X},
	{PCIC_ID_RICOH_RL5C475, "RF5C475 PCI-CardBus Bridge", CB_RF5C47X},
	{PCIC_ID_RICOH_RL5C476, "RF5C476 PCI-CardBus Bridge", CB_RF5C47X},
	{PCIC_ID_RICOH_RL5C477, "RF5C477 PCI-CardBus Bridge", CB_RF5C47X},
	{PCIC_ID_RICOH_RL5C478, "RF5C478 PCI-CardBus Bridge", CB_RF5C47X},

	/* Toshiba products */
	{PCIC_ID_TOPIC95, "ToPIC95 PCI-CardBus Bridge", CB_TOPIC95},
	{PCIC_ID_TOPIC95B, "ToPIC95B PCI-CardBus Bridge", CB_TOPIC95},
	{PCIC_ID_TOPIC97, "ToPIC97 PCI-CardBus Bridge", CB_TOPIC97},
	{PCIC_ID_TOPIC100, "ToPIC100 PCI-CardBus Bridge", CB_TOPIC97},

	/* Cirrus Logic */
	{PCIC_ID_CLPD6832, "CLPD6832 PCI-CardBus Bridge", CB_CIRRUS},
	{PCIC_ID_CLPD6833, "CLPD6833 PCI-CardBus Bridge", CB_CIRRUS},
	{PCIC_ID_CLPD6834, "CLPD6834 PCI-CardBus Bridge", CB_CIRRUS},

	/* 02Micro */
	{PCIC_ID_OZ6832, "O2Micro OZ6832/6833 PCI-CardBus Bridge", CB_O2MICRO},
	{PCIC_ID_OZ6860, "O2Micro OZ6836/6860 PCI-CardBus Bridge", CB_O2MICRO},
	{PCIC_ID_OZ6872, "O2Micro OZ6812/6872 PCI-CardBus Bridge", CB_O2MICRO},
	{PCIC_ID_OZ6912, "O2Micro OZ6912/6972 PCI-CardBus Bridge", CB_O2MICRO},
	{PCIC_ID_OZ6922, "O2Micro OZ6922 PCI-CardBus Bridge", CB_O2MICRO},
	{PCIC_ID_OZ6933, "O2Micro OZ6933 PCI-CardBus Bridge", CB_O2MICRO},
	{PCIC_ID_OZ711E1, "O2Micro OZ711E1 PCI-CardBus Bridge", CB_O2MICRO},
	{PCIC_ID_OZ711M1, "O2Micro OZ711M1 PCI-CardBus Bridge", CB_O2MICRO},

	/* sentinel */
	{0 /* null id */, "unknown", CB_UNKNOWN},
};

/************************************************************************/
/* Probe/Attach								*/
/************************************************************************/

static int
cbb_chipset(uint32_t pci_id, const char **namep)
{
	struct yenta_chipinfo *ycp;

	for (ycp = yc_chipsets; ycp->yc_id != 0 && pci_id != ycp->yc_id; ++ycp)
		continue;
	if (namep != NULL)
		*namep = ycp->yc_name;
	return (ycp->yc_chiptype);
}

static int
cbb_pci_probe(device_t brdev)
{
	const char *name;
	uint32_t progif;
	uint32_t subclass;

	/*
	 * Do we know that we support the chipset?  If so, then we
	 * accept the device.
	 */
	if (cbb_chipset(pci_get_devid(brdev), &name) != CB_UNKNOWN) {
		device_set_desc(brdev, name);
		return (BUS_PROBE_DEFAULT);
	}

	/*
	 * We do support generic CardBus bridges.  All that we've seen
	 * to date have progif 0 (the Yenta spec, and successors mandate
	 * this).
	 */
	subclass = pci_get_subclass(brdev);
	progif = pci_get_progif(brdev);
	if (subclass == PCIS_BRIDGE_CARDBUS && progif == 0) {
		device_set_desc(brdev, "PCI-CardBus Bridge");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

/*
 * Still need this because the pci code only does power for type 0
 * header devices.
 */
static void
cbb_powerstate_d0(device_t dev)
{
	u_int32_t membase, irq;

	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		/* Save important PCI config data. */
		membase = pci_read_config(dev, CBBR_SOCKBASE, 4);
		irq = pci_read_config(dev, PCIR_INTLINE, 4);

		/* Reset the power state. */
		device_printf(dev, "chip is in D%d power mode "
		    "-- setting to D0\n", pci_get_powerstate(dev));

		pci_set_powerstate(dev, PCI_POWERSTATE_D0);

		/* Restore PCI config data. */
		pci_write_config(dev, CBBR_SOCKBASE, membase, 4);
		pci_write_config(dev, PCIR_INTLINE, irq, 4);
	}
}

/*
 * Print out the config space
 */
static void
cbb_print_config(device_t dev)
{
	int i;
	
	device_printf(dev, "PCI Configuration space:");
	for (i = 0; i < 256; i += 4) {
		if (i % 16 == 0)
			printf("\n  0x%02x: ", i);
		printf("0x%08x ", pci_read_config(dev, i, 4));
	}
	printf("\n");
}

static int
cbb_pci_attach(device_t brdev)
{
	static int curr_bus_number = 2; /* XXX EVILE BAD (see below) */
	struct cbb_softc *sc = (struct cbb_softc *)device_get_softc(brdev);
	int rid, bus, pribus;
	device_t parent;

	parent = device_get_parent(brdev);
	mtx_init(&sc->mtx, device_get_nameunit(brdev), "cbb", MTX_DEF);
	cv_init(&sc->cv, "cbb cv");
	cv_init(&sc->powercv, "cbb cv");
	sc->chipset = cbb_chipset(pci_get_devid(brdev), NULL);
	sc->dev = brdev;
	sc->cbdev = NULL;
	sc->exca[0].pccarddev = NULL;
	sc->secbus = pci_read_config(brdev, PCIR_SECBUS_2, 1);
	sc->subbus = pci_read_config(brdev, PCIR_SUBBUS_2, 1);
	SLIST_INIT(&sc->rl);
	STAILQ_INIT(&sc->intr_handlers);
	cbb_powerstate_d0(brdev);

	rid = CBBR_SOCKBASE;
	sc->base_res = bus_alloc_resource_any(brdev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->base_res) {
		device_printf(brdev, "Could not map register memory\n");
		mtx_destroy(&sc->mtx);
		cv_destroy(&sc->cv);
		return (ENOMEM);
	} else {
		DEVPRINTF((brdev, "Found memory at %08lx\n",
		    rman_get_start(sc->base_res)));
	}
		
	sc->bst = rman_get_bustag(sc->base_res);
	sc->bsh = rman_get_bushandle(sc->base_res);
	exca_init(&sc->exca[0], brdev, sc->bst, sc->bsh, CBB_EXCA_OFFSET);
	sc->exca[0].flags |= EXCA_HAS_MEMREG_WIN;
	sc->exca[0].chipset = EXCA_CARDBUS;
	sc->chipinit = cbb_chipinit;
	sc->chipinit(sc);

	/*
	 * This is a gross hack.  We should be scanning the entire pci
	 * tree, assigning bus numbers in a way such that we (1) can
	 * reserve 1 extra bus just in case and (2) all sub busses 
	 * are in an appropriate range.
	 */
	bus = pci_read_config(brdev, PCIR_SECBUS_2, 1);
	pribus = pcib_get_bus(parent);
	DEVPRINTF((brdev, "Secondary bus is %d\n", bus));
	if (bus == 0) {
		if (curr_bus_number <= pribus)
			curr_bus_number = pribus + 1;
		if (pci_read_config(brdev, PCIR_PRIBUS_2, 1) != pribus) {
			DEVPRINTF((brdev, "Setting primary bus to %d\n", pribus));
			pci_write_config(brdev, PCIR_PRIBUS_2, pribus, 1);
		}
		bus = curr_bus_number;
		DEVPRINTF((brdev, "Secondary bus set to %d subbus %d\n", bus,
		    bus + 1));
		sc->secbus = bus;
		sc->subbus = bus + 1;
		pci_write_config(brdev, PCIR_SECBUS_2, bus, 1);
		pci_write_config(brdev, PCIR_SUBBUS_2, bus + 1, 1);
		curr_bus_number += 2;
	}

	/* attach children */
	sc->cbdev = device_add_child(brdev, "cardbus", -1);
	if (sc->cbdev == NULL)
		DEVPRINTF((brdev, "WARNING: cannot add cardbus bus.\n"));
	else if (device_probe_and_attach(sc->cbdev) != 0)
		DEVPRINTF((brdev, "WARNING: cannot attach cardbus bus!\n"));

	sc->exca[0].pccarddev = device_add_child(brdev, "pccard", -1);
	if (sc->exca[0].pccarddev == NULL)
		DEVPRINTF((brdev, "WARNING: cannot add pccard bus.\n"));
	else if (device_probe_and_attach(sc->exca[0].pccarddev) != 0)
		DEVPRINTF((brdev, "WARNING: cannot attach pccard bus.\n"));

	/* Map and establish the interrupt. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(brdev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		printf("cbb: Unable to map IRQ...\n");
		goto err;
	}

	if (bus_setup_intr(brdev, sc->irq_res, INTR_TYPE_AV | INTR_MPSAFE,
	    cbb_intr, sc, &sc->intrhand)) {
		device_printf(brdev, "couldn't establish interrupt");
		goto err;
	}

	/* reset 16-bit pcmcia bus */
	exca_clrb(&sc->exca[0], EXCA_INTR, EXCA_INTR_RESET);

	/* turn off power */
	cbb_power(brdev, CARD_OFF);

	/* CSC Interrupt: Card detect interrupt on */
	cbb_setb(sc, CBB_SOCKET_MASK, CBB_SOCKET_MASK_CD);

	/* reset interrupt */
	cbb_set(sc, CBB_SOCKET_EVENT, cbb_get(sc, CBB_SOCKET_EVENT));

	if (bootverbose)
		cbb_print_config(brdev);

	/* Start the thread */
	if (kthread_create(cbb_event_thread, sc, &sc->event_thread, 0, 0,
	    "%s", device_get_nameunit(brdev))) {
		device_printf(brdev, "unable to create event thread.\n");
		panic("cbb_create_event_thread");
	}
	return (0);
err:
	if (sc->irq_res)
		bus_release_resource(brdev, SYS_RES_IRQ, 0, sc->irq_res);
	if (sc->base_res) {
		bus_release_resource(brdev, SYS_RES_MEMORY, CBBR_SOCKBASE,
		    sc->base_res);
	}
	mtx_destroy(&sc->mtx);
	cv_destroy(&sc->cv);
	return (ENOMEM);
}

static void
cbb_chipinit(struct cbb_softc *sc)
{
	uint32_t mux, sysctrl, reg;

	/* Set CardBus latency timer */
	if (pci_read_config(sc->dev, PCIR_SECLAT_1, 1) < 0x20)
		pci_write_config(sc->dev, PCIR_SECLAT_1, 0x20, 1);

	/* Set PCI latency timer */
	if (pci_read_config(sc->dev, PCIR_LATTIMER, 1) < 0x20)
		pci_write_config(sc->dev, PCIR_LATTIMER, 0x20, 1);

	/* Enable memory access */
	PCI_MASK_CONFIG(sc->dev, PCIR_COMMAND,
	    | PCIM_CMD_MEMEN
	    | PCIM_CMD_PORTEN
	    | PCIM_CMD_BUSMASTEREN, 2);

	/* disable Legacy IO */
	switch (sc->chipset) {
	case CB_RF5C46X:
		PCI_MASK_CONFIG(sc->dev, CBBR_BRIDGECTRL,
		    & ~(CBBM_BRIDGECTRL_RL_3E0_EN |
		    CBBM_BRIDGECTRL_RL_3E2_EN), 2);
		break;
	default:
		pci_write_config(sc->dev, CBBR_LEGACY, 0x0, 4);
		break;
	}

	/* Use PCI interrupt for interrupt routing */
	PCI_MASK2_CONFIG(sc->dev, CBBR_BRIDGECTRL,
	    & ~(CBBM_BRIDGECTRL_MASTER_ABORT |
	    CBBM_BRIDGECTRL_INTR_IREQ_ISA_EN),
	    | CBBM_BRIDGECTRL_WRITE_POST_EN,
	    2);

	/*
	 * XXX this should be a function table, ala OLDCARD.  This means
	 * that we could more easily support ISA interrupts for pccard
	 * cards if we had to.
	 */
	switch (sc->chipset) {
	case CB_TI113X:
		/*
		 * The TI 1031, TI 1130 and TI 1131 all require another bit
		 * be set to enable PCI routing of interrupts, and then
		 * a bit for each of the CSC and Function interrupts we
		 * want routed.
		 */
		PCI_MASK_CONFIG(sc->dev, CBBR_CBCTRL,
		    | CBBM_CBCTRL_113X_PCI_INTR |
		    CBBM_CBCTRL_113X_PCI_CSC | CBBM_CBCTRL_113X_PCI_IRQ_EN,
		    1);
		PCI_MASK_CONFIG(sc->dev, CBBR_DEVCTRL,
		    & ~(CBBM_DEVCTRL_INT_SERIAL |
		    CBBM_DEVCTRL_INT_PCI), 1);
		break;
	case CB_TI12XX:
		/*
		 * Some TI 12xx (and [14][45]xx) based pci cards
		 * sometimes have issues with the MFUNC register not
		 * being initialized due to a bad EEPROM on board.
		 * Laptops that this matters on have this register
		 * properly initialized.
		 *
		 * The TI125X parts have a different register.
		 */
		mux = pci_read_config(sc->dev, CBBR_MFUNC, 4);
		sysctrl = pci_read_config(sc->dev, CBBR_SYSCTRL, 4);
		if (mux == 0) {
			mux = (mux & ~CBBM_MFUNC_PIN0) |
			    CBBM_MFUNC_PIN0_INTA;
			if ((sysctrl & CBBM_SYSCTRL_INTRTIE) == 0)
				mux = (mux & ~CBBM_MFUNC_PIN1) |
				    CBBM_MFUNC_PIN1_INTB;
			pci_write_config(sc->dev, CBBR_MFUNC, mux, 4);
		}
		/*FALLTHROUGH*/
	case CB_TI125X:
		/*
		 * Disable zoom video.  Some machines initialize this
		 * improperly and exerpience has shown that this helps
		 * prevent strange behavior.
		 */
		pci_write_config(sc->dev, CBBR_MMCTRL, 0, 4);
		break;
	case CB_O2MICRO:
		/*
		 * Issue #1: INT# generated at the same time as
		 * selected ISA IRQ.  When IREQ# or STSCHG# is active,
		 * in addition to the ISA IRQ being generated, INT#
		 * will also be generated at the same time.
		 *
		 * Some of the older controllers have an issue in
		 * which the slot's PCI INT# will be asserted whenever
		 * IREQ# or STSCGH# is asserted even if ExCA registers
		 * 03h or 05h have an ISA IRQ selected.
		 *
		 * The fix for this issue, which will work for any
		 * controller (old or new), is to set ExCA registers
		 * 3Ah (slot 0) & 7Ah (slot 1) bits 7:4 = 1010b.
		 * These bits are undocumented.  By setting this
		 * register (of each slot) to '1010xxxxb' a routing of
		 * IREQ# to INTC# and STSCHG# to INTC# is selected.
		 * Since INTC# isn't connected there will be no
		 * unexpected PCI INT when IREQ# or STSCHG# is active.
		 * However, INTA# (slot 0) or INTB# (slot 1) will
		 * still be correctly generated if NO ISA IRQ is
		 * selected (ExCA regs 03h or 05h are cleared).
		 */
		reg = exca_getb(&sc->exca[0], EXCA_O2MICRO_CTRL_C);
		reg = (reg & 0x0f) |
		    EXCA_O2CC_IREQ_INTC | EXCA_O2CC_STSCHG_INTC;
		exca_putb(&sc->exca[0], EXCA_O2MICRO_CTRL_C, reg);

		break;
	case CB_TOPIC97:
		/*
		 * Disable Zoom Video, ToPIC 97, 100.
		 */
		pci_write_config(sc->dev, CBBR_TOPIC_ZV_CONTROL, 0, 1);
		/*
		 * ToPIC 97, 100
		 * At offset 0xa1: INTERRUPT CONTROL register
		 * 0x1: Turn on INT interrupts.
		 */
		PCI_MASK_CONFIG(sc->dev, CBBR_TOPIC_INTCTRL,
		    | CBBM_TOPIC_INTCTRL_INTIRQSEL, 1);
		goto topic_common;
	case CB_TOPIC95:
		/*
		 * SOCKETCTRL appears to be TOPIC 95/B specific
		 */
		PCI_MASK_CONFIG(sc->dev, CBBR_TOPIC_SOCKETCTRL,
		    | CBBM_TOPIC_SOCKETCTRL_SCR_IRQSEL, 4);

	topic_common:;
		/*
		 * At offset 0xa0: SLOT CONTROL
		 * 0x80 Enable CardBus Functionality
		 * 0x40 Enable CardBus and PC Card registers
		 * 0x20 Lock ID in exca regs
		 * 0x10 Write protect ID in config regs
		 * Clear the rest of the bits, which defaults the slot
		 * in legacy mode to 0x3e0 and offset 0. (legacy
		 * mode is determined elsewhere)
		 */
		pci_write_config(sc->dev, CBBR_TOPIC_SLOTCTRL,
		    CBBM_TOPIC_SLOTCTRL_SLOTON |
		    CBBM_TOPIC_SLOTCTRL_SLOTEN |
		    CBBM_TOPIC_SLOTCTRL_ID_LOCK |
		    CBBM_TOPIC_SLOTCTRL_ID_WP, 1);

		/*
		 * At offset 0xa3 Card Detect Control Register
		 * 0x80 CARDBUS enbale
		 * 0x01 Cleared for hardware change detect
		 */
		PCI_MASK2_CONFIG(sc->dev, CBBR_TOPIC_CDC,
		    | CBBM_TOPIC_CDC_CARDBUS,
		    & ~CBBM_TOPIC_CDC_SWDETECT, 4);
		break;
	}

	/*
	 * Need to tell ExCA registers to CSC interrupts route via PCI
	 * interrupts.  There are two ways to do this.  Once is to set
	 * INTR_ENABLE and the other is to set CSC to 0.  Since both
	 * methods are mutually compatible, we do both.
	 */
	exca_putb(&sc->exca[0], EXCA_INTR, EXCA_INTR_ENABLE);
	exca_putb(&sc->exca[0], EXCA_CSC_INTR, 0);

	cbb_disable_func_intr(sc);

	/* close all memory and io windows */
	pci_write_config(sc->dev, CBBR_MEMBASE0, 0xffffffff, 4);
	pci_write_config(sc->dev, CBBR_MEMLIMIT0, 0, 4);
	pci_write_config(sc->dev, CBBR_MEMBASE1, 0xffffffff, 4);
	pci_write_config(sc->dev, CBBR_MEMLIMIT1, 0, 4);
	pci_write_config(sc->dev, CBBR_IOBASE0, 0xffffffff, 4);
	pci_write_config(sc->dev, CBBR_IOLIMIT0, 0, 4);
	pci_write_config(sc->dev, CBBR_IOBASE1, 0xffffffff, 4);
	pci_write_config(sc->dev, CBBR_IOLIMIT1, 0, 4);
}

static int
cbb_route_interrupt(device_t pcib, device_t dev, int pin)
{
	struct cbb_softc *sc = (struct cbb_softc *)device_get_softc(pcib);

	return (rman_get_start(sc->irq_res));
}

static device_method_t cbb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			cbb_pci_probe),
	DEVMETHOD(device_attach,		cbb_pci_attach),
	DEVMETHOD(device_detach,		cbb_detach),
	DEVMETHOD(device_shutdown,		cbb_shutdown),
	DEVMETHOD(device_suspend,		cbb_suspend),
	DEVMETHOD(device_resume,		cbb_resume),

	/* bus methods */
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		cbb_read_ivar),
	DEVMETHOD(bus_write_ivar,		cbb_write_ivar),
	DEVMETHOD(bus_alloc_resource,		cbb_alloc_resource),
	DEVMETHOD(bus_release_resource,		cbb_release_resource),
	DEVMETHOD(bus_activate_resource,	cbb_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	cbb_deactivate_resource),
	DEVMETHOD(bus_driver_added,		cbb_driver_added),
	DEVMETHOD(bus_child_detached,		cbb_child_detached),
	DEVMETHOD(bus_setup_intr,		cbb_setup_intr),
	DEVMETHOD(bus_teardown_intr,		cbb_teardown_intr),
	DEVMETHOD(bus_child_present,		cbb_child_present),

	/* 16-bit card interface */
	DEVMETHOD(card_set_res_flags,		cbb_pcic_set_res_flags),
	DEVMETHOD(card_set_memory_offset,	cbb_pcic_set_memory_offset),

	/* power interface */
	DEVMETHOD(power_enable_socket,		cbb_power_enable_socket),
	DEVMETHOD(power_disable_socket,		cbb_power_disable_socket),

	/* pcib compatibility interface */
	DEVMETHOD(pcib_maxslots,		cbb_maxslots),
	DEVMETHOD(pcib_read_config,		cbb_read_config),
	DEVMETHOD(pcib_write_config,		cbb_write_config),
	DEVMETHOD(pcib_route_interrupt,		cbb_route_interrupt),

	{0,0}
};

static driver_t cbb_driver = {
	"cbb",
	cbb_methods,
	sizeof(struct cbb_softc)
};

DRIVER_MODULE(cbb, pci, cbb_driver, cbb_devclass, 0, 0);
MODULE_DEPEND(cbb, exca, 1, 1, 1);
