/*
 * Copyright (c) 2000,2001 Jonathan Chen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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

/*
 * Driver for PCI to Cardbus Bridge chips
 *
 * References:
 *  TI Datasheets:
 *   http://www-s.ti.com/cgi-bin/sc/generic2.cgi?family=PCI+CARDBUS+CONTROLLERS
 * Much of the 16-bit PC Card compatibility code stolen from dev/pcic/i82365.c
 * XXX and should be cleaned up to share as much as possible.
 *
 * Written by Jonathan Chen <jon@freebsd.org>
 * The author would like to acknowledge:
 *  * HAYAKAWA Koichi: Author of the NetBSD code for the same thing
 *  * Warner Losh: Newbus/newcard guru and author of the pccard side of things
 *  * YAMAMOTO Shigeru: Author of another FreeBSD cardbus driver
 *  * David Cross: Author of the initial ugly hack for a specific cardbus card
 */

#define CBB_DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <machine/clock.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>
#include <dev/pcic/i82365reg.h>

#include <dev/pccbb/pccbbreg.h>
#include <dev/pccbb/pccbbvar.h>

#include "power_if.h"
#include "card_if.h"
#include "pcib_if.h"

#if defined CBB_DEBUG
#define	DPRINTF(x) printf x
#define	DEVPRINTF(x) device_printf x
#else
#define	DPRINTF(x)
#define	DEVPRINTF(x)
#endif

#define	PCI_MASK_CONFIG(DEV,REG,MASK,SIZE) 				\
	pci_write_config(DEV, REG, pci_read_config(DEV, REG, SIZE) MASK, SIZE)
#define	PCI_MASK2_CONFIG(DEV,REG,MASK1,MASK2,SIZE)			\
	pci_write_config(DEV, REG, (					\
		pci_read_config(DEV, REG, SIZE) MASK1) MASK2, SIZE)

/*
 * XXX all the pcic code really doesn't belong here and needs to be
 * XXX migrated to its own file, shared with the 16-bit code
 */
#define	PCIC_READ(SC,REG)						\
	(((u_int8_t*)((SC)->sc_socketreg))[0x800+(REG)])
#define	PCIC_WRITE(SC,REG,val)						\
	(((u_int8_t*)((SC)->sc_socketreg))[0x800+(REG)]) = (val)
#define	PCIC_MASK(SC,REG,MASK)						\
	PCIC_WRITE(SC,REG,PCIC_READ(SC,REG) MASK)
#define	PCIC_MASK2(SC,REG,MASK,MASK2)					\
	PCIC_WRITE(SC,REG,(PCIC_READ(SC,REG) MASK) MASK2)

struct pccbb_sclist {
	struct pccbb_softc *sc;
	STAILQ_ENTRY(pccbb_sclist) entries;
};

static STAILQ_HEAD(, pccbb_sclist) softcs;
static int softcs_init = 0;


struct yenta_chipinfo {
	u_int32_t yc_id;
	const char *yc_name;
	int yc_chiptype;
	int yc_flags;
} yc_chipsets[] = {
	/* Texas Instruments chips */
	{PCI_DEVICE_ID_PCIC_TI1130, "TI1130 PCI-CardBus Bridge", CB_TI113X,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1131, "TI1131 PCI-CardBus Bridge", CB_TI113X,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},

	{PCI_DEVICE_ID_PCIC_TI1211, "TI1211 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1220, "TI1220 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1221, "TI1221 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1225, "TI1225 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1250, "TI1250 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1251, "TI1251 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1251B,"TI1251B PCI-CardBus Bridge",CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1410, "TI1410 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1420, "TI1420 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1450, "TI1450 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI1451, "TI1451 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_TI4451, "TI4451 PCI-CardBus Bridge", CB_TI12XX,
		PCCBB_PCIC_IO_RELOC | PCCBB_PCIC_MEM_32},

	/* Ricoh chips */
	{PCI_DEVICE_ID_RICOH_RL5C465, "RF5C465 PCI-CardBus Bridge",
		CB_RF5C46X, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_RICOH_RL5C466, "RF5C466 PCI-CardBus Bridge",
		CB_RF5C46X, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_RICOH_RL5C475, "RF5C475 PCI-CardBus Bridge",
		CB_RF5C47X, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_RICOH_RL5C476, "RF5C476 PCI-CardBus Bridge",
		CB_RF5C47X, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_RICOH_RL5C478, "RF5C478 PCI-CardBus Bridge",
		CB_RF5C47X, PCCBB_PCIC_MEM_32},

	/* Toshiba products */
	{PCI_DEVICE_ID_TOSHIBA_TOPIC95, "ToPIC95 PCI-CardBus Bridge",
		CB_TOPIC95, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_TOSHIBA_TOPIC95B, "ToPIC95B PCI-CardBus Bridge",
		CB_TOPIC95B, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_TOSHIBA_TOPIC97, "ToPIC97 PCI-CardBus Bridge",
		CB_TOPIC97, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_TOSHIBA_TOPIC100, "ToPIC100 PCI-CardBus Bridge",
		CB_TOPIC97, PCCBB_PCIC_MEM_32},

	/* Cirrus Logic */
	{PCI_DEVICE_ID_PCIC_CLPD6832, "CLPD6832 PCI-CardBus Bridge",
		CB_CIRRUS, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_CLPD6833, "CLPD6833 PCI-CardBus Bridge",
		CB_CIRRUS, PCCBB_PCIC_MEM_32},

	/* 02Micro */
	{PCI_DEVICE_ID_PCIC_OZ6832, "O2Mirco OZ6832/6833 PCI-CardBus Bridge",
		CB_CIRRUS, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_OZ6860, "O2Mirco OZ6836/6860 PCI-CardBus Bridge",
		CB_CIRRUS, PCCBB_PCIC_MEM_32},
	{PCI_DEVICE_ID_PCIC_OZ6872, "O2Mirco OZ6812/6872 PCI-CardBus Bridge",
		CB_CIRRUS, PCCBB_PCIC_MEM_32},

	/* sentinel */
	{0 /* null id */, "unknown",
		CB_UNKNOWN, 0},
};


static int cb_chipset(u_int32_t pci_id, const char** namep, int* flagp);
static int pccbb_probe(device_t dev);
static void pccbb_chipinit(struct pccbb_softc* sc);
static int pccbb_attach(device_t dev);
static int pccbb_detach(device_t dev);
static void pccbb_driver_added(device_t dev, driver_t *driver);
static void pccbb_child_detached(device_t dev, device_t child);
static void pccbb_event_thread (void *arg);
static void pccbb_create_event_thread (struct pccbb_softc *sc);
static void pccbb_start_threads(void *arg);
static void pccbb_insert (struct pccbb_softc *sc);
static void pccbb_removal (struct pccbb_softc *sc);
static void pccbb_intr(void* arg);
static int pccbb_detect_voltage(device_t dev);
static int pccbb_power(device_t dev, int volts);
static void pccbb_cardbus_reset(device_t dev);
static int pccbb_cardbus_power_enable_socket(device_t self, device_t child);
static void pccbb_cardbus_power_disable_socket(device_t self, device_t child);
static int pccbb_cardbus_io_open(device_t dev, int win,
				 u_int32_t start, u_int32_t end);
static int pccbb_cardbus_mem_open(device_t dev, int win,
				  u_int32_t start, u_int32_t end);
static void pccbb_cardbus_auto_open(struct pccbb_softc *sc, int type);
static int pccbb_cardbus_activate_resource(device_t self, device_t child,
					   int type, int rid,
					   struct resource *r);
static int pccbb_cardbus_deactivate_resource(device_t self, device_t child,
					     int type, int rid,
					     struct resource *r);
static struct resource* pccbb_cardbus_alloc_resource(device_t self,
			     device_t child, int type, int* rid,
			     u_long start, u_long end, u_long count,
			     u_int flags);
static int pccbb_cardbus_release_resource(device_t self, device_t child,
					  int type,int rid,
					  struct resource *r);
static int pccbb_pcic_power_enable_socket(device_t self, device_t child);
static void pccbb_pcic_power_disable_socket(device_t self, device_t child);
static void pccbb_pcic_wait_ready(struct pccbb_softc *sc);
static void pccbb_pcic_do_mem_map(struct pccbb_softc *sc, int win);
static int pccbb_pcic_mem_map(struct pccbb_softc *sc, int kind,
			      struct resource *r, bus_addr_t card_addr,
			      int *win);
static void pccbb_pcic_mem_unmap(struct pccbb_softc *sc, int window);
static void pccbb_pcic_do_io_map(struct pccbb_softc *sc, int win);
static int pccbb_pcic_io_map(struct pccbb_softc *sc, int width,
			     struct resource *r, bus_addr_t card_addr,
			     int *win);
static void pccbb_pcic_io_unmap(struct pccbb_softc *sc, int window);
static int pccbb_pcic_activate_resource(device_t self, device_t child,
					int type, int rid, struct resource *r);
static int pccbb_pcic_deactivate_resource(device_t self, device_t child,
					 int type,int rid, struct resource *r);
static struct resource* pccbb_pcic_alloc_resource(device_t self,device_t child,
				int type, int* rid, u_long start,
				u_long end, u_long count, u_int flags);
