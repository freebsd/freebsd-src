/*	$NetBSD: if_ray.c,v 1.12 2000/02/07 09:36:27 augustss Exp $	*/
/*-
 * Copyright (C) 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.
 *
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 */

/*-
 * Copyright (c) 2000 Christian E. Hopps
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Card configuration
 * ==================
 *
 * This card is unusual in that it uses both common and attribute
 * memory whilst working. It should use common memory and an IO port.
 *
 * The bus resource allocations need to work around the brain deadness
 * of pccardd (where it reads the CIS for common memory, sets it all
 * up and then throws it all away assuming the card is an ed
 * driver...). Note that this could be dangerous (because it doesn't
 * interact with pccardd) if you use other memory mapped cards in the
 * same pccard slot as currently old mappings are not cleaned up very well
 * by the bus_release_resource methods or pccardd.
 *
 * There is no support for running this driver on 4.0.
 *
 * Ad-hoc and infra-structure modes
 * ================================
 * 
 * The driver supports ad-hoc mode for V4 firmware and infrastructure
 * mode for V5 firmware. V5 firmware in ad-hoc mode is untested and should
 * work.
 *
 * The Linux driver also seems to have the capability to act as an AP.
 * I wonder what facilities the "AP" can provide within a driver? We can
 * probably use the BRIDGE code to form an ESS but I don't think
 * power saving etc. is easy.
 *
 *
 * Packet framing/encapsulation/translation
 * ========================================
 * 
 * Currently we support the Webgear encapsulation:
 *	802.11	header <net/if_ieee80211.h>struct ieee80211_frame
 *	802.3	header <net/ethernet.h>struct ether_header
 *	IP/ARP	payload
 *
 * and RFC1042 encapsulation of IP datagrams (translation):
 *	802.11	header <net/if_ieee80211.h>struct ieee80211_frame
 *	802.2	LLC header
 *	802.2	SNAP header
 *	802.3	Ethertype
 *	IP/ARP	payload
 *
 * Framing should be selected via if_media stuff or link types but
 * is currently hardcoded to:
 *	V4	encapsulation
 *	V5	translation
 *
 *
 * Authentication
 * ==============
 *
 * 802.11 provides two authentication mechanisms. The first is a very
 * simple host based mechanism (like xhost) called Open System and the
 * second is a more complex challenge/response called Shared Key built
 * ontop of WEP.
 *
 * This driver only supports Open System and does not implement any
 * host based control lists. In otherwords authentication is always
 * granted to hosts wanting to authenticate with this station. This is
 * the only sensible behaviour as the Open System mechanism uses MAC
 * addresses to identify hosts. Send me patches if you need it!
 */

/*
 * ***check all XXX_INFRA code - reassoc not done well at all!
 * ***watchdog to catch screwed up removals?
 * ***error handling of RAY_COM_RUNQ
 * ***error handling of ECF command completions
 * ***can't seem to create a n/w that Win95 wants to see.
 * ***remove panic in ray_com_ecf by re-quing or timeout
 * ***use new ioctl stuff - probably need to change RAY_COM_FCHKRUNNING things?
 * 	consider user doing:
 *		ifconfig ray0 192.168.200.38 -bssid "freed"
 *		ifconfig ray0 192.168.200.38 -bssid "fred"
 * 	here the second one would be missed in this code
 * check that v5 needs timeouts on ecf commands
 * write up driver structure in comments above
 * UPDATE_PARAMS seems to return via an interrupt - maybe the timeout
 *	is needed for wrong values?
 * proper setting of mib_hop_seq_len with country code for v4 firmware
 *	best done with raycontrol?
 * countrycode setting is broken I think
 *	userupdate should trap and do via startjoin etc.
 * fragmentation when rx level drops?
 * v5 might not need download
 *	defaults are as documented apart from hop_seq_length
 *	settings are sane for ad-hoc not infra
 *
 * driver state
 *	most state is implied by the sequence of commands in the runq
 *	but in fact any of the rx and tx path that uses variables
 *	in the sc_c are potentially going to get screwed?
 *
 * infra mode stuff
 * 	proper handling of the basic rate set - see the manual
 *	all ray_sj, ray_assoc sequencues need a "nicer" solution as we
 *		remember association and authentication
 *	need to consider WEP
 *	acting as ap - should be able to get working from the manual
 *	need to finish RAY_ECMD_REJOIN_DONE
 *	finish authenitcation code, it doesn't handle errors/timeouts/
 *	REJOIN etc.
 *
 * ray_nw_param
 *	promisc in here too? - done
 *	should be able to update the parameters before we download to the
 *		device. This means we must attach a desired struct to the
 *		runq entry and maybe have another big case statement to
 *		move these desired into current when not running.
 *		init must then use the current settings (pre-loaded
 *		in attach now!) and pass to download. But we can't access
 *		current nw params outside of the runq - ahhh
 * 	differeniate between parameters set in attach and init
 * 	sc_station_addr in here too (for changing mac address)
 * 	move desired into the command structure?
 *	take downloaded MIB from a complete nw_param?
 *	longer term need to attach a desired nw params to the runq entry
 *
 *
 * RAY_COM_RUNQ errors
 *
 * if sleeping in ccs_alloc with eintr/erestart/enxio/enodev
 *	erestart	try again from the top
 *			XXX do not malloc more comqs
 *			XXX ccs allocation hard
 *	eintr		clean up and return
 *	enxio		clean up and return - done in macro
 *
 * if sleeping in runq_arr itself with eintr/erestart/enxio/enodev
 *	erestart	try again from the top
 *			XXX do not malloc more comqs
 *			XXX ccs allocation hard
 *			XXX reinsert comqs at head of list
 *	eintr		clean up and return
 *	enxio		clean up and return - done in macro
 */

#define XXX		0
#define XXX_ACTING_AP	0
#define XXX_INFRA	0
#define RAY_DEBUG	(				\
                        /* RAY_DBG_AUTH		| */  	\
 			/* RAY_DBG_SUBR		| */ 	\
			/* RAY_DBG_BOOTPARAM	| */ 	\
			/* RAY_DBG_STARTJOIN	| */ 	\
			/* RAY_DBG_CCS		| */	\
                        /* RAY_DBG_IOCTL	| */	\
                        /* RAY_DBG_MBUF		| */ 	\
                        /* RAY_DBG_RX		| */	\
                        /* RAY_DBG_CM		| */	\
                        /* RAY_DBG_COM		| */  	\
                        /* RAY_DBG_STOP		| */	\
                        /* RAY_DBG_CTL		| */	\
                        /* RAY_DBG_MGT		| */ 	\
                        /* RAY_DBG_TX		| */  	\
                        /* RAY_DBG_DCOM		| */  	\
			0				\
			)

/*
 * XXX build options - move to LINT
 */
#define RAY_CM_RID		0	/* pccardd abuses windows 0 and 1 */
#define RAY_AM_RID		3	/* pccardd abuses windows 0 and 1 */
#define RAY_COM_TIMEOUT		(hz/2)	/* Timeout for CCS commands */
#define RAY_TX_TIMEOUT		(hz/2)	/* Timeout for rescheduling TX */
#define RAY_ECF_SPIN_DELAY	1000	/* Wait 1ms before checking ECF ready */
#define RAY_ECF_SPIN_TRIES	10	/* Wait this many times for ECF ready */
/*
 * XXX build options - move to LINT
 */

#ifndef RAY_DEBUG
#define RAY_DEBUG 		0x0000
#endif /* RAY_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net80211/ieee80211.h>
#include <net/if_llc.h>
#include <net/if_types.h>

#include <dev/pccard/pccardvar.h>
#include "card_if.h"

#include <dev/ray/if_rayreg.h>
#include <dev/ray/if_raymib.h>
#include <dev/ray/if_raydbg.h>
#include <dev/ray/if_rayvar.h>

static MALLOC_DEFINE(M_RAYCOM, "raycom", "Raylink command queue entry");
/*
 * Prototyping
 */
static int	ray_attach		(device_t);
static int	ray_ccs_alloc		(struct ray_softc *sc, size_t *ccsp, char *wmesg);
static void	ray_ccs_fill		(struct ray_softc *sc, size_t ccs, u_int cmd);
static void	ray_ccs_free 		(struct ray_softc *sc, size_t ccs);
static int	ray_ccs_tx		(struct ray_softc *sc, size_t *ccsp, size_t *bufpp);
static void	ray_com_ecf		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_com_ecf_done	(struct ray_softc *sc);
static void	ray_com_ecf_timo	(void *xsc);
static struct ray_comq_entry *
		ray_com_init		(struct ray_comq_entry *com, ray_comqfn_t function, int flags, char *mesg);
static struct ray_comq_entry *
		ray_com_malloc		(ray_comqfn_t function, int flags, char *mesg);
static void	ray_com_runq		(struct ray_softc *sc);
static int	ray_com_runq_add	(struct ray_softc *sc, struct ray_comq_entry *com[], int ncom, char *wmesg);
static void	ray_com_runq_done	(struct ray_softc *sc);
static int	ray_detach		(device_t);
static void	ray_init		(void *xsc);
static int	ray_init_user		(struct ray_softc *sc);
static void	ray_init_assoc		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_assoc_done	(struct ray_softc *sc, u_int8_t status, size_t ccs);
static void	ray_init_auth		(struct ray_softc *sc, struct ray_comq_entry *com);
static int	ray_init_auth_send	(struct ray_softc *sc, u_int8_t *dst, int sequence);
static void	ray_init_auth_done	(struct ray_softc *sc, u_int8_t status);
static void	ray_init_download	(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_download_done	(struct ray_softc *sc, u_int8_t status, size_t ccs);
static void	ray_init_download_v4	(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_download_v5	(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_mcast		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_sj		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_sj_done	(struct ray_softc *sc, u_int8_t status, size_t ccs);
static void	ray_intr		(void *xsc);
static void	ray_intr_ccs		(struct ray_softc *sc, u_int8_t cmd, u_int8_t status, size_t ccs);
static void	ray_intr_rcs		(struct ray_softc *sc, u_int8_t cmd, size_t ccs);
static void	ray_intr_updt_errcntrs	(struct ray_softc *sc);
static int	ray_ioctl		(struct ifnet *ifp, u_long command, caddr_t data);
static void	ray_mcast		(struct ray_softc *sc, struct ray_comq_entry *com); 
static void	ray_mcast_done		(struct ray_softc *sc, u_int8_t status, size_t ccs); 
static int	ray_mcast_user		(struct ray_softc *sc); 
static int	ray_probe		(device_t);
static void	ray_promisc		(struct ray_softc *sc, struct ray_comq_entry *com); 
static void	ray_repparams		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_repparams_done	(struct ray_softc *sc, u_int8_t status, size_t ccs);
static int	ray_repparams_user	(struct ray_softc *sc, struct ray_param_req *pr);
static int	ray_repstats_user	(struct ray_softc *sc, struct ray_stats_req *sr);
static int	ray_res_alloc_am	(struct ray_softc *sc);
static int	ray_res_alloc_cm	(struct ray_softc *sc);
static int	ray_res_alloc_irq	(struct ray_softc *sc);
static void	ray_res_release		(struct ray_softc *sc);
static void	ray_rx			(struct ray_softc *sc, size_t rcs);
static void	ray_rx_ctl		(struct ray_softc *sc, struct mbuf *m0);
static void	ray_rx_data		(struct ray_softc *sc, struct mbuf *m0, u_int8_t siglev, u_int8_t antenna);
static void	ray_rx_mgt		(struct ray_softc *sc, struct mbuf *m0);
static void	ray_rx_mgt_auth		(struct ray_softc *sc, struct mbuf *m0);
static void	ray_rx_mgt_beacon	(struct ray_softc *sc, struct mbuf *m0);
static void	ray_rx_mgt_info		(struct ray_softc *sc, struct mbuf *m0, union ieee80211_information *elements);
static void	ray_rx_update_cache	(struct ray_softc *sc, u_int8_t *src, u_int8_t siglev, u_int8_t antenna);
static void	ray_stop		(struct ray_softc *sc, struct ray_comq_entry *com);
static int	ray_stop_user		(struct ray_softc *sc);
static void	ray_tx			(struct ifnet *ifp);
static void	ray_tx_done		(struct ray_softc *sc, u_int8_t status, size_t ccs);
static void	ray_tx_timo		(void *xsc);
static int	ray_tx_send		(struct ray_softc *sc, size_t ccs, int pktlen, u_int8_t *dst);
static size_t	ray_tx_wrhdr		(struct ray_softc *sc, size_t bufp, u_int8_t type, u_int8_t fc1, u_int8_t *addr1, u_int8_t *addr2, u_int8_t *addr3);
static void	ray_upparams		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_upparams_done	(struct ray_softc *sc, u_int8_t status, size_t ccs);
static int	ray_upparams_user	(struct ray_softc *sc, struct ray_param_req *pr);
static void	ray_watchdog		(struct ifnet *ifp);
static u_int8_t ray_tx_best_antenna	(struct ray_softc *sc, u_int8_t *dst);

#if RAY_DEBUG & RAY_DBG_COM
static void	ray_com_ecf_check	(struct ray_softc *sc, size_t ccs, char *mesg);
#endif /* RAY_DEBUG & RAY_DBG_COM */
#if RAY_DEBUG & RAY_DBG_MBUF
static void	ray_dump_mbuf		(struct ray_softc *sc, struct mbuf *m, char *s);
#endif /* RAY_DEBUG & RAY_DBG_MBUF */

/*
 * PC-Card (PCMCIA) driver definition
 */
static device_method_t ray_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ray_probe),
	DEVMETHOD(device_attach,	ray_attach),
	DEVMETHOD(device_detach,	ray_detach),

	{ 0, 0 }
};

static driver_t ray_driver = {
	"ray",
	ray_methods,
	sizeof(struct ray_softc)
};

static devclass_t ray_devclass;

DRIVER_MODULE(ray, pccard, ray_driver, ray_devclass, 0, 0);

/* 
 * Probe for the card by checking its startup results.
 *
 * Fixup any bugs/quirks for different firmware.
 */
static int
ray_probe(device_t dev)
{
	struct ray_softc *sc = device_get_softc(dev);
	struct ray_ecf_startup_v5 *ep = &sc->sc_ecf_startup;
	int error;

	sc->dev = dev;
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	/*
	 * Read startup results from the card.
	 */
	error = ray_res_alloc_cm(sc);
	if (error)
		return (error);
	error = ray_res_alloc_am(sc);
	if (error) {
		ray_res_release(sc);
		return (error);
	}
	RAY_MAP_CM(sc);
	SRAM_READ_REGION(sc, RAY_ECF_TO_HOST_BASE, ep,
	    sizeof(sc->sc_ecf_startup));
	ray_res_release(sc);

	/*
	 * Check the card is okay and work out what version we are using.
	 */
	if (ep->e_status != RAY_ECFS_CARD_OK) {
		RAY_PRINTF(sc, "card failed self test 0x%b",
		    ep->e_status, RAY_ECFS_PRINTFB);
		return (ENXIO);
	}
	if (sc->sc_version != RAY_ECFS_BUILD_4 &&
	    sc->sc_version != RAY_ECFS_BUILD_5) {
		RAY_PRINTF(sc, "unsupported firmware version 0x%0x",
		    ep->e_fw_build_string);
		return (ENXIO);
	}
	RAY_DPRINTF(sc, RAY_DBG_BOOTPARAM, "found a card");
	sc->sc_gone = 0;

	/*
	 * Fixup tib size to be correct - on build 4 it is garbage
	 */
	if (sc->sc_version == RAY_ECFS_BUILD_4 && sc->sc_tibsize == 0x55)
		sc->sc_tibsize = sizeof(struct ray_tx_tib);

	return (0);
}

/*
 * Attach the card into the kernel
 */
static int
ray_attach(device_t dev)
{
	struct ray_softc *sc = device_get_softc(dev);
	struct ray_ecf_startup_v5 *ep = &sc->sc_ecf_startup;
	struct ifnet *ifp;
	size_t ccs;
	int i, error;

	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL)
		return (ENOSPC);

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	if ((sc == NULL) || (sc->sc_gone)) {
		if_free(ifp);
		return (ENXIO);
	}

	/*
	 * Grab the resources I need
	 */
	error = ray_res_alloc_cm(sc);
	if (error) {
		if_free(ifp);
		return (error);
	}
	error = ray_res_alloc_am(sc);
	if (error) {
		if_free(ifp);
		ray_res_release(sc);
		return (error);
	}
	error = ray_res_alloc_irq(sc);
	if (error) {
		if_free(ifp);
		ray_res_release(sc);
		return (error);
	}

	/*
	 * Reset any pending interrupts
	 */
	RAY_HCS_CLEAR_INTR(sc);

	/*
	 * Set the parameters that will survive stop/init and
	 * reset a few things on the card.
	 *
	 * Do not update these in ray_init_download's parameter setup
	 *
	 */
	RAY_MAP_CM(sc);
	bzero(&sc->sc_d, sizeof(struct ray_nw_param));
	bzero(&sc->sc_c, sizeof(struct ray_nw_param));

	/* Clear statistics counters */
	sc->sc_rxoverflow = 0;
	sc->sc_rxcksum = 0;
	sc->sc_rxhcksum = 0;
	sc->sc_rxnoise = 0;

	/* Clear signal and antenna cache */
	bzero(sc->sc_siglevs, sizeof(sc->sc_siglevs));

	/* Set all ccs to be free */
	bzero(sc->sc_ccsinuse, sizeof(sc->sc_ccsinuse));
	ccs = RAY_CCS_ADDRESS(0);
	for (i = 0; i < RAY_CCS_LAST; ccs += RAY_CCS_SIZE, i++)
		RAY_CCS_FREE(sc, ccs);

	/*
	 * Initialise the network interface structure
	 */
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_timer = 0;
	ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT);
	ifp->if_hdrlen = sizeof(struct ieee80211_frame) + 
	    sizeof(struct ether_header);
	ifp->if_baudrate = 1000000; /* Is this baud or bps ;-) */
	ifp->if_start = ray_tx;
	ifp->if_ioctl = ray_ioctl;
	ifp->if_watchdog = ray_watchdog;
	ifp->if_init = ray_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	ether_ifattach(ifp, ep->e_station_addr);

	/*
	 * Initialise the timers and driver
	 */
	callout_handle_init(&sc->com_timerh);
	callout_handle_init(&sc->tx_timerh);
	TAILQ_INIT(&sc->sc_comq);

	/*
	 * Print out some useful information
	 */
	if (bootverbose || (RAY_DEBUG & RAY_DBG_BOOTPARAM)) {
		RAY_PRINTF(sc, "start up results");
		if (sc->sc_version == RAY_ECFS_BUILD_4)
			printf(".  Firmware version 4\n");
		else
			printf(".  Firmware version 5\n");
		printf(".  Status 0x%b\n", ep->e_status, RAY_ECFS_PRINTFB);
		printf(".  Ether address %6D\n", ep->e_station_addr, ":");
		if (sc->sc_version == RAY_ECFS_BUILD_4) {
			printf(".  Program checksum %0x\n", ep->e_resv0);
			printf(".  CIS checksum %0x\n", ep->e_rates[0]);
		} else {
			printf(".  (reserved word) %0x\n", ep->e_resv0);
			printf(".  Supported rates %8D\n", ep->e_rates, ":");
		}
		printf(".  Japan call sign %12D\n", ep->e_japan_callsign, ":");
		if (sc->sc_version == RAY_ECFS_BUILD_5) {
			printf(".  Program checksum %0x\n", ep->e_prg_cksum);
			printf(".  CIS checksum %0x\n", ep->e_cis_cksum);
			printf(".  Firmware version %0x\n",
			    ep->e_fw_build_string);
			printf(".  Firmware revision %0x\n", ep->e_fw_build);
			printf(".  (reserved word) %0x\n", ep->e_fw_resv);
			printf(".  ASIC version %0x\n", ep->e_asic_version);
			printf(".  TIB size %0x\n", ep->e_tibsize);
		}
	}

	return (0);
}

