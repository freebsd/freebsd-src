/*-
 * Copyright (c) 2002 Matthew N. Dodd <winter@jurai.net>
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
 * $FreeBSD$
 */

/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <sys/bus.h>
#include <sys/conf.h>

#include <sys/module.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/if.h>
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

#include <dev/hea/eni_stats.h>
#include <dev/hea/eni.h>
#include <dev/hea/eni_suni.h>
#include <dev/hea/eni_var.h>

#include <dev/hea/hea_freebsd.h>

devclass_t hea_devclass;

static int hea_modevent(module_t, int, void *);

int
hea_alloc (device_t dev)
{
	struct hea_softc *sc;
	int error;

	sc = (struct hea_softc *)device_get_softc(dev);
	error = 0;

	sc->mem = bus_alloc_resource(dev, sc->mem_type, &sc->mem_rid,
					   0, ~0, 1, RF_ACTIVE);
	if (sc->mem == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENXIO;
		goto fail;
	}

	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid,
					0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq == NULL) {
		device_printf(dev, "Unable to allocate interrupt resource.\n");
		error = ENXIO;
		goto fail;
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), "Interrupt lock", MTX_DEF|MTX_RECURSE);

fail:
	return (error);
}

int
hea_free (device_t dev)
{
	struct hea_softc *sc;

	sc = (struct hea_softc *)device_get_softc(dev);
	if (sc->mem)
		bus_release_resource(dev, sc->mem_type, sc->mem_rid, sc->mem);
	if (sc->irq_ih)
		bus_teardown_intr(dev, sc->irq, sc->irq_ih);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);

	/*
	 * Destroy the mutex.
	 */
	if (mtx_initialized(&sc->mtx) != 0)
		 mtx_destroy(&sc->mtx);

	return (0);
}
 
int
hea_attach (device_t dev)
{
	struct hea_softc *sc;
	Eni_unit *eup;
	int error;
	long val;

	sc = (struct hea_softc *)device_get_softc(dev);
	eup = &sc->eup;
	error = 0;

	/*
	 * Start initializing it
	 */
	eup->eu_unit = device_get_unit(dev);
	eup->eu_mtu = ENI_IFF_MTU;
	eup->eu_vcc_zone = eni_vcc_zone;
	eup->eu_nif_zone = eni_nif_zone;
	eup->eu_ioctl = eni_atm_ioctl;
	eup->eu_instvcc = eni_instvcc;
	eup->eu_openvcc = eni_openvcc;
	eup->eu_closevcc = eni_closevcc;
	eup->eu_output = eni_output;

	eup->eu_pcitag = dev;
	eup->eu_softc = (void *)sc;

	/*
	 * Map memory structures into adapter space
	 */
	eup->eu_suni = (Eni_mem)(eup->eu_base + SUNI_OFFSET);
	eup->eu_midway = (Eni_mem)(eup->eu_base + MIDWAY_OFFSET);
	eup->eu_vcitbl = (VCI_Table *)(eup->eu_base + VCITBL_OFFSET);
	eup->eu_rxdma = (Eni_mem)(eup->eu_base + RXQUEUE_OFFSET);
	eup->eu_txdma = (Eni_mem)(eup->eu_base + TXQUEUE_OFFSET);
	eup->eu_svclist = (Eni_mem)(eup->eu_base + SVCLIST_OFFSET);
	eup->eu_servread = 0;

	/*
	 * Reset the midway chip
	 */
	eup->eu_midway[MIDWAY_ID] = MIDWAY_RESET;

	/*
	 * Size and test adapter memory. Initialize our adapter memory
	 * allocater.
	 */
	if (eni_init_memory(eup) < 0) {
		/*
		 * Adapter memory test failed. Clean up and
		 * return.
		 */
		device_printf(dev, "memory test failed.\n");
		error = ENXIO;
		goto fail;
	}

	if (eup->eu_type == TYPE_ADP) {
		int i;
#define MID_ADPMACOFF   0xffc0          /* mac address offset (adaptec only) */

		for (i = 0; i < sizeof(struct mac_addr); i++) {
			eup->eu_pif.pif_macaddr.ma_data[i] =
				bus_space_read_1(rman_get_bustag(sc->mem),
						rman_get_bushandle(sc->mem),
						MID_ADPMACOFF + i);
		}

	} else
	if (eup->eu_type == TYPE_ENI) {
		/*
	 	 * Read the contents of the SEEPROM
	 	 */
		eni_read_seeprom(eup);

		/*
	 	 * Copy MAC address to PIF and config structures
		 */
		bcopy((caddr_t)&eup->eu_seeprom[SEPROM_MAC_OFF],
		      (caddr_t)&eup->eu_pif.pif_macaddr,
		      sizeof(struct mac_addr));
		/*
		 * Copy serial number into config space
		 */
		eup->eu_config.ac_serial =
			ntohl(*(u_long *)&eup->eu_seeprom[SEPROM_SN_OFF]);
	} else {
		device_printf(dev, "Unknown adapter type!\n");
		error = ENXIO;
		goto fail;
	}

	eup->eu_config.ac_macaddr = eup->eu_pif.pif_macaddr;

	/*
	 * Setup some of the adapter configuration
	 */
	/*
	 * Get MIDWAY ID
	 */
	val = eup->eu_midway[MIDWAY_ID];
	eup->eu_config.ac_vendor = VENDOR_ENI;
	eup->eu_config.ac_vendapi = VENDAPI_ENI_1;
	eup->eu_config.ac_device = DEV_ENI_155P;
	eup->eu_config.ac_media = val & MEDIA_MASK ? MEDIA_UTP155 : MEDIA_OC3C;
	eup->eu_pif.pif_pcr = ATM_PCR_OC3C;

	/*
	 * Make a hw version number from the ID register values.
	 * Format: {Midway ID}.{Mother board ID}.{Daughter board ID}
	 */
	snprintf(eup->eu_config.ac_hard_vers,
		 sizeof(eup->eu_config.ac_hard_vers),
		 "%ld/%ld/%ld",
		 (val >> ID_SHIFT) & ID_MASK,
		 (val >> MID_SHIFT) & MID_MASK,
		 (val >> DID_SHIFT) & DID_MASK );

	/*
	 * There is no software version number
	 */
	eup->eu_config.ac_firm_vers[0] = '\0';

	/*
	 * Save device ram info for user-level programs
	 * NOTE: This really points to start of EEPROM
	 * and includes all the device registers in the
	 * lower 2 Megabytes.
	 */
	eup->eu_config.ac_ram = (long)eup->eu_base;
	eup->eu_config.ac_ramsize = eup->eu_ramsize + ENI_REG_SIZE;

	/*
	 * Setup max VPI/VCI values
	 */
	eup->eu_pif.pif_maxvpi = ENI_MAX_VPI;
	eup->eu_pif.pif_maxvci = ENI_MAX_VCI;

	/*
	 * Register this interface with ATM core services
	 */
	error = atm_physif_register((Cmn_unit *)eup, ENI_DEV_NAME, eni_services);
	if (error)
		 goto fail;

	eni_units[device_get_unit(dev)] = eup;

	/*
	 * Initialize driver processing
	 */
	error = eni_init(eup);
	if (error) {
		device_printf(dev, "adapter init failed.\n");
		goto fail;
	}

fail:
	return (error);
}