static int pccbb_pcic_release_resource(device_t self, device_t child, int type,
				       int rid, struct resource *res);
static int pccbb_pcic_set_res_flags(device_t self, device_t child, int type,
				    int rid, u_int32_t flags);
static int pccbb_pcic_set_memory_offset(device_t self, device_t child, int rid,
					u_int32_t offset, u_int32_t *deltap);
static int pccbb_power_enable_socket(device_t self, device_t child);
static void pccbb_power_disable_socket(device_t self, device_t child);
static int pccbb_activate_resource(device_t self, device_t child, int type,
				   int rid, struct resource *r);
static int pccbb_deactivate_resource(device_t self, device_t child, int type,
				     int rid, struct resource *r);
static struct resource* pccbb_alloc_resource(device_t self, device_t child,
					     int type, int* rid, u_long start,
					     u_long end, u_long count,
					     u_int flags);
static int pccbb_release_resource(device_t self, device_t child,
				  int type, int rid, struct resource *r);
static int pccbb_maxslots(device_t dev);
static u_int32_t pccbb_read_config(device_t dev, int b, int s, int f,
				   int reg, int width);
static void pccbb_write_config(device_t dev, int b, int s, int f, int reg,
			       u_int32_t val, int width);


/************************************************************************/
/* Probe/Attach								*/
/************************************************************************/

static int
cb_chipset(u_int32_t pci_id, const char** namep, int* flagp)
{
	int loopend = sizeof(yc_chipsets)/sizeof(yc_chipsets[0]);
	struct yenta_chipinfo *ycp, *ycend;
	ycend = yc_chipsets + loopend;

	for (ycp = yc_chipsets; ycp < ycend && pci_id != ycp->yc_id; ++ycp);
	if (ycp == ycend) {
		/* not found */
		ycp = yc_chipsets + loopend - 1; /* to point the sentinel */
	}
	if (namep != NULL) {
		*namep = ycp->yc_name;
	}
	if (flagp != NULL) {
		*flagp = ycp->yc_flags;
	}
	return ycp->yc_chiptype;
}

static int
pccbb_probe(device_t dev)
{
	const char *name;

	if (cb_chipset(pci_get_devid(dev), &name, NULL) == CB_UNKNOWN)
		return ENXIO;
	device_set_desc(dev, name);
	return 0;
}

static void
pccbb_chipinit(struct pccbb_softc* sc)
{
	/* Set CardBus latency timer */
	if (pci_read_config(sc->sc_dev, PCIR_SECLAT_1, 1) < 0x20)
		pci_write_config(sc->sc_dev, PCIR_SECLAT_1, 0x20, 1);

	/* Set PCI latency timer */
	if (pci_read_config(sc->sc_dev, PCIR_LATTIMER, 1) < 0x20)
		pci_write_config(sc->sc_dev, PCIR_LATTIMER, 0x20, 1);

	/* Enable memory access */
	PCI_MASK_CONFIG(sc->sc_dev, PCIR_COMMAND,
			| PCIM_CMD_MEMEN
			| PCIM_CMD_PORTEN
			| PCIM_CMD_BUSMASTEREN, 2);

	/* disable Legacy IO */
	switch (sc->sc_chipset) {
	case CB_RF5C46X:
	case CB_RF5C47X:
		PCI_MASK_CONFIG(sc->sc_dev, PCCBBR_BRIDGECTRL,
				& ~(PCCBBM_BRIDGECTRL_RL_3E0_EN|
				    PCCBBM_BRIDGECTRL_RL_3E2_EN), 2);
		break;
	default:
		pci_write_config(sc->sc_dev, PCCBBR_LEGACY, 0x0, 4);
		break;
	}

	/* Use PCI interrupt for interrupt routing */
	PCI_MASK2_CONFIG(sc->sc_dev, PCCBBR_BRIDGECTRL,
			 & ~(PCCBBM_BRIDGECTRL_MASTER_ABORT |
			     PCCBBM_BRIDGECTRL_INTR_IREQ_EN),
			 | PCCBBM_BRIDGECTRL_WRITE_POST_EN,
			 2);

	switch (sc->sc_chipset) {
	case CB_TI113X:
		PCI_MASK2_CONFIG(sc->sc_dev, PCCBBR_CBCTRL,
				 & ~PCCBBM_CBCTRL_113X_PCI_INTR,
				 | PCCBBM_CBCTRL_113X_PCI_CSC
				 | PCCBBM_CBCTRL_113X_PCI_IRQ_EN, 1);
		PCI_MASK_CONFIG(sc->sc_dev, PCCBBR_DEVCTRL,
				& ~(PCCBBM_DEVCTRL_INT_SERIAL|
				    PCCBBM_DEVCTRL_INT_PCI), 1);
		PCIC_WRITE(sc, PCIC_INTR, PCIC_INTR_ENABLE);
		PCIC_WRITE(sc, PCIC_CSC_INTR, 0);
		break;
	case CB_TI12XX:
		PCIC_WRITE(sc, PCIC_INTR, PCIC_INTR_ENABLE);
		PCIC_WRITE(sc, PCIC_CSC_INTR, 0);
		break;
	case CB_TOPIC95B:
		PCI_MASK_CONFIG(sc->sc_dev, PCCBBR_TOPIC_SOCKETCTRL,
				| PCCBBM_TOPIC_SOCKETCTRL_SCR_IRQSEL, 4);
		PCI_MASK2_CONFIG(sc->sc_dev, PCCBBR_TOPIC_SLOTCTRL,
				 | PCCBBM_TOPIC_SLOTCTRL_SLOTON
				 | PCCBBM_TOPIC_SLOTCTRL_SLOTEN
				 | PCCBBM_TOPIC_SLOTCTRL_ID_LOCK
				 | PCCBBM_TOPIC_SLOTCTRL_CARDBUS,
				 & ~PCCBBM_TOPIC_SLOTCTRL_SWDETECT, 4);
		break;
	}

	/* close all memory and io windows */
	pci_write_config(sc->sc_dev, PCCBBR_MEMBASE0, 0xffffffff, 4);
	pci_write_config(sc->sc_dev, PCCBBR_MEMLIMIT0, 0, 4);
	pci_write_config(sc->sc_dev, PCCBBR_MEMBASE1, 0xffffffff, 4);
	pci_write_config(sc->sc_dev, PCCBBR_MEMLIMIT1, 0, 4);
	pci_write_config(sc->sc_dev, PCCBBR_IOBASE0, 0xffffffff, 4);
	pci_write_config(sc->sc_dev, PCCBBR_IOLIMIT0, 0, 4);
	pci_write_config(sc->sc_dev, PCCBBR_IOBASE1, 0xffffffff, 4);
	pci_write_config(sc->sc_dev, PCCBBR_IOLIMIT1, 0, 4);
}

static int
pccbb_attach(device_t dev)
{
	struct pccbb_softc *sc = (struct pccbb_softc *)device_get_softc(dev);
	int rid;
	u_int32_t tmp;

	if (!softcs_init) {
		softcs_init = 1;
		STAILQ_INIT(&softcs);
	}
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), MTX_DEF);
	sc->sc_chipset = cb_chipset(pci_get_devid(dev), NULL, &sc->sc_flags);
	sc->sc_dev = dev;
	sc->sc_cbdev = NULL;
	sc->sc_pccarddev = NULL;
	sc->sc_secbus = pci_read_config(dev, PCIR_SECBUS_2, 1);
	sc->sc_subbus = pci_read_config(dev, PCIR_SUBBUS_2, 1);
	sc->memalloc = 0;
	sc->ioalloc = 0;
	SLIST_INIT(&sc->rl);

	/* Ths PCI bus should have given me memory... right? */
	rid=PCCBBR_SOCKBASE;
	sc->sc_base_res=bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
					   0,~0,1, RF_ACTIVE);
	if (!sc->sc_base_res) {
		/*
		 * XXX eVILE HACK BAD THING! XXX
		 * The pci bus device should do this for us.
		 * Some BIOSes doesn't assign a memory space properly.
		 * So we try to manually put one in...
		 */
		u_int32_t sockbase;

		sockbase = pci_read_config(dev, rid, 4);
		if (sockbase < 0x100000 || sockbase >= 0xfffffff0) {
			pci_write_config(dev, rid, 0xffffffff, 4);
			sockbase = pci_read_config(dev, rid, 4);
			sockbase = (sockbase & 0xfffffff0) &
				-(sockbase & 0xfffffff0);
			sc->sc_base_res = bus_generic_alloc_resource(
				device_get_parent(dev), dev, SYS_RES_MEMORY,
				&rid, CARDBUS_SYS_RES_MEMORY_START,
				CARDBUS_SYS_RES_MEMORY_END, sockbase,
				RF_ACTIVE|rman_make_alignment_flags(sockbase));
			if (!sc->sc_base_res){
				device_printf(dev,
					"Could not grab register memory\n");
				mtx_destroy(&sc->sc_mtx);
				return ENOMEM;
			}
			pci_write_config(dev, PCCBBR_SOCKBASE,
					 rman_get_start(sc->sc_base_res), 4);
			DEVPRINTF((dev, "PCI Memory allocated: %08lx\n",
				   rman_get_start(sc->sc_base_res)));
		} else {
			device_printf(dev, "Could not map register memory\n");
			mtx_destroy(&sc->sc_mtx);
			return ENOMEM;
		}
	}

	sc->sc_socketreg =
		(struct pccbb_socketreg *)rman_get_virtual(sc->sc_base_res);
	pccbb_chipinit(sc);

	/* CSC Interrupt: Card detect interrupt on */
	sc->sc_socketreg->socket_mask |= PCCBB_SOCKET_MASK_CD;

	/* reset interrupt */
	tmp = sc->sc_socketreg->socket_event;
	sc->sc_socketreg->socket_event = tmp;

	/* Map and establish the interrupt. */
	rid=0;
	sc->sc_irq_res=bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
					  RF_SHAREABLE | RF_ACTIVE);
	if (sc->sc_irq_res == NULL)  {
		printf("pccbb: Unable to map IRQ...\n");
		bus_release_resource(dev, SYS_RES_MEMORY, PCCBBR_SOCKBASE,
				     sc->sc_base_res);
		mtx_destroy(&sc->sc_mtx);
		return ENOMEM;
	}

	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO, pccbb_intr, sc,
		     &(sc->sc_intrhand))) {
		device_printf(dev, "couldn't establish interrupt");
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY, PCCBBR_SOCKBASE,
				     sc->sc_base_res);
		mtx_destroy(&sc->sc_mtx);
		return ENOMEM;
	}

	/* attach children */
	sc->sc_cbdev = device_add_child(dev, "cardbus", -1);
	if (sc->sc_cbdev == NULL)
		DEVPRINTF((dev, "WARNING: cannot add cardbus bus.\n"));
	else if (device_probe_and_attach(sc->sc_cbdev) != 0) {
		DEVPRINTF((dev, "WARNING: cannot attach cardbus bus!\n"));
		sc->sc_cbdev = NULL;
	}

	sc->sc_pccarddev = device_add_child(dev, "pccard", -1);
	if (sc->sc_pccarddev == NULL)
		DEVPRINTF((dev, "WARNING: cannot add pccard bus.\n"));
	else if (device_probe_and_attach(sc->sc_pccarddev) != 0) {
		DEVPRINTF((dev, "WARNING: cannot attach pccard bus.\n"));
		sc->sc_pccarddev = NULL;
	}