/*
 * Detach the card
 *
 * This is usually called when the card is ejected, but
 * can be caused by a modunload of a controller driver.
 * The idea is to reset the driver's view of the device
 * and ensure that any driver entry points such as
 * read and write do not hang.
 */
static int
ray_detach(device_t dev)
{
	struct ray_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->ifp;
	struct ray_comq_entry *com;
	int s;

	s = splimp();

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STOP, "");

	if ((sc == NULL) || (sc->sc_gone))
		return (0);

	/*
	 * Mark as not running and detach the interface.
	 *
	 * N.B. if_detach can trigger ioctls so we do it first and
	 * then clean the runq.
	 */
	sc->sc_gone = 1;
	sc->sc_c.np_havenet = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ether_ifdetach(ifp);
	if_free(ifp);

	/*
	 * Stop the runq and wake up anyone sleeping for us.
	 */
	untimeout(ray_com_ecf_timo, sc, sc->com_timerh);
	untimeout(ray_tx_timo, sc, sc->tx_timerh);
	com = TAILQ_FIRST(&sc->sc_comq);
	TAILQ_FOREACH(com, &sc->sc_comq, c_chain) {
		com->c_flags |= RAY_COM_FDETACHED;
		com->c_retval = 0;
		RAY_DPRINTF(sc, RAY_DBG_STOP, "looking at com %p %b",
		    com, com->c_flags, RAY_COM_FLAGS_PRINTFB);
		if (com->c_flags & RAY_COM_FWOK) {
			RAY_DPRINTF(sc, RAY_DBG_STOP, "waking com %p", com);
			wakeup(com->c_wakeup);
		}
	}
	
	/*
	 * Release resources
	 */
	ray_res_release(sc);
	RAY_DPRINTF(sc, RAY_DBG_STOP, "unloading complete");

	splx(s);

	return (0);
}

/*
 * Network ioctl request.
 */
static int
ray_ioctl(register struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ray_softc *sc = ifp->if_softc;
	struct ray_param_req pr;
	struct ray_stats_req sr;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error, error2;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_IOCTL, "");

	if ((sc == NULL) || (sc->sc_gone))
		return (ENXIO);

	error = error2 = 0;
	s = splimp();

	switch (command) {
	case SIOCSIFFLAGS:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "SIFFLAGS 0x%0x", ifp->if_flags);
		/*
		 * If the interface is marked up we call ray_init_user.
		 * This will deal with mcast and promisc flags as well as
		 * initialising the hardware if it needs it.
		 */
		if (ifp->if_flags & IFF_UP)
			error = ray_init_user(sc);
		else
			error = ray_stop_user(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "ADDMULTI/DELMULTI");
		error = ray_mcast_user(sc);
		break;

	case SIOCSRAYPARAM:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "SRAYPARAM");
		if ((error = copyin(ifr->ifr_data, &pr, sizeof(pr))))
			break;
		error = ray_upparams_user(sc, &pr);
		error2 = copyout(&pr, ifr->ifr_data, sizeof(pr));
		error = error2 ? error2 : error;
		break;

	case SIOCGRAYPARAM:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GRAYPARAM");
		if ((error = copyin(ifr->ifr_data, &pr, sizeof(pr))))
			break;
		error = ray_repparams_user(sc, &pr);
		error2 = copyout(&pr, ifr->ifr_data, sizeof(pr));
		error = error2 ? error2 : error;
		break;

	case SIOCGRAYSTATS:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GRAYSTATS");
		error = ray_repstats_user(sc, &sr);
		error2 = copyout(&sr, ifr->ifr_data, sizeof(sr));
		error = error2 ? error2 : error;
		break;

	case SIOCGRAYSIGLEV:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GRAYSIGLEV");
		error = copyout(sc->sc_siglevs, ifr->ifr_data,
		    sizeof(sc->sc_siglevs));
		break;

	case SIOCGIFFLAGS:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GIFFLAGS");
		error = EINVAL;
		break;

	case SIOCGIFMETRIC:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GIFMETRIC");
		error = EINVAL;
		break;

	case SIOCGIFMTU:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GIFMTU");
		error = EINVAL;
		break;

	case SIOCGIFPHYS:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GIFPYHS");
		error = EINVAL;
		break;

	case SIOCSIFMEDIA:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "SIFMEDIA");
		error = EINVAL;
		break;

	case SIOCGIFMEDIA:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GIFMEDIA");
		error = EINVAL;
		break;

	default:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "OTHER (pass to ether)");
		error = ether_ioctl(ifp, command, data);
		break;

	}

	splx(s);

	return (error);
}

/*
 * Ethernet layer entry to ray_init - discard errors
 */
static void
ray_init(void *xsc)
{
	struct ray_softc *sc = (struct ray_softc *)xsc;

	ray_init_user(sc);
}

/*
 * User land entry to network initialisation and changes in interface flags.
 * 
 * We do a very little work here, just creating runq entries to
 * processes the actions needed to cope with interface flags. We do it
 * this way in case there are runq entries outstanding from earlier
 * ioctls that modify the interface flags.
 *
 * Returns values are either 0 for success, a varity of resource allocation
 * failures or errors in the command sent to the card.
 *
 * Note, IFF_RUNNING is eventually set by init_sj_done or init_assoc_done
 */
static int
ray_init_user(struct ray_softc *sc)
{
	struct ray_comq_entry *com[6];
	int error, ncom;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");

	/*
	 * Create the following runq entries to bring the card up.
	 *
	 *		init_download	- download the network to the card
	 *		init_mcast	- reset multicast list
	 *		init_sj		- find or start a BSS
	 *		init_auth	- authenticate with an ESSID if needed
	 *		init_assoc	- associate with an ESSID if needed
	 *
	 * They are only actually executed if the card is not running.
	 * We may enter this routine from a simple change of IP
	 * address and do not need to get the card to do these things.
	 * However, we cannot perform the check here as there may be
	 * commands in the runq that change the IFF_RUNNING state of
	 * the interface.
	 */
	ncom = 0;
	com[ncom++] = RAY_COM_MALLOC(ray_init_download, RAY_COM_FCHKRUNNING);
	com[ncom++] = RAY_COM_MALLOC(ray_init_mcast, RAY_COM_FCHKRUNNING);
	com[ncom++] = RAY_COM_MALLOC(ray_init_sj, RAY_COM_FCHKRUNNING);
	com[ncom++] = RAY_COM_MALLOC(ray_init_auth, RAY_COM_FCHKRUNNING);
	com[ncom++] = RAY_COM_MALLOC(ray_init_assoc, RAY_COM_FCHKRUNNING);

	/*
	 * Create runq entries to process flags
	 *
	 *		promisc		- set/reset PROMISC and ALLMULTI flags
	 *
	 * They are only actually executed if the card is running
	 */
	com[ncom++] = RAY_COM_MALLOC(ray_promisc, 0);

	RAY_COM_RUNQ(sc, com, ncom, "rayinit", error);

	/* XXX no real error processing from anything yet! */

	RAY_COM_FREE(com, ncom);

	return (error);
}

/*
 * Runq entry for resetting driver and downloading start up structures to card
 */
static void
ray_init_download(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp = sc->ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");

	/* If the card already running we might not need to download */
	RAY_COM_CHKRUNNING(sc, com, ifp);

	/*
	 * Reset instance variables
	 *
	 * The first set are network parameters that are read back when
	 * the card starts or joins the network.
	 *
	 * The second set are network parameters that are downloaded to
	 * the card.
	 *
	 * The third set are driver parameters.
	 *
	 * All of the variables in these sets can be updated by the
	 * card or ioctls.
	 *
	 */
	sc->sc_d.np_upd_param = 0;
	bzero(sc->sc_d.np_bss_id, ETHER_ADDR_LEN);
	sc->sc_d.np_inited = 0;
	sc->sc_d.np_def_txrate = RAY_MIB_BASIC_RATE_SET_DEFAULT;
	sc->sc_d.np_encrypt = 0;

	bzero(sc->sc_d.np_ssid, IEEE80211_NWID_LEN);
	if (sc->sc_version == RAY_ECFS_BUILD_4) {
		sc->sc_d.np_net_type = RAY_MIB_NET_TYPE_V4;
		strncpy(sc->sc_d.np_ssid, RAY_MIB_SSID_V4, IEEE80211_NWID_LEN);
		sc->sc_d.np_ap_status = RAY_MIB_AP_STATUS_V4;
		sc->sc_d.np_framing = RAY_FRAMING_ENCAPSULATION;
	} else {
		sc->sc_d.np_net_type = RAY_MIB_NET_TYPE_V5;
		strncpy(sc->sc_d.np_ssid, RAY_MIB_SSID_V5, IEEE80211_NWID_LEN);
		sc->sc_d.np_ap_status = RAY_MIB_AP_STATUS_V5;
		sc->sc_d.np_framing = RAY_FRAMING_TRANSLATION;
	}
	sc->sc_d.np_priv_start = RAY_MIB_PRIVACY_MUST_START_DEFAULT;
	sc->sc_d.np_priv_join = RAY_MIB_PRIVACY_CAN_JOIN_DEFAULT;
	sc->sc_d.np_promisc = !!(ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI));

/* XXX this is a hack whilst I transition the code. The instance
 * XXX variables above should be set somewhere else. This is needed for
 * XXX start_join */
bcopy(&sc->sc_d, &com->c_desired, sizeof(struct ray_nw_param));
	    
	/*
	 * Download the right firmware defaults
	 */
	if (sc->sc_version == RAY_ECFS_BUILD_4)
		ray_init_download_v4(sc, com);
	else
		ray_init_download_v5(sc, com);

	/*
	 * Kick the card
	 */
	ray_ccs_fill(sc, com->c_ccs, RAY_CMD_DOWNLOAD_PARAMS);
	ray_com_ecf(sc, com);
}

#define	PUT2(p, v) 	\
    do { (p)[0] = ((v >> 8) & 0xff); (p)[1] = (v & 0xff); } while(0)
/*
 * Firmware version 4 defaults - see if_raymib.h for details
 */
static void
ray_init_download_v4(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ray_mib_4 ray_mib_4_default;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

#define MIB4(m)		ray_mib_4_default.m

	MIB4(mib_net_type)		= com->c_desired.np_net_type;
	MIB4(mib_ap_status)		= com->c_desired.np_ap_status;
	bcopy(com->c_desired.np_ssid, MIB4(mib_ssid), IEEE80211_NWID_LEN);
	MIB4(mib_scan_mode)		= RAY_MIB_SCAN_MODE_V4;
	MIB4(mib_apm_mode)		= RAY_MIB_APM_MODE_V4;
	bcopy(sc->sc_station_addr, MIB4(mib_mac_addr), ETHER_ADDR_LEN);
   PUT2(MIB4(mib_frag_thresh), 		  RAY_MIB_FRAG_THRESH_V4);
   PUT2(MIB4(mib_dwell_time),		  RAY_MIB_DWELL_TIME_V4);
   PUT2(MIB4(mib_beacon_period),	  RAY_MIB_BEACON_PERIOD_V4);
	MIB4(mib_dtim_interval)		= RAY_MIB_DTIM_INTERVAL_V4;
	MIB4(mib_max_retry)		= RAY_MIB_MAX_RETRY_V4;
	MIB4(mib_ack_timo)		= RAY_MIB_ACK_TIMO_V4;
	MIB4(mib_sifs)			= RAY_MIB_SIFS_V4;
	MIB4(mib_difs)			= RAY_MIB_DIFS_V4;
	MIB4(mib_pifs)			= RAY_MIB_PIFS_V4;
   PUT2(MIB4(mib_rts_thresh),		  RAY_MIB_RTS_THRESH_V4);
   PUT2(MIB4(mib_scan_dwell),		  RAY_MIB_SCAN_DWELL_V4);
   PUT2(MIB4(mib_scan_max_dwell),	  RAY_MIB_SCAN_MAX_DWELL_V4);
	MIB4(mib_assoc_timo)		= RAY_MIB_ASSOC_TIMO_V4;
	MIB4(mib_adhoc_scan_cycle)	= RAY_MIB_ADHOC_SCAN_CYCLE_V4;
	MIB4(mib_infra_scan_cycle)	= RAY_MIB_INFRA_SCAN_CYCLE_V4;
	MIB4(mib_infra_super_scan_cycle)
	 				= RAY_MIB_INFRA_SUPER_SCAN_CYCLE_V4;
	MIB4(mib_promisc)		= com->c_desired.np_promisc;
   PUT2(MIB4(mib_uniq_word),		  RAY_MIB_UNIQ_WORD_V4);
	MIB4(mib_slot_time)		= RAY_MIB_SLOT_TIME_V4;
	MIB4(mib_roam_low_snr_thresh)	= RAY_MIB_ROAM_LOW_SNR_THRESH_V4;
	MIB4(mib_low_snr_count)		= RAY_MIB_LOW_SNR_COUNT_V4;
	MIB4(mib_infra_missed_beacon_count)
	 				= RAY_MIB_INFRA_MISSED_BEACON_COUNT_V4;
	MIB4(mib_adhoc_missed_beacon_count)	
	 				= RAY_MIB_ADHOC_MISSED_BEACON_COUNT_V4;
	MIB4(mib_country_code)		= RAY_MIB_COUNTRY_CODE_V4;
	MIB4(mib_hop_seq)		= RAY_MIB_HOP_SEQ_V4;
	MIB4(mib_hop_seq_len)		= RAY_MIB_HOP_SEQ_LEN_V4;
	MIB4(mib_cw_max)		= RAY_MIB_CW_MAX_V4;
	MIB4(mib_cw_min)		= RAY_MIB_CW_MIN_V4;
	MIB4(mib_noise_filter_gain)	= RAY_MIB_NOISE_FILTER_GAIN_DEFAULT;
	MIB4(mib_noise_limit_offset)	= RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT;
	MIB4(mib_rssi_thresh_offset)	= RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT;
	MIB4(mib_busy_thresh_offset)	= RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT;
	MIB4(mib_sync_thresh)		= RAY_MIB_SYNC_THRESH_DEFAULT;
	MIB4(mib_test_mode)		= RAY_MIB_TEST_MODE_DEFAULT;
	MIB4(mib_test_min_chan)		= RAY_MIB_TEST_MIN_CHAN_DEFAULT;
	MIB4(mib_test_max_chan)		= RAY_MIB_TEST_MAX_CHAN_DEFAULT;
#undef MIB4

	SRAM_WRITE_REGION(sc, RAY_HOST_TO_ECF_BASE,
	    &ray_mib_4_default, sizeof(ray_mib_4_default));
}

