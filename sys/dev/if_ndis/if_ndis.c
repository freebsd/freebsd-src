/*
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bdg.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#include <dev/wi/if_wavelan_ieee.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/cfg_var.h>
#include <dev/if_ndis/if_ndisvar.h>

#include "ndis_driver_data.h"

MODULE_DEPEND(ndis, pci, 1, 1, 1);
MODULE_DEPEND(ndis, ether, 1, 1, 1);
MODULE_DEPEND(ndis, wlan, 1, 1, 1);
MODULE_DEPEND(ndis, ndisapi, 1, 1, 1);

/*
 * Various supported device vendors/types and their names.
 * These are defined in the ndis_driver_data.h file.
 */
static struct ndis_type ndis_devs[] = {
#ifdef NDIS_DEV_TABLE
	NDIS_DEV_TABLE
#endif
	{ 0, 0, 0, NULL }
};

#define __stdcall __attribute__((__stdcall__))

static int ndis_probe		(device_t);
static int ndis_attach		(device_t);
static int ndis_detach		(device_t);

static __stdcall void ndis_txeof	(ndis_handle,
	ndis_packet *, ndis_status);
static __stdcall void ndis_rxeof	(ndis_handle,
	ndis_packet **, uint32_t);
static __stdcall void ndis_linksts	(ndis_handle,
	ndis_status, void *, uint32_t);
static __stdcall void ndis_linksts_done	(ndis_handle);

static void ndis_intr		(void *);
static void ndis_intrtask	(void *, int);
static void ndis_tick		(void *);
static void ndis_ticktask	(void *, int);
static void ndis_start		(struct ifnet *);
static void ndis_starttask	(void *, int);
static int ndis_ioctl		(struct ifnet *, u_long, caddr_t);
static int ndis_wi_ioctl_get	(struct ifnet *, u_long, caddr_t);
static int ndis_wi_ioctl_set	(struct ifnet *, u_long, caddr_t);
static void ndis_init		(void *);
static void ndis_stop		(struct ndis_softc *);
static void ndis_watchdog	(struct ifnet *);
static void ndis_shutdown	(device_t);
static int ndis_ifmedia_upd	(struct ifnet *);
static void ndis_ifmedia_sts	(struct ifnet *, struct ifmediareq *);
static int ndis_get_assoc	(struct ndis_softc *, ndis_wlan_bssid_ex *);
static void ndis_getstate_80211	(struct ndis_softc *);
static void ndis_setstate_80211	(struct ndis_softc *);
static void ndis_media_status	(struct ifnet *, struct ifmediareq *);

static void ndis_reset		(struct ndis_softc *);
static void ndis_setmulti	(struct ndis_softc *);
static void ndis_map_sclist	(void *, bus_dma_segment_t *,
	int, bus_size_t, int);

extern struct mtx_pool *ndis_mtxpool;

static device_method_t ndis_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ndis_probe),
	DEVMETHOD(device_attach,	ndis_attach),
	DEVMETHOD(device_detach,	ndis_detach),
	DEVMETHOD(device_shutdown,	ndis_shutdown),

	{ 0, 0 }
};

static driver_t ndis_driver = {
#ifdef NDIS_DEVNAME
	NDIS_DEVNAME,
#else
	"ndis",
#endif
	ndis_methods,
	sizeof(struct ndis_softc)
};

static devclass_t ndis_devclass;

#ifdef NDIS_MODNAME
#define NDIS_MODNAME_OVERRIDE_PCI(x)					\
	DRIVER_MODULE(x, pci, ndis_driver, ndis_devclass, 0, 0)
#define NDIS_MODNAME_OVERRIDE_CARDBUS(x)				\
	DRIVER_MODULE(x, cardbus, ndis_driver, ndis_devclass, 0, 0)
NDIS_MODNAME_OVERRIDE_PCI(NDIS_MODNAME);
NDIS_MODNAME_OVERRIDE_CARDBUS(NDIS_MODNAME);
#else
DRIVER_MODULE(ndis, pci, ndis_driver, ndis_devclass, 0, 0);
DRIVER_MODULE(ndis, cardbus, ndis_driver, ndis_devclass, 0, 0);
#endif

/*
 * Program the 64-bit multicast hash filter.
 */
static void
ndis_setmulti(sc)
	struct ndis_softc	*sc;
{
#ifdef notyet
	uint32_t		ndis_filter;
	int			len;
#endif
	return;
}

static void
ndis_reset(sc)
	struct ndis_softc	*sc;
{
	ndis_reset_nic(sc);
        return;
}