#ifndef KLD_MODULE
	if (sc->sc_cbdev == NULL && sc->sc_pccarddev == NULL) {
		device_printf(dev, "ERROR: Failed to attach cardbus/pccard bus!\n");
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY, PCCBBR_SOCKBASE,
				     sc->sc_base_res);
		mtx_destroy(&sc->sc_mtx);
		return ENOMEM;
	}
#endif

	{
		struct pccbb_sclist *sclist;
		sclist = malloc(sizeof(struct pccbb_sclist), M_DEVBUF,
				M_WAITOK);
		sclist->sc = sc;
		STAILQ_INSERT_TAIL(&softcs, sclist, entries);
	}
	return 0;
}

static int
pccbb_detach(device_t dev)
{
	struct pccbb_softc *sc = device_get_softc(dev);
	int numdevs;
	device_t *devlist;
	int tmp;
	int error;

	device_get_children(dev, &devlist, &numdevs);

	error = 0;
	for (tmp = 0; tmp < numdevs; tmp++) {
		if (device_detach(devlist[tmp]) == 0)
			device_delete_child(dev, devlist[tmp]);
		else
			error++;
	}
	free(devlist, M_TEMP);
	if (error > 0)
		return ENXIO;

	mtx_lock(&sc->sc_mtx);
	bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intrhand);

	sc->sc_flags |= PCCBB_KTHREAD_DONE;
	if (sc->sc_flags & PCCBB_KTHREAD_RUNNING) {
		wakeup(sc);
		mtx_unlock(&sc->sc_mtx);
		DEVPRINTF((dev, "waiting for kthread exit..."));
		error = tsleep(sc, PWAIT, "pccbb-detach-wait", 60 * hz);
		if (error)
			DPRINTF(("timeout\n"));
		else
			DPRINTF(("done\n"));
	} else
		mtx_unlock(&sc->sc_mtx);

	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	bus_release_resource(dev, SYS_RES_MEMORY, PCCBBR_SOCKBASE,
			     sc->sc_base_res);
	mtx_destroy(&sc->sc_mtx);
	return 0;
}

static void
pccbb_driver_added(device_t dev, driver_t *driver)
{
	struct pccbb_softc *sc = device_get_softc(dev);
	device_t *devlist;
	int tmp;
	int numdevs;

	DEVICE_IDENTIFY(driver, dev);
	device_get_children(dev, &devlist, &numdevs);
	for (tmp = 0; tmp < numdevs; tmp++) {
		if (device_get_state(devlist[tmp]) == DS_NOTPRESENT &&
		    device_probe_and_attach(devlist[tmp]) == 0) {
			if (devlist[tmp] == NULL)
				/* NOTHING */;
			else if (strcmp(driver->name, "cardbus") == 0) {
				sc->sc_cbdev = devlist[tmp];
				if ((sc->sc_socketreg->socket_state
				     & PCCBB_SOCKET_STAT_CD) == 0) {
					mtx_lock(&sc->sc_mtx);
					wakeup(sc);
					mtx_unlock(&sc->sc_mtx);
				}
			} else if (strcmp(driver->name, "pccard") == 0) {
				sc->sc_pccarddev = devlist[tmp];
				if ((sc->sc_socketreg->socket_state
				     & PCCBB_SOCKET_STAT_CD) == 0) {
					mtx_lock(&sc->sc_mtx);
					wakeup(sc);
					mtx_unlock(&sc->sc_mtx);
				}
			} else
				device_printf(dev,
					      "Unsupported child bus: %s\n",
					      driver->name);
		}
	}
	free(devlist, M_TEMP);
}

static void
pccbb_child_detached(device_t dev, device_t child)
{
	struct pccbb_softc *sc = device_get_softc(dev);
	if (child == sc->sc_cbdev)
		sc->sc_cbdev = NULL;
	else if (child == sc->sc_pccarddev)
		sc->sc_pccarddev = NULL;
	else
		device_printf(dev, "Unknown child detached: %s %p/%p\n",
			      device_get_nameunit(child), sc->sc_cbdev, sc->sc_pccarddev);
}

/************************************************************************/
/* Kthreads								*/
/************************************************************************/

static void
pccbb_event_thread (void *arg)
{
	struct pccbb_softc *sc = arg;
	u_int32_t status;

	mtx_lock(&Giant);
	for(;;) {
		if (!(sc->sc_flags & PCCBB_KTHREAD_RUNNING))
			sc->sc_flags |= PCCBB_KTHREAD_RUNNING;
		else {
			tsleep (sc, PWAIT, "pccbbev", 0);
			/*
			 * Delay 1 second, make sure the user is done with
			 * whatever he is doing.  We tsleep on sc->sc_flags,
			 * which should never be woken up.
			 */
			tsleep (&sc->sc_flags, PWAIT, "pccbbev", 1*hz);
		}
		mtx_lock(&sc->sc_mtx);
		if (sc->sc_flags & PCCBB_KTHREAD_DONE)
			break;

		status = sc->sc_socketreg->socket_state;
		if ((status & PCCBB_SOCKET_STAT_CD) == 0) {
			pccbb_insert(sc);
		} else {
			pccbb_removal(sc);
		}
		mtx_unlock(&sc->sc_mtx);
	}
	mtx_unlock(&sc->sc_mtx);
	sc->sc_flags &= ~PCCBB_KTHREAD_RUNNING;
	wakeup(sc);
	kthread_exit(0);
}

static void
pccbb_create_event_thread (struct pccbb_softc *sc)
{
	if (kthread_create(pccbb_event_thread, sc, &sc->event_thread,
			   0, "%s%d", device_get_name(sc->sc_dev),
			   device_get_unit(sc->sc_dev))) {
		device_printf (sc->sc_dev, "unable to create event thread.\n");
		panic ("pccbb_create_event_thread");
	}
}

static void
pccbb_start_threads(void *arg)
{
	struct pccbb_sclist *sclist;

	STAILQ_FOREACH(sclist, &softcs, entries) {
		pccbb_create_event_thread(sclist->sc);
	}
}

/************************************************************************/
/* Insert/removal							*/
/************************************************************************/

static void
pccbb_insert (struct pccbb_softc *sc)
{
	u_int32_t sockevent, sockstate;
	int timeout = 30;

	do {
		sockevent = sc->sc_socketreg->socket_event;
		sockstate = sc->sc_socketreg->socket_state;
	} while (sockstate & PCCBB_SOCKET_STAT_CD && --timeout > 0);

	if (timeout < 0) {
		device_printf (sc->sc_dev, "insert timeout");
		return;
	}

	DEVPRINTF((sc->sc_dev, "card inserted: event=0x%08x, state=%08x\n",
		   sockevent, sockstate));

	if (sockstate & PCCBB_SOCKET_STAT_16BIT && sc->sc_pccarddev != NULL) {
		sc->sc_flags |= PCCBB_16BIT_CARD;
		if (CARD_ATTACH_CARD(sc->sc_pccarddev) != 0)
			device_printf(sc->sc_dev, "card activation failed\n");
	} else if (sockstate & PCCBB_SOCKET_STAT_CB && sc->sc_cbdev != NULL) {
		sc->sc_flags &= ~PCCBB_16BIT_CARD;
		if (CARD_ATTACH_CARD(sc->sc_cbdev) != 0)
			device_printf(sc->sc_dev, "card activation failed\n");
	} else {
		device_printf (sc->sc_dev, "Unsupported card type detected\n");
	}
}