/*
 * Firmware version 5 defaults - see if_raymib.h for details
 */
static void
ray_init_download_v5(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ray_mib_5 ray_mib_5_default;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

#define MIB5(m)		ray_mib_5_default.m
	MIB5(mib_net_type)		= com->c_desired.np_net_type;
	MIB5(mib_ap_status)		= com->c_desired.np_ap_status;
	bcopy(com->c_desired.np_ssid, MIB5(mib_ssid), IEEE80211_NWID_LEN);
	MIB5(mib_scan_mode)		= RAY_MIB_SCAN_MODE_V5;
	MIB5(mib_apm_mode)		= RAY_MIB_APM_MODE_V5;
	bcopy(sc->sc_station_addr, MIB5(mib_mac_addr), ETHER_ADDR_LEN);
   PUT2(MIB5(mib_frag_thresh), 	  	  RAY_MIB_FRAG_THRESH_V5);
   PUT2(MIB5(mib_dwell_time),		  RAY_MIB_DWELL_TIME_V5);
   PUT2(MIB5(mib_beacon_period),	  RAY_MIB_BEACON_PERIOD_V5);
	MIB5(mib_dtim_interval)		= RAY_MIB_DTIM_INTERVAL_V5;
	MIB5(mib_max_retry)		= RAY_MIB_MAX_RETRY_V5;
	MIB5(mib_ack_timo)		= RAY_MIB_ACK_TIMO_V5;
	MIB5(mib_sifs)			= RAY_MIB_SIFS_V5;
	MIB5(mib_difs)			= RAY_MIB_DIFS_V5;
	MIB5(mib_pifs)			= RAY_MIB_PIFS_V5;
   PUT2(MIB5(mib_rts_thresh),		  RAY_MIB_RTS_THRESH_V5);
   PUT2(MIB5(mib_scan_dwell),		  RAY_MIB_SCAN_DWELL_V5);
   PUT2(MIB5(mib_scan_max_dwell),	  RAY_MIB_SCAN_MAX_DWELL_V5);
	MIB5(mib_assoc_timo)		= RAY_MIB_ASSOC_TIMO_V5;
	MIB5(mib_adhoc_scan_cycle)	= RAY_MIB_ADHOC_SCAN_CYCLE_V5;
	MIB5(mib_infra_scan_cycle)	= RAY_MIB_INFRA_SCAN_CYCLE_V5;
	MIB5(mib_infra_super_scan_cycle)
	 				= RAY_MIB_INFRA_SUPER_SCAN_CYCLE_V5;
	MIB5(mib_promisc)		= com->c_desired.np_promisc;
   PUT2(MIB5(mib_uniq_word),		  RAY_MIB_UNIQ_WORD_V5);
	MIB5(mib_slot_time)		= RAY_MIB_SLOT_TIME_V5;
	MIB5(mib_roam_low_snr_thresh)	= RAY_MIB_ROAM_LOW_SNR_THRESH_V5;
	MIB5(mib_low_snr_count)	= RAY_MIB_LOW_SNR_COUNT_V5;
	MIB5(mib_infra_missed_beacon_count)
					= RAY_MIB_INFRA_MISSED_BEACON_COUNT_V5;
	MIB5(mib_adhoc_missed_beacon_count)
	 				= RAY_MIB_ADHOC_MISSED_BEACON_COUNT_V5;
	MIB5(mib_country_code)		= RAY_MIB_COUNTRY_CODE_V5;
	MIB5(mib_hop_seq)		= RAY_MIB_HOP_SEQ_V5;
	MIB5(mib_hop_seq_len)		= RAY_MIB_HOP_SEQ_LEN_V5;
   PUT2(MIB5(mib_cw_max),		  RAY_MIB_CW_MAX_V5);
   PUT2(MIB5(mib_cw_min),		  RAY_MIB_CW_MIN_V5);
	MIB5(mib_noise_filter_gain)	= RAY_MIB_NOISE_FILTER_GAIN_DEFAULT;
	MIB5(mib_noise_limit_offset)	= RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT;
	MIB5(mib_rssi_thresh_offset)	= RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT;
	MIB5(mib_busy_thresh_offset)	= RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT;
	MIB5(mib_sync_thresh)		= RAY_MIB_SYNC_THRESH_DEFAULT;
	MIB5(mib_test_mode)		= RAY_MIB_TEST_MODE_DEFAULT;
	MIB5(mib_test_min_chan)		= RAY_MIB_TEST_MIN_CHAN_DEFAULT;
	MIB5(mib_test_max_chan)		= RAY_MIB_TEST_MAX_CHAN_DEFAULT;
	MIB5(mib_allow_probe_resp)	= RAY_MIB_ALLOW_PROBE_RESP_DEFAULT;
	MIB5(mib_privacy_must_start)	= com->c_desired.np_priv_start;
	MIB5(mib_privacy_can_join)	= com->c_desired.np_priv_join;
	MIB5(mib_basic_rate_set[0])	= com->c_desired.np_def_txrate;
#undef MIB5

	SRAM_WRITE_REGION(sc, RAY_HOST_TO_ECF_BASE,
	    &ray_mib_5_default, sizeof(ray_mib_5_default));
}
#undef PUT2

/*
 * Download completion routine
 */
static void
ray_init_download_done(struct ray_softc *sc, u_int8_t status, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_COM_CHECK(sc, ccs);

	RAY_CCSERR(sc, status, if_oerrors); /* XXX error counter */

	ray_com_ecf_done(sc);
}

/*
 * Runq entry to empty the multicast filter list
 */
static void
ray_init_mcast(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp = sc->ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

	/* If the card already running we might not need to reset the list */
	RAY_COM_CHKRUNNING(sc, com, ifp);

	/*
	 * Kick the card
	 */
	ray_ccs_fill(sc, com->c_ccs, RAY_CMD_UPDATE_MCAST);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_update_mcast, c_nmcast, 0);

	ray_com_ecf(sc, com);
}

/*
 * Runq entry to starting or joining a network
 */
static void
ray_init_sj(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp = sc->ifp;
	struct ray_net_params np;
	int update;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

	/* If the card already running we might not need to start the n/w */
	RAY_COM_CHKRUNNING(sc, com, ifp);

	/*
	 * Set up the right start or join command and determine
	 * whether we should tell the card about a change in operating
	 * parameters.
	 */
	sc->sc_c.np_havenet = 0;
	if (sc->sc_d.np_net_type == RAY_MIB_NET_TYPE_ADHOC)
		ray_ccs_fill(sc, com->c_ccs, RAY_CMD_START_NET);
	else
		ray_ccs_fill(sc, com->c_ccs, RAY_CMD_JOIN_NET);

	update = 0;
	if (sc->sc_c.np_net_type != sc->sc_d.np_net_type)
		update++;
	if (bcmp(sc->sc_c.np_ssid, sc->sc_d.np_ssid, IEEE80211_NWID_LEN))
		update++;
	if (sc->sc_c.np_priv_join != sc->sc_d.np_priv_join)
		update++;
	if (sc->sc_c.np_priv_start != sc->sc_d.np_priv_start)
		update++;
	RAY_DPRINTF(sc, RAY_DBG_STARTJOIN,
	    "%s updating nw params", update?"is":"not");
	if (update) {
		bzero(&np, sizeof(np));
		np.p_net_type = sc->sc_d.np_net_type;
		bcopy(sc->sc_d.np_ssid, np.p_ssid,  IEEE80211_NWID_LEN);
		np.p_privacy_must_start = sc->sc_d.np_priv_start;
		np.p_privacy_can_join = sc->sc_d.np_priv_join;
		SRAM_WRITE_REGION(sc, RAY_HOST_TO_ECF_BASE, &np, sizeof(np));
		SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_net, c_upd_param, 1);
	} else
		SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_net, c_upd_param, 0);

	/*
	 * Kick the card
	 */
	ray_com_ecf(sc, com);
}

/*
 * Complete start command or intermediate step in assoc command
 */
static void
ray_init_sj_done(struct ray_softc *sc, u_int8_t status, size_t ccs)
{
	struct ifnet *ifp = sc->ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);
	RAY_COM_CHECK(sc, ccs);

	RAY_CCSERR(sc, status, if_oerrors); /* XXX error counter */

	/*
	 * Read back network parameters that the ECF sets
	 */
	SRAM_READ_REGION(sc, ccs, &sc->sc_c.p_1, sizeof(struct ray_cmd_net));

	/* Adjust values for buggy firmware */
	if (sc->sc_c.np_inited == 0x55)
		sc->sc_c.np_inited = 0;
	if (sc->sc_c.np_def_txrate == 0x55)
		sc->sc_c.np_def_txrate = sc->sc_d.np_def_txrate;
	if (sc->sc_c.np_encrypt == 0x55)
		sc->sc_c.np_encrypt = sc->sc_d.np_encrypt;

	/*
	 * Update our local state if we updated the network parameters
	 * when the START_NET or JOIN_NET was issued.
	 */
	if (sc->sc_c.np_upd_param) {
		RAY_DPRINTF(sc, RAY_DBG_STARTJOIN, "updated parameters");
		SRAM_READ_REGION(sc, RAY_HOST_TO_ECF_BASE,
		    &sc->sc_c.p_2, sizeof(struct ray_net_params));
	}

	/*
	 * Hurrah! The network is now active.
	 *
	 * Clearing IFF_OACTIVE will ensure that the system will send us
	 * packets. Just before we return from the interrupt context
	 * we check to see if packets have been queued.
	 */
	if (SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd) == RAY_CMD_START_NET) {
		sc->sc_c.np_havenet = 1;
		sc->sc_c.np_framing = sc->sc_d.np_framing;
		ifp->if_flags |= IFF_RUNNING;
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	ray_com_ecf_done(sc);
}

/*
 * Runq entry to authenticate with an access point or another station
 */
static void
ray_init_auth(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp = sc->ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN | RAY_DBG_AUTH, "");

	/* If card already running we might not need to authenticate */
	RAY_COM_CHKRUNNING(sc, com, ifp);

	/*
	 * Don't do anything if we are not in a managed network
	 *
	 * XXX V4 adhoc does not need this, V5 adhoc unknown
	 */
	if (sc->sc_c.np_net_type != RAY_MIB_NET_TYPE_INFRA) {
		ray_com_runq_done(sc);
		return;
	}

/*
 * XXX_AUTH need to think of run queue when doing auths from request i.e. would
 * XXX_AUTH need to have auth at top of runq?
 * XXX_AUTH ditto for sending any auth response packets...what about timeouts?
 */

	/*
	 * Kick the card
	 */
/* XXX_AUTH check exit status and retry or fail as we can't associate without this */
	ray_init_auth_send(sc, sc->sc_c.np_bss_id, IEEE80211_AUTH_OPEN_REQUEST);
}

/*
 * Build and send an authentication packet
 *
 * If an error occurs, returns 1 else returns 0.
 */
static int
ray_init_auth_send(struct ray_softc *sc, u_int8_t *dst, int sequence)
{
	size_t ccs, bufp;
	int pktlen = 0;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN | RAY_DBG_AUTH, "");

	/* Get a control block */
	if (ray_ccs_tx(sc, &ccs, &bufp)) {
	    	RAY_RECERR(sc, "could not obtain a ccs");
		return (1);
	}

	/* Fill the header in */
	bufp = ray_tx_wrhdr(sc, bufp,
	    IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_AUTH,
	    IEEE80211_FC1_DIR_NODS,
	    dst,
	    IFP2ENADDR(sc->ifp),
	    sc->sc_c.np_bss_id);

	/* Add algorithm number */
	SRAM_WRITE_1(sc, bufp + pktlen++, IEEE80211_AUTH_ALG_OPEN);
	SRAM_WRITE_1(sc, bufp + pktlen++, 0);

	/* Add sequence number */
	SRAM_WRITE_1(sc, bufp + pktlen++, sequence);
	SRAM_WRITE_1(sc, bufp + pktlen++, 0);

	/* Add status code */
	SRAM_WRITE_1(sc, bufp + pktlen++, 0);
	SRAM_WRITE_1(sc, bufp + pktlen++, 0);
	pktlen += sizeof(struct ieee80211_frame);

	return (ray_tx_send(sc, ccs, pktlen, dst));
}

/*
 * Complete authentication runq
 */
static void
ray_init_auth_done(struct ray_softc *sc, u_int8_t status)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN | RAY_DBG_AUTH, "");

	if (status != IEEE80211_STATUS_SUCCESS)
		RAY_RECERR(sc, "authentication failed with status %d", status);
/*
 * XXX_AUTH retry? if not just recall ray_init_auth_send and dont clear runq?
 * XXX_AUTH association requires that authenitcation is successful
 * XXX_AUTH before we associate, and the runq is the only way to halt the
 * XXX_AUTH progress of associate.
 * XXX_AUTH In this case I might not need the RAY_AUTH_NEEDED state
 */
	ray_com_runq_done(sc);
}

/*
 * Runq entry to starting an association with an access point
 */
static void
ray_init_assoc(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp = sc->ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");

	/* If the card already running we might not need to associate */
	RAY_COM_CHKRUNNING(sc, com, ifp);

	/*
	 * Don't do anything if we are not in a managed network
	 */
	if (sc->sc_c.np_net_type != RAY_MIB_NET_TYPE_INFRA) {
		ray_com_runq_done(sc);
		return;
	}

	/*
	 * Kick the card
	 */
	ray_ccs_fill(sc, com->c_ccs, RAY_CMD_START_ASSOC);
	ray_com_ecf(sc, com);
}

/*
 * Complete association
 */
static void
ray_init_assoc_done(struct ray_softc *sc, u_int8_t status, size_t ccs)
{
	struct ifnet *ifp = sc->ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_COM_CHECK(sc, ccs);

	RAY_CCSERR(sc, status, if_oerrors); /* XXX error counter */
	
	/*
	 * Hurrah! The network is now active.
	 *
	 * Clearing IFF_OACTIVE will ensure that the system will send us
	 * packets. Just before we return from the interrupt context
	 * we check to see if packets have been queued.
	 */
	sc->sc_c.np_havenet = 1;
	sc->sc_c.np_framing = sc->sc_d.np_framing;
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	ray_com_ecf_done(sc);
}

/*
 * Network stop.
 *
 * Inhibit card - if we can't prevent reception then do not worry;
 * stopping a NIC only guarantees no TX.
 *
 * The change to the interface flags is done via the runq so that any
 * existing commands can execute normally.
 */
static int
ray_stop_user(struct ray_softc *sc)
{
	struct ray_comq_entry *com[1];
	int error, ncom;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STOP, "");

	/*
	 * Schedule the real stop routine
	 */
	ncom = 0;
	com[ncom++] = RAY_COM_MALLOC(ray_stop, 0);

	RAY_COM_RUNQ(sc, com, ncom, "raystop", error);

	/* XXX no real error processing from anything yet! */

	RAY_COM_FREE(com, ncom);

	return (error);
}

/*
 * Runq entry for stopping the interface activity
 */
static void
ray_stop(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp = sc->ifp;
	struct mbuf *m;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STOP, "");

	/*
	 * Mark as not running and drain output queue
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		m_freem(m);
	}

	ray_com_runq_done(sc);
}

static void
ray_watchdog(struct ifnet *ifp)
{
	struct ray_softc *sc = ifp->if_softc;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	if ((sc == NULL) || (sc->sc_gone))
		return;

	RAY_PRINTF(sc, "watchdog timeout");
}

/*
 * Transmit packet handling
 */