/*
 * Probe for an NDIS device. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
ndis_probe(dev)
	device_t		dev;
{
	struct ndis_type		*t;

	t = ndis_devs;

	while(t->ndis_name != NULL) {
		if ((pci_get_vendor(dev) == t->ndis_vid) &&
		    (pci_get_device(dev) == t->ndis_did) &&
		    ((pci_read_config(dev, PCIR_SUBVEND_0, 4) ==
		    t->ndis_subsys) || t->ndis_subsys == 0)) {
			device_set_desc(dev, t->ndis_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
ndis_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ndis_softc		*sc;
	struct ifnet		*ifp = NULL;
	int			unit, error = 0, rid, len;
	void			*img;
	struct ndis_type	*t;
	int			i, devidx = 0, defidx = 0;
	struct resource_list	*rl;
	struct resource_list_entry	*rle;


	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sc->ndis_dev = dev;

	sc->ndis_mtx = mtx_pool_alloc(ndis_mtxpool);
	sc->ndis_intrmtx = mtx_pool_alloc(ndis_mtxpool);
	TASK_INIT(&sc->ndis_intrtask, 0, ndis_intrtask, sc);
	TASK_INIT(&sc->ndis_ticktask, 0, ndis_ticktask, sc);
	TASK_INIT(&sc->ndis_starttask, 0, ndis_starttask, sc);


	/*
	 * Map control/status registers.
	 */

	pci_enable_busmaster(dev);

	rl = BUS_GET_RESOURCE_LIST(device_get_parent(dev), dev);
	if (rl != NULL) {
		SLIST_FOREACH(rle, rl, link) {
			switch (rle->type) {
			case SYS_RES_IOPORT:
				sc->ndis_io_rid = rle->rid;
				sc->ndis_res_io = bus_alloc_resource(dev,
				    SYS_RES_IOPORT, &sc->ndis_io_rid,
				    0, ~0, 1, RF_ACTIVE);
				if (sc->ndis_res_io == NULL) {
					device_printf(dev,
					    "couldn't map iospace\n");
					error = ENXIO;
					goto fail;
				}
				break;
			case SYS_RES_MEMORY:
				if (sc->ndis_res_altmem != NULL &&
				    sc->ndis_res_mem != NULL) {
					device_printf(dev,
					    "too many memory resources\n");
					error = ENXIO;
					goto fail;
				}
				if (rle->rid == PCIR_BAR(2)) {
					sc->ndis_altmem_rid = rle->rid;
					sc->ndis_res_altmem =
					    bus_alloc_resource(dev,
					        SYS_RES_MEMORY,
						&sc->ndis_altmem_rid,
						0, ~0, 1, RF_ACTIVE);
					if (sc->ndis_res_altmem == NULL) {
						device_printf(dev,
						    "couldn't map alt "
						    "memory\n");
						error = ENXIO;
						goto fail;
					}
				} else {
					sc->ndis_mem_rid = rle->rid;
					sc->ndis_res_mem =
					    bus_alloc_resource(dev,
					        SYS_RES_MEMORY,
						&sc->ndis_mem_rid,
						0, ~0, 1, RF_ACTIVE);
					if (sc->ndis_res_mem == NULL) {
						device_printf(dev,
						    "couldn't map memory\n");
						error = ENXIO;
						goto fail;
					}
				}
				break;
			case SYS_RES_IRQ:
				rid = rle->rid;
				sc->ndis_irq = bus_alloc_resource(dev,
				    SYS_RES_IRQ, &rid, 0, ~0, 1,
	    			    RF_SHAREABLE | RF_ACTIVE);
				if (sc->ndis_irq == NULL) {
					device_printf(dev,
					    "couldn't map interrupt\n");
					error = ENXIO;
					goto fail;
				}
				break;
			default:
				break;
			}
			sc->ndis_rescnt++;
		}
	}

        /*
	 * Hook interrupt early, since calling the driver's
	 * init routine may trigger an interrupt.
	 */

	error = bus_setup_intr(dev, sc->ndis_irq, INTR_TYPE_NET,
	    ndis_intr, sc, &sc->ndis_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up irq\n");
		goto fail;
	}

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define NDIS_NSEG_NEW 32
	error = bus_dma_tag_create(NULL,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
                        BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, NDIS_NSEG_NEW,/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			BUS_DMA_ALLOCNOW,       /* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->ndis_parent_tag);

        if (error)
                goto fail;

	img = drv_data;
	sc->ndis_regvals = ndis_regvals;
	sc->ndis_iftype = PCIBus;

	/* Figure out exactly which device we matched. */

	t = ndis_devs;

	while(t->ndis_name != NULL) {
		if ((pci_get_vendor(dev) == t->ndis_vid) &&
		    (pci_get_device(dev) == t->ndis_did)) {
			if (t->ndis_subsys == 0)
				defidx = devidx;
			else {
				if (t->ndis_subsys ==
				    pci_read_config(dev, PCIR_SUBVEND_0, 4))
					break;
			}
		}
		t++;
		devidx++;
	}

	if (ndis_devs[devidx].ndis_name == NULL)
		sc->ndis_devidx = defidx;
	else
		sc->ndis_devidx = devidx;

	sysctl_ctx_init(&sc->ndis_ctx);

	/* Create sysctl registry nodes */
	ndis_create_sysctls(sc);

	/* Set up driver image in memory. */
	ndis_load_driver((vm_offset_t)img, sc);

	/* Tell the user what version of the API the driver is using. */
	device_printf(dev, "NDIS API version: %d.%d\n",
	    sc->ndis_chars.nmc_version_major,
	    sc->ndis_chars.nmc_version_minor);

	/* Do resource conversion. */
	ndis_convert_res(sc);

	/* Install our RX and TX interrupt handlers. */
	sc->ndis_block.nmb_senddone_func = ndis_txeof;
	sc->ndis_block.nmb_pktind_func = ndis_rxeof;

	/* Call driver's init routine. */
	if (ndis_init_nic(sc)) {
		device_printf (dev, "init handler failed\n");
		error = ENXIO;
		goto fail;
	}

	/* Reset the adapter. */
	ndis_reset(sc);

	/*
	 * Get station address from the driver.
	 */
	len = sizeof(eaddr);
	ndis_get_info(sc, OID_802_3_CURRENT_ADDRESS, &eaddr, &len);

	sc->ndis_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	/*
	 * Figure out of we're allowed to use multipacket sends
	 * with this driver, and if so, how many.
	 */

	if (sc->ndis_chars.nmc_sendsingle_func)
		sc->ndis_maxpkts = 1;
	else {
		len = sizeof(sc->ndis_maxpkts);
		ndis_get_info(sc, OID_GEN_MAXIMUM_SEND_PACKETS,
		    &sc->ndis_maxpkts, &len);
		sc->ndis_txarray = malloc(sizeof(ndis_packet *) *
		    sc->ndis_maxpkts, M_DEVBUF, M_NOWAIT);
		bzero((char *)sc->ndis_txarray, sizeof(ndis_packet *) *
		    sc->ndis_maxpkts);
	}

	sc->ndis_txpending = sc->ndis_maxpkts;

	sc->ndis_oidcnt = 0;
	/* Get supported oid list. */
	ndis_get_supported_oids(sc, &sc->ndis_oids, &sc->ndis_oidcnt);

	/* If the NDIS module requested scatter/gather, init maps. */
	if (sc->ndis_sc)
		ndis_init_dma(sc);

	/*
	 * See if the OID_802_11_NETWORK_TYPES_SUPPORTED OID is
	 * supported by this driver. If it is, then this an 802.11
	 * wireless driver, and we should set up media for wireless.
	 */
	for (i = 0; i < sc->ndis_oidcnt; i++) {
		if (sc->ndis_oids[i] == OID_802_11_NETWORK_TYPES_SUPPORTED) {
			sc->ndis_80211++;
			break;
		}
	}

	/*
	 * An NDIS device was detected. Inform the world.
	 */
	device_printf(dev, "%s address: %6D\n",
	    sc->ndis_80211 ? "802.11" : "Ethernet", eaddr, ":");

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ndis_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = ndis_start;
	ifp->if_watchdog = ndis_watchdog;
	ifp->if_init = ndis_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = 50;

	/* Do media setup */
	if (sc->ndis_80211) {
		struct ieee80211com	*ic = (void *)ifp;
		ndis_80211_config	config;
		ndis_80211_rates	rates;
		struct ndis_80211_nettype_list *ntl;
		uint32_t		arg;
		int			r;

	        ic->ic_phytype = IEEE80211_T_DS;
		ic->ic_opmode = IEEE80211_M_STA;
		ic->ic_caps = IEEE80211_C_IBSS;
		ic->ic_state = IEEE80211_S_ASSOC;
		ic->ic_modecaps = (1<<IEEE80211_MODE_AUTO);
		len = 0;
		r = ndis_get_info(sc, OID_802_11_NETWORK_TYPES_SUPPORTED,
		    NULL, &len);
		if (r != ENOSPC)
			goto nonettypes;
		ntl = malloc(len, M_DEVBUF, M_WAITOK);
		r = ndis_get_info(sc, OID_802_11_NETWORK_TYPES_SUPPORTED,
		    ntl, &len);
		if (r != 0) {
			free(ntl, M_DEVBUF);
			goto nonettypes;
		}

		for (i = 0; i < ntl->ntl_items; i++) {
			switch (ntl->ntl_type[i]) {
			case NDIS_80211_NETTYPE_11FH:
				ic->ic_modecaps |= (1<<IEEE80211_MODE_11B);
				break;
			case NDIS_80211_NETTYPE_11DS:
				ic->ic_modecaps |= (1<<IEEE80211_MODE_11B);
				break;
			case NDIS_80211_NETTYPE_11OFDM5:
				ic->ic_modecaps |= (1<<IEEE80211_MODE_11A);
				break;
			case NDIS_80211_NETTYPE_11OFDM24:
				ic->ic_modecaps |= (1<<IEEE80211_MODE_11G);
				break;
			default:
				break;
			}
		}
		free(ntl, M_DEVBUF);
nonettypes:
		len = sizeof(rates);
		bzero((char *)&rates, len);
		r = ndis_get_info(sc, OID_802_11_SUPPORTED_RATES,
		    (void *)rates, &len);
		if (r)
			device_printf (dev, "get rates failed: 0x%x\n", r);
		/*
		 * Since the supported rates only up to 8 can be supported,
		 * if this is not 802.11b we're just going to be faking it
		 * all up to heck.
		 */
#define SETRATE(x, y)	\
	ic->ic_sup_rates[x].rs_rates[ic->ic_sup_rates[x].rs_nrates] = (y)
#define INCRATE(x)	\
	ic->ic_sup_rates[x].rs_nrates++

		ic->ic_curmode = IEEE80211_MODE_AUTO;
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11A))
			ic->ic_sup_rates[IEEE80211_MODE_11A].rs_nrates = 0;
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11B))
			ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates = 0;
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11G))
			ic->ic_sup_rates[IEEE80211_MODE_11G].rs_nrates = 0;
		for (i = 0; i < len; i++) {
			switch (rates[i] & IEEE80211_RATE_VAL) {
			case 2:
			case 4:
			case 11:
			case 10:
			case 22:
				if (!(ic->ic_modecaps &
				    (1<<IEEE80211_MODE_11B))) {
					/* Lazy-init 802.11b. */
					ic->ic_modecaps |=
					    (1<<IEEE80211_MODE_11B);
					ic->ic_sup_rates[IEEE80211_MODE_11B].
					    rs_nrates = 0;
				}
				SETRATE(IEEE80211_MODE_11B, rates[i]);
				INCRATE(IEEE80211_MODE_11B);
				break;
			default:
				if (ic->ic_modecaps & (1<<IEEE80211_MODE_11A)) {
					SETRATE(IEEE80211_MODE_11A, rates[i]);
					INCRATE(IEEE80211_MODE_11A);
				}
				if (ic->ic_modecaps & (1<<IEEE80211_MODE_11G)) {
					SETRATE(IEEE80211_MODE_11G, rates[i]);
					INCRATE(IEEE80211_MODE_11G);
				}
				break;
			}
		}

		/*
		 * If the hardware supports 802.11g, it most
		 * likely supports 802.11b and all of the
		 * 802.11b and 802.11g speeds, so maybe we can
		 * just cheat here.  Just how in the heck do
		 * we detect turbo modes, though?
		 */
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11G)) {
			SETRATE(IEEE80211_MODE_11G, 47);
			INCRATE(IEEE80211_MODE_11G);
			SETRATE(IEEE80211_MODE_11G, 72);
			INCRATE(IEEE80211_MODE_11G);
			SETRATE(IEEE80211_MODE_11G, 96);
			INCRATE(IEEE80211_MODE_11G);
			SETRATE(IEEE80211_MODE_11G, 108);
			INCRATE(IEEE80211_MODE_11G);
		}
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11A)) {
			SETRATE(IEEE80211_MODE_11A, 47);
			INCRATE(IEEE80211_MODE_11A);
			SETRATE(IEEE80211_MODE_11A, 72);
			INCRATE(IEEE80211_MODE_11A);
			SETRATE(IEEE80211_MODE_11A, 96);
			INCRATE(IEEE80211_MODE_11A);
			SETRATE(IEEE80211_MODE_11A, 108);
			INCRATE(IEEE80211_MODE_11A);
		}