static void
pccbb_removal (struct pccbb_softc *sc)
{
	u_int32_t sockstate;
	struct pccbb_reslist *rle;

	sockstate = sc->sc_socketreg->socket_state;

	if (sockstate & PCCBB_16BIT_CARD && sc->sc_pccarddev != NULL)
		CARD_DETACH_CARD(sc->sc_pccarddev, DETACH_FORCE);
	else if ((!(sockstate & PCCBB_16BIT_CARD)) && sc->sc_cbdev != NULL)
		CARD_DETACH_CARD(sc->sc_cbdev, DETACH_FORCE);

	while (NULL != (rle = SLIST_FIRST(&sc->rl))) {
		device_printf(sc->sc_dev, "Danger Will Robinson: Resource "
		    "left allocated!  This is a bug... "
		    "(rid=%x, type=%d, addr=%x)\n", rle->rid, rle->type,
		    rle->start);
		SLIST_REMOVE_HEAD(&sc->rl, entries);
	}
}

/************************************************************************/
/* Interrupt Handler							*/
/************************************************************************/

static void
pccbb_intr(void* arg)
{
	struct pccbb_softc *sc = arg;
	u_int32_t sockevent;

	if (!(sockevent = sc->sc_socketreg->socket_event)) {
		/* not for me. */
		return;
	}

	/* reset bit */
	sc->sc_socketreg->socket_event = sockevent | 0x01;

	if (sockevent & PCCBB_SOCKET_EVENT_CD) {
		mtx_lock(&sc->sc_mtx);
		wakeup(sc);
		mtx_unlock(&sc->sc_mtx);
	} else {
		if (sockevent & PCCBB_SOCKET_EVENT_CSTS) {
			DPRINTF((" cstsevent occures, 0x%08x\n",
				 sc->sc_socketreg->socket_state));
		}
		if (sockevent & PCCBB_SOCKET_EVENT_POWER) {
			DPRINTF((" pwrevent occures, 0x%08x\n",
				 sc->sc_socketreg->socket_state));
		}
	}

	return;
}

/************************************************************************/
/* Generic Power functions						*/
/************************************************************************/

static int
pccbb_detect_voltage(device_t dev)
{
	struct pccbb_softc *sc = device_get_softc(dev);
	u_int32_t psr;
	int vol = CARD_UKN_CARD;

	psr = sc->sc_socketreg->socket_state;

	if (psr & PCCBB_SOCKET_STAT_5VCARD) {
		vol |= CARD_5V_CARD;
	}
	if (psr & PCCBB_SOCKET_STAT_3VCARD) {
		vol |= CARD_3V_CARD;
	}
	if (psr & PCCBB_SOCKET_STAT_XVCARD) {
		vol |= CARD_XV_CARD;
	}
	if (psr & PCCBB_SOCKET_STAT_YVCARD) {
		vol |= CARD_YV_CARD;
	}

	return vol;
}

static int
pccbb_power(device_t dev, int volts)
{
	u_int32_t status, sock_ctrl;
	struct pccbb_softc *sc = device_get_softc(dev);

	DEVPRINTF((sc->sc_dev, "pccbb_power: %s and %s [%x]\n",
		   (volts & CARD_VCCMASK) == CARD_VCC_UC ? "CARD_VCC_UC" :
		   (volts & CARD_VCCMASK) == CARD_VCC_5V ? "CARD_VCC_5V" :
		   (volts & CARD_VCCMASK) == CARD_VCC_3V ? "CARD_VCC_3V" :
		   (volts & CARD_VCCMASK) == CARD_VCC_XV ? "CARD_VCC_XV" :
		   (volts & CARD_VCCMASK) == CARD_VCC_YV ? "CARD_VCC_YV" :
		   (volts & CARD_VCCMASK) == CARD_VCC_0V ? "CARD_VCC_0V" :
		   "VCC-UNKNOWN",
		   (volts & CARD_VPPMASK) == CARD_VPP_UC ? "CARD_VPP_UC" :
		   (volts & CARD_VPPMASK) == CARD_VPP_12V ? "CARD_VPP_12V" :
		   (volts & CARD_VPPMASK) == CARD_VPP_VCC ? "CARD_VPP_VCC" :
		   (volts & CARD_VPPMASK) == CARD_VPP_0V ? "CARD_VPP_0V" :
		   "VPP-UNKNOWN",
		   volts));

	status = sc->sc_socketreg->socket_state;
	sock_ctrl = sc->sc_socketreg->socket_control;

	switch (volts & CARD_VCCMASK) {
	case CARD_VCC_UC:
		break;
	case CARD_VCC_5V:
		if (PCCBB_SOCKET_STAT_5VCARD & status) { /* check 5 V card */
			sock_ctrl &= ~PCCBB_SOCKET_CTRL_VCCMASK;
			sock_ctrl |= PCCBB_SOCKET_CTRL_VCC_5V;
		} else {
			device_printf(sc->sc_dev,
				      "BAD voltage request: no 5 V card\n");
		}
		break;
	case CARD_VCC_3V:
		if (PCCBB_SOCKET_STAT_3VCARD & status) {
			sock_ctrl &= ~PCCBB_SOCKET_CTRL_VCCMASK;
			sock_ctrl |= PCCBB_SOCKET_CTRL_VCC_3V;
		} else {
			device_printf(sc->sc_dev,
				      "BAD voltage request: no 3.3 V card\n");
		}
		break;
	case CARD_VCC_0V:
		sock_ctrl &= ~PCCBB_SOCKET_CTRL_VCCMASK;
		break;
	default:
		return 0;			/* power NEVER changed */
		break;
	}

	switch (volts & CARD_VPPMASK) {
	case CARD_VPP_UC:
		break;
	case CARD_VPP_0V:
		sock_ctrl &= ~PCCBB_SOCKET_CTRL_VPPMASK;
		break;
	case CARD_VPP_VCC:
		sock_ctrl &= ~PCCBB_SOCKET_CTRL_VPPMASK;
		sock_ctrl |= ((sock_ctrl >> 4) & 0x07);
		break;
	case CARD_VPP_12V:
		sock_ctrl &= ~PCCBB_SOCKET_CTRL_VPPMASK;
		sock_ctrl |= PCCBB_SOCKET_CTRL_VPP_12V;
		break;
	}

	if (sc->sc_socketreg->socket_control == sock_ctrl)
		return 1; /* no change necessary */

	sc->sc_socketreg->socket_control = sock_ctrl;
	status = sc->sc_socketreg->socket_state;

	{
		int timeout = 20;
		u_int32_t sockevent;
		do {
			DELAY(20*1000);
			sockevent = sc->sc_socketreg->socket_event;
		} while (!(sockevent & PCCBB_SOCKET_EVENT_POWER) &&
			 --timeout > 0);
		/* reset event status */
		sc->sc_socketreg->socket_event = sockevent;
		if ( timeout < 0 ) {
			printf ("VCC supply failed.\n");
			return 0;
		}
	}
	/* XXX
	 * delay 400 ms: thgough the standard defines that the Vcc set-up time
	 * is 20 ms, some PC-Card bridge requires longer duration.
	 */
	DELAY(400*1000);

	if (status & PCCBB_SOCKET_STAT_BADVCC) {
		device_printf(sc->sc_dev,
			      "bad Vcc request. ctrl=0x%x, status=0x%x\n",
			      sock_ctrl ,status);
		printf("pccbb_power: %s and %s [%x]\n",
		       (volts & CARD_VCCMASK) == CARD_VCC_UC ? "CARD_VCC_UC" :
		       (volts & CARD_VCCMASK) == CARD_VCC_5V ? "CARD_VCC_5V" :
		       (volts & CARD_VCCMASK) == CARD_VCC_3V ? "CARD_VCC_3V" :
		       (volts & CARD_VCCMASK) == CARD_VCC_XV ? "CARD_VCC_XV" :
		       (volts & CARD_VCCMASK) == CARD_VCC_YV ? "CARD_VCC_YV" :
		       (volts & CARD_VCCMASK) == CARD_VCC_0V ? "CARD_VCC_0V" :
		       "VCC-UNKNOWN",
		       (volts & CARD_VPPMASK) == CARD_VPP_UC ? "CARD_VPP_UC" :
		       (volts & CARD_VPPMASK) == CARD_VPP_12V ? "CARD_VPP_12V":
		       (volts & CARD_VPPMASK) == CARD_VPP_VCC ? "CARD_VPP_VCC":
		       (volts & CARD_VPPMASK) == CARD_VPP_0V ? "CARD_VPP_0V" :
		       "VPP-UNKNOWN",
		       volts);
		return 0;
	}
	return 1;		/* power changed correctly */
}