/*
 * Send a packet.
 *
 * We make two assumptions here:
 *  1) That the current priority is set to splimp _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) That the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 *
 * A simple one packet at a time TX routine is used - we don't bother
 * chaining TX buffers. Performance is sufficient to max out the
 * wireless link on a P75.
 *
 * AST J30 Windows 95A (100MHz Pentium) to
 *   Libretto 50CT FreeBSD-3.1 (75MHz Pentium)		167.37kB/s
 *   Nonname box FreeBSD-3.4 (233MHz AMD K6)		161.82kB/s
 *
 * Libretto 50CT FreeBSD-3.1 (75MHz Pentium) to
 *   AST J30 Windows 95A (100MHz Pentium) 		167.37kB/s
 *   Nonname box FreeBSD-3.4 (233MHz AMD K6)		161.38kB/s
 *
 * Given that 160kB/s is saturating the 2Mb/s wireless link we
 * are about there.
 *
 * In short I'm happy that the added complexity of chaining TX
 * packets together isn't worth it for my machines.
 */
static void
ray_tx(struct ifnet *ifp)
{
	struct ray_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct ether_header *eh;
	struct llc *llc;
	size_t ccs, bufp;
	int pktlen, len;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_TX, "");
	RAY_MAP_CM(sc);

	/*
	 * Some simple checks first - some are overkill
	 */
	if ((sc == NULL) || (sc->sc_gone))
		return;
	if (!(ifp->if_flags & IFF_RUNNING)) {
		RAY_RECERR(sc, "cannot transmit - not running");
		return;
	}
	if (!sc->sc_c.np_havenet) {
		RAY_RECERR(sc, "cannot transmit - no network");
		return;
	}
	if (!RAY_ECF_READY(sc)) {
		/* Can't assume that the ECF is busy because of this driver */
		if ((sc->tx_timerh.callout == NULL) ||
		    (!callout_active(sc->tx_timerh.callout))) {
			sc->tx_timerh =
			    timeout(ray_tx_timo, sc, RAY_TX_TIMEOUT);
			return;
		    }
	} else
		untimeout(ray_tx_timo, sc, sc->tx_timerh);

	/*
	 * We find a ccs before we process the mbuf so that we are sure it
	 * is worthwhile processing the packet. All errors in the mbuf
	 * processing are either errors in the mbuf or gross configuration
	 * errors and the packet wouldn't get through anyway.
	 */
	if (ray_ccs_tx(sc, &ccs, &bufp)) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}
    
	/*
	 * Get the mbuf and process it - we have to remember to free the
	 * ccs if there are any errors.
	 */
	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL) {
		RAY_CCS_FREE(sc, ccs);
		return;
	}

	pktlen = m0->m_pkthdr.len;
	if (pktlen > ETHER_MAX_LEN - ETHER_CRC_LEN) {
		RAY_RECERR(sc, "mbuf too long %d", pktlen);
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		m_freem(m0);
		return;
	}

	m0 = m_pullup(m0, sizeof(struct ether_header));
	if (m0 == NULL) {
		RAY_RECERR(sc, "could not pullup ether");
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		return;
	}
	eh = mtod(m0, struct ether_header *);

	/*
	 * Write the 802.11 header according to network type etc.
	 */
	if (sc->sc_c.np_net_type == RAY_MIB_NET_TYPE_ADHOC)
		bufp = ray_tx_wrhdr(sc, bufp,
		    IEEE80211_FC0_TYPE_DATA,
		    IEEE80211_FC1_DIR_NODS,
		    eh->ether_dhost,
		    eh->ether_shost,
		    sc->sc_c.np_bss_id);
	else
		if (sc->sc_c.np_ap_status == RAY_MIB_AP_STATUS_TERMINAL)
			bufp = ray_tx_wrhdr(sc, bufp,
			    IEEE80211_FC0_TYPE_DATA,
			    IEEE80211_FC1_DIR_TODS,
			    sc->sc_c.np_bss_id,
			    eh->ether_shost,
			    eh->ether_dhost);
		else
			bufp = ray_tx_wrhdr(sc, bufp,
			    IEEE80211_FC0_TYPE_DATA,
			    IEEE80211_FC1_DIR_FROMDS,
			    eh->ether_dhost,
			    sc->sc_c.np_bss_id,
			    eh->ether_shost);

	/*
	 * Framing
	 *
	 * Add to the mbuf.
	 */
	switch (sc->sc_c.np_framing) {

    	case RAY_FRAMING_ENCAPSULATION:
		/* Nice and easy - nothing! (just add an 802.11 header) */
		break;

    	case RAY_FRAMING_TRANSLATION:
		/*
		 * Drop the first address in the ethernet header and
		 * write an LLC and SNAP header over the second.
		 */
		m_adj(m0, ETHER_ADDR_LEN);
		if (m0 == NULL) {
			RAY_RECERR(sc, "could not get space for 802.2 header");
			RAY_CCS_FREE(sc, ccs);
			ifp->if_oerrors++;
			return;
		}
		llc = mtod(m0, struct llc *);
		llc->llc_dsap = LLC_SNAP_LSAP;
		llc->llc_ssap = LLC_SNAP_LSAP;
		llc->llc_control = LLC_UI;
		llc->llc_un.type_snap.org_code[0] = 0;
		llc->llc_un.type_snap.org_code[1] = 0;
		llc->llc_un.type_snap.org_code[2] = 0;
		break;

	default:
		RAY_RECERR(sc, "unknown framing type %d", sc->sc_c.np_framing);
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		m_freem(m0);
		return;

	}
	if (m0 == NULL) {
		RAY_RECERR(sc, "could not frame packet");
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		return;
	}
	RAY_MBUF_DUMP(sc, RAY_DBG_TX, m0, "framed packet");

	/*
	 * Copy the mbuf to the buffer in common memory
	 *
	 * We drop and don't bother wrapping as Ethernet packets are 1518
	 * bytes, we checked the mbuf earlier, and our TX buffers are 2048
	 * bytes. We don't have 530 bytes of headers etc. so something
	 * must be fubar.
	 */
	pktlen = sizeof(struct ieee80211_frame);
	for (m = m0; m != NULL; m = m->m_next) {
		pktlen += m->m_len;
		if ((len = m->m_len) == 0)
			continue;
		if ((bufp + len) < RAY_TX_END)
			SRAM_WRITE_REGION(sc, bufp, mtod(m, u_int8_t *), len);
		else {
			RAY_RECERR(sc, "tx buffer overflow");
			RAY_CCS_FREE(sc, ccs);
			ifp->if_oerrors++;
			m_freem(m0);
			return;
		}
		bufp += len;
	}

	/*
	 * Send it off
	 */
	if (ray_tx_send(sc, ccs, pktlen, eh->ether_dhost))
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;
	m_freem(m0);
}

/*
 * Start timeout routine.
 *
 * Used when card was busy but we needed to send a packet.
 */
static void
ray_tx_timo(void *xsc)
{
	struct ray_softc *sc = (struct ray_softc *)xsc;
	struct ifnet *ifp = sc->ifp;
	int s;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	if (!(ifp->if_flags & IFF_OACTIVE) && (ifp->if_snd.ifq_head != NULL)) {
		s = splimp();
		ray_tx(ifp);
		splx(s);
	}
}

/*
 * Write an 802.11 header into the Tx buffer space and return the
 * adjusted buffer pointer.
 */
static size_t
ray_tx_wrhdr(struct ray_softc *sc, size_t bufp, u_int8_t type, u_int8_t fc1, u_int8_t *addr1, u_int8_t *addr2, u_int8_t *addr3)
{
	struct ieee80211_frame header;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_TX, "");
	RAY_MAP_CM(sc);

	bzero(&header, sizeof(struct ieee80211_frame));
	header.i_fc[0] = (IEEE80211_FC0_VERSION_0 | type);
	header.i_fc[1] = fc1;
	bcopy(addr1, header.i_addr1, ETHER_ADDR_LEN);
	bcopy(addr2, header.i_addr2, ETHER_ADDR_LEN);
	bcopy(addr3, header.i_addr3, ETHER_ADDR_LEN);

	SRAM_WRITE_REGION(sc, bufp, (u_int8_t *)&header,
	    sizeof(struct ieee80211_frame));

	return (bufp + sizeof(struct ieee80211_frame));
}

/*
 * Fill in a few loose ends and kick the card to send the packet
 *
 * Returns 0 on success, 1 on failure
 */
static int
ray_tx_send(struct ray_softc *sc, size_t ccs, int pktlen, u_int8_t *dst)
{
	int i = 0;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_TX, "");
	RAY_MAP_CM(sc);

	while (!RAY_ECF_READY(sc)) {
		DELAY(RAY_ECF_SPIN_DELAY);
		if (++i > RAY_ECF_SPIN_TRIES) {
			RAY_RECERR(sc, "ECF busy, dropping packet");
			RAY_CCS_FREE(sc, ccs);
			return (1);
		}
	}
	if (i != 0)
		RAY_RECERR(sc, "spun %d times", i);

	SRAM_WRITE_FIELD_2(sc, ccs, ray_cmd_tx, c_len, pktlen);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_antenna,
	    ray_tx_best_antenna(sc, dst));
	SRAM_WRITE_1(sc, RAY_SCB_CCSI, RAY_CCS_INDEX(ccs));
	RAY_ECF_START_CMD(sc);

	return (0);
}

/*
 * Determine best antenna to use from rx level and antenna cache
 */
static u_int8_t
ray_tx_best_antenna(struct ray_softc *sc, u_int8_t *dst)
{
	struct ray_siglev *sl;
	int i;
	u_int8_t antenna;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_TX, "");

	if (sc->sc_version == RAY_ECFS_BUILD_4) 
		return (0);

	/* try to find host */
	for (i = 0; i < RAY_NSIGLEVRECS; i++) {
		sl = &sc->sc_siglevs[i];
		if (bcmp(sl->rsl_host, dst, ETHER_ADDR_LEN) == 0)
			goto found;
	}
	/* not found, return default setting */
	return (0);

found:
    	/* This is a simple thresholding scheme that takes the mean
	 * of the best antenna history. This is okay but as it is a
	 * filter, it adds a bit of lag in situations where the
	 * best antenna swaps from one side to the other slowly. Don't know
	 * how likely this is given the horrible fading though.
	 */
	antenna = 0;
	for (i = 0; i < RAY_NANTENNA; i++) {
	    	antenna += sl->rsl_antennas[i];
	}

	return (antenna > (RAY_NANTENNA >> 1));
}

/*
 * Transmit now complete so clear ccs and network flags.
 */
static void
ray_tx_done(struct ray_softc *sc, u_int8_t status, size_t ccs)
{
	struct ifnet *ifp = sc->ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_TX, "");

	RAY_CCSERR(sc, status, if_oerrors);

	RAY_CCS_FREE(sc, ccs);
	ifp->if_timer = 0;
	if (ifp->if_flags & IFF_OACTIVE)
	    ifp->if_flags &= ~IFF_OACTIVE;
}

/*
 * Receiver packet handling
 */

/*
 * Receive a packet from the card
 */
static void
ray_rx(struct ray_softc *sc, size_t rcs)
{
	struct ieee80211_frame *header;
	struct ifnet *ifp = sc->ifp;
	struct mbuf *m0;
	size_t pktlen, fraglen, readlen, tmplen;
	size_t bufp, ebufp;
	u_int8_t siglev, antenna;
	u_int first, ni, i;
	u_int8_t *mp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	RAY_DPRINTF(sc, RAY_DBG_CCS, "using rcs 0x%x", rcs);

	m0 = NULL;
	readlen = 0;

	/*
	 * Get first part of packet and the length. Do some sanity checks
	 * and get a mbuf.
	 */
	first = RAY_CCS_INDEX(rcs);
	pktlen = SRAM_READ_FIELD_2(sc, rcs, ray_cmd_rx, c_pktlen);
	siglev = SRAM_READ_FIELD_1(sc, rcs, ray_cmd_rx, c_siglev);
	antenna = SRAM_READ_FIELD_1(sc, rcs, ray_cmd_rx, c_antenna);

	if ((pktlen > MCLBYTES) || (pktlen < sizeof(struct ieee80211_frame))) {
		RAY_RECERR(sc, "packet too big or too small");
		ifp->if_ierrors++;
		goto skip_read;
	}

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL) {
		RAY_RECERR(sc, "MGETHDR failed");
		ifp->if_ierrors++;
		goto skip_read;
	}
	if (pktlen > MHLEN) {
		MCLGET(m0, M_DONTWAIT);
		if (!(m0->m_flags & M_EXT)) {
			RAY_RECERR(sc, "MCLGET failed");
			ifp->if_ierrors++;
			m_freem(m0);
			m0 = NULL;
			goto skip_read;
		}
	}
	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = pktlen;
	m0->m_len = pktlen;
	mp = mtod(m0, u_int8_t *);

	/*
	 * Walk the fragment chain to build the complete packet.
	 *
	 * The use of two index variables removes a race with the
	 * hardware. If one index were used the clearing of the CCS would
	 * happen before reading the next pointer and the hardware can get in.
	 * Not my idea but verbatim from the NetBSD driver.
	 */
	i = ni = first;
	while ((i = ni) && (i != RAY_CCS_LINK_NULL)) {
		rcs = RAY_CCS_ADDRESS(i);
		ni = SRAM_READ_FIELD_1(sc, rcs, ray_cmd_rx, c_nextfrag);
		bufp = SRAM_READ_FIELD_2(sc, rcs, ray_cmd_rx, c_bufp);
		fraglen = SRAM_READ_FIELD_2(sc, rcs, ray_cmd_rx, c_len);
		if (fraglen + readlen > pktlen) {
			RAY_RECERR(sc, "bad length current 0x%zx pktlen 0x%zx",
			    fraglen + readlen, pktlen);
			ifp->if_ierrors++;
			m_freem(m0);
			m0 = NULL;
			goto skip_read;
		}
		if ((i < RAY_RCS_FIRST) || (i > RAY_RCS_LAST)) {
			RAY_RECERR(sc, "bad rcs index 0x%x", i);
			ifp->if_ierrors++;
			m_freem(m0);
			m0 = NULL;
			goto skip_read;
		}

		ebufp = bufp + fraglen;
		if (ebufp <= RAY_RX_END)
			SRAM_READ_REGION(sc, bufp, mp, fraglen);
		else {
			SRAM_READ_REGION(sc, bufp, mp,
			    (tmplen = RAY_RX_END - bufp));
			SRAM_READ_REGION(sc, RAY_RX_BASE, mp + tmplen,
			    ebufp - RAY_RX_END);
		}
		mp += fraglen;
		readlen += fraglen;
	}

skip_read:

	/*
	 * Walk the chain again to free the rcss.
	 */
	i = ni = first;
	while ((i = ni) && (i != RAY_CCS_LINK_NULL)) {
		rcs = RAY_CCS_ADDRESS(i);
		ni = SRAM_READ_FIELD_1(sc, rcs, ray_cmd_rx, c_nextfrag);
		RAY_CCS_FREE(sc, rcs);
	}

	if (m0 == NULL)
		return;

	/*
	 * Check the 802.11 packet type and hand off to
	 * appropriate functions.
	 */
	header = mtod(m0, struct ieee80211_frame *);
	if ((header->i_fc[0] & IEEE80211_FC0_VERSION_MASK)
	    != IEEE80211_FC0_VERSION_0) {
		RAY_RECERR(sc, "header not version 0 fc0 0x%x",
		    header->i_fc[0]);
		ifp->if_ierrors++;
		m_freem(m0);
		return;
	}
	switch (header->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {

	case IEEE80211_FC0_TYPE_DATA:
		ray_rx_data(sc, m0, siglev, antenna);
		break;

	case IEEE80211_FC0_TYPE_MGT:
		ray_rx_mgt(sc, m0);
		break;

	case IEEE80211_FC0_TYPE_CTL:
		ray_rx_ctl(sc, m0);
		break;

	default:
		RAY_RECERR(sc, "unknown packet fc0 0x%x", header->i_fc[0]);
		ifp->if_ierrors++;
		m_freem(m0);
	}
}

/*
 * Deal with DATA packet types
 */