#undef SETRATE
#undef INCRATE
		/*
		 * Taking yet more guesses here.
		 */
		for (i = 1; i < IEEE80211_CHAN_MAX; i++) {
			int chanflag = 0;

			if (ic->ic_sup_rates[IEEE80211_MODE_11G].rs_nrates)
				chanflag |= IEEE80211_CHAN_G;
			if (i <= 14)
				chanflag |= IEEE80211_CHAN_B;
			if (chanflag == 0)
				break;
			ic->ic_channels[i].ic_freq =
			    ieee80211_ieee2mhz(i, chanflag);
			ic->ic_channels[i].ic_flags = chanflag;
		}

		i = sizeof(arg);
		r = ndis_get_info(sc, OID_802_11_WEP_STATUS, &arg, &i);
		if (arg != NDIS_80211_WEPSTAT_NOTSUPPORTED)
			ic->ic_caps |= IEEE80211_C_WEP;
		i = sizeof(arg);
		r = ndis_get_info(sc, OID_802_11_POWER_MODE, &arg, &i);
		if (r == 0)
			ic->ic_caps |= IEEE80211_C_PMGT;
		i = sizeof(config);
		r = ndis_get_info(sc, OID_802_11_CONFIGURATION, &config, &i);
		if (r == 0) {
			int chan;
			chan = ieee80211_mhz2ieee(config.nc_dsconfig / 1000, 0);
			if (chan < 0 || chan >= IEEE80211_CHAN_MAX) {
				ic->ic_ibss_chan = &ic->ic_channels[1];
			} else
				ic->ic_ibss_chan = &ic->ic_channels[chan];
		} else {
			device_printf(sc->ndis_dev, "couldn't retrieve "
			    "channel info: %d\n", r);
			ic->ic_ibss_chan = &ic->ic_channels[1];
		}
		bcopy(eaddr, &ic->ic_myaddr, sizeof(eaddr));
		ieee80211_ifattach(ifp);
		ieee80211_media_init(ifp, ieee80211_media_change,
		    ndis_media_status);
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	} else {
		ifmedia_init(&sc->ifmedia, IFM_IMASK, ndis_ifmedia_upd,
		    ndis_ifmedia_sts);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->ifmedia, IFM_ETHER|IFM_AUTO);
		ether_ifattach(ifp, eaddr);
	}

	/* Override the status handler so we can detect link changes. */
	sc->ndis_block.nmb_status_func = ndis_linksts;
	sc->ndis_block.nmb_statusdone_func = ndis_linksts_done;