/************************************************************************/
/* Cardbus power functions						*/
/************************************************************************/

static void
pccbb_cardbus_reset(device_t dev)
{
	struct pccbb_softc *sc = device_get_softc(dev);
	int delay_us;

	delay_us = sc->sc_chipset == CB_RF5C47X ? 400*1000 : 20*1000;

	PCI_MASK_CONFIG(dev, PCCBBR_BRIDGECTRL, |PCCBBM_BRIDGECTRL_RESET, 2);

	DELAY(delay_us);

	/* If a card exists, unreset it! */
	if ((sc->sc_socketreg->socket_state & PCCBB_SOCKET_STAT_CD) == 0) {
		PCI_MASK_CONFIG(dev, PCCBBR_BRIDGECTRL,
				&~PCCBBM_BRIDGECTRL_RESET, 2);
		DELAY(delay_us);
	}
}

static int
pccbb_cardbus_power_enable_socket(device_t self, device_t child)
{
	struct pccbb_softc *sc = device_get_softc(self);
	int voltage;

	if ((sc->sc_socketreg->socket_state & PCCBB_SOCKET_STAT_CD)
	    == PCCBB_SOCKET_STAT_CD)
		return ENODEV;

	voltage = pccbb_detect_voltage(self);

	pccbb_power(self, CARD_VCC_0V | CARD_VPP_0V);
	if (voltage & CARD_5V_CARD)
		pccbb_power(self, CARD_VCC_5V | CARD_VPP_VCC);
	else if (voltage & CARD_3V_CARD)
		pccbb_power(self, CARD_VCC_3V | CARD_VPP_VCC);
	else {
		device_printf(self, "Unknown card voltage\n");
		return ENXIO;
	}

	pccbb_cardbus_reset(self);
	return 0;
}

static void
pccbb_cardbus_power_disable_socket(device_t self, device_t child)
{
	pccbb_power(self, CARD_VCC_0V | CARD_VPP_0V);
	pccbb_cardbus_reset(self);
}

/************************************************************************/
/* Cardbus Resource							*/
/************************************************************************/

static int
pccbb_cardbus_io_open(device_t dev, int win, u_int32_t start, u_int32_t end)
{
	int basereg;
	int limitreg;

	if ((win < 0) || (win > 1)) {
		DEVPRINTF((dev,
			   "pccbb_cardbus_io_open: window out of range %d\n",
			   win));
		return EINVAL;
	}

	basereg = win*8 + PCCBBR_IOBASE0;
	limitreg = win*8 + PCCBBR_IOLIMIT0;

	pci_write_config(dev, basereg, start, 4);
	pci_write_config(dev, limitreg, end, 4);
	return 0;
}

static int
pccbb_cardbus_mem_open(device_t dev, int win, u_int32_t start, u_int32_t end)
{
	int basereg;
	int limitreg;

	if ((win < 0) || (win > 1)) {
		DEVPRINTF((dev,
			   "pccbb_cardbus_mem_open: window out of range %d\n",
			   win));
		return EINVAL;
	}

	basereg = win*8 + PCCBBR_MEMBASE0;
	limitreg = win*8 + PCCBBR_MEMLIMIT0;

	pci_write_config(dev, basereg, start, 4);
	pci_write_config(dev, limitreg, end, 4);
	return 0;
}

static void
pccbb_cardbus_auto_open(struct pccbb_softc *sc, int type)
{
	u_int32_t starts[2];
	u_int32_t ends[2];
	struct pccbb_reslist *rle;
	int align;

	starts[0] = starts[1] = 0xffffffff;
	ends[0] = ends[1] = 0;

	SLIST_FOREACH(rle, &sc->rl, entries) {
		if (rle->type != type)
			;
		else if (starts[0] == 0xffffffff) {
			starts[0] = rle->start;
			ends[0] = rle->end;
			rle->win = 0;
		} else if (rle->end > ends[0] &&
			   rle->start - ends[0] < PCCBB_AUTO_OPEN_SMALLHOLE) {
			ends[0] = rle->end;
			rle->win = 0;
		} else if (rle->start < starts[0] &&
			   starts[0] - rle->end < PCCBB_AUTO_OPEN_SMALLHOLE) {
			starts[0] = rle->start;
			rle->win = 0;
		} else if (starts[1] == 0xffffffff) {
			starts[1] = rle->start;
			ends[1] = rle->end;
			rle->win = 1;
		} else if (rle->end > ends[1] &&
			   rle->start - ends[1] < PCCBB_AUTO_OPEN_SMALLHOLE) {
			ends[1] = rle->end;
			rle->win = 1;
		} else if (rle->start < starts[1] &&
			   starts[1] - rle->end < PCCBB_AUTO_OPEN_SMALLHOLE) {
			starts[1] = rle->start;
			rle->win = 1;
		} else {
			u_int32_t diffs[2];

			diffs[0] = diffs[1] = 0xffffffff;
			if (rle->start > ends[0])
				diffs[0] = rle->start - ends[0];
			else if (rle->end < starts[0])
				diffs[0] = starts[0] - rle->end;
			if (rle->start > ends[1])
				diffs[1] = rle->start - ends[1];
			else if (rle->end < starts[1])
				diffs[1] = starts[1] - rle->end;

			rle->win = (diffs[0] <= diffs[1])?0:1;
			if (rle->start > ends[rle->win])
				ends[rle->win] = rle->end;
			else if (rle->end < starts[rle->win])
				starts[rle->win] = rle->start;
		}
	}
	if (type == SYS_RES_MEMORY)
		align = PCCBB_MEMALIGN;
	else if (type == SYS_RES_IOPORT)
		align = PCCBB_IOALIGN;
	else
		align = 1;

	if (starts[0] != 0xffffffff)
		starts[0] -= starts[0] % align;
	if (starts[1] != 0xffffffff)
		starts[1] -= starts[1] % align;
	if (ends[0] % align != 0)
		ends[0] += align - ends[0]%align - 1;
	if (ends[1] % align != 0)
		ends[1] += align - ends[1]%align - 1;

	if (type == SYS_RES_MEMORY) {
		pccbb_cardbus_mem_open(sc->sc_dev, 0, starts[0], ends[0]);
		pccbb_cardbus_mem_open(sc->sc_dev, 1, starts[1], ends[1]);
	} else if (type == SYS_RES_IOPORT) {
		pccbb_cardbus_io_open(sc->sc_dev, 0, starts[0], ends[0]);
		pccbb_cardbus_io_open(sc->sc_dev, 1, starts[1], ends[1]);
	}
}

static int
pccbb_cardbus_activate_resource(device_t self, device_t child, int type,
				int rid, struct resource *r)
{
	struct pccbb_softc *sc = device_get_softc(self);
	struct pccbb_reslist *rle;

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		SLIST_FOREACH(rle, &sc->rl, entries) {
			if (type == rle->type && rid == rle->rid &&
			    child == rle->odev)
				return bus_generic_activate_resource(
					self, child, type, rid, r);
		}
		rle = malloc(sizeof(struct pccbb_reslist), M_DEVBUF, M_WAITOK);
		rle->type = type;
		rle->rid = rid;
		rle->start = rman_get_start(r);
		rle->end = rman_get_end(r);
		rle->odev = child;
		rle->win = -1;
		SLIST_INSERT_HEAD(&sc->rl, rle, entries);

		pccbb_cardbus_auto_open(sc, type);
	}
	return bus_generic_activate_resource(self, child, type, rid, r);
}

static int
pccbb_cardbus_deactivate_resource(device_t self, device_t child, int type,
				  int rid, struct resource *r)
{
	struct pccbb_softc *sc = device_get_softc(self);
	struct pccbb_reslist *rle;

	SLIST_FOREACH(rle, &sc->rl, entries) {
		if (type == rle->type && rid == rle->rid &&
		    child == rle->odev) {
			SLIST_REMOVE(&sc->rl, rle, pccbb_reslist, entries);
			if (type == SYS_RES_IOPORT ||
			    type == SYS_RES_MEMORY)
				pccbb_cardbus_auto_open(sc, type);
			free(rle, M_DEVBUF);
			break;
		}
	}
	return bus_generic_deactivate_resource(self, child, type, rid, r);
}

static struct resource*
pccbb_cardbus_alloc_resource(device_t self, device_t child, int type, int* rid,
			     u_long start, u_long end, u_long count,
			     u_int flags)
{
	if (type == SYS_RES_IRQ) {
		struct pccbb_softc *sc = device_get_softc(self);
		if (start == 0) {
			start = end = rman_get_start(sc->sc_irq_res);
		}
		return bus_generic_alloc_resource(self, child, type, rid,
						  start, end, count, flags);
	} else {
		if (type == SYS_RES_MEMORY && start == 0 && end == ~0) {
			start = CARDBUS_SYS_RES_MEMORY_START;
			end = CARDBUS_SYS_RES_MEMORY_END;
		} else if (type == SYS_RES_IOPORT && start == 0 && end == ~0) {
			start = CARDBUS_SYS_RES_IOPORT_START;
			end = CARDBUS_SYS_RES_IOPORT_END;
		}
		return bus_generic_alloc_resource(self, child, type, rid,
						  start, end, count, flags);
	}
}