static void
ray_rx_data(struct ray_softc *sc, struct mbuf *m0, u_int8_t siglev, u_int8_t antenna)
{
	struct ifnet *ifp = sc->ifp;
	struct ieee80211_frame *header = mtod(m0, struct ieee80211_frame *);
	struct llc *llc;
	u_int8_t *sa = NULL, *da = NULL, *ra = NULL, *ta = NULL;
	int trim = 0;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_RX, "");

	/*
	 * Check the the data packet subtype, some packets have
	 * nothing in them so we will drop them here.
	 */
	switch (header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {

	case IEEE80211_FC0_SUBTYPE_DATA:
	case IEEE80211_FC0_SUBTYPE_CF_ACK:
	case IEEE80211_FC0_SUBTYPE_CF_POLL:
	case IEEE80211_FC0_SUBTYPE_CF_ACPL:
		RAY_DPRINTF(sc, RAY_DBG_RX, "DATA packet");
		break;

	case IEEE80211_FC0_SUBTYPE_NODATA:
	case IEEE80211_FC0_SUBTYPE_CFACK:
	case IEEE80211_FC0_SUBTYPE_CFPOLL:
	case IEEE80211_FC0_SUBTYPE_CF_ACK_CF_ACK:
		RAY_DPRINTF(sc, RAY_DBG_RX, "NULL packet");
	    	m_freem(m0);
		return;
		break;

	default:
		RAY_RECERR(sc, "reserved DATA packet subtype 0x%x",
		    header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		ifp->if_ierrors++;
		m_freem(m0);
		return;
	}

	/*
	 * Parse the To DS and From DS fields to determine the length
	 * of the 802.11 header for use later on.
	 *
	 * Additionally, furtle out the right destination and
	 * source MAC addresses for the packet. Packets may come via
	 * APs so the MAC addresses of the immediate node may be
	 * different from the node that actually sent us the packet.
	 *
	 *	da	destination address of final recipient
	 *	sa	source address of orginator
	 *	ra	receiver address of immediate recipient
	 *	ta	transmitter address of immediate orginator
	 *
	 * Address matching is performed on da or sa with the AP or
	 * BSSID in ra and ta.
	 */
	RAY_MBUF_DUMP(sc, RAY_DBG_RX, m0, "(1) packet before framing");
	switch (header->i_fc[1] & IEEE80211_FC1_DIR_MASK) {

	case IEEE80211_FC1_DIR_NODS:
		da = ra = header->i_addr1;
		sa = ta = header->i_addr2;
		trim = sizeof(struct ieee80211_frame);
		RAY_DPRINTF(sc, RAY_DBG_RX, "from %6D to %6D",
		    sa, ":", da, ":");
		break;

	case IEEE80211_FC1_DIR_FROMDS:
		da = ra = header->i_addr1;
		ta = header->i_addr2;
		sa = header->i_addr3;
		trim = sizeof(struct ieee80211_frame);
		RAY_DPRINTF(sc, RAY_DBG_RX, "ap %6D from %6D to %6D",
		    ta, ":", sa, ":", da, ":");
		break;

	case IEEE80211_FC1_DIR_TODS:
	    	ra = header->i_addr1;
		sa = ta = header->i_addr2;
		da = header->i_addr3;
		trim = sizeof(struct ieee80211_frame);
		RAY_DPRINTF(sc, RAY_DBG_RX, "from %6D to %6D ap %6D",
		    sa, ":", da, ":", ra, ":");
		break;

	case IEEE80211_FC1_DIR_DSTODS:
		ra = header->i_addr1;
		ta = header->i_addr2;
		da = header->i_addr3;
		sa = (u_int8_t *)header+1;
		trim = sizeof(struct ieee80211_frame) + ETHER_ADDR_LEN;
		RAY_DPRINTF(sc, RAY_DBG_RX, "from %6D to %6D ap %6D to %6D",
		    sa, ":", da, ":", ta, ":", ra, ":");
		break;
	}

	/*
	 * Framing
	 *
	 * Each case must leave an Ethernet header and adjust trim.
	 */
	switch (sc->sc_c.np_framing) {

    	case RAY_FRAMING_ENCAPSULATION:
		/* A NOP as the Ethernet header is in the packet */
		break;

	case RAY_FRAMING_TRANSLATION:
	    	/* Check that we have an LLC and SNAP sequence */
		llc = (struct llc *)((u_int8_t *)header + trim);
		if (llc->llc_dsap == LLC_SNAP_LSAP &&
		    llc->llc_ssap == LLC_SNAP_LSAP &&
		    llc->llc_control == LLC_UI &&
		    llc->llc_un.type_snap.org_code[0] == 0 &&
		    llc->llc_un.type_snap.org_code[1] == 0 &&
		    llc->llc_un.type_snap.org_code[2] == 0) {
			struct ether_header *eh;
			/*
			 * This is not magic. RFC1042 header is 8
			 * bytes, with the last two bytes being the
			 * ether type. So all we need is another
			 * ETHER_ADDR_LEN bytes to write the
			 * destination into.
			 */
			trim -= ETHER_ADDR_LEN;
			eh = (struct ether_header *)((u_int8_t *)header + trim);

			/*
			 * Copy carefully to avoid mashing the MAC
			 * addresses. The address layout in the .11 header
			 * does make sense, honest, but it is a pain.
			 * 
			 * NODS  	da sa		no risk              
			 * FROMDS	da ta sa	sa then da           
			 * DSTODS	ra ta da sa	sa then da           
			 * TODS		ra sa da	da then sa           
			 */
			if (sa > da) {
				/* Copy sa first */
				bcopy(sa, eh->ether_shost, ETHER_ADDR_LEN);
				bcopy(da, eh->ether_dhost, ETHER_ADDR_LEN);
			} else {
				/* Copy da first */
				bcopy(da, eh->ether_dhost, ETHER_ADDR_LEN);
				bcopy(sa, eh->ether_shost, ETHER_ADDR_LEN);
			}

		} else {

			/* Assume RAY_FRAMING_ENCAPSULATION */
			RAY_RECERR(sc,
			    "got encapsulated packet but in translation mode");

		}
		break;

	default:
		RAY_RECERR(sc, "unknown framing type %d", sc->sc_c.np_framing);
		ifp->if_ierrors++;
		m_freem(m0);
		return;
	}
	RAY_MBUF_DUMP(sc, RAY_DBG_RX, m0, "(2) packet after framing");

	/*
	 * Finally, do a bit of house keeping before sending the packet
	 * up the stack.
	 */
	m_adj(m0, trim);
	RAY_MBUF_DUMP(sc, RAY_DBG_RX, m0, "(3) packet after trimming");
	ifp->if_ipackets++;
	ray_rx_update_cache(sc, header->i_addr2, siglev, antenna);
	(*ifp->if_input)(ifp, m0);
}

/*
 * Deal with MGT packet types
 */
static void
ray_rx_mgt(struct ray_softc *sc, struct mbuf *m0)
{
	struct ifnet *ifp = sc->ifp;
	struct ieee80211_frame *header = mtod(m0, struct ieee80211_frame *);

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_MGT, "");

	if ((header->i_fc[1] & IEEE80211_FC1_DIR_MASK) !=
	    IEEE80211_FC1_DIR_NODS) {
		RAY_RECERR(sc, "MGT TODS/FROMDS wrong fc1 0x%x",
		    header->i_fc[1] & IEEE80211_FC1_DIR_MASK);
		ifp->if_ierrors++;
		m_freem(m0);
		return;
	}

	/*
	 * Check the the mgt packet subtype, some packets should be
	 * dropped depending on the mode the station is in. See pg
	 * 52(60) of docs
	 *
	 * P - proccess, J - Junk, E - ECF deals with, I - Illegal
 	 * ECF Proccesses
  	 *  AHDOC procces or junk
   	 *   INFRA STA process or junk
    	 *    INFRA AP process or jumk
	 * 
 	 * +PPP	IEEE80211_FC0_SUBTYPE_BEACON
 	 * +EEE	IEEE80211_FC0_SUBTYPE_PROBE_REQ
 	 * +EEE	IEEE80211_FC0_SUBTYPE_PROBE_RESP
 	 *  PPP	IEEE80211_FC0_SUBTYPE_AUTH
 	 *  PPP	IEEE80211_FC0_SUBTYPE_DEAUTH
 	 *  JJP	IEEE80211_FC0_SUBTYPE_ASSOC_REQ
 	 *  JPJ	IEEE80211_FC0_SUBTYPE_ASSOC_RESP
 	 *  JPP	IEEE80211_FC0_SUBTYPE_DISASSOC
 	 *  JJP	IEEE80211_FC0_SUBTYPE_REASSOC_REQ
 	 *  JPJ	IEEE80211_FC0_SUBTYPE_REASSOC_RESP
 	 * +EEE	IEEE80211_FC0_SUBTYPE_ATIM
	 */
	RAY_MBUF_DUMP(sc, RAY_DBG_MGT, m0, "MGT packet");
	switch (header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {

 	case IEEE80211_FC0_SUBTYPE_BEACON:
		RAY_DPRINTF(sc, RAY_DBG_MGT, "BEACON MGT packet");
		ray_rx_mgt_beacon(sc, m0);
		break;

 	case IEEE80211_FC0_SUBTYPE_AUTH:
		RAY_DPRINTF(sc, RAY_DBG_MGT, "AUTH MGT packet");
		ray_rx_mgt_auth(sc, m0);
		break;

 	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		RAY_DPRINTF(sc, RAY_DBG_MGT, "DEAUTH MGT packet");
		/* XXX ray_rx_mgt_deauth(sc, m0); */
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		RAY_DPRINTF(sc, RAY_DBG_MGT, "(RE)ASSOC_REQ MGT packet");
		if ((sc->sc_c.np_net_type == RAY_MIB_NET_TYPE_INFRA) &&
		    (sc->sc_c.np_ap_status == RAY_MIB_AP_STATUS_AP))
			RAY_RECERR(sc, "can't be an AP yet"); /* XXX_ACTING_AP */
		break;
			
 	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		RAY_DPRINTF(sc, RAY_DBG_MGT, "(RE)ASSOC_RESP MGT packet");
		if ((sc->sc_c.np_net_type == RAY_MIB_NET_TYPE_INFRA) &&
		    (sc->sc_c.np_ap_status == RAY_MIB_AP_STATUS_TERMINAL))
			RAY_RECERR(sc, "can't be in INFRA yet"); /* XXX_INFRA */
		break;

	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		RAY_DPRINTF(sc, RAY_DBG_MGT, "DISASSOC MGT packet");
		if (sc->sc_c.np_net_type == RAY_MIB_NET_TYPE_INFRA)
			RAY_RECERR(sc, "can't be in INFRA yet"); /* XXX_INFRA */
		break;

 	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
 	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_ATIM:
		RAY_RECERR(sc, "unexpected MGT packet subtype 0x%0x",
		    header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		ifp->if_ierrors++;
		break;
	    	
	default:
		RAY_RECERR(sc, "reserved MGT packet subtype 0x%x",
		    header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		ifp->if_ierrors++;
	}

	m_freem(m0);
}

/*
 * Deal with BEACON management packet types
 * XXX furtle anything interesting out
 * XXX Note that there are rules governing what beacons to read
 * XXX see 8802 S7.2.3, S11.1.2.3
 * XXX is this actually useful?
 */
static void
ray_rx_mgt_beacon(struct ray_softc *sc, struct mbuf *m0)
{
	struct ieee80211_frame *header = mtod(m0, struct ieee80211_frame *);
	ieee80211_mgt_beacon_t beacon = (u_int8_t *)(header+1);
	union ieee80211_information elements;

	u_int64_t *timestamp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_MGT, "");

	timestamp = (u_int64_t *)beacon;

RAY_DPRINTF(sc, RAY_DBG_MGT, "timestamp\t0x%x", *timestamp);
RAY_DPRINTF(sc, RAY_DBG_MGT, "interval\t\t0x%x", IEEE80211_BEACON_INTERVAL(beacon));
RAY_DPRINTF(sc, RAY_DBG_MGT, "capability\t0x%x", IEEE80211_BEACON_CAPABILITY(beacon));

	ray_rx_mgt_info(sc, m0, &elements);

}

static void
ray_rx_mgt_info(struct ray_softc *sc, struct mbuf *m0, union ieee80211_information *elements)
{
	struct ifnet *ifp = sc->ifp;
	struct ieee80211_frame *header = mtod(m0, struct ieee80211_frame *);
	ieee80211_mgt_beacon_t beacon = (u_int8_t *)(header+1);
	ieee80211_mgt_beacon_t bp, be;
	int len;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_MGT, "");

	bp = beacon + 12;
	be = mtod(m0, u_int8_t *) + m0->m_len;
    	
    	while (bp < be) {
		len = *(bp + 1);
		RAY_DPRINTF(sc, RAY_DBG_MGT, "id 0x%02x length %d", *bp, len);

		switch (*bp) {

		case IEEE80211_ELEMID_SSID:
			if (len  > IEEE80211_NWID_LEN) {
				RAY_RECERR(sc, "bad SSD length: %d from %6D",
				    len, header->i_addr2, ":");
			}
		    	strncpy(elements->ssid, bp + 2, len);
		    	elements->ssid[len] = 0;
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "beacon ssid %s", elements->ssid);
			break; 

		case IEEE80211_ELEMID_RATES:
			RAY_DPRINTF(sc, RAY_DBG_MGT, "rates");
			break;

		case IEEE80211_ELEMID_FHPARMS:
			elements->fh.dwell = bp[2] + (bp[3] << 8);
			elements->fh.set = bp[4];
			elements->fh.pattern = bp[5];
			elements->fh.index = bp[6];
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "fhparams dwell\t0x%04x", elements->fh.dwell);
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "fhparams set\t0x%02x", elements->fh.set);
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "fhparams pattern\t0x%02x", elements->fh.pattern);
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "fhparams index\t0x%02x", elements->fh.index);
		    	break;

		case IEEE80211_ELEMID_DSPARMS:
		    	RAY_RECERR(sc, "got direct sequence params!");
			break;

		case IEEE80211_ELEMID_CFPARMS:
		    	RAY_DPRINTF(sc, RAY_DBG_MGT, "cfparams");
			break;

		case IEEE80211_ELEMID_TIM:
			elements->tim.count = bp[2];
			elements->tim.period = bp[3];
			elements->tim.bitctl = bp[4];
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "tim count\t0x%02x", elements->tim.count);
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "tim period\t0x%02x", elements->tim.period);
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "tim bitctl\t0x%02x", elements->tim.bitctl);
#if RAY_DEBUG & RAY_DBG_MGT
			{
				int i;
				for (i = 5; i < len + 1; i++)
					RAY_DPRINTF(sc, RAY_DBG_MGT,
					    "tim pvt[%03d]\t0x%02x", i-5, bp[i]);
			}
#endif /* (RAY_DEBUG & RAY_DBG_MGT) */
			break;
		    	
		case IEEE80211_ELEMID_IBSSPARMS:
			elements->ibss.atim = bp[2] + (bp[3] << 8);
			RAY_DPRINTF(sc, RAY_DBG_MGT,
			    "ibssparams atim\t0x%02x", elements->ibss.atim);
			break;

		case IEEE80211_ELEMID_CHALLENGE:
			RAY_DPRINTF(sc, RAY_DBG_MGT, "challenge");
			break;

		default:
			RAY_RECERR(sc, "reserved MGT element id 0x%x", *bp);
			ifp->if_ierrors++;break;
		}
		bp += bp[1] + 2; 
	}
}

/*
 * Deal with AUTH management packet types
 */
static void
ray_rx_mgt_auth(struct ray_softc *sc, struct mbuf *m0)
{
	struct ieee80211_frame *header = mtod(m0, struct ieee80211_frame *);
	ieee80211_mgt_auth_t auth = (u_int8_t *)(header+1);

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_AUTH, "");

	switch (IEEE80211_AUTH_ALGORITHM(auth)) {
	    
	case IEEE80211_AUTH_ALG_OPEN:
		RAY_DPRINTF(sc, RAY_DBG_AUTH,
		    "open system authentication sequence number %d",
		    IEEE80211_AUTH_TRANSACTION(auth));
		if (IEEE80211_AUTH_TRANSACTION(auth) ==
		    IEEE80211_AUTH_OPEN_REQUEST) {

/* XXX_AUTH use ray_init_auth_send */

		} else if (IEEE80211_AUTH_TRANSACTION(auth) == 
		    IEEE80211_AUTH_OPEN_RESPONSE)
			ray_init_auth_done(sc, IEEE80211_AUTH_STATUS(auth));
		break;

	case IEEE80211_AUTH_ALG_SHARED:
		RAY_RECERR(sc,
		    "shared key authentication sequence number %d",
		    IEEE80211_AUTH_TRANSACTION(auth));
		break;
	
	default:
		RAY_RECERR(sc,
		    "reserved authentication subtype 0x%04hx",
		    IEEE80211_AUTH_ALGORITHM(auth));
		break;
	}
}

/*
 * Deal with CTL packet types
 */