fail:
	if (error)
		ndis_detach(dev);
	else {
		/* We're done talking to the NIC for now; halt it. */
		ifp->if_flags |= IFF_UP;
		ndis_halt_nic(sc);
		ifp->if_flags &= ~IFF_UP;
	}

	return(error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
ndis_detach(dev)
	device_t		dev;
{
	struct ndis_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->ndis_mtx), ("ndis mutex not initialized"));
	NDIS_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ifp->if_flags &= ~IFF_UP;

	if (device_is_attached(dev)) {
		NDIS_UNLOCK(sc);
		ndis_stop(sc);
		if (sc->ndis_80211)
			ieee80211_ifdetach(ifp);
		else
			ether_ifdetach(ifp);
	} else
		NDIS_UNLOCK(sc);

	bus_generic_detach(dev);

	if (sc->ndis_intrhand)
		bus_teardown_intr(dev, sc->ndis_irq, sc->ndis_intrhand);
	if (sc->ndis_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ndis_irq);
	if (sc->ndis_res_io)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->ndis_io_rid, sc->ndis_res_io);
	if (sc->ndis_res_mem)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->ndis_mem_rid, sc->ndis_res_mem);
	if (sc->ndis_res_altmem)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->ndis_altmem_rid, sc->ndis_res_altmem);

	if (sc->ndis_sc)
		ndis_destroy_dma(sc);

	ndis_unload_driver((void *)ifp);

	bus_dma_tag_destroy(sc->ndis_parent_tag);

	sysctl_ctx_free(&sc->ndis_ctx);

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 *
 * When handling received NDIS packets, the 'status' field in the
 * out-of-band portion of the ndis_packet has special meaning. In the
 * most common case, the underlying NDIS driver will set this field
 * to NDIS_STATUS_SUCCESS, which indicates that it's ok for us to
 * take posession of it. We then change the status field to
 * NDIS_STATUS_PENDING to tell the driver that we now own the packet,
 * and that we will return it at some point in the future via the
 * return packet handler.
 *
 * If the driver hands us a packet with a status of NDIS_STATUS_RESOURCES,
 * this means the driver is running out of packet/buffer resources and
 * wants to maintain ownership of the packet. In this case, we have to
 * copy the packet data into local storage and let the driver keep the
 * packet.
 */
__stdcall static void
ndis_rxeof(adapter, packets, pktcnt)
	ndis_handle		adapter;
	ndis_packet		**packets;
	uint32_t		pktcnt;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	ndis_packet		*p;
	struct ifnet		*ifp;
	struct mbuf		*m0, *m;
	int			i;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)(block->nmb_ifp);
	ifp = block->nmb_ifp;

	for (i = 0; i < pktcnt; i++) {
		p = packets[i];
		/* Stash the softc here so ptom can use it. */
		p->np_softc = sc;
		if (ndis_ptom(&m0, p)) {
			device_printf (sc->ndis_dev, "ptom failed\n");
			if (p->np_oob.npo_status == NDIS_STATUS_SUCCESS)
				ndis_return_packet(sc, p);
		} else {
			if (p->np_oob.npo_status == NDIS_STATUS_RESOURCES) {
				m = m_dup(m0, M_DONTWAIT);
				m_freem(m0);
				if (m == NULL)
					ifp->if_ierrors++;
				else
					m0 = m;
			} else
				p->np_oob.npo_status = NDIS_STATUS_PENDING;
			m0->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;
			(*ifp->if_input)(ifp, m0);
		}
	}

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
__stdcall static void
ndis_txeof(adapter, packet, status)
	ndis_handle		adapter;
	ndis_packet		*packet;
	ndis_status		status;

{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	struct ifnet		*ifp;
	int			idx;
	struct mbuf		*m;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_ifp;
	ifp = block->nmb_ifp;

	m = packet->np_m0;
	idx = packet->np_txidx;
	ifp->if_opackets++;
	if (sc->ndis_sc)
		bus_dmamap_unload(sc->ndis_ttag, sc->ndis_tmaps[idx]);

	ndis_free_packet(packet);
	sc->ndis_txarray[idx] = NULL;
	sc->ndis_txpending++;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	m_freem(m);

	taskqueue_enqueue(taskqueue_swi, &sc->ndis_starttask);

	return;
}

__stdcall static void
ndis_linksts(adapter, status, sbuf, slen)
	ndis_handle		adapter;
	ndis_status		status;
	void			*sbuf;
	uint32_t		slen;
{
	ndis_miniport_block	*block;

	block = adapter;
	block->nmb_getstat = status;

	return;
}

__stdcall static void
ndis_linksts_done(adapter)
	ndis_handle		adapter;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	struct ifnet		*ifp;

	block = adapter;
	ifp = block->nmb_ifp;
	sc = ifp->if_softc;

	if (!(ifp->if_flags & IFF_UP))
		return;

	switch (block->nmb_getstat) {
	case NDIS_STATUS_MEDIA_CONNECT:
		taskqueue_enqueue(taskqueue_swi, &sc->ndis_ticktask);
		taskqueue_enqueue(taskqueue_swi, &sc->ndis_starttask);
		break;
	case NDIS_STATUS_MEDIA_DISCONNECT:
		if (sc->ndis_80211)
			ndis_getstate_80211(sc);
		taskqueue_enqueue(taskqueue_swi, &sc->ndis_ticktask);
		break;
	default:
		break;
	}

	return;
}

static void
ndis_intrtask(arg, pending)
	void			*arg;
	int			pending;
{
	struct ndis_softc	*sc;
	struct ifnet		*ifp;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	NDIS_LOCK(sc);
	ndis_intrhand(sc);
	NDIS_UNLOCK(sc);
	mtx_lock(sc->ndis_intrmtx);
	ndis_enable_intr(sc);
	mtx_unlock(sc->ndis_intrmtx);

	if (ifp->if_snd.ifq_head != NULL)
		ndis_start(ifp);

	return;
}

static void
ndis_intr(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	struct ifnet		*ifp;
	int			is_our_intr = 0;
	int			call_isr = 0;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP) &&
	    sc->ndis_block.nmb_miniportadapterctx == NULL)
		return;

	mtx_lock(sc->ndis_intrmtx);
	if (sc->ndis_block.nmb_interrupt->ni_isrreq == TRUE)
		ndis_isr(sc, &is_our_intr, &call_isr);
	else {
		ndis_disable_intr(sc);
		call_isr = 1;
	}
	mtx_unlock(sc->ndis_intrmtx);

	if ((is_our_intr || call_isr) && (ifp->if_flags & IFF_UP))
		taskqueue_enqueue(taskqueue_swi, &sc->ndis_intrtask);

	return;
}

static void
ndis_tick(xsc)
	void			*xsc;
{
	struct ndis_softc	*sc;
	sc = xsc;
	taskqueue_enqueue(taskqueue_swi, &sc->ndis_ticktask);
	sc->ndis_stat_ch = timeout(ndis_tick, sc, hz *
	    sc->ndis_block.nmb_checkforhangsecs);

}