static int
pccbb_cardbus_release_resource(device_t self, device_t child, int type,
			       int rid, struct resource *r)
{
	return bus_generic_release_resource(self, child, type, rid, r);
}

/************************************************************************/
/* PC Card Power Functions						*/
/************************************************************************/

static int
pccbb_pcic_power_enable_socket(device_t self, device_t child)
{
	struct pccbb_softc *sc = device_get_softc(self);

	DPRINTF(("pccbb_pcic_socket_enable:\n"));

	/* power down/up the socket to reset */
	{
		int voltage = pccbb_detect_voltage(self);

		pccbb_power(self, CARD_VCC_0V | CARD_VPP_0V);
		if (voltage & CARD_5V_CARD)
			pccbb_power(self, CARD_VCC_5V | CARD_VPP_VCC);
		else if (voltage & CARD_3V_CARD)
			pccbb_power(self, CARD_VCC_3V | CARD_VPP_VCC);
		else {
			device_printf(self, "Unknown card voltage\n");
			return ENXIO;
		}
	}

	/* enable socket i/o */
	PCIC_MASK(sc, PCIC_PWRCTL, | PCIC_PWRCTL_OE);

	PCIC_WRITE(sc, PCIC_INTR, PCIC_INTR_ENABLE);
	/* hold reset for 30ms */
	DELAY(30*1000);
	/* clear the reset flag */
	PCIC_MASK(sc, PCIC_INTR, | PCIC_INTR_RESET);
	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */
	DELAY(20*1000);

	pccbb_pcic_wait_ready(sc);

	/* disable all address windows */
	PCIC_WRITE(sc, PCIC_ADDRWIN_ENABLE, 0);

	{
		int cardtype;
		CARD_GET_TYPE(child, &cardtype);
		PCIC_MASK(sc, PCIC_INTR, | ((cardtype == PCCARD_IFTYPE_IO) ?
					    PCIC_INTR_CARDTYPE_IO :
					    PCIC_INTR_CARDTYPE_MEM));
		DEVPRINTF((sc->sc_dev, "card type is %s\n",
			   (cardtype == PCCARD_IFTYPE_IO) ? "io" : "mem"));
	}

	/* reinstall all the memory and io mappings */
	{
		int win;

		for (win = 0; win < PCIC_MEM_WINS; ++win) {
			if (sc->memalloc & (1 << win)) {
				pccbb_pcic_do_mem_map(sc, win);
			}
		}
		for (win = 0; win < PCIC_IO_WINS; ++win) {
			if (sc->ioalloc & (1 << win)) {
				pccbb_pcic_do_io_map(sc, win);
			}
		}
	}
	return 0;
}

static void
pccbb_pcic_power_disable_socket(device_t self, device_t child)
{
	struct pccbb_softc *sc = device_get_softc(self);

	DPRINTF(("pccbb_pcic_socket_disable\n"));

	/* reset signal asserting... */
	PCIC_MASK(sc, PCIC_INTR, & ~PCIC_INTR_RESET);
	DELAY(2*1000);

	/* power down the socket */
	PCIC_MASK(sc, PCIC_PWRCTL, &~PCIC_PWRCTL_OE);
	pccbb_power(self, CARD_VCC_0V | CARD_VPP_0V);

	/* wait 300ms until power fails (Tpf). */
	DELAY(300 * 1000);
}

/************************************************************************/
/* PC Card Resource Functions						*/
/************************************************************************/

static void
pccbb_pcic_wait_ready(struct pccbb_softc *sc)
{
	int i;
	DEVPRINTF((sc->sc_dev, "pccbb_pcic_wait_ready: status 0x%02x\n",
		   PCIC_READ(sc, PCIC_IF_STATUS)));
	for (i = 0; i < 10000; i++) {
		if (PCIC_READ(sc, PCIC_IF_STATUS) & PCIC_IF_STATUS_READY) {
			return;
		}
		DELAY(500);
	}
	device_printf(sc->sc_dev, "ready never happened, status = %02x\n",
		      PCIC_READ(sc, PCIC_IF_STATUS));
}

#define PCIC_MEMINFO(NUM) { \
	PCIC_SYSMEM_ADDR ## NUM ## _START_LSB, \
	PCIC_SYSMEM_ADDR ## NUM ## _START_MSB, \
	PCIC_SYSMEM_ADDR ## NUM ## _STOP_LSB, \
	PCIC_SYSMEM_ADDR ## NUM ## _STOP_MSB, \
	PCIC_SYSMEM_ADDR ## NUM ## _WIN, \
	PCIC_CARDMEM_ADDR ## NUM ## _LSB, \
	PCIC_CARDMEM_ADDR ## NUM ## _MSB, \
	PCIC_ADDRWIN_ENABLE_MEM ## NUM ## , \
}

static struct mem_map_index_st {
	int sysmem_start_lsb;
	int sysmem_start_msb;
	int sysmem_stop_lsb;
	int sysmem_stop_msb;
	int sysmem_win;
	int cardmem_lsb;
	int cardmem_msb;
	int memenable;
} mem_map_index[] = {
	PCIC_MEMINFO(0),
	PCIC_MEMINFO(1),
	PCIC_MEMINFO(2),
	PCIC_MEMINFO(3),
	PCIC_MEMINFO(4),
};
#undef PCIC_MEMINFO

static void
pccbb_pcic_do_mem_map(struct pccbb_softc *sc, int win)
{
	PCIC_WRITE(sc, mem_map_index[win].sysmem_start_lsb,
		   (sc->mem[win].addr >> PCIC_SYSMEM_ADDRX_SHIFT) & 0xff);
	PCIC_WRITE(sc, mem_map_index[win].sysmem_start_msb,
		   ((sc->mem[win].addr >> (PCIC_SYSMEM_ADDRX_SHIFT + 8)) &
		    PCIC_SYSMEM_ADDRX_START_MSB_ADDR_MASK) | 0x80);

	PCIC_WRITE(sc, mem_map_index[win].sysmem_stop_lsb,
		   ((sc->mem[win].addr + sc->mem[win].realsize - 1) >>
		    PCIC_SYSMEM_ADDRX_SHIFT) & 0xff);
	PCIC_WRITE(sc, mem_map_index[win].sysmem_stop_msb,
		   (((sc->mem[win].addr + sc->mem[win].realsize - 1) >>
		     (PCIC_SYSMEM_ADDRX_SHIFT + 8)) &
		    PCIC_SYSMEM_ADDRX_STOP_MSB_ADDR_MASK) |
		   PCIC_SYSMEM_ADDRX_STOP_MSB_WAIT2);

	PCIC_WRITE(sc, mem_map_index[win].sysmem_win,
		   (sc->mem[win].addr >> PCIC_MEMREG_WIN_SHIFT) & 0xff);

	PCIC_WRITE(sc, mem_map_index[win].cardmem_lsb,
		   (sc->mem[win].offset >> PCIC_CARDMEM_ADDRX_SHIFT) & 0xff);
	PCIC_WRITE(sc, mem_map_index[win].cardmem_msb,
		   ((sc->mem[win].offset >> (PCIC_CARDMEM_ADDRX_SHIFT + 8)) &
		    PCIC_CARDMEM_ADDRX_MSB_ADDR_MASK) |
		   ((sc->mem[win].kind == PCCARD_MEM_ATTR) ?
		    PCIC_CARDMEM_ADDRX_MSB_REGACTIVE_ATTR : 0));

	PCIC_MASK(sc, PCIC_ADDRWIN_ENABLE, | PCIC_ADDRWIN_ENABLE_MEMCS16
		  			   | mem_map_index[win].memenable);

	DELAY(100);

#ifdef CBB_DEBUG
	{
		int r1, r2, r3, r4, r5, r6, r7;
		r1 = PCIC_READ(sc, mem_map_index[win].sysmem_start_msb);
		r2 = PCIC_READ(sc, mem_map_index[win].sysmem_start_lsb);
		r3 = PCIC_READ(sc, mem_map_index[win].sysmem_stop_msb);
		r4 = PCIC_READ(sc, mem_map_index[win].sysmem_stop_lsb);
		r5 = PCIC_READ(sc, mem_map_index[win].cardmem_msb);
		r6 = PCIC_READ(sc, mem_map_index[win].cardmem_lsb);
		r7 = PCIC_READ(sc, mem_map_index[win].sysmem_win);
		DPRINTF(("pccbb_pcic_do_mem_map window %d: %02x%02x %02x%02x "
			 "%02x%02x %02x (%08x+%08x.%08x*%08lx)\n", win, r1, r2, r3, r4, r5, r6, r7,
			 sc->mem[win].addr, sc->mem[win].size, sc->mem[win].realsize, sc->mem[win].offset));
	}
#endif
}

static int
pccbb_pcic_mem_map(struct pccbb_softc *sc, int kind,
		   struct resource *r, bus_addr_t card_addr, int *win)
{
	int i;

	*win = -1;
	for (i = 0; i < PCIC_MEM_WINS; i++) {
		if ((sc->memalloc & (1 << i)) == 0) {
			*win = i;
			sc->memalloc |= (1 << i);
			break;
		}
	}
	if (*win == -1)
		return (1);