static void
ray_rx_ctl(struct ray_softc *sc, struct mbuf *m0)
{
	struct ifnet *ifp = sc->ifp;
	struct ieee80211_frame *header = mtod(m0, struct ieee80211_frame *);

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_CTL, "");

	if ((header->i_fc[1] & IEEE80211_FC1_DIR_MASK) !=
	    IEEE80211_FC1_DIR_NODS) {
		RAY_RECERR(sc, "CTL TODS/FROMDS wrong fc1 0x%x",
		    header->i_fc[1] & IEEE80211_FC1_DIR_MASK);
		ifp->if_ierrors++;
		m_freem(m0);
		return;
	}

	/*
	 * Check the the ctl packet subtype, some packets should be
	 * dropped depending on the mode the station is in. The ECF
	 * should deal with everything but the power save poll to an
	 * AP. See pg 52(60) of docs.
	 */
	RAY_MBUF_DUMP(sc, RAY_DBG_CTL, m0, "CTL packet");
	switch (header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {

	case IEEE80211_FC0_SUBTYPE_PS_POLL:
		RAY_DPRINTF(sc, RAY_DBG_CTL, "PS_POLL CTL packet");
		if ((sc->sc_d.np_net_type == RAY_MIB_NET_TYPE_INFRA) &&
		    (sc->sc_c.np_ap_status == RAY_MIB_AP_STATUS_AP))
			RAY_RECERR(sc, "can't be an AP yet"); /* XXX_ACTING_AP */
		break;

	case IEEE80211_FC0_SUBTYPE_RTS:
	case IEEE80211_FC0_SUBTYPE_CTS:
	case IEEE80211_FC0_SUBTYPE_ACK:
	case IEEE80211_FC0_SUBTYPE_CF_END:
	case IEEE80211_FC0_SUBTYPE_CF_END_ACK:
		RAY_RECERR(sc, "unexpected CTL packet subtype 0x%0x",
		    header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		ifp->if_ierrors++;
		break;

	default:
		RAY_RECERR(sc, "reserved CTL packet subtype 0x%x",
		    header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		ifp->if_ierrors++;
	}

	m_freem(m0);
}

/*
 * Update rx level and antenna cache
 */
static void
ray_rx_update_cache(struct ray_softc *sc, u_int8_t *src, u_int8_t siglev, u_int8_t antenna)
{
	struct timeval mint;
	struct ray_siglev *sl;
	int i, mini;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	/* Try to find host */
	for (i = 0; i < RAY_NSIGLEVRECS; i++) {
		sl = &sc->sc_siglevs[i];
		if (bcmp(sl->rsl_host, src, ETHER_ADDR_LEN) == 0)
			goto found;
	}
	/* Not found, find oldest slot */
	mini = 0;
	mint.tv_sec = LONG_MAX;
	mint.tv_usec = 0;
	for (i = 0; i < RAY_NSIGLEVRECS; i++) {
		sl = &sc->sc_siglevs[i];
		if (timevalcmp(&sl->rsl_time, &mint, <)) {
			mini = i;
			mint = sl->rsl_time;
		}
	}
	sl = &sc->sc_siglevs[mini];
	bzero(sl->rsl_siglevs, RAY_NSIGLEV);
	bzero(sl->rsl_antennas, RAY_NANTENNA);
	bcopy(src, sl->rsl_host, ETHER_ADDR_LEN);

found:
	microtime(&sl->rsl_time);
	bcopy(sl->rsl_siglevs, &sl->rsl_siglevs[1], RAY_NSIGLEV-1);
	sl->rsl_siglevs[0] = siglev;
	if (sc->sc_version != RAY_ECFS_BUILD_4) {
		bcopy(sl->rsl_antennas, &sl->rsl_antennas[1], RAY_NANTENNA-1);
		sl->rsl_antennas[0] = antenna;
	}
}

/*
 * Interrupt handling
 */

/*
 * Process an interrupt
 */
static void
ray_intr(void *xsc)
{
	struct ray_softc *sc = (struct ray_softc *)xsc;
	struct ifnet *ifp = sc->ifp;
	size_t ccs;
	u_int8_t cmd, status;
	int ccsi;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	if ((sc == NULL) || (sc->sc_gone))
		return;

	/*
	 * Check that the interrupt was for us, if so get the rcs/ccs
	 * and vector on the command contained within it.
	 */
	if (RAY_HCS_INTR(sc)) {
		ccsi = SRAM_READ_1(sc, RAY_SCB_RCSI);
		ccs = RAY_CCS_ADDRESS(ccsi);
		cmd = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd);
		status = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
		if (ccsi <= RAY_CCS_LAST)
			ray_intr_ccs(sc, cmd, status, ccs);
		else if (ccsi <= RAY_RCS_LAST)
			ray_intr_rcs(sc, cmd, ccs);
		else
		    RAY_RECERR(sc, "bad ccs index 0x%x", ccsi);
		RAY_HCS_CLEAR_INTR(sc);
	}

	/* Send any packets lying around and update error counters */
	if (!(ifp->if_flags & IFF_OACTIVE) && (ifp->if_snd.ifq_head != NULL))
		ray_tx(ifp);
	if ((++sc->sc_checkcounters % 32) == 0)
		ray_intr_updt_errcntrs(sc);
}

/*
 * Read the error counters.
 */
static void
ray_intr_updt_errcntrs(struct ray_softc *sc)
{
	size_t csc;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	/*
	 * The card implements the following protocol to keep the
	 * values from being changed while read: It checks the `own'
	 * bit and if zero writes the current internal counter value,
	 * it then sets the `own' bit to 1. If the `own' bit was 1 it
	 * incremenets its internal counter. The user thus reads the
	 * counter if the `own' bit is one and then sets the own bit
	 * to 0.
	 */
	csc = RAY_STATUS_BASE;
	if (SRAM_READ_FIELD_1(sc, csc, ray_csc, csc_mrxo_own)) {
		sc->sc_rxoverflow +=
		    SRAM_READ_FIELD_2(sc, csc, ray_csc, csc_mrx_overflow);
		SRAM_WRITE_FIELD_1(sc, csc, ray_csc, csc_mrxo_own, 0);
	}
	if (SRAM_READ_FIELD_1(sc, csc, ray_csc, csc_mrxc_own)) {
		sc->sc_rxcksum +=
		    SRAM_READ_FIELD_2(sc, csc, ray_csc, csc_mrx_overflow);
		SRAM_WRITE_FIELD_1(sc, csc, ray_csc, csc_mrxc_own, 0);
	}
	if (SRAM_READ_FIELD_1(sc, csc, ray_csc, csc_rxhc_own)) {
		sc->sc_rxhcksum +=
		    SRAM_READ_FIELD_2(sc, csc, ray_csc, csc_rx_hcksum);
		SRAM_WRITE_FIELD_1(sc, csc, ray_csc, csc_rxhc_own, 0);
	}
	sc->sc_rxnoise = SRAM_READ_FIELD_1(sc, csc, ray_csc, csc_rx_noise);
}

/*
 * Process CCS command completion
 */
static void
ray_intr_ccs(struct ray_softc *sc, u_int8_t cmd, u_int8_t status, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	switch (cmd) {

	case RAY_CMD_DOWNLOAD_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START_PARAMS");
		ray_init_download_done(sc, status, ccs);
		break;

	case RAY_CMD_UPDATE_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "UPDATE_PARAMS");
		ray_upparams_done(sc, status, ccs);
		break;

	case RAY_CMD_REPORT_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "REPORT_PARAMS");
		ray_repparams_done(sc, status, ccs);
		break;

	case RAY_CMD_UPDATE_MCAST:
		RAY_DPRINTF(sc, RAY_DBG_COM, "UPDATE_MCAST");
		ray_mcast_done(sc, status, ccs);
		break;

	case RAY_CMD_START_NET:
	case RAY_CMD_JOIN_NET:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START|JOIN_NET");
		ray_init_sj_done(sc, status, ccs);
		break;

	case RAY_CMD_TX_REQ:
		RAY_DPRINTF(sc, RAY_DBG_COM, "TX_REQ");
		ray_tx_done(sc, status, ccs);
		break;

	case RAY_CMD_START_ASSOC:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START_ASSOC");
		ray_init_assoc_done(sc, status, ccs);
		break;

	case RAY_CMD_UPDATE_APM:
		RAY_RECERR(sc, "unexpected UPDATE_APM");
		break;

	case RAY_CMD_TEST_MEM:
		RAY_RECERR(sc, "unexpected TEST_MEM");
		break;

	case RAY_CMD_SHUTDOWN:
		RAY_RECERR(sc, "unexpected SHUTDOWN");
		break;

	case RAY_CMD_DUMP_MEM:
		RAY_RECERR(sc, "unexpected DUMP_MEM");
		break;

	case RAY_CMD_START_TIMER:
		RAY_RECERR(sc, "unexpected START_TIMER");
		break;

	default:
		RAY_RECERR(sc, "unknown command 0x%x", cmd);
		break;
	}
}

/*
 * Process ECF command request
 */
static void
ray_intr_rcs(struct ray_softc *sc, u_int8_t cmd, size_t rcs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	switch (cmd) {

	case RAY_ECMD_RX_DONE:
		RAY_DPRINTF(sc, RAY_DBG_RX, "RX_DONE");
		ray_rx(sc, rcs);
		break;

	case RAY_ECMD_REJOIN_DONE:
		RAY_DPRINTF(sc, RAY_DBG_RX, "REJOIN_DONE");
		sc->sc_c.np_havenet = 1;
		break;

	case RAY_ECMD_ROAM_START:
		RAY_DPRINTF(sc, RAY_DBG_RX, "ROAM_START");
		sc->sc_c.np_havenet = 0;
		break;

	case RAY_ECMD_JAPAN_CALL_SIGNAL:
		RAY_RECERR(sc, "unexpected JAPAN_CALL_SIGNAL");
		break;

	default:
		RAY_RECERR(sc, "unknown command 0x%x", cmd);
		break;
	}

	RAY_CCS_FREE(sc, rcs);
}

/*
 * User land entry to multicast list changes
 */
static int
ray_mcast_user(struct ray_softc *sc)
{
	struct ray_comq_entry *com[2];
	int error, ncom;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	/*
	 * Do all checking in the runq to preserve ordering.
	 *
	 * We run promisc to pick up changes to the ALL_MULTI
	 * interface flag.
	 */
	ncom = 0;
	com[ncom++] = RAY_COM_MALLOC(ray_mcast, 0);
	com[ncom++] = RAY_COM_MALLOC(ray_promisc, 0);

	RAY_COM_RUNQ(sc, com, ncom, "raymcast", error);

	/* XXX no real error processing from anything yet! */

	RAY_COM_FREE(com, ncom);

	return (error);
}

/*
 * Runq entry to setting the multicast filter list
 *
 * MUST always be followed by a call to ray_promisc to pick up changes
 * to promisc flag
 */
static void
ray_mcast(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp = sc->ifp;
	struct ifmultiaddr *ifma;
	size_t bufp;
	int count = 0;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	/*
	 * If card is not running we don't need to update this.
	 */
	if (!(ifp->if_flags & IFF_RUNNING)) {
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "not running");
		ray_com_runq_done(sc);
		return;
	}

	/*
	 * The multicast list is only 16 items long so use promiscuous
	 * mode and don't bother updating the multicast list.
	 */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		count++;
	if (count == 0) {
		IF_ADDR_UNLOCK(ifp);
		ray_com_runq_done(sc);
		return;
	} else if (count > 16) {
		ifp->if_flags |= IFF_ALLMULTI;
		IF_ADDR_UNLOCK(ifp);
		ray_com_runq_done(sc);
		return;
	} else if (ifp->if_flags & IFF_ALLMULTI)
		ifp->if_flags &= ~IFF_ALLMULTI;

	/*
	 * Kick the card
	 */
	ray_ccs_fill(sc, com->c_ccs, RAY_CMD_UPDATE_MCAST);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs,
	    ray_cmd_update_mcast, c_nmcast, count);
	bufp = RAY_HOST_TO_ECF_BASE;
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		SRAM_WRITE_REGION(
		    sc,
		    bufp,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    ETHER_ADDR_LEN
		);
		bufp += ETHER_ADDR_LEN;
	}
	IF_ADDR_UNLOCK(ifp);

	ray_com_ecf(sc, com);
}

/*
 * Complete the multicast filter list update
 */
static void
ray_mcast_done(struct ray_softc *sc, u_int8_t status, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_COM_CHECK(sc, ccs);

	RAY_CCSERR(sc, status, if_oerrors); /* XXX error counter */

	ray_com_ecf_done(sc);
}

/*
 * Runq entry to set/reset promiscuous mode
 */
static void
ray_promisc(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp = sc->ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	/*
	 * If card not running or we already have the right flags
	 * we don't need to update this
	 */
	sc->sc_d.np_promisc = !!(ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI));
	if (!(ifp->if_flags & IFF_RUNNING) ||
	    (sc->sc_c.np_promisc == sc->sc_d.np_promisc)) {
		ray_com_runq_done(sc);
		return;
	}

	/*
	 * Kick the card
	 */
	ray_ccs_fill(sc, com->c_ccs, RAY_CMD_UPDATE_PARAMS);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs,
	    ray_cmd_update, c_paramid, RAY_MIB_PROMISC);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_update, c_nparam, 1);
	SRAM_WRITE_1(sc, RAY_HOST_TO_ECF_BASE, sc->sc_d.np_promisc);

	ray_com_ecf(sc, com);
}

/*
 * User land entry to parameter reporting
 *
 * As we by pass the runq to report current parameters this function
 * only provides a snap shot of the driver's state.
 */
static int
ray_repparams_user(struct ray_softc *sc, struct ray_param_req *pr)
{
	struct ray_comq_entry *com[1];
	int error, ncom;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	/*
	 * Test for illegal values or immediate responses
	 */
	if (pr->r_paramid > RAY_MIB_MAX)
		return (EINVAL);
	if ((sc->sc_version == RAY_ECFS_BUILD_4) &&
	    !(mib_info[pr->r_paramid][0] & RAY_V4))
		return (EINVAL);
	if ((sc->sc_version == RAY_ECFS_BUILD_5) &&
	    !(mib_info[pr->r_paramid][0] & RAY_V5))
		return (EINVAL);
	if (pr->r_paramid > RAY_MIB_LASTUSER) {
	    	switch (pr->r_paramid) {

		case  RAY_MIB_VERSION:
			if (sc->sc_version == RAY_ECFS_BUILD_4)
			    *pr->r_data = RAY_V4;
			else
			    *pr->r_data = RAY_V5;
			break;
		case  RAY_MIB_CUR_BSSID:
		    	bcopy(sc->sc_c.np_bss_id, pr->r_data, ETHER_ADDR_LEN);
			break;
		case  RAY_MIB_CUR_INITED:
		    	*pr->r_data = sc->sc_c.np_inited;
			break;
		case  RAY_MIB_CUR_DEF_TXRATE:
		    	*pr->r_data = sc->sc_c.np_def_txrate;
			break;
		case  RAY_MIB_CUR_ENCRYPT:
		    	*pr->r_data = sc->sc_c.np_encrypt;
			break;
		case  RAY_MIB_CUR_NET_TYPE:
		    	*pr->r_data = sc->sc_c.np_net_type;
			break;
		case  RAY_MIB_CUR_SSID:
		    	bcopy(sc->sc_c.np_ssid, pr->r_data, IEEE80211_NWID_LEN);
			break;
		case  RAY_MIB_CUR_PRIV_START:
		    	*pr->r_data = sc->sc_c.np_priv_start;
			break;
		case  RAY_MIB_CUR_PRIV_JOIN:
		    	*pr->r_data = sc->sc_c.np_priv_join;
			break;
		case  RAY_MIB_DES_BSSID:
		    	bcopy(sc->sc_d.np_bss_id, pr->r_data, ETHER_ADDR_LEN);
			break;
		case  RAY_MIB_DES_INITED:
		    	*pr->r_data = sc->sc_d.np_inited;
			break;
		case  RAY_MIB_DES_DEF_TXRATE:
		    	*pr->r_data = sc->sc_d.np_def_txrate;
			break;
		case  RAY_MIB_DES_ENCRYPT:
		    	*pr->r_data = sc->sc_d.np_encrypt;
			break;
		case  RAY_MIB_DES_NET_TYPE:
		    	*pr->r_data = sc->sc_d.np_net_type;
			break;
		case  RAY_MIB_DES_SSID:
		    	bcopy(sc->sc_d.np_ssid, pr->r_data, IEEE80211_NWID_LEN);
			break;
		case  RAY_MIB_DES_PRIV_START:
		    	*pr->r_data = sc->sc_d.np_priv_start;
			break;
		case  RAY_MIB_DES_PRIV_JOIN:
		    	*pr->r_data = sc->sc_d.np_priv_join;
			break;
		case  RAY_MIB_CUR_AP_STATUS:
		    	*pr->r_data = sc->sc_c.np_ap_status;
			break;
		case  RAY_MIB_CUR_PROMISC:
		    	*pr->r_data = sc->sc_c.np_promisc;
			break;
		case  RAY_MIB_DES_AP_STATUS:
		    	*pr->r_data = sc->sc_d.np_ap_status;
			break;
		case  RAY_MIB_DES_PROMISC:
		    	*pr->r_data = sc->sc_d.np_promisc;
			break;
		case RAY_MIB_CUR_FRAMING:
		    	*pr->r_data = sc->sc_c.np_framing;
			break;
		case RAY_MIB_DES_FRAMING:
		    	*pr->r_data = sc->sc_d.np_framing;
			break;

		default:
		    	return (EINVAL);
			break;
		}
		pr->r_failcause = 0;
		if (sc->sc_version == RAY_ECFS_BUILD_4)
		    pr->r_len = mib_info[pr->r_paramid][RAY_MIB_INFO_SIZ4];
		else if (sc->sc_version == RAY_ECFS_BUILD_5)
		    pr->r_len = mib_info[pr->r_paramid][RAY_MIB_INFO_SIZ5];
		return (0);
	}

	pr->r_failcause = 0;
	ncom = 0;
	com[ncom++] = RAY_COM_MALLOC(ray_repparams, RAY_COM_FWOK);
	com[ncom-1]->c_pr = pr;

	RAY_COM_RUNQ(sc, com, ncom, "rayrparm", error);

	/* XXX no real error processing from anything yet! */
	if (!com[0]->c_retval && pr->r_failcause)
		error = EINVAL;

	RAY_COM_FREE(com, ncom);

	return (error);
}