static void
ndis_ticktask(xsc, pending)
	void			*xsc;
	int			pending;
{
	struct ndis_softc	*sc;
	__stdcall ndis_checkforhang_handler hangfunc;
	uint8_t			rval;
	ndis_media_state	linkstate;
	int			error, len;

	sc = xsc;

	len = sizeof(linkstate);
	error = ndis_get_info(sc, OID_GEN_MEDIA_CONNECT_STATUS,
	    (void *)&linkstate, &len);

	NDIS_LOCK(sc);

	if (sc->ndis_link == 0 && linkstate == nmc_connected) {
		device_printf(sc->ndis_dev, "link up\n");
		sc->ndis_link = 1;
		if (sc->ndis_80211)
			ndis_getstate_80211(sc);
	}

	if (sc->ndis_link == 1 && linkstate == nmc_disconnected) {
		device_printf(sc->ndis_dev, "link down\n");
		sc->ndis_link = 0;
	}

	NDIS_UNLOCK(sc);

	hangfunc = sc->ndis_chars.nmc_checkhang_func;

	if (hangfunc != NULL) {
		rval = hangfunc(sc->ndis_block.nmb_miniportadapterctx);
		if (rval == TRUE)
			ndis_reset_nic(sc);
	}

	return;
}

static void
ndis_map_sclist(arg, segs, nseg, mapsize, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	bus_size_t		mapsize;
	int			error;

{
	struct ndis_sc_list	*sclist;
	int			i;

	if (error || arg == NULL)
		return;

	sclist = arg;

	sclist->nsl_frags = nseg;

	for (i = 0; i < nseg; i++) {
		sclist->nsl_elements[i].nse_addr.np_quad = segs[i].ds_addr;
		sclist->nsl_elements[i].nse_len = segs[i].ds_len;
	}

	return;
}

static void
ndis_starttask(arg, pending)
	void			*arg;
	int			pending;
{
	struct ifnet		*ifp;

	ifp = arg;
	if (ifp->if_snd.ifq_head != NULL)
		ndis_start(ifp);
	return;
}

/*
 * Main transmit routine. To make NDIS drivers happy, we need to
 * transform mbuf chains into NDIS packets and feed them to the
 * send packet routines. Most drivers allow you to send several
 * packets at once (up to the maxpkts limit). Unfortunately, rather
 * that accepting them in the form of a linked list, they expect
 * a contiguous array of pointers to packets.
 *
 * For those drivers which use the NDIS scatter/gather DMA mechanism,
 * we need to perform busdma work here. Those that use map registers
 * will do the mapping themselves on a buffer by buffer basis.
 */

static void
ndis_start(ifp)
	struct ifnet		*ifp;
{
	struct ndis_softc	*sc;
	struct mbuf		*m = NULL;
	ndis_packet		**p0 = NULL, *p = NULL;
	int			pcnt = 0;

	sc = ifp->if_softc;

	NDIS_LOCK(sc);

	if (!sc->ndis_link || ifp->if_flags & IFF_OACTIVE) {
		NDIS_UNLOCK(sc);
		return;
	}

	p0 = &sc->ndis_txarray[sc->ndis_txidx];

	while(sc->ndis_txpending) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		sc->ndis_txarray[sc->ndis_txidx] = NULL;

		if (ndis_mtop(m, &sc->ndis_txarray[sc->ndis_txidx])) {
			NDIS_UNLOCK(sc);
			IF_PREPEND(&ifp->if_snd, m);
			return;
		}

		/*
		 * Save pointer to original mbuf
		 * so we can free it later.
		 */

		p = sc->ndis_txarray[sc->ndis_txidx];
		p->np_txidx = sc->ndis_txidx;
		p->np_m0 = m;
		p->np_oob.npo_status = NDIS_STATUS_PENDING;

		/*
		 * Do scatter/gather processing, if driver requested it.
		 */
		if (sc->ndis_sc) {
			bus_dmamap_load_mbuf(sc->ndis_ttag,
			    sc->ndis_tmaps[sc->ndis_txidx], m,
			    ndis_map_sclist, &p->np_sclist, BUS_DMA_NOWAIT);
			bus_dmamap_sync(sc->ndis_ttag,
			    sc->ndis_tmaps[sc->ndis_txidx],
			    BUS_DMASYNC_PREREAD);
			p->np_ext.npe_info[ndis_sclist_info] = &p->np_sclist;
		}

		NDIS_INC(sc);
		sc->ndis_txpending--;

		pcnt++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */

		BPF_MTAP(ifp, m);

		/*
		 * The array that p0 points to must appear contiguous,
		 * so we must not wrap past the end of sc->ndis_txarray[].
		 * If it looks like we're about to wrap, break out here
		 * so the this batch of packets can be transmitted, then
		 * wait for txeof to ask us to send the rest.
		 */

		if (sc->ndis_txidx == 0)
			break;
	}

	if (sc->ndis_txpending == 0)
		ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	NDIS_UNLOCK(sc);

	ndis_send_packets(sc, p0, pcnt);

	return;
}

static void
ndis_init(xsc)
	void			*xsc;
{
	struct ndis_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			i, error;
	uint32_t		ndis_filter = 0;

	/*NDIS_LOCK(sc);*/

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	ndis_reset(sc);
	ndis_stop(sc);
	ndis_init_nic(sc);

	/* Init our MAC address */
#ifdef notdef
	/*
	 * Program the multicast filter, if necessary.
	 */
	ndis_setmulti(sc);
#endif

	/* Program the packet filter */

	ndis_filter = NDIS_PACKET_TYPE_DIRECTED;

	if (ifp->if_flags & IFF_BROADCAST)
		ndis_filter |= NDIS_PACKET_TYPE_BROADCAST;

	if (ifp->if_flags & IFF_PROMISC)
		ndis_filter |= NDIS_PACKET_TYPE_PROMISCUOUS;

	if (ifp->if_flags & IFF_MULTICAST)
		ndis_filter |= NDIS_PACKET_TYPE_MULTICAST;

	i = sizeof(ndis_filter);

	error = ndis_set_info(sc, OID_GEN_CURRENT_PACKET_FILTER,
	    &ndis_filter, &i);

	sc->ndis_filter = ndis_filter;

	if (error)
		device_printf (sc->ndis_dev, "set filter failed: %d\n", error);

	sc->ndis_txidx = 0;
	sc->ndis_txpending = sc->ndis_maxpkts;
	sc->ndis_link = 0;

	ndis_enable_intr(sc);

	if (sc->ndis_80211)
		ndis_setstate_80211(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Some drivers don't set this value. The NDIS spec says
	 * the default checkforhang timeout is approximately 2
	 * seconds.
	 */

	if (sc->ndis_block.nmb_checkforhangsecs == 0)
		sc->ndis_block.nmb_checkforhangsecs = 2;

	sc->ndis_stat_ch = timeout(ndis_tick, sc,
	    hz * sc->ndis_block.nmb_checkforhangsecs);

	/*NDIS_UNLOCK(sc);*/

	return;
}

/*
 * Set media options.
 */
static int
ndis_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct ndis_softc		*sc;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		ndis_init(sc);

	return(0);
}

/*
 * Report current media status.
 */