	card_addr = card_addr - card_addr % PCIC_MEM_PAGESIZE;
	sc->mem[*win].memt = rman_get_bustag(r);
	sc->mem[*win].memh = rman_get_bushandle(r);
	sc->mem[*win].addr = rman_get_start(r);
	sc->mem[*win].size = rman_get_end(r) - sc->mem[*win].addr + 1;
	sc->mem[*win].realsize = sc->mem[*win].size + PCIC_MEM_PAGESIZE - 1;
	sc->mem[*win].realsize = sc->mem[*win].realsize -
				 (sc->mem[*win].realsize % PCIC_MEM_PAGESIZE);
	sc->mem[*win].offset = ((long)card_addr) -
			       ((long)(sc->mem[*win].addr));
	sc->mem[*win].kind = kind;

	DPRINTF(("pccbb_pcic_mem_map window %d bus %x+%x+%lx card addr %x\n",
		 *win, sc->mem[*win].addr, sc->mem[*win].size,
		 sc->mem[*win].offset, card_addr));

	pccbb_pcic_do_mem_map(sc, *win);

	return (0);
}

static void
pccbb_pcic_mem_unmap(struct pccbb_softc *sc, int window)
{
	if (window >= PCIC_MEM_WINS)
		panic("pccbb_pcic_mem_unmap: window out of range");

	PCIC_MASK(sc, PCIC_ADDRWIN_ENABLE, & ~mem_map_index[window].memenable);
	sc->memalloc &= ~(1 << window);
}

#define PCIC_IOINFO(NUM) { \
	PCIC_IOADDR ## NUM ## _START_LSB, \
	PCIC_IOADDR ## NUM ## _START_MSB, \
	PCIC_IOADDR ## NUM ## _STOP_LSB, \
	PCIC_IOADDR ## NUM ## _STOP_MSB, \
	PCIC_ADDRWIN_ENABLE_IO ## NUM ## , \
	PCIC_IOCTL_IO ## NUM ## _WAITSTATE \
	| PCIC_IOCTL_IO ## NUM ## _ZEROWAIT \
	| PCIC_IOCTL_IO ## NUM ## _IOCS16SRC_MASK \
	| PCIC_IOCTL_IO ## NUM ## _DATASIZE_MASK, \
	{ \
		PCIC_IOCTL_IO ## NUM ## _IOCS16SRC_CARD, \
		PCIC_IOCTL_IO ## NUM ## _IOCS16SRC_DATASIZE \
		| PCIC_IOCTL_IO ## NUM ## _DATASIZE_8BIT, \
		PCIC_IOCTL_IO ## NUM ## _IOCS16SRC_DATASIZE \
		| PCIC_IOCTL_IO ## NUM ## _DATASIZE_16BIT, \
	} \
}

static struct io_map_index_st {
	int start_lsb;
	int start_msb;
	int stop_lsb;
	int stop_msb;
	int ioenable;
	int ioctlmask;
	int ioctlbits[3]; /* indexed by PCCARD_WIDTH_* */
} io_map_index[] = {
	PCIC_IOINFO(0),
	PCIC_IOINFO(1),
};
#undef PCIC_IOINFO

static void pccbb_pcic_do_io_map(struct pccbb_softc *sc, int win)
{
	PCIC_WRITE(sc, io_map_index[win].start_lsb, sc->io[win].addr & 0xff);
	PCIC_WRITE(sc, io_map_index[win].start_msb,
		   (sc->io[win].addr >> 8) & 0xff);

	PCIC_WRITE(sc, io_map_index[win].stop_lsb,
		   (sc->io[win].addr + sc->io[win].size - 1) & 0xff);
	PCIC_WRITE(sc, io_map_index[win].stop_msb,
		   ((sc->io[win].addr + sc->io[win].size - 1) >> 8) & 0xff);

	PCIC_MASK2(sc, PCIC_IOCTL,
		   & ~io_map_index[win].ioctlmask,
		   | io_map_index[win].ioctlbits[sc->io[win].width]);

	PCIC_MASK(sc, PCIC_ADDRWIN_ENABLE, | io_map_index[win].ioenable);
#ifdef CBB_DEBUG
	{
		int r1, r2, r3, r4;
		r1 = PCIC_READ(sc, io_map_index[win].start_msb);
		r2 = PCIC_READ(sc, io_map_index[win].start_lsb);
		r3 = PCIC_READ(sc, io_map_index[win].stop_msb);
		r4 = PCIC_READ(sc, io_map_index[win].stop_lsb);
		DPRINTF(("pccbb_pcic_do_io_map window %d: %02x%02x %02x%02x "
			 "(%08x+%08x)\n", win, r1, r2, r3, r4,
			 sc->io[win].addr, sc->io[win].size));
	}
#endif
}

static int
pccbb_pcic_io_map(struct pccbb_softc *sc, int width,
		  struct resource *r, bus_addr_t card_addr, int *win)
{
	int i;
#ifdef CBB_DEBUG
	static char *width_names[] = { "auto", "io8", "io16"};
#endif

	*win = -1;
	for (i=0; i < PCIC_IO_WINS; i++) {
		if ((sc->ioalloc & (1 << i)) == 0) {
			*win = i;
			sc->ioalloc |= (1 << i);
			break;
		}
	}
	if (*win == -1)
		return (1);

	sc->io[*win].iot = rman_get_bustag(r);
	sc->io[*win].ioh = rman_get_bushandle(r);
	sc->io[*win].addr = rman_get_start(r);
	sc->io[*win].size = rman_get_end(r) - sc->io[*win].addr + 1;
	sc->io[*win].flags = 0;
	sc->io[*win].width = width;

	DPRINTF(("pccbb_pcic_io_map window %d %s port %x+%x\n",
		 *win, width_names[width], sc->io[*win].addr,
		 sc->io[*win].size));

	pccbb_pcic_do_io_map(sc, *win);

	return (0);
}

static void
pccbb_pcic_io_unmap(struct pccbb_softc *sc, int window)
{
	if (window >= PCIC_IO_WINS)
		panic("pccbb_pcic_io_unmap: window out of range");

	PCIC_MASK(sc, PCIC_ADDRWIN_ENABLE, & ~io_map_index[window].ioenable);

	sc->ioalloc &= ~(1 << window);

	sc->io[window].iot = 0;
	sc->io[window].ioh = 0;
	sc->io[window].addr = 0;
	sc->io[window].size = 0;
	sc->io[window].flags = 0;
	sc->io[window].width = 0;
}

static int
pccbb_pcic_activate_resource(device_t self, device_t child, int type, int rid,
			     struct resource *r)
{
	int err;
	int win;
	struct pccbb_reslist *rle;
	struct pccbb_softc *sc = device_get_softc(self);

	if (rman_get_flags(r) & RF_ACTIVE)
		return 0;

	switch (type) {
	case SYS_RES_IOPORT:
		err = pccbb_pcic_io_map(sc, 0, r, 0, &win);
		if (err)
			return err;
		break;
	case SYS_RES_MEMORY:
		err = pccbb_pcic_mem_map(sc, 0, r, 0, &win);
		if (err)
			return err;
		break;
	default:
		break;
	}
	SLIST_FOREACH(rle, &sc->rl, entries) {
		if (type == rle->type && rid == rle->rid &&
		    child == rle->odev) {
			rle->win = win;
			break;
		}
	}
	err = bus_generic_activate_resource(self, child, type, rid, r);
	return (err);
}

static int
pccbb_pcic_deactivate_resource(device_t self, device_t child, int type,
			       int rid, struct resource *r)
{
	struct pccbb_softc *sc = device_get_softc(self);
	int win;
	struct pccbb_reslist *rle;
	win = -1;
	SLIST_FOREACH(rle, &sc->rl, entries) {
		if (type == rle->type && rid == rle->rid &&
		    child == rle->odev) {
			win = rle->win;
			break;
		}
	}
	if (win == -1) {
		panic("pccbb_pcic: deactivating bogus resoure");
		return 1;
	}

	switch (type) {
	case SYS_RES_IOPORT:
		pccbb_pcic_io_unmap(sc, win);
		break;
	case SYS_RES_MEMORY:
		pccbb_pcic_mem_unmap(sc, win);
		break;
	default:
		break;
	}
	return bus_generic_deactivate_resource(self, child, type, rid, r);
}