int
hea_detach (device_t dev)
{
	struct hea_softc *sc;
	Eni_unit *eup;
	int error;

	sc = (struct hea_softc *)device_get_softc(dev);
	eup = &sc->eup;
	error = 0;

	/*
	 * De-Register this interface with ATM core services
	 */
	error = atm_physif_deregister((Cmn_unit *)eup);

	/*
	 * Reset the board.
	 */
	hea_reset(dev);

	hea_free(dev);

	return (error);
}

void
hea_intr (void * arg)
{
	struct hea_softc *sc;

	sc = (struct hea_softc *)arg;

	HEA_LOCK(sc);
	eni_intr(&sc->eup);
	HEA_UNLOCK(sc);

	return;
}

void
hea_reset (device_t dev)
{
	struct hea_softc *sc;
	Eni_unit *eup;

	sc = (struct hea_softc *)device_get_softc(dev);
	eup = &sc->eup;

	/*
	 * We should really close down any open VCI's and
	 * release all memory (TX and RX) buffers. For now,
	 * we assume we're shutting the card down for good.
	 */

	if (eup->eu_midway) {
	        /*
	         * Issue RESET command to Midway chip
	         */
	        eup->eu_midway[MIDWAY_ID] = MIDWAY_RESET;

	        /*
	         * Delay to allow everything to terminate
	         */
	        DELAY(MIDWAY_DELAY);
	}

	return;
}

static int
hea_modevent (module_t mod, int type, void *data)
{
	int error;

	error = 0;

	switch (type) {
	case MOD_LOAD:

		eni_nif_zone = uma_zcreate("eni nif", sizeof(struct atm_nif),
			NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (eni_nif_zone == NULL)
			panic("%s(): uma_zcreate nif", __func__);
		uma_zone_set_max(eni_nif_zone, 52);

		eni_vcc_zone = uma_zcreate("eni vcc", sizeof(Eni_vcc),
			NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (eni_vcc_zone == NULL)
			panic("%s(): uma_zcreate vcc", __func__);
		uma_zone_set_max(eni_vcc_zone, 100);

		break;
	
	case MOD_UNLOAD:

		uma_zdestroy(eni_nif_zone);
		uma_zdestroy(eni_vcc_zone);

		break;
	default:
		break;
	}

	return (error);
}

static moduledata_t hea_moduledata = {
	"hea",
	hea_modevent,
	NULL
};
DECLARE_MODULE(hea, hea_moduledata, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(hea, 1);