static void
ndis_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct ndis_softc	*sc;
	uint32_t		media_info;
	ndis_media_state	linkstate;
	int			error, len;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!(ifp->if_flags & IFF_UP))
		return;

	sc = ifp->if_softc;

	len = sizeof(linkstate);
	error = ndis_get_info(sc, OID_GEN_MEDIA_CONNECT_STATUS,
	    (void *)&linkstate, &len);

	len = sizeof(media_info);
	error = ndis_get_info(sc, OID_GEN_LINK_SPEED,
	    (void *)&media_info, &len);

	if (linkstate == nmc_connected)
		ifmr->ifm_status |= IFM_ACTIVE;

	switch(media_info) {
	case 100000:
		ifmr->ifm_active |= IFM_10_T;
		break;
	case 1000000:
		ifmr->ifm_active |= IFM_100_TX;
		break;
	case 10000000:
		ifmr->ifm_active |= IFM_1000_T;
		break;
	default:
		device_printf(sc->ndis_dev, "unknown speed: %d\n", media_info);
		break;
	}

	return;
}

static void
ndis_setstate_80211(sc)
	struct ndis_softc	*sc;
{
	struct ieee80211com	*ic;
	ndis_80211_ssid		ssid;
	ndis_80211_config	config;
	ndis_80211_wep		wep;
	int			i, rval = 0, len;
	uint32_t		arg;
	struct ifnet		*ifp;

	ic = &sc->ic;
	ifp = &sc->ic.ic_ac.ac_if;

	if (!(ifp->if_flags & IFF_UP))
		return;

	arg = NDIS_80211_AUTHMODE_AUTO;
	len = sizeof(arg);
	rval = ndis_set_info(sc, OID_802_11_AUTHENTICATION_MODE, &arg, &len);

	if (rval)
		device_printf (sc->ndis_dev, "set auth failed: %d\n", rval);

	/* Set network infrastructure mode. */

	len = sizeof(arg);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		arg = NDIS_80211_NET_INFRA_IBSS;
	else
		arg = NDIS_80211_NET_INFRA_BSS;

	rval = ndis_set_info(sc, OID_802_11_INFRASTRUCTURE_MODE, &arg, &len);

	if (rval)
		device_printf (sc->ndis_dev, "set infra failed: %d\n", rval);

	/* Set WEP */

#ifdef IEEE80211_F_WEPON
	if (ic->ic_flags & IEEE80211_F_WEPON) {
#else
	if (ic->ic_wep_mode >= IEEE80211_WEP_ON) {
#endif
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			if (ic->ic_nw_keys[i].wk_len) {
				bzero((char *)&wep, sizeof(wep));
				wep.nw_keylen = ic->ic_nw_keys[i].wk_len;
#ifdef notdef
				/* 5 and 13 are the only valid key lengths */
				if (ic->ic_nw_keys[i].wk_len < 5)
					wep.nw_keylen = 5;
				else if (ic->ic_nw_keys[i].wk_len > 5 &&
				     ic->ic_nw_keys[i].wk_len < 13)
					wep.nw_keylen = 13;
#endif
				wep.nw_keyidx = i;
				wep.nw_length = (sizeof(uint32_t) * 3)
				    + wep.nw_keylen;
				if (i == ic->ic_wep_txkey)
					wep.nw_keyidx |= NDIS_80211_WEPKEY_TX;
				bcopy(ic->ic_nw_keys[i].wk_key,
				    wep.nw_keydata, wep.nw_length);
				len = sizeof(wep);
				rval = ndis_set_info(sc,
				    OID_802_11_ADD_WEP, &wep, &len);
				if (rval)
					device_printf(sc->ndis_dev,
					    "set wepkey failed: %d\n", rval);
			}
		}
		arg = NDIS_80211_WEPSTAT_ENABLED;
		len = sizeof(arg);
		rval = ndis_set_info(sc, OID_802_11_WEP_STATUS, &arg, &len);
		if (rval)
			device_printf(sc->ndis_dev,
			    "enable WEP failed: %d\n", rval);
#ifndef IEEE80211_F_WEPON
		if (ic->ic_wep_mode != IEEE80211_WEP_8021X &&
		    ic->ic_wep_mode != IEEE80211_WEP_ON)
			arg = NDIS_80211_PRIVFILT_ACCEPTALL;
		else
#endif
			arg = NDIS_80211_PRIVFILT_8021XWEP;
		len = sizeof(arg);
		rval = ndis_set_info(sc, OID_802_11_PRIVACY_FILTER, &arg, &len);
#ifdef IEEE80211_WEP_8021X /*IEEE80211_F_WEPON*/
		/* Accept that we only have "shared" and 802.1x modes. */
		if (rval == 0) {
			if (arg == NDIS_80211_PRIVFILT_ACCEPTALL)
				ic->ic_wep_mode = IEEE80211_WEP_MIXED;
			else
				ic->ic_wep_mode = IEEE80211_WEP_8021X;
		}
#endif
	} else {
		arg = NDIS_80211_WEPSTAT_DISABLED;
		len = sizeof(arg);
		ndis_set_info(sc, OID_802_11_WEP_STATUS, &arg, &len);
	}

	/* Set SSID. */

	len = sizeof(ssid);
	bzero((char *)&ssid, len);
	ssid.ns_ssidlen = ic->ic_des_esslen;
	if (ssid.ns_ssidlen == 0) {
		ssid.ns_ssidlen = 1;
	} else
		bcopy(ic->ic_des_essid, ssid.ns_ssid, ssid.ns_ssidlen);
	rval = ndis_set_info(sc, OID_802_11_SSID, &ssid, &len);

	if (rval)
		device_printf (sc->ndis_dev, "set ssid failed: %d\n", rval);

	len = sizeof(config);
	rval = ndis_get_info(sc, OID_802_11_CONFIGURATION, &config, &len);   
	if (rval == 0) { 
		int chan;

		chan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
		if (chan != ieee80211_mhz2ieee(config.nc_dsconfig / 1000, 0)) {
			config.nc_dsconfig =
			    ic->ic_bss->ni_chan->ic_freq * 1000;
			rval = ndis_set_info(sc, OID_802_11_CONFIGURATION,
			    &config, &len);
			if (rval)
				device_printf(sc->ndis_dev, "couldn't change "
				    "DS config to %ukHz: %d\n",
				    config.nc_dsconfig, rval);
		}
	} else
		device_printf(sc->ndis_dev, "couldn't retrieve "
		    "channel info: %d\n", rval);

	return;
}