static struct resource*
pccbb_pcic_alloc_resource(device_t self, device_t child, int type, int* rid,
			  u_long start, u_long end, u_long count, u_int flags)
{
	struct resource *r = NULL;
	struct pccbb_softc *sc = device_get_softc(self);
	struct pccbb_reslist *rle;

	if ((sc->sc_flags & PCCBB_PCIC_MEM_32) == 0) {
		panic("PCCBB bridge cannot handle non MEM_32 bridges\n");
	}

	switch (type) {
	case SYS_RES_MEMORY:
		/* Nearly default */
		if (start == 0 && end == ~0 && count != 1) {
			start = CARDBUS_SYS_RES_MEMORY_START; /* XXX -- should be tweakable*/
			end = CARDBUS_SYS_RES_MEMORY_END;
		}
		flags = (flags & ~RF_ALIGNMENT_MASK)
			| rman_make_alignment_flags(PCCBB_MEMALIGN);
		break;
	case SYS_RES_IOPORT:
		if (start < 0x100)
			start = 0x100;		/* XXX tweakable? */
		if (end < start)
			end = start;
		break;
	case SYS_RES_IRQ:
		flags |= RF_SHAREABLE;
		start = end = rman_get_start(sc->sc_irq_res);
		break;
	}
	r = bus_generic_alloc_resource(self, child, type, rid, start, end,
				       count, flags & ~RF_ACTIVE);
	if (r == NULL)
		return NULL;

	rle = malloc(sizeof(struct pccbb_reslist), M_DEVBUF, M_WAITOK);
	rle->type = type;
	rle->rid = *rid;
	rle->start = rman_get_start(r);
	rle->end = rman_get_end(r);
	rle->odev = child;
	rle->win = -1;
	SLIST_INSERT_HEAD(&sc->rl, rle, entries);

	if (flags & RF_ACTIVE) {
		if (bus_activate_resource(child, type, *rid, r) != 0) {
			BUS_RELEASE_RESOURCE(self, child, type, *rid, r);
			return NULL;
		}
	}

	return r;
}

static int
pccbb_pcic_release_resource(device_t self, device_t child, int type,
			    int rid, struct resource *res)
{
	struct pccbb_softc *sc = device_get_softc(self);
	struct pccbb_reslist *rle;
	int count = 0;

	if (rman_get_flags(res) & RF_ACTIVE) {
		int error;
		error = bus_deactivate_resource(child, type, rid, res);
		if (error != 0)
			return error;
	}

	SLIST_FOREACH(rle, &sc->rl, entries) {
		if (type == rle->type && rid == rle->rid &&
		    child == rle->odev) {
			SLIST_REMOVE(&sc->rl, rle, pccbb_reslist, entries);
			free(rle, M_DEVBUF);
			count++;
			break;
		}
	}
	if (count == 0) {
		panic("pccbb_pcic: releasing bogus resource");
	}

	return bus_generic_release_resource(self, child, type, rid, res);
}

/************************************************************************/
/* PC Card methods							*/
/************************************************************************/

static int
pccbb_pcic_set_res_flags(device_t self, device_t child, int type, int rid,
			 u_int32_t flags)
{
	struct pccbb_softc *sc = device_get_softc(self);

	if (type != SYS_RES_MEMORY)
		return (EINVAL);
	sc->mem[rid].kind = flags;
	pccbb_pcic_do_mem_map(sc, rid);
	return 0;
}

static int
pccbb_pcic_set_memory_offset(device_t self, device_t child, int rid,
    u_int32_t cardaddr, u_int32_t *deltap)
{
	struct pccbb_softc *sc = device_get_softc(self);
	int win;
	struct pccbb_reslist *rle;
	u_int32_t delta;

	win = -1;

	SLIST_FOREACH(rle, &sc->rl, entries) {
		if (SYS_RES_MEMORY == rle->type && rid == rle->rid &&
		    child == rle->odev) {
			win = rle->win;
			break;
		}
	}
	if (win == -1) {
		panic("pccbb_pcic: setting memory offset of bogus resource");
		return 1;
	}

	delta = cardaddr % PCIC_MEM_PAGESIZE;
	if (deltap)
		*deltap = delta;
	cardaddr -= delta;
	sc->mem[win].realsize = sc->mem[win].size + delta + 
	    PCIC_MEM_PAGESIZE - 1;
	sc->mem[win].realsize = sc->mem[win].realsize -
	    (sc->mem[win].realsize % PCIC_MEM_PAGESIZE);
	sc->mem[win].offset = cardaddr - sc->mem[win].addr;
	pccbb_pcic_do_mem_map(sc, win);

	return 0;
}

/************************************************************************/
/* POWER methods							*/
/************************************************************************/

static int
pccbb_power_enable_socket(device_t self, device_t child)
{
	struct pccbb_softc *sc = device_get_softc(self);

	if (sc->sc_flags & PCCBB_16BIT_CARD)
		return pccbb_pcic_power_enable_socket(self, child);
	else
		return pccbb_cardbus_power_enable_socket(self, child);
}

static void
pccbb_power_disable_socket(device_t self, device_t child)
{
	struct pccbb_softc *sc = device_get_softc(self);
	if (sc->sc_flags & PCCBB_16BIT_CARD)
		pccbb_pcic_power_disable_socket(self, child);
	else
		pccbb_cardbus_power_disable_socket(self, child);
}
/************************************************************************/
/* BUS Methods								*/
/************************************************************************/


static int
pccbb_activate_resource(device_t self, device_t child, int type, int rid,
			struct resource *r)
{
	struct pccbb_softc *sc = device_get_softc(self);

	if (sc->sc_flags & PCCBB_16BIT_CARD)
		return pccbb_pcic_activate_resource(self, child, type, rid, r);
	else
		return pccbb_cardbus_activate_resource(self, child, type, rid,
						       r);
}

static int
pccbb_deactivate_resource(device_t self, device_t child, int type,
			  int rid, struct resource *r)
{
	struct pccbb_softc *sc = device_get_softc(self);

	if (sc->sc_flags & PCCBB_16BIT_CARD)
		return pccbb_pcic_deactivate_resource(self, child, type,
						      rid, r);
	else
		return pccbb_cardbus_deactivate_resource(self, child, type,
						      rid, r);
}

static struct resource*
pccbb_alloc_resource(device_t self, device_t child, int type, int* rid,
		     u_long start, u_long end, u_long count, u_int flags)
{
	struct pccbb_softc *sc = device_get_softc(self);

	if (sc->sc_flags & PCCBB_16BIT_CARD)
		return pccbb_pcic_alloc_resource(self, child, type, rid,
						 start, end, count, flags);
	else
		return pccbb_cardbus_alloc_resource(self, child, type, rid,
						 start, end, count, flags);
}

static int
pccbb_release_resource(device_t self, device_t child, int type, int rid,
		       struct resource *r)
{
	struct pccbb_softc *sc = device_get_softc(self);

	if (sc->sc_flags & PCCBB_16BIT_CARD)
		return pccbb_pcic_release_resource(self, child, type,
						   rid, r);
	else
		return pccbb_cardbus_release_resource(self, child, type,
						      rid, r);
}

static int
pccbb_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct pccbb_softc	*sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		*result = sc->sc_secbus;
		return(0);
	}
	return(ENOENT);
}

static int
pccbb_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct pccbb_softc	*sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_secbus = value;
		break;
	}
	return(ENOENT);
}

/************************************************************************/
/* PCI compat methods							*/
/************************************************************************/

static int
pccbb_maxslots(device_t dev)
{
	return 0;
}

static u_int32_t
pccbb_read_config(device_t dev, int b, int s, int f, int reg, int width)
{
	/*
	 * Pass through to the next ppb up the chain (i.e. our grandparent).
	 */
	return PCIB_READ_CONFIG(device_get_parent(device_get_parent(dev)),
				b, s, f, reg, width);
}

static void
pccbb_write_config(device_t dev, int b, int s, int f, int reg, u_int32_t val,
		   int width)
{
	/*
	 * Pass through to the next ppb up the chain (i.e. our grandparent).
	 */
	PCIB_WRITE_CONFIG(device_get_parent(device_get_parent(dev)),
			  b, s, f, reg, val, width);
}

static device_method_t pccbb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			pccbb_probe),
	DEVMETHOD(device_attach,		pccbb_attach),
	DEVMETHOD(device_detach,		pccbb_detach),
	DEVMETHOD(device_suspend,		bus_generic_suspend),
	DEVMETHOD(device_resume,		bus_generic_resume),

	/* bus methods */
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		pccbb_read_ivar),
	DEVMETHOD(bus_write_ivar,		pccbb_write_ivar),
	DEVMETHOD(bus_alloc_resource,		pccbb_alloc_resource),
	DEVMETHOD(bus_release_resource,		pccbb_release_resource),
	DEVMETHOD(bus_activate_resource,	pccbb_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	pccbb_deactivate_resource),
	DEVMETHOD(bus_driver_added,		pccbb_driver_added),
	DEVMETHOD(bus_child_detached,		pccbb_child_detached),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

        /* 16-bit card interface */
	DEVMETHOD(card_set_res_flags,		pccbb_pcic_set_res_flags),
	DEVMETHOD(card_set_memory_offset,	pccbb_pcic_set_memory_offset),

        /* power interface */
	DEVMETHOD(power_enable_socket,		pccbb_power_enable_socket),
	DEVMETHOD(power_disable_socket,		pccbb_power_disable_socket),

        /* pcib compatibility interface */
	DEVMETHOD(pcib_maxslots,		pccbb_maxslots),
	DEVMETHOD(pcib_read_config,		pccbb_read_config),
	DEVMETHOD(pcib_write_config,		pccbb_write_config),
	{0,0}
};

static driver_t pccbb_driver = {
	"pccbb",
	pccbb_methods,
	sizeof(struct pccbb_softc)
};

static devclass_t pccbb_devclass;

DRIVER_MODULE(pccbb, pci, pccbb_driver, pccbb_devclass, 0, 0);

SYSINIT(pccbb, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, pccbb_start_threads, 0);