/*
 * Runq entry to read the required parameter
 *
 * The card and driver are happy for parameters to be read
 * whenever the card is plugged in
 */
static void
ray_repparams(struct ray_softc *sc, struct ray_comq_entry *com)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	/*
	 * Kick the card
	 */
	ray_ccs_fill(sc, com->c_ccs, RAY_CMD_REPORT_PARAMS);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs,
	    ray_cmd_report, c_paramid, com->c_pr->r_paramid);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_report, c_nparam, 1);

	ray_com_ecf(sc, com);
}

/*
 * Complete the parameter reporting
 */
static void
ray_repparams_done(struct ray_softc *sc, u_int8_t status, size_t ccs)
{
	struct ray_comq_entry *com;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);
	RAY_COM_CHECK(sc, ccs);

	RAY_CCSERR(sc, status, if_oerrors); /* XXX error counter */

	com = TAILQ_FIRST(&sc->sc_comq);
	com->c_pr->r_failcause =
	    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_report, c_failcause);
	com->c_pr->r_len =
	    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_report, c_len);
	SRAM_READ_REGION(sc, RAY_ECF_TO_HOST_BASE,
	    com->c_pr->r_data, com->c_pr->r_len);

	ray_com_ecf_done(sc);
}

/*
 * User land entry (and exit) to the error counters
 */
static int
ray_repstats_user(struct ray_softc *sc, struct ray_stats_req *sr)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	sr->rxoverflow = sc->sc_rxoverflow;
	sr->rxcksum = sc->sc_rxcksum;
	sr->rxhcksum = sc->sc_rxhcksum;
	sr->rxnoise = sc->sc_rxnoise;

	return (0);
}

/*
 * User land entry to parameter update changes
 *
 * As a parameter change can cause the network parameters to be
 * invalid we have to re-start/join.
 */
static int
ray_upparams_user(struct ray_softc *sc, struct ray_param_req *pr)
{
	struct ray_comq_entry *com[4];
	int error, ncom, todo;
#define RAY_UPP_SJ	0x1
#define RAY_UPP_PARAMS	0x2

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	/*
	 * Check that the parameter is available based on firmware version
	 */
	pr->r_failcause = 0;
	if (pr->r_paramid > RAY_MIB_LASTUSER)
		return (EINVAL);
	if ((sc->sc_version == RAY_ECFS_BUILD_4) &&
	    !(mib_info[pr->r_paramid][0] & RAY_V4))
		return (EINVAL);
	if ((sc->sc_version == RAY_ECFS_BUILD_5) &&
	    !(mib_info[pr->r_paramid][0] & RAY_V5))
		return (EINVAL);

	/*
	 * Handle certain parameters specially
	 */
	todo = 0;
	switch (pr->r_paramid) {
	case RAY_MIB_NET_TYPE:		/* Updated via START_NET JOIN_NET  */
		sc->sc_d.np_net_type = *pr->r_data;
		todo |= RAY_UPP_SJ;
		break;

	case RAY_MIB_SSID:		/* Updated via START_NET JOIN_NET  */
		bcopy(pr->r_data, sc->sc_d.np_ssid, IEEE80211_NWID_LEN);
		todo |= RAY_UPP_SJ;
		break;

	case RAY_MIB_PRIVACY_MUST_START:/* Updated via START_NET */
		if (sc->sc_c.np_net_type != RAY_MIB_NET_TYPE_ADHOC)
			return (EINVAL);
		sc->sc_d.np_priv_start = *pr->r_data;
		todo |= RAY_UPP_SJ;
		break;

	case RAY_MIB_PRIVACY_CAN_JOIN:	/* Updated via START_NET JOIN_NET  */
		sc->sc_d.np_priv_join = *pr->r_data;
		todo |= RAY_UPP_SJ;
		break;

	case RAY_MIB_BASIC_RATE_SET:
		sc->sc_d.np_def_txrate = *pr->r_data;
		todo |= RAY_UPP_PARAMS;
		break;

	case RAY_MIB_AP_STATUS:	/* Unsupported */
	case RAY_MIB_MAC_ADDR:	/* XXX Need interface up but could be done */
	case RAY_MIB_PROMISC:	/* BPF */
		return (EINVAL);
		break;

	default:
		todo |= RAY_UPP_PARAMS;
		todo |= RAY_UPP_SJ;
		break;
	}

	/*
	 * Generate the runq entries as needed
	 */
	ncom = 0;
	if (todo & RAY_UPP_PARAMS) {
		com[ncom++] = RAY_COM_MALLOC(ray_upparams, 0);
		com[ncom-1]->c_pr = pr;
	}
	if (todo & RAY_UPP_SJ) {
		com[ncom++] = RAY_COM_MALLOC(ray_init_sj, 0);
		com[ncom++] = RAY_COM_MALLOC(ray_init_auth, 0);
		com[ncom++] = RAY_COM_MALLOC(ray_init_assoc, 0);
	}

	RAY_COM_RUNQ(sc, com, ncom, "rayuparam", error);

	/* XXX no real error processing from anything yet! */
	if (!com[0]->c_retval && pr->r_failcause)
		error = EINVAL;

	RAY_COM_FREE(com, ncom);

	return (error);
}

/*
 * Runq entry to update a parameter
 *
 * The card and driver are basically happy for parameters to be updated
 * whenever the card is plugged in. However, there may be a couple of
 * network hangs whilst the update is performed. Reading parameters back
 * straight away may give the wrong answer and some parameters cannot be
 * read at all. Local copies should be kept.
 */
static void
ray_upparams(struct ray_softc *sc, struct ray_comq_entry *com)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ray_ccs_fill(sc, com->c_ccs, RAY_CMD_UPDATE_PARAMS);

	SRAM_WRITE_FIELD_1(sc, com->c_ccs,
	    ray_cmd_update, c_paramid, com->c_pr->r_paramid);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_update, c_nparam, 1);
	SRAM_WRITE_REGION(sc, RAY_HOST_TO_ECF_BASE,
	    com->c_pr->r_data, com->c_pr->r_len);

	ray_com_ecf(sc, com);
}

/*
 * Complete the parameter update, note that promisc finishes up here too
 */
static void
ray_upparams_done(struct ray_softc *sc, u_int8_t status, size_t ccs)
{
	struct ray_comq_entry *com;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);
	RAY_COM_CHECK(sc, ccs);

	RAY_CCSERR(sc, status, if_oerrors); /* XXX error counter */

	com = TAILQ_FIRST(&sc->sc_comq);

	switch (SRAM_READ_FIELD_1(sc, ccs, ray_cmd_update, c_paramid)) {

	case RAY_MIB_PROMISC:
		sc->sc_c.np_promisc = SRAM_READ_1(sc, RAY_HOST_TO_ECF_BASE);
		RAY_DPRINTF(sc, RAY_DBG_IOCTL,
		    "promisc value %d", sc->sc_c.np_promisc);
		break;

	default:
		com->c_pr->r_failcause =
		    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_update, c_failcause);
		break;

	}

	ray_com_ecf_done(sc);
}

/*
 * Command queuing and execution
 */

/*
 * Set up a comq entry struct
 */
static struct ray_comq_entry *
ray_com_init(struct ray_comq_entry *com, ray_comqfn_t function, int flags, char *mesg)
{
	com->c_function = function;
	com->c_flags = flags;
	com->c_retval = 0;
	com->c_ccs = 0;
	com->c_wakeup = NULL;
	com->c_pr = NULL;
	com->c_mesg = mesg;

	return (com);
}

/*
 * Malloc and set up a comq entry struct
 */
static struct ray_comq_entry *
ray_com_malloc(ray_comqfn_t function, int flags, char *mesg)
{
	struct ray_comq_entry *com;

	MALLOC(com, struct ray_comq_entry *,
	    sizeof(struct ray_comq_entry), M_RAYCOM, M_WAITOK);
    
	return (ray_com_init(com, function, flags, mesg));
}

/*
 * Add an array of commands to the runq, get some ccs's for them and
 * then run, waiting on the last command.
 *
 * We add the commands to the queue first to preserve ioctl ordering.
 *
 * On recoverable errors, this routine removes the entries from the
 * runq. A caller can requeue the commands (and still preserve its own
 * processes ioctl ordering) but doesn't have to. When the card is
 * detached we get out quickly to prevent panics and don't bother
 * about the runq.
 */
static int
ray_com_runq_add(struct ray_softc *sc, struct ray_comq_entry *com[], int ncom, char *wmesg)
{
	int i, error;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

	error = 0;
	/*
	 * Add the commands to the runq but don't let it run until
	 * the ccs's are allocated successfully
	 */
	com[0]->c_flags |= RAY_COM_FWAIT;
	for (i = 0; i < ncom; i++) {
		com[i]->c_wakeup = com[ncom-1];
		RAY_DPRINTF(sc, RAY_DBG_COM, "adding %p", com[i]);
		RAY_DCOM(sc, RAY_DBG_DCOM, com[i], "adding");
		TAILQ_INSERT_TAIL(&sc->sc_comq, com[i], c_chain);
	}
	com[ncom-1]->c_flags |= RAY_COM_FWOK;

	/*
	 * Allocate ccs's for each command.
	 */
	for (i = 0; i < ncom; i++) {
		error = ray_ccs_alloc(sc, &com[i]->c_ccs, wmesg);
		if (error == ENXIO)
			return (ENXIO);
		else if (error)
			goto cleanup;
	}

	/*
	 * Allow the queue to run and sleep if needed.
	 *
	 * Iff the FDETACHED flag is set in the com entry we waited on
	 * the driver is in a zombie state! The softc structure has been
	 * freed by the generic bus detach methods - eek. We tread very
	 * carefully!
	 */
	com[0]->c_flags &= ~RAY_COM_FWAIT;
	ray_com_runq(sc);
	if (TAILQ_FIRST(&sc->sc_comq) != NULL) {
		RAY_DPRINTF(sc, RAY_DBG_COM, "sleeping");
		error = tsleep(com[ncom-1], PCATCH | PRIBIO, wmesg, 0);
		if (com[ncom-1]->c_flags & RAY_COM_FDETACHED)
			return (ENXIO);
		RAY_DPRINTF(sc, RAY_DBG_COM,
		    "awakened, tsleep returned 0x%x", error);
	} else
		error = 0;

cleanup:
	/*
	 * Only clean the queue on real errors - we don't care about it
	 * when we detach as the queue entries are freed by the callers.
	 */
	if (error && (error != ENXIO))
		for (i = 0; i < ncom; i++)
		    	if (!(com[i]->c_flags & RAY_COM_FCOMPLETED)) {
				RAY_DPRINTF(sc, RAY_DBG_COM, "removing %p",
				    com[i]);
				RAY_DCOM(sc, RAY_DBG_DCOM, com[i], "removing");
				TAILQ_REMOVE(&sc->sc_comq, com[i], c_chain);
				ray_ccs_free(sc, com[i]->c_ccs);
				com[i]->c_ccs = 0;
			}

	return (error);
}

/*
 * Run the command at the head of the queue (if not already running)
 */
static void
ray_com_runq(struct ray_softc *sc)
{
	struct ray_comq_entry *com;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

	com = TAILQ_FIRST(&sc->sc_comq);
	if ((com == NULL) ||
	    (com->c_flags & RAY_COM_FRUNNING) ||
	    (com->c_flags & RAY_COM_FWAIT) ||
	    (com->c_flags & RAY_COM_FDETACHED))
		return;

	com->c_flags |= RAY_COM_FRUNNING;
	RAY_DPRINTF(sc, RAY_DBG_COM, "running %p", com);
	RAY_DCOM(sc, RAY_DBG_DCOM, com, "running");
	com->c_function(sc, com);
}

/*
 * Remove run command, free ccs and wakeup caller.
 *
 * Minimal checks are done here as we ensure that the com and command
 * handler were matched up earlier. Must be called at splnet or higher
 * so that entries on the command queue are correctly removed.
 *
 * Remove the com from the comq, and wakeup the caller if it requested
 * to be woken. This is used for ensuring a sequence of commands
 * completes. Finally, re-run the queue.
 */
static void
ray_com_runq_done(struct ray_softc *sc)
{
    	struct ray_comq_entry *com;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

	com = TAILQ_FIRST(&sc->sc_comq); /* XXX shall we check this as below */
	RAY_DPRINTF(sc, RAY_DBG_COM, "removing %p", com);
	RAY_DCOM(sc, RAY_DBG_DCOM, com, "removing");
	TAILQ_REMOVE(&sc->sc_comq, com, c_chain);

	com->c_flags &= ~RAY_COM_FRUNNING;
	com->c_flags |= RAY_COM_FCOMPLETED;
	com->c_retval = 0;
	ray_ccs_free(sc, com->c_ccs);
	com->c_ccs = 0;

	if (com->c_flags & RAY_COM_FWOK)
		wakeup(com->c_wakeup);

	ray_com_runq(sc);

	/* XXX what about error on completion then? deal with when i fix
	 * XXX the status checking
	 *
	 * XXX all the runq_done calls from IFF_RUNNING checks in runq
	 * XXX routines should return EIO but shouldn't abort the runq
	 */
}

/*
 * Send a command to the ECF.
 */
static void
ray_com_ecf(struct ray_softc *sc, struct ray_comq_entry *com)
{
	int i = 0;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");
	RAY_MAP_CM(sc);

	while (!RAY_ECF_READY(sc)) {
		DELAY(RAY_ECF_SPIN_DELAY);
		if (++i > RAY_ECF_SPIN_TRIES)
			RAY_PANIC(sc, "spun too long");
	}
	if (i != 0)
		RAY_RECERR(sc, "spun %d times", i);

	RAY_DPRINTF(sc, RAY_DBG_COM, "sending %p", com);
	RAY_DCOM(sc, RAY_DBG_DCOM, com, "sending");
	SRAM_WRITE_1(sc, RAY_SCB_CCSI, RAY_CCS_INDEX(com->c_ccs));
	RAY_ECF_START_CMD(sc);

	if (RAY_COM_NEEDS_TIMO(
	    SRAM_READ_FIELD_1(sc, com->c_ccs, ray_cmd, c_cmd))) {
	    	RAY_DPRINTF(sc, RAY_DBG_COM, "adding timeout");
		sc->com_timerh = timeout(ray_com_ecf_timo, sc, RAY_COM_TIMEOUT);
	}
}

/*
 * Deal with commands that require a timeout to test completion.
 *
 * This routine is coded to only expect one outstanding request for the
 * timed out requests at a time, but thats all that can be outstanding
 * per hardware limitations and all that we issue anyway.
 *
 * We don't do any fancy testing of the command currently issued as we
 * know it must be a timeout based one...unless I've got this wrong!
 */