static void
ndis_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
        struct ieee80211com *ic = (void *)ifp;
        struct ieee80211_node *ni = NULL;

        imr->ifm_status = IFM_AVALID;
        imr->ifm_active = IFM_IEEE80211;
        if (ic->ic_state == IEEE80211_S_RUN)
                imr->ifm_status |= IFM_ACTIVE;
        imr->ifm_active |= IFM_AUTO;
        switch (ic->ic_opmode) {
        case IEEE80211_M_STA:
                ni = ic->ic_bss;
                /* calculate rate subtype */
                imr->ifm_active |= ieee80211_rate2media(ic,
                        ni->ni_rates.rs_rates[ni->ni_txrate], ic->ic_curmode);
                break;
        case IEEE80211_M_IBSS:
                ni = ic->ic_bss;
                /* calculate rate subtype */
                imr->ifm_active |= ieee80211_rate2media(ic,
                        ni->ni_rates.rs_rates[ni->ni_txrate], ic->ic_curmode);
                imr->ifm_active |= IFM_IEEE80211_ADHOC;
                break;
        case IEEE80211_M_AHDEMO:
                /* should not come here */
                break;
        case IEEE80211_M_HOSTAP:
                imr->ifm_active |= IFM_IEEE80211_HOSTAP;
                break;
        case IEEE80211_M_MONITOR:
                imr->ifm_active |= IFM_IEEE80211_MONITOR;
                break;
        }
        switch (ic->ic_curmode) {
        case IEEE80211_MODE_11A:
                imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11A);
                break;
        case IEEE80211_MODE_11B:
                imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11B);
                break;
        case IEEE80211_MODE_11G:
                imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11G);
                break;
        case IEEE80211_MODE_TURBO:
                imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11A)
                                |  IFM_IEEE80211_TURBO;
                break;
        }
}

static int
ndis_get_assoc(sc, assoc)
	struct ndis_softc	*sc;
	ndis_wlan_bssid_ex	*assoc;
{
	ndis_80211_bssid_list_ex	*bl;
	ndis_wlan_bssid_ex	*bs;
	ndis_80211_macaddr	bssid;
	int			i, len, error;

	len = sizeof(bssid);
	error = ndis_get_info(sc, OID_802_11_BSSID, &bssid, &len);
	if (error) {
		device_printf(sc->ndis_dev, "failed to get bssid");
		return(ENOENT);
	}
	len = 0;
	error = ndis_get_info(sc, OID_802_11_BSSID_LIST, NULL, &len);
	if (error != ENOSPC) {
		device_printf(sc->ndis_dev, "bssid_list failed");
		return (error);
	}

	bl = malloc(len, M_TEMP, M_NOWAIT);
	error = ndis_get_info(sc, OID_802_11_BSSID_LIST, bl, &len);
	if (error) {
		free(bl, M_TEMP);
		device_printf(sc->ndis_dev, "bssid_list failed");
		return (error);
	}

	bs = (ndis_wlan_bssid_ex *)&bl->nblx_bssid[0];
	for (i = 0; i < bl->nblx_items; i++) {
		if (bcmp(bs->nwbx_macaddr, bssid, sizeof(bssid)) == 0) {
			bcopy((char *)bs, (char *)assoc, bs->nwbx_len);
			free(bl, M_TEMP);
			return(0);
		}	
		bs = (ndis_wlan_bssid_ex *)((char *)bs + bs->nwbx_len);
	}

	free(bl, M_TEMP);
	return(ENOENT);
}

static void
ndis_getstate_80211(sc)
	struct ndis_softc	*sc;
{
	struct ieee80211com	*ic;
	ndis_80211_ssid		ssid;
	ndis_80211_config	config;
	ndis_wlan_bssid_ex	bs;
	int			rval, len, i = 0;
	uint32_t		arg;
	struct ifnet		*ifp;

	ic = &sc->ic;
	ifp = &sc->ic.ic_ac.ac_if;

	if (!(ifp->if_flags & IFF_UP))
		return;

	if (sc->ndis_link)
		ic->ic_state = IEEE80211_S_RUN;
	else
		ic->ic_state = IEEE80211_S_ASSOC;


	/*
	 * If we're associated, retrieve info on the current bssid.
	 */
	if (ndis_get_assoc(sc, &bs) == 0) {
		switch(bs.nwbx_nettype) {
		case NDIS_80211_NETTYPE_11FH:
		case NDIS_80211_NETTYPE_11DS:
			ic->ic_curmode = IEEE80211_MODE_11B;
			break;
		case NDIS_80211_NETTYPE_11OFDM5:
			ic->ic_curmode = IEEE80211_MODE_11A;
			break;
		case NDIS_80211_NETTYPE_11OFDM24:
			ic->ic_curmode = IEEE80211_MODE_11G;
			break;
		default:
			device_printf(sc->ndis_dev,
			    "unknown nettype %d\n", arg);
			break;
		}
	}

	len = sizeof(ssid);
	bzero((char *)&ssid, len);
	rval = ndis_get_info(sc, OID_802_11_SSID, &ssid, &len);

	if (rval)
		device_printf (sc->ndis_dev, "get ssid failed: %d\n", rval);
	bcopy(ssid.ns_ssid, ic->ic_bss->ni_essid, ssid.ns_ssidlen);
	ic->ic_bss->ni_esslen = ssid.ns_ssidlen;

	len = sizeof(arg);
	rval = ndis_get_info(sc, OID_GEN_LINK_SPEED, &arg, &len);

	if (ic->ic_modecaps & (1<<IEEE80211_MODE_11B)) {
		ic->ic_bss->ni_rates = ic->ic_sup_rates[IEEE80211_MODE_11B];
		for (i = 0; i < ic->ic_bss->ni_rates.rs_nrates; i++) {
			if ((ic->ic_bss->ni_rates.rs_rates[i] &
			    IEEE80211_RATE_VAL) == arg / 5000)
				break;
		}
	}

	if (i == ic->ic_bss->ni_rates.rs_nrates &&
	    ic->ic_modecaps & (1<<IEEE80211_MODE_11G)) {
		ic->ic_bss->ni_rates = ic->ic_sup_rates[IEEE80211_MODE_11G];
		for (i = 0; i < ic->ic_bss->ni_rates.rs_nrates; i++) {
			if ((ic->ic_bss->ni_rates.rs_rates[i] &
			    IEEE80211_RATE_VAL) == arg / 5000)
				break;
		}
	}

	if (i == ic->ic_bss->ni_rates.rs_nrates)
		device_printf(sc->ndis_dev, "no matching rate for: %d\n",
		    arg / 5000);
	else
		ic->ic_bss->ni_txrate = i;

	if (ic->ic_caps & IEEE80211_C_PMGT) {
		len = sizeof(arg);
		rval = ndis_get_info(sc, OID_802_11_POWER_MODE, &arg, &len);

		if (rval)
			device_printf(sc->ndis_dev,
			    "get power mode failed: %d\n", rval);
		if (arg == NDIS_80211_POWERMODE_CAM)
			ic->ic_flags &= ~IEEE80211_F_PMGTON;
		else
			ic->ic_flags |= IEEE80211_F_PMGTON;
	}

