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

#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <dev/hfa/fore_var.h>
#include <dev/hfa/fore_include.h>

#include <dev/hfa/hfa_freebsd.h>

devclass_t hfa_devclass;

static int hfa_modevent(module_t, int, void *);

int
hfa_alloc (device_t dev)
{
	struct hfa_softc *sc;
	int error;

	sc = (struct hfa_softc *)device_get_softc(dev);
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
hfa_free (device_t dev)
{
	struct hfa_softc *sc;

	sc = (struct hfa_softc *)device_get_softc(dev);

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
hfa_attach (device_t dev)
{
	struct hfa_softc *sc;
	Fore_unit *fup;
	int error;
	int err_count;

	sc = (struct hfa_softc *)device_get_softc(dev);
	fup = &sc->fup;
	error = 0;
	err_count = BOOT_LOOPS;

	/*
	 * Start initializing it
	 */
	fup->fu_unit = device_get_unit(dev);
	fup->fu_mtu = FORE_IFF_MTU;
	fup->fu_vcc_zone = fore_vcc_zone;
	fup->fu_nif_zone = fore_nif_zone;
	fup->fu_ioctl = fore_atm_ioctl;
	fup->fu_instvcc = fore_instvcc;
	fup->fu_openvcc = fore_openvcc;
	fup->fu_closevcc = fore_closevcc;
	fup->fu_output = fore_output;
	fup->fu_softc = (void *)sc;

	callout_handle_init(&fup->fu_thandle);

	/*
	 * Poke the hardware - boot the CP and prepare it for downloading
	 */
	hfa_reset(dev);

	/*
	 * Wait for the monitor to perform self-test
	 */
	while (CP_READ(fup->fu_mon->mon_bstat) != BOOT_MONREADY) {
		 if (CP_READ(fup->fu_mon->mon_bstat) == BOOT_FAILTEST) {
			  device_printf(dev, "failed self-test\n");
			  goto fail;
		 } else if ( --err_count == 0 ) {
			  device_printf(dev, "unable to boot - status=0x%lx\n",
				   (u_long)CP_READ(fup->fu_mon->mon_bstat));
			  goto fail;
		 }
		 DELAY ( BOOT_DELAY );
	}

	/*
	 * Setup the adapter config info - at least as much as we can
	 */
	fup->fu_config.ac_vendor = VENDOR_FORE;
	fup->fu_config.ac_vendapi = VENDAPI_FORE_1;
	fup->fu_config.ac_media = MEDIA_OC3C;
	fup->fu_pif.pif_pcr = ATM_PCR_OC3C;

	/*
	 * Save device ram info for user-level programs
	 */
	fup->fu_config.ac_ram = (long)fup->fu_ram;
	fup->fu_config.ac_ramsize = fup->fu_ramsize;

	/*
	 * Set device capabilities
	 */
	fup->fu_pif.pif_maxvpi = FORE_MAX_VPI;
	fup->fu_pif.pif_maxvci = FORE_MAX_VCI;

	/*
	 * Register this interface with ATM core services
	 */
	error = atm_physif_register((Cmn_unit *)fup, FORE_DEV_NAME, fore_services);
	if (error)
		 goto fail;

	fore_units[device_get_unit(dev)] = fup;
	fore_nunits++;

	/*
	 * Initialize the CP microcode program.
	 */
	fore_initialize(fup);

fail:
	return (error);
}

int
hfa_detach (device_t dev)
{
	struct hfa_softc *sc;
	Fore_unit *fup;
	int error;

	sc = (struct hfa_softc *)device_get_softc(dev);
	fup = &sc->fup;
	error = 0;

	/*
	 * De-Register this interface with ATM core services
	 */
	error = atm_physif_deregister((Cmn_unit *)fup);

	/*
	 * Reset the board and return it to cold_start state.
	 * Hopefully, this will prevent use of resources as
	 * we're trying to free things up.
	 */
	hfa_reset(dev);

	/*
	 * Lock out all device interrupts
	 */
	DEVICE_LOCK((Cmn_unit *)fup);

	/*
	 * Remove any pending timeout()'s
	 */
	(void)untimeout((KTimeout_ret(*)(void *))fore_initialize,
		 (void *)fup, fup->fu_thandle);

	hfa_free(dev);

	DEVICE_UNLOCK((Cmn_unit *)fup);

	/*
	 * Free any Fore-specific device resources
	 */
	fore_interface_free(fup);

	return (error);
}

void
hfa_intr (void * arg)
{
	struct hfa_softc *sc;

	sc = (struct hfa_softc *)arg;

	HFA_LOCK(sc);
	fore_intr(&sc->fup);
	HFA_UNLOCK(sc);

	return;
}

void
hfa_reset (device_t dev)
{
	struct hfa_softc *sc;
	Fore_unit *fup;

	sc = (struct hfa_softc *)device_get_softc(dev);
	fup = &sc->fup;
	HFA_LOCK(sc);

	/*
	 * Reset the board and return it to cold_start state
	 */
	if (fup->fu_mon)
		fup->fu_mon->mon_bstat = CP_WRITE(BOOT_COLDSTART);

	if (fup->fu_ctlreg) {

		switch (fup->fu_config.ac_device) {
		case DEV_FORE_ESA200E:

			break;

		case DEV_FORE_SBA200E:
			/*
			 * Reset i960 by setting and clearing RESET
			 */
			SBA200E_HCR_INIT(*fup->fu_ctlreg, SBA200E_RESET);
			SBA200E_HCR_CLR(*fup->fu_ctlreg, SBA200E_RESET);
			break;

		case DEV_FORE_SBA200:
			/*
			 * Reset i960 by setting and clearing RESET
			 *
			 * SBA200 will NOT reset if bit is OR'd in!
			 */
			*fup->fu_ctlreg = SBA200_RESET;
			*fup->fu_ctlreg = SBA200_RESET_CLR;
			break;

		case DEV_FORE_PCA200E:
			/*
			 * Reset i960 by setting and clearing RESET
			 */
			PCA200E_HCR_INIT(*fup->fu_ctlreg, PCA200E_RESET);
			DELAY(10000);
			PCA200E_HCR_CLR(*fup->fu_ctlreg, PCA200E_RESET);
			break;
		default:
			break;
		}
	}

	HFA_UNLOCK(sc);
	return;
}

static int	 
hfa_modevent (module_t mod, int what, void *arg)
{
	int error;

	error = 0;

	switch (what) {
	case MOD_LOAD:
		/*
		* Verify software version
		*/
		if (atm_version != ATM_VERSION) {
			printf("hfa: version mismatch: fore=%d.%d kernel=%d.%d\n",
				ATM_VERS_MAJ(ATM_VERSION),
				ATM_VERS_MIN(ATM_VERSION),
				ATM_VERS_MAJ(atm_version),
				ATM_VERS_MIN(atm_version));
			error = EINVAL;
			break;
		}

		fore_nif_zone = uma_zcreate("fore nif", sizeof(struct atm_nif), NULL,
		    NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (fore_nif_zone == NULL)
			panic("hfa_modevent:uma_zcreate nif");
		uma_zone_set_max(fore_nif_zone, 52);

		fore_vcc_zone = uma_zcreate("fore vcc", sizeof(Fore_vcc), NULL,
		    NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (fore_vcc_zone == NULL)
			panic("hfa_modevent: uma_zcreate vcc");
		uma_zone_set_max(fore_vcc_zone, 100);
	
		/*
		* Start up watchdog timer
		*/
		atm_timeout(&fore_timer, ATM_HZ * FORE_TIME_TICK, fore_timeout);

		break;
	case MOD_UNLOAD:
		/*
		 * Stop watchdog timer
		 */
		atm_untimeout(&fore_timer);

		uma_zdestroy(fore_nif_zone);
		uma_zdestroy(fore_vcc_zone);

		break;
	default:
		break;
	}

	return (error);
}

static moduledata_t hfa_moduledata = {
	"hfa",
	hfa_modevent,
	NULL
};
DECLARE_MODULE(hfa, hfa_moduledata, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(hfa, 1);