static void
ray_com_ecf_timo(void *xsc)
{
	struct ray_softc *sc = (struct ray_softc *)xsc;
    	struct ray_comq_entry *com;
	u_int8_t cmd, status;
	int s;

	s = splnet();

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");
	RAY_MAP_CM(sc);

	com = TAILQ_FIRST(&sc->sc_comq);

	cmd = SRAM_READ_FIELD_1(sc, com->c_ccs, ray_cmd, c_cmd);
	status = SRAM_READ_FIELD_1(sc, com->c_ccs, ray_cmd, c_status);
	switch (status) {

	case RAY_CCS_STATUS_COMPLETE:
	case RAY_CCS_STATUS_FREE:			/* Buggy firmware */
		ray_intr_ccs(sc, cmd, status, com->c_ccs);
		break;

	case RAY_CCS_STATUS_BUSY:
		sc->com_timerh = timeout(ray_com_ecf_timo, sc, RAY_COM_TIMEOUT);
		break;

	default:					/* Replicates NetBSD */
		if (sc->sc_ccsinuse[RAY_CCS_INDEX(com->c_ccs)] == 1) {
			/* give a chance for the interrupt to occur */
			sc->sc_ccsinuse[RAY_CCS_INDEX(com->c_ccs)] = 2;
			sc->com_timerh = timeout(ray_com_ecf_timo, sc,
			    RAY_COM_TIMEOUT);
		} else
			ray_intr_ccs(sc, cmd, status, com->c_ccs);
		break;

	}

	splx(s);
}

/*
 * Called when interrupt handler for the command has done all it
 * needs to. Will be called at splnet.
 */
static void
ray_com_ecf_done(struct ray_softc *sc)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

	untimeout(ray_com_ecf_timo, sc, sc->com_timerh);

	ray_com_runq_done(sc);
}

#if RAY_DEBUG & RAY_DBG_COM
/*
 * Process completed ECF commands that probably came from the command queue
 *
 * This routine is called after vectoring the completed ECF command
 * to the appropriate _done routine. It helps check everything is okay.
 */
static void
ray_com_ecf_check(struct ray_softc *sc, size_t ccs, char *mesg)
{
    	struct ray_comq_entry *com;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "%s", mesg);

	com = TAILQ_FIRST(&sc->sc_comq);

	if (com == NULL)
		RAY_PANIC(sc, "no command queue");
	if (com->c_ccs != ccs)
		RAY_PANIC(sc, "ccs's don't match");
}
#endif /* RAY_DEBUG & RAY_DBG_COM */

/*
 * CCS allocators
 */

/*
 * Obtain a ccs for a commmand
 *
 * Returns 0 and in `ccsp' the bus offset of the free ccs. Will block
 * awaiting free ccs if needed - if the sleep is interrupted
 * EINTR/ERESTART is returned, if the card is ejected we return ENXIO.
 */
static int
ray_ccs_alloc(struct ray_softc *sc, size_t *ccsp, char *wmesg)
{
	size_t ccs;
	u_int i;
	int error;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_CCS, "");
	RAY_MAP_CM(sc);

	for (;;) {
		for (i = RAY_CCS_CMD_FIRST; i <= RAY_CCS_CMD_LAST; i++) {
			/* we probe here to make the card go */
			(void)SRAM_READ_FIELD_1(sc, RAY_CCS_ADDRESS(i), ray_cmd,
			    c_status);
			if (!sc->sc_ccsinuse[i])
				break;
		}
		if (i > RAY_CCS_CMD_LAST) {
			RAY_DPRINTF(sc, RAY_DBG_CCS, "sleeping");
			error = tsleep(ray_ccs_alloc, PCATCH | PRIBIO,
			    wmesg, 0);
			if ((sc == NULL) || (sc->sc_gone))
				return (ENXIO);
			RAY_DPRINTF(sc, RAY_DBG_CCS,
			    "awakened, tsleep returned 0x%x", error);
			if (error)
				return (error);
		} else
			break;
	}
	RAY_DPRINTF(sc, RAY_DBG_CCS, "allocated 0x%02x", i);
	sc->sc_ccsinuse[i] = 1;
	ccs = RAY_CCS_ADDRESS(i);
	*ccsp = ccs;

	return (0);
}

/*
 * Fill the easy bits in of a pre-allocated CCS
 */
static void
ray_ccs_fill(struct ray_softc *sc, size_t ccs, u_int cmd)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_CCS, "");
	RAY_MAP_CM(sc);

	if (ccs == 0)
		RAY_PANIC(sc, "ccs not allocated");

	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_BUSY);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_cmd, cmd);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_link, RAY_CCS_LINK_NULL);
}

/*
 * Free up a ccs allocated via ray_ccs_alloc
 *
 * Return the old status. This routine is only used for ccs allocated via
 * ray_ccs_alloc (not tx, rx or ECF command requests).
 */
static void
ray_ccs_free(struct ray_softc *sc, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_CCS, "");
	RAY_MAP_CM(sc);

#if 1 | (RAY_DEBUG & RAY_DBG_CCS)
	if (!sc->sc_ccsinuse[RAY_CCS_INDEX(ccs)])
		RAY_RECERR(sc, "freeing free ccs 0x%02zx", RAY_CCS_INDEX(ccs));
#endif /* RAY_DEBUG & RAY_DBG_CCS */
	if (!sc->sc_gone)
		RAY_CCS_FREE(sc, ccs);
	sc->sc_ccsinuse[RAY_CCS_INDEX(ccs)] = 0;
	RAY_DPRINTF(sc, RAY_DBG_CCS, "freed 0x%02zx", RAY_CCS_INDEX(ccs));
	wakeup(ray_ccs_alloc);
}

/*
 * Obtain a ccs and tx buffer to transmit with and fill them in.
 *
 * Returns 0 and in `ccsp' the bus offset of the free ccs. Will not block
 * and if none available and will returns EAGAIN.
 *
 * The caller must fill in the length later.
 * The caller must clear the ccs on errors.
 */
static int
ray_ccs_tx(struct ray_softc *sc, size_t *ccsp, size_t *bufpp)
{
	size_t ccs, bufp;
	int i;
	u_int8_t status;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_CCS, "");
	RAY_MAP_CM(sc);

	i = RAY_CCS_TX_FIRST;
	do {
		status = SRAM_READ_FIELD_1(sc, RAY_CCS_ADDRESS(i),
		    ray_cmd, c_status);
		if (status == RAY_CCS_STATUS_FREE)
			break;
		i++;
	} while (i <= RAY_CCS_TX_LAST);
	if (i > RAY_CCS_TX_LAST) {
		return (EAGAIN);
	}
	RAY_DPRINTF(sc, RAY_DBG_CCS, "allocated 0x%02x", i);

	/*
	 * Reserve and fill the ccs - must do the length later.
	 *
	 * Even though build 4 and build 5 have different fields all these
	 * are common apart from tx_rate. Neither the NetBSD driver or Linux
	 * driver bother to overwrite this for build 4 cards.
	 *
	 * The start of the buffer must be aligned to a 256 byte boundary
	 * (least significant byte of address = 0x00).
	 */
	ccs = RAY_CCS_ADDRESS(i);
	bufp = RAY_TX_BASE + i * RAY_TX_BUF_SIZE;
	bufp += sc->sc_tibsize;
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_status, RAY_CCS_STATUS_BUSY);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_cmd, RAY_CMD_TX_REQ);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_link, RAY_CCS_LINK_NULL);
	SRAM_WRITE_FIELD_2(sc, ccs, ray_cmd_tx, c_bufp, bufp);
	SRAM_WRITE_FIELD_1(sc,
	    ccs, ray_cmd_tx, c_tx_rate, sc->sc_c.np_def_txrate);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_apm_mode, 0);
	bufp += sizeof(struct ray_tx_phy_header);

	*ccsp = ccs;
	*bufpp = bufp;
    	return (0);
}

/*
 * Routines to obtain resources for the card
 */

/*
 * Allocate the attribute memory on the card
 *
 * The attribute memory space is abused by these devices as IO space. As such
 * the OS card services don't have a chance of knowing that they need to keep
 * the attribute space mapped. We have to do it manually.
 */
static int
ray_res_alloc_am(struct ray_softc *sc)
{
	int error;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_CM, "");

	sc->am_rid = RAY_AM_RID;
	sc->am_res = bus_alloc_resource(sc->dev, SYS_RES_MEMORY,
	    &sc->am_rid, 0UL, ~0UL, 0x1000, RF_ACTIVE);
	if (!sc->am_res) {
		RAY_PRINTF(sc, "Cannot allocate attribute memory");
		return (ENOMEM);
	}
	error = CARD_SET_MEMORY_OFFSET(device_get_parent(sc->dev), sc->dev,
	    sc->am_rid, 0, NULL);
	if (error) {
		RAY_PRINTF(sc, "CARD_SET_MEMORY_OFFSET returned 0x%0x", error);
		return (error);
	}
	error = CARD_SET_RES_FLAGS(device_get_parent(sc->dev), sc->dev,
	    SYS_RES_MEMORY, sc->am_rid, PCCARD_A_MEM_ATTR);
	if (error) {
		RAY_PRINTF(sc, "CARD_SET_RES_FLAGS returned 0x%0x", error);
		return (error);
	}
	error = CARD_SET_RES_FLAGS(device_get_parent(sc->dev), sc->dev,
	    SYS_RES_MEMORY, sc->am_rid, PCCARD_A_MEM_8BIT);
	if (error) {
		RAY_PRINTF(sc, "CARD_SET_RES_FLAGS returned 0x%0x", error);
		return (error);
	}
	sc->am_bsh = rman_get_bushandle(sc->am_res);
	sc->am_bst = rman_get_bustag(sc->am_res);

#if RAY_DEBUG & (RAY_DBG_CM | RAY_DBG_BOOTPARAM)
{
    	u_long flags;
	u_int32_t offset;
	CARD_GET_RES_FLAGS(device_get_parent(sc->dev), sc->dev,
	    SYS_RES_MEMORY, sc->am_rid, &flags);
	CARD_GET_MEMORY_OFFSET(device_get_parent(sc->dev), sc->dev,
	    sc->am_rid, &offset);
	RAY_PRINTF(sc, "allocated attribute memory:\n"
	    ".  start 0x%0lx count 0x%0lx flags 0x%0lx offset 0x%0x",
	    bus_get_resource_start(sc->dev, SYS_RES_MEMORY, sc->am_rid),
	    bus_get_resource_count(sc->dev, SYS_RES_MEMORY, sc->am_rid),
	    flags, offset);
}
#endif /* RAY_DEBUG & (RAY_DBG_CM | RAY_DBG_BOOTPARAM) */

	return (0);
}

/*
 * Allocate the common memory on the card
 *
 * As this memory is described in the CIS, the OS card services should
 * have set the map up okay, but the card uses 8 bit RAM. This is not
 * described in the CIS.
 */
static int
ray_res_alloc_cm(struct ray_softc *sc)
{
	u_long start, count, end;
	int error;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_CM, "");

	RAY_DPRINTF(sc,RAY_DBG_CM | RAY_DBG_BOOTPARAM,
	    "cm start 0x%0lx count 0x%0lx",
	    bus_get_resource_start(sc->dev, SYS_RES_MEMORY, RAY_CM_RID),
	    bus_get_resource_count(sc->dev, SYS_RES_MEMORY, RAY_CM_RID));

	sc->cm_rid = RAY_CM_RID;
	start = bus_get_resource_start(sc->dev, SYS_RES_MEMORY, sc->cm_rid);
	count = bus_get_resource_count(sc->dev, SYS_RES_MEMORY, sc->cm_rid);
	end = start + count - 1;
	sc->cm_res = bus_alloc_resource(sc->dev, SYS_RES_MEMORY,
	    &sc->cm_rid, start, end, count, RF_ACTIVE);
	if (!sc->cm_res) {
		RAY_PRINTF(sc, "Cannot allocate common memory");
		return (ENOMEM);
	}
	error = CARD_SET_MEMORY_OFFSET(device_get_parent(sc->dev), sc->dev,
	    sc->cm_rid, 0, NULL);
	if (error) {
		RAY_PRINTF(sc, "CARD_SET_MEMORY_OFFSET returned 0x%0x", error);
		return (error);
	}
	error = CARD_SET_RES_FLAGS(device_get_parent(sc->dev), sc->dev,
	    SYS_RES_MEMORY, sc->cm_rid, PCCARD_A_MEM_COM);
	if (error) {
		RAY_PRINTF(sc, "CARD_SET_RES_FLAGS returned 0x%0x", error);
		return (error);
	}
	error = CARD_SET_RES_FLAGS(device_get_parent(sc->dev), sc->dev,
	    SYS_RES_MEMORY, sc->cm_rid, PCCARD_A_MEM_8BIT);
	if (error) {
		RAY_PRINTF(sc, "CARD_SET_RES_FLAGS returned 0x%0x", error);
		return (error);
	}
	sc->cm_bsh = rman_get_bushandle(sc->cm_res);
	sc->cm_bst = rman_get_bustag(sc->cm_res);

#if RAY_DEBUG & (RAY_DBG_CM | RAY_DBG_BOOTPARAM)
{
	u_long flags;
	u_int32_t offset;
	CARD_GET_RES_FLAGS(device_get_parent(sc->dev), sc->dev,
	    SYS_RES_MEMORY, sc->cm_rid, &flags);
	CARD_GET_MEMORY_OFFSET(device_get_parent(sc->dev), sc->dev,
	    sc->cm_rid, &offset);
	RAY_PRINTF(sc, "allocated common memory:\n"
	    ".  start 0x%0lx count 0x%0lx flags 0x%0lx offset 0x%0x",
	    bus_get_resource_start(sc->dev, SYS_RES_MEMORY, sc->cm_rid),
	    bus_get_resource_count(sc->dev, SYS_RES_MEMORY, sc->cm_rid),
	    flags, offset);
}
#endif /* RAY_DEBUG & (RAY_DBG_CM | RAY_DBG_BOOTPARAM) */

	return (0);
}

/*
 * Get an irq and attach it to the bus
 */
static int
ray_res_alloc_irq(struct ray_softc *sc)
{
	int error;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	RAY_DPRINTF(sc,RAY_DBG_CM | RAY_DBG_BOOTPARAM,
	    "irq start 0x%0lx count 0x%0lx",
	    bus_get_resource_start(sc->dev, SYS_RES_IRQ, 0),
	    bus_get_resource_count(sc->dev, SYS_RES_IRQ, 0));

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE);
	if (!sc->irq_res) {
		RAY_PRINTF(sc, "Cannot allocate irq");
		return (ENOMEM);
	}
	if ((error = bus_setup_intr(sc->dev, sc->irq_res, INTR_TYPE_NET,
	    ray_intr, sc, &sc->irq_handle)) != 0) {
		RAY_PRINTF(sc, "Failed to setup irq");
		return (error);
	}
	RAY_DPRINTF(sc, RAY_DBG_CM | RAY_DBG_BOOTPARAM, "allocated irq:\n"
	    ".  start 0x%0lx count 0x%0lx",
	    bus_get_resource_start(sc->dev, SYS_RES_IRQ, sc->irq_rid),
	    bus_get_resource_count(sc->dev, SYS_RES_IRQ, sc->irq_rid));

	return (0);
}

/*
 * Release all of the card's resources
 */
static void
ray_res_release(struct ray_softc *sc)
{
	if (sc->irq_res != 0) {
		bus_teardown_intr(sc->dev, sc->irq_res, sc->irq_handle);
		bus_release_resource(sc->dev, SYS_RES_IRQ,
		    sc->irq_rid, sc->irq_res);
		sc->irq_res = 0;
	}
	if (sc->am_res != 0) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    sc->am_rid, sc->am_res);
		sc->am_res = 0;
	}
	if (sc->cm_res != 0) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
		    sc->cm_rid, sc->cm_res);
		sc->cm_res = 0;
	}
}

/*
 * mbuf dump
 */
#if RAY_DEBUG & RAY_DBG_MBUF
static void
ray_dump_mbuf(struct ray_softc *sc, struct mbuf *m, char *s)
{
	u_int8_t *d, *ed;
	u_int i;
	char p[17];

	RAY_PRINTF(sc, "%s", s);
	RAY_PRINTF(sc, "\nm0->data\t0x%p\nm_pkthdr.len\t%d\nm_len\t%d",
	    mtod(m, u_int8_t *), m->m_pkthdr.len, m->m_len);
	i = 0;
	bzero(p, 17);
	for (; m; m = m->m_next) {
		d = mtod(m, u_int8_t *);
		ed = d + m->m_len;

		for (; d < ed; i++, d++) {
			if ((i % 16) == 0) {
				printf("  %s\n\t", p);
			} else if ((i % 8) == 0)
				printf("  ");
			printf(" %02x", *d);
			p[i % 16] = ((*d >= 0x20) && (*d < 0x80)) ? *d : '.';
		}
	}
	if ((i - 1) % 16)
		printf("  %s\n", p);
}
#endif /* RAY_DEBUG & RAY_DBG_MBUF */