	len = sizeof(config);
	rval = ndis_get_info(sc, OID_802_11_CONFIGURATION, &config, &len);   
	if (rval == 0) { 
		int chan;

		chan = ieee80211_mhz2ieee(config.nc_dsconfig / 1000, 0);
		if (chan < 0 || chan >= IEEE80211_CHAN_MAX) {
			if (ifp->if_flags & IFF_DEBUG)
				device_printf(sc->ndis_dev, "current channel "
				    "(%uMHz) out of bounds\n", 
				    config.nc_dsconfig / 1000);
			ic->ic_bss->ni_chan = &ic->ic_channels[1];
		} else
			ic->ic_bss->ni_chan = &ic->ic_channels[chan];
	} else
		device_printf(sc->ndis_dev, "couldn't retrieve "
		    "channel info: %d\n", rval);

/*
	len = sizeof(arg);
	rval = ndis_get_info(sc, OID_802_11_WEP_STATUS, &arg, &len);

	if (rval)
		device_printf (sc->ndis_dev,
		    "get wep status failed: %d\n", rval);

	if (arg == NDIS_80211_WEPSTAT_ENABLED)
		ic->ic_flags |= IEEE80211_F_WEPON;
	else
		ic->ic_flags &= ~IEEE80211_F_WEPON;
*/
	return;
}

static int
ndis_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct ndis_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			i, error = 0;

	/*NDIS_LOCK(sc);*/


	switch(command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->ndis_if_flags & IFF_PROMISC)) {
				sc->ndis_filter |=
				    NDIS_PACKET_TYPE_PROMISCUOUS;
				ndis_set_info(sc,
				    OID_GEN_CURRENT_PACKET_FILTER,
				    &sc->ndis_filter, &i);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->ndis_if_flags & IFF_PROMISC) {
				sc->ndis_filter &=
				    ~NDIS_PACKET_TYPE_PROMISCUOUS;
				ndis_set_info(sc,
				    OID_GEN_CURRENT_PACKET_FILTER,
				    &sc->ndis_filter, &i);
			} else
				ndis_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ndis_stop(sc);
		}
		sc->ndis_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ndis_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->ndis_80211) {
			error = ieee80211_ioctl(ifp, command, data);
			if (error == ENETRESET) {
				/*ndis_setstate_80211(sc);*/
				ndis_init(sc);
				error = 0;
			}
		} else
			error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	case SIOCGIFGENERIC:
	case SIOCSIFGENERIC:
		if (sc->ndis_80211 && ifp->if_flags & IFF_UP) {
			if (command == SIOCGIFGENERIC)
				error = ndis_wi_ioctl_get(ifp, command, data);
			else
				error = ndis_wi_ioctl_set(ifp, command, data);
		} else
			error = ENOTTY;
		if (error != ENOTTY)
			break;
	default:
		if (sc->ndis_80211) {
			error = ieee80211_ioctl(ifp, command, data);
			if (error == ENETRESET) {
				ndis_setstate_80211(sc);
				error = 0;
			}
		} else
			error = ether_ioctl(ifp, command, data);
		break;
	}

	/*NDIS_UNLOCK(sc);*/

	return(error);
}

static int
ndis_wi_ioctl_get(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct wi_req		wreq;
	struct ifreq		*ifr;
	struct ndis_softc	*sc;
	ndis_80211_bssid_list_ex *bl;
	ndis_wlan_bssid_ex	*wb;
	struct wi_apinfo	*api;
	int			error, i, j, len, maxaps;

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;
	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return (error);

	switch (wreq.wi_type) {
	case WI_RID_READ_APS:
		len = 0;
		error = ndis_set_info(sc, OID_802_11_BSSID_LIST_SCAN,
		    NULL, &len);
		if (error == 0)
			tsleep(&error, PPAUSE|PCATCH, "ssidscan", hz * 2);
		len = 0;
		error = ndis_get_info(sc, OID_802_11_BSSID_LIST, NULL, &len);
		if (error != ENOSPC)
			break;
		bl = malloc(len, M_DEVBUF, M_WAITOK);
		error = ndis_get_info(sc, OID_802_11_BSSID_LIST, bl, &len);
		if (error) {
			free(bl, M_DEVBUF);
			break;
		}
		maxaps = (2 * wreq.wi_len - sizeof(int)) / sizeof(*api);
		maxaps = MIN(maxaps, bl->nblx_items);
		wreq.wi_len = (maxaps * sizeof(*api) + sizeof(int)) / 2;
		*(int *)&wreq.wi_val = maxaps;
		api = (struct wi_apinfo *)&((int *)&wreq.wi_val)[1];
		wb = bl->nblx_bssid;
		while (maxaps--) {
			bzero(api, sizeof(*api));
			bcopy(&wb->nwbx_macaddr, &api->bssid,
			    sizeof(api->bssid));
			api->namelen = wb->nwbx_ssid.ns_ssidlen;
			bcopy(&wb->nwbx_ssid.ns_ssid, &api->name, api->namelen);
			if (wb->nwbx_privacy)
				api->capinfo |= IEEE80211_CAPINFO_PRIVACY;
			/* XXX Where can we get noise information? */
			api->signal = wb->nwbx_rssi + 149;	/* XXX */
			api->quality = api->signal;
			api->channel =
			    ieee80211_mhz2ieee(wb->nwbx_config.nc_dsconfig /
			    1000, 0);
			/* In "auto" infrastructure mode, this is useless. */
			if (wb->nwbx_netinfra == NDIS_80211_NET_INFRA_IBSS)
				api->capinfo |= IEEE80211_CAPINFO_IBSS;
			if (wb->nwbx_len > sizeof(ndis_wlan_bssid)) {
				j = sizeof(ndis_80211_rates_ex);
				/* handle other extended things */
			} else
				j = sizeof(ndis_80211_rates);
			for (i = api->rate = 0; i < j; i++)
				api->rate = MAX(api->rate, 5 *
				    (wb->nwbx_supportedrates[i] & 0x7f));
			api++;
			wb = (ndis_wlan_bssid_ex *)((char *)wb + wb->nwbx_len);
		}
		free(bl, M_DEVBUF);
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
ndis_wi_ioctl_set(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct wi_req		wreq;
	struct ifreq		*ifr;
	struct ndis_softc	*sc;
	uint32_t		foo;
	int			error, len;

	error = suser(curthread);
	if (error)
		return (error);

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;
	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return (error);

	switch (wreq.wi_type) {
	case WI_RID_SCAN_APS:
	case WI_RID_SCAN_REQ:			/* arguments ignored */
		len = sizeof(foo);
		foo = 0;
		error = ndis_set_info(sc, OID_802_11_BSSID_LIST_SCAN, &foo,
		    &len);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static void
ndis_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct ndis_softc		*sc;

	sc = ifp->if_softc;

	NDIS_LOCK(sc);
	ifp->if_oerrors++;
	device_printf(sc->ndis_dev, "watchdog timeout\n");

	ndis_reset(sc);

	taskqueue_enqueue(taskqueue_swi, &sc->ndis_starttask);

	NDIS_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
ndis_stop(sc)
	struct ndis_softc		*sc;
{
	struct ifnet		*ifp;

/*	NDIS_LOCK(sc);*/
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	sc->ndis_link = 0;
	untimeout(ndis_tick, sc, sc->ndis_stat_ch);

	ndis_halt_nic(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	/*NDIS_UNLOCK(sc);*/

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
ndis_shutdown(dev)
	device_t		dev;
{
	struct ndis_softc		*sc;

	sc = device_get_softc(dev);
	ndis_shutdown_nic(sc);

	return;
}
