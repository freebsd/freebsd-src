/*
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
 * $Id: if_ray.c,v 1.25 2000/05/07 15:00:06 dmlb Exp $
 *
 */

/*	$NetBSD: if_ray.c,v 1.12 2000/02/07 09:36:27 augustss Exp $	*/
/* 
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

/*
 *
 * Card configuration
 * ==================
 *
 * This card is unusual in that it uses both common and attribute
 * memory whilst working. The -stable versions of FreeBSD have a real
 * problem managing and setting up the correct memory maps. This
 * driver should reset the memory maps correctly under PAO and non-PAO
 * -stable systems. Work is in hand to fix these problems for -current.
 *
 * So, if you want to use this driver make sure that
 *	options RAY_NEED_CM_FIXUP
 *	options RAY_NEED_CM_REMAPPING
 * are in your kernel configuration file.
 *
 * The first fixes the brain deadness of pccardd (where it reads the
 * CIS for common memory, sets it all up and then throws it all away
 * assuming the card is an ed driver...). Note that this could be
 * dangerous (because it doesn't interact with pccardd) if you
 * use other memory mapped cards at the same time.
 *
 * The second option ensures that common memory is remapped whenever
 * we are going to access it (we can't just do it once, as something
 * like pccardd may have read the attribute memory and pccard.c
 * doesn't re-map the last active window - it remaps the last
 * non-active window...).
 *
 *
 * Ad-hoc and infra-structure modes
 * ================================
 * 
 * At present only the ad-hoc mode is being worked on.
 *
 * Apart from just writing the code for infrastructure mode I have a
 * few concerns about both the Linux and NetBSD drivers in this area.
 * They don't seem to differentiate between the MAC address of the AP
 * and the BSS_ID of the network. I presume this is handled when
 * joining a managed n/w and the network parameters are updated, but
 * I'm not sure. How does this interact with ARP? For mobility we want
 * to be able to move around without worrying about which AP we are
 * actually talking to - we should always talk to the BSS_ID.
 *
 * The Linux driver also seems to have the capability to act as an AP.
 * I wonder what facilities the "AP" can provide within a driver? We can
 * probably use the BRIDGE code to form an ESS but I don't think
 * power saving etc. is easy.
 *
 *
 * Packet translation/encapsulation
 * ================================
 * 
 * Currently we only support the Webgear encapsulation
 *	802.11	header <net/if_ieee80211.h>struct ieee80211_header
 *	802.3	header <net/ethernet.h>struct ether_header
 *	802.2	LLC header
 *	802.2	SNAP header
 *
 * We should support whatever packet types the following drivers have
 *   	if_wi.c		FreeBSD, RFC1042
 *	if_ray.c	NetBSD	Webgear, RFC1042
 *	rayctl.c	Linux Webgear, RFC1042
 * also whatever we can divine from the NDC Access points and Kanda's boxes.
 *
 * Most drivers appear to have a RFC1042 translation. The incoming packet is
 *	802.11	header <net/if_ieee80211.h>struct ieee80211_header
 *	802.2	LLC header
 *	802.2	SNAP header
 *
 * This is translated to
 *	802.3	header <net/ethernet.h>struct ether_header
 *	802.2	LLC header
 *	802.2	SNAP header
 *
 * Linux seems to look at the SNAP org_code and do some translations
 * for IPX and APPLEARP on that. This just may be how Linux does IPX
 * and NETATALK. Need to see how FreeBSD does these.
 *
 * Translation should be selected via if_media stuff or link types.
 */

/*
 * TODO
 *
 * _stop - mostly done
 *	would be nice to understand shutdown/or power save to prevent RX
 * _reset - done
 * 	just needs calling in the right places
 *	converted panics to resets - when tx packets are the wrong length
 *	may be needed in a couple of other places when I do more commands
 * havenet - mostly done
 *	i think i've got all the places to set it right, but not so sure
 *	we reset it in all the right places
 * _unload - done
 *	recreated most of stop but as card is unplugged don't try and
 *	access it to turn it off
 * TX bpf - done
 * RX bpf - done
 *	I would much prefer to have the complete 802.11 packet dropped to
 *	the bpf tap and then have a user land program parse the headers
 *	as needed. This way, tcpdump -w can be used to grab the raw data. If
 *	needed the 802.11 aware program can "translate" the .11 to ethernet
 *	for tcpdump -r
 * use std timeout code for download - done
 *	was mainly moving a call and removing a load of stuff in
 *	download_done as it duplicates check_ccs and ccs_done
 * promisoius - done
 * add the start_join_net - done
 *	i needed it anyway
 * remove startccs and startcmd - done
 *	as those were used for the NetBSD start timeout
 * multicast - done but UNTESTED
 *	I don't have the ability/facilty to test this
 * rxlevel - done
 *	stats reported via raycontrol
 * getparams ioctl - done
 *	reported via raycontrol
 * start_join_done needs a restart in download_done - done
 *	now use netbsd style start up
 * ioctls - done
 *	use raycontrol
 *	translation, BSS_ID, countrycode, changing mode
 * ifp->if_hdr length - done
 * rx level and antenna cache - done
 *	antenna not used yet
 * antenna tx side - done
 *	not tested!
 * shutdown - done
 *	the driver seems to do the right thing for plugging and unplugging
 *	cards
 * apm/resume - ignore
 *	apm+pccard is borken for 3.x - no one knows how to do it anymore
 * fix the XXX code in start_join_done - n/a
 *	i've removed this as the error handling should be consistent for
 *	all ECF commands and none of the other commands bother!
 * ray_update_params_done needs work - done
 *	as part of scheduler/promisc re-write
 * raycontrol to be firmware version aware - done
 *	also report and update parameters IOCTLs are version aware
 * make RAY_DPRINTFN RAY_DPRINTF - done
 * make all printfs RAY_PRINTF - done
 * faster TX routine - done
 *	see comments but OACTIVE is gone
 * __P to die - done
 *	the rest is ansi anyway
 *
 * ***stop/unload needs to drain comq
 * ***stop/unload checks in more routines
 * ***reset in ray_init_user?
 * ***IFF_RUNNING checks are they really needed?
 * ***PCATCH tsleeps and have something that will clean the runq
 * ***watchdog to catch screwed up removals?
 * ***check and rationalise CM mappings
 * ***should the desired nw parameters move into the comq entry to maintain
 *    correct sequencing?
 * why can't download use sc_promisc?
 * macro for gone and check is at head of all externally called routines
 * for ALLMULTI must go into PROMISC and filter unicast packets
 * mcast code resurrection
 * softc and ifp in variable definition block
 * UPDATE_PARAMS seems to return an interrupt - maybe the timeout
 *     is needed for wrong values?
 *	remember it must be serialised as it uses the HCF-ECF area
 * check all RECERRs and make sure that some are RAY_PRINTF not RAY_DPRINTF
 * havenet needs checking again
 * error handling of ECF command completions
 * probably function/macro to test unload at top of commands
 * proper setting of mib_hop_seq_len with country code for v4 firmware
 * _reset - check where needed
 * splimp or splnet?
 * more translations
 * tidy #includes - we cant need all of these
 * infrastructure mode
 *	needs handling of basic rate set
 *	all ray_sj, ray_assoc sequencues need a "nicer" solution as we
 *	need to consider WEP
 * acting as ap - should be able to get working from the manual
 * differeniate between parameters set in attach and init
 * spinning in ray_cmd_issue
 * make RAY_DEBUG a knob somehow - either sysctl or IFF_DEBUG
 * callout handles need rationalising. can probably remove sj_timerh
 * fragmentation when rx level drops?
 * proper handling of the basic rate set - see the manual
 */

#define XXX		0
#define XXX_CLEARCCS_IN_INIT	0
#define XXX_ASSOC	0
#define XXX_MCAST	0
#define XXX_RESET	0
#define XXX_IFQ_PEEK	0
#define RAY_DEBUG	(				\
 			/* RAY_DBG_RECERR	| */	\
 			/* RAY_DBG_SUBR		| */ 	\
			/* RAY_DBG_BOOTPARAM	| */	\
			/* RAY_DBG_STARTJOIN	| */	\
			/* RAY_DBG_CCS		| */	\
                        /* RAY_DBG_IOCTL	| */	\
                        /* RAY_DBG_MBUF		| */ 	\
                        /* RAY_DBG_RX		| */	\
                        /* RAY_DBG_CM		| */	\
                        /* RAY_DBG_COM		| */	\
			0				\
			)

/*
 * XXX build options - move to LINT
 */
#define RAY_NEED_CM_FIXUP	1	/* Needed until pccardd hacks for ed drivers are removed (pccardd forces 16bit memory and 0x4000 size) THIS IS A DANGEROUS THING TO USE IF YOU USE OTHER MEMORY MAPPED PCCARDS */

#define RAY_NEED_CM_REMAPPING	1	/* Needed until pccard maps more than one memory area */

#define RAY_COM_TIMEOUT		(hz/2)	/* Timeout for CCS commands */
#define RAY_RESET_TIMEOUT	(5*hz)	/* Timeout for resetting the card */
#define RAY_TX_TIMEOUT		(hz/2)	/* Timeout for rescheduling TX */

#define RAY_USE_CALLOUT_STOP	0	/* Set for kernels with callout_stop function - 3.3 and above */
/*
 * XXX build options - move to LINT
 */

#ifndef RAY_DEBUG
#define RAY_DEBUG 		0x0000
#endif /* RAY_DEBUG */

#include "ray.h"
#include "card.h"
#include "bpfilter.h"

#if NRAY > 0

#include <sys/param.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/callout.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/ucred.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif /* NBPFILTER */

#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/limits.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

#include <pccard/cardinfo.h>
#include <pccard/cis.h>
#include <pccard/driver.h>
#include <pccard/slot.h>

#include <i386/isa/if_ieee80211.h>
#include <i386/isa/if_rayreg.h>
#include <i386/isa/if_raymib.h>
#include <i386/isa/if_raydbg.h>
#include <i386/isa/if_rayvar.h>

/*
 * Prototyping
 */
static int	ray_attach		(struct isa_device *dev);
static int	ray_attr_read		(struct ray_softc *sc, off_t offset, u_int8_t *buf, int size);
static int	ray_attr_write		(struct ray_softc *sc, off_t offset, u_int8_t byte);
static int	ray_ccs_alloc		(struct ray_softc *sc, size_t *ccsp, u_int cmd, int timo);
static u_int8_t	ray_ccs_free 		(struct ray_softc *sc, size_t ccs);
static void	ray_com_ecf		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_com_ecf_done	(struct ray_softc *sc);
static void	ray_com_ecf_timo	(void *xsc);
#if RAY_DEBUG & RAY_DBG_COM
static struct ray_comq_entry *
		ray_com_malloc		(ray_comqfn_t function, int flags, char *mesg);
#else
static struct ray_comq_entry *
		ray_com_malloc		(ray_comqfn_t function, int flags);
#endif /* RAY_DEBUG & RAY_DBG_COM */
static void	ray_com_runq		(struct ray_softc *sc);
static void	ray_com_runq_add	(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_com_runq_arr	(struct ray_softc *sc, struct ray_comq_entry *com[], int ncom, char *wmesg);
static void	ray_com_runq_done	(struct ray_softc *sc);
static void	ray_detach		(struct pccard_devinfo *dev_p);
static void	ray_init_user		(void *xsc);
static void	ray_init_assoc		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_assoc_done	(struct ray_softc *sc, size_t ccs);
static void	ray_init_download	(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_download_done	(struct ray_softc *sc, size_t ccs);
static void	ray_init_sj		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_init_sj_done	(struct ray_softc *sc, size_t ccs);
static int	ray_intr		(struct pccard_devinfo *dev_p);
static void	ray_intr_ccs		(struct ray_softc *sc, u_int8_t cmd, size_t ccs);
static void	ray_intr_rcs		(struct ray_softc *sc, u_int8_t cmd, size_t ccs);
static void	ray_intr_updt_errcntrs	(struct ray_softc *sc);
static int	ray_ioctl		(struct ifnet *ifp, u_long command, caddr_t data);
static void	ray_mcast		(struct ray_softc *sc, struct ray_comq_entry *com); 
static void	ray_mcast_done		(struct ray_softc *sc, size_t ccs); 
static int	ray_mcast_user		(struct ray_softc *sc); 
static int	ray_probe		(struct pccard_devinfo *dev_p);
static void	ray_promisc		(struct ray_softc *sc, struct ray_comq_entry *com); 
static int	ray_promisc_user	(struct ray_softc *sc); 
static u_int8_t	ray_read_reg		(struct ray_softc *sc, off_t reg);
static void	ray_repparams		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_repparams_done	(struct ray_softc *sc, size_t ccs);
static int	ray_repparams_user	(struct ray_softc *sc, struct ray_param_req *pr);
static int	ray_repstats_user	(struct ray_softc *sc, struct ray_stats_req *sr);
static void	ray_reset		(struct ray_softc *sc);
static void	ray_reset_timo		(void *xsc);
static void	ray_rx			(struct ray_softc *sc, size_t rcs);
static void	ray_rx_update_cache	(struct ray_softc *sc, u_int8_t *src, u_int8_t siglev, u_int8_t antenna);
static void	ray_stop		(struct ray_softc *sc);
static void	ray_tx			(struct ifnet *ifp);
static void	ray_tx_done		(struct ray_softc *sc, size_t ccs);
static void	ray_tx_timo		(void *xsc);
static size_t	ray_tx_wrhdr		(struct ray_softc *sc, struct ether_header *eh, size_t bufp);
static void	ray_upparams		(struct ray_softc *sc, struct ray_comq_entry *com);
static void	ray_upparams_done	(struct ray_softc *sc, size_t ccs);
static int	ray_upparams_user	(struct ray_softc *sc, struct ray_param_req *pr);
static void	ray_watchdog		(struct ifnet *ifp);
static u_int8_t ray_tx_best_antenna	(struct ray_softc *sc, u_int8_t *dst);

#if RAY_DEBUG & RAY_DBG_COM
static void	ray_com_ecf_check	(struct ray_softc *sc, size_t ccs, char *mesg);
#endif /* RAY_DEBUG & RAY_DBG_COM */
#if RAY_DEBUG & RAY_DBG_MBUF
static void	ray_dump_mbuf		(struct ray_softc *sc, struct mbuf *m, char *s);
#endif /* RAY_DEBUG & RAY_DBG_MBUF */
#if (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP)
static void	ray_attr_getmap		(struct ray_softc *sc);
static void	ray_attr_mapcm		(struct ray_softc *sc);
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

/*
 * PC-Card (PCMCIA) driver definition
 */
PCCARD_MODULE(ray, ray_probe, ray_detach, ray_intr, 0, net_imask);

/*
 * Probe for the card by checking its startup results.
 *
 * Fixup any bugs/quirks for different firmware.
 */
static int
ray_probe(struct pccard_devinfo *dev_p)
{
	struct ray_softc *sc;
	int doRemap;

	if (dev_p->isahd.id_unit >= NRAY)
		return (ENODEV);

	sc = &ray_softc[dev_p->isahd.id_unit];
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

#if (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP)
	sc->slotnum = dev_p->slt->slotnum;
	ray_attr_getmap(sc);
	RAY_DPRINTF(sc, RAY_DBG_RECERR,
	    "Memory window flags 0x%02x, start %p, "
	    "size 0x%x, card address 0x%lx",
	    sc->md.flags, sc->md.start, sc->md.size, sc->md.card);
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

#if RAY_NEED_CM_FIXUP
	doRemap = 0;
	if (sc->md.start == 0x0) {
		RAY_PRINTF(sc, "pccardd did not map CM - giving up");
		return (ENXIO);
	}
	if (sc->md.flags != MDF_ACTIVE) {
		RAY_PRINTF(sc, "fixing up CM flags from 0x%x to 0x40",
		    sc->md.flags);
		doRemap = 1;
		sc->md.flags = MDF_ACTIVE;
	}
	if (sc->md.size != 0xc000) {
		RAY_PRINTF(sc, "fixing up CM size from 0x%x to 0xc000",
		    sc->md.size);
		doRemap = 1;
		sc->md.size = 0xc000;
		dev_p->isahd.id_msize = sc->md.size;
	}
	if (sc->md.card != 0) {
		RAY_PRINTF(sc, "fixing up CM card address from 0x%lx to 0x0",
		    sc->md.card);
		doRemap = 1;
		sc->md.card = 0;
	}
	if (doRemap)
		ray_attr_mapcm(sc);
#endif /* RAY_NEED_CM_FIXUP */

	sc->gone = 0;
	sc->unit = dev_p->isahd.id_unit;
	sc->maddr = dev_p->isahd.id_maddr;
	sc->flags = dev_p->isahd.id_flags;

	printf("ray%d: <Raylink/IEEE 802.11>"
	    "maddr %p msize 0x%x irq %d flags 0x%x on isa (PC-Card slot %d)\n",
	    sc->unit,
	    sc->maddr,
	    dev_p->isahd.id_msize,
	    ffs(dev_p->isahd.id_irq) - 1,
	    sc->flags,
	    sc->slotnum);

	if (ray_attach(&dev_p->isahd))
		return (ENXIO);

	return (0);
}

/*
 * PCCard unload.
 */
static void
ray_detach(struct pccard_devinfo *dev_p)
{
	struct ray_softc *sc;
	struct ifnet *ifp;

	sc = &ray_softc[dev_p->isahd.id_unit];
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	if (sc->gone) {
	    RAY_PRINTF(sc, "unloaded");
	    return;
	}

	/*
	 * Clear out timers and sort out driver state
	 *
	 * We use callout_stop to unconditionally kill the ccs and general
	 * timers as they are used with multiple arguments.
	 */
#if RAY_USE_CALLOUT_STOP
	callout_stop(sc->com_timerh);
	callout_stop(sc->reset_timerh);
#else
	untimeout(ray_com_ecf_timo, sc, sc->com_timerh);
	untimeout(ray_reset_timo, sc, sc->reset_timerh);
#endif /* RAY_USE_CALLOUT_STOP */
	untimeout(ray_tx_timo, sc, sc->tx_timerh);
	sc->sc_havenet = 0;

	/*
	 * Mark as not running
	 */
	ifp = &sc->arpcom.ac_if;
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Cleardown interface
	 */
	if_down(ifp); /* XXX should be if_detach for -current */

	/*
	 * Mark card as gone
	 */
	sc->gone = 1;
	RAY_PRINTF(sc, "unloading complete");

	return;
}

/*
 * PCCard attach.
 */
static int
ray_attach(struct isa_device *dev_p)
{
	struct ray_softc *sc;
	struct ray_ecf_startup_v5 *ep;
	struct ifnet *ifp;
	size_t ccs;
	char ifname[IFNAMSIZ];
	int i;

	sc = &ray_softc[dev_p->id_unit];
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	if (sc->gone) {
		RAY_PRINTF(sc, "unloaded");
		return (1);
	}

	/*
	 * Read startup results, check the card is okay and work out what
	 * version we are using.
	 */
	ep = &sc->sc_ecf_startup;
	ray_read_region(sc, RAY_ECF_TO_HOST_BASE, ep,
	    sizeof(sc->sc_ecf_startup));
	if (ep->e_status != RAY_ECFS_CARD_OK) {
		RAY_PRINTF(sc, "card failed self test 0x%b\n", 
		    ep->e_status, RAY_ECFS_PRINTFB);
		return (1);
	}
	if (sc->sc_version != RAY_ECFS_BUILD_4 &&
	    sc->sc_version != RAY_ECFS_BUILD_5) {
		RAY_PRINTF(sc, "unsupported firmware version 0x%0x\n", 
		    ep->e_fw_build_string);
		return (1);
	}

	if (bootverbose || (RAY_DEBUG & RAY_DBG_BOOTPARAM)) {
		RAY_PRINTF(sc, "start up results");
		if (sc->sc_version == RAY_ECFS_BUILD_4)
			printf("  Firmware version 4\n");
		else
			printf("  Firmware version 5\n");
		printf("  Status 0x%b\n", ep->e_status, RAY_ECFS_PRINTFB);
		printf("  Ether address %6D\n", ep->e_station_addr, ":");
		if (sc->sc_version == RAY_ECFS_BUILD_4) {
			printf("  Program checksum %0x\n", ep->e_resv0);
			printf("  CIS checksum %0x\n", ep->e_rates[0]);
		} else {
			printf("  (reserved word) %0x\n", ep->e_resv0);
			printf("  Supported rates %8D\n", ep->e_rates, ":");
		}
		printf("  Japan call sign %12D\n", ep->e_japan_callsign, ":");
		if (sc->sc_version == RAY_ECFS_BUILD_5) {
			printf("  Program checksum %0x\n", ep->e_prg_cksum);
			printf("  CIS checksum %0x\n", ep->e_cis_cksum);
			printf("  Firmware version %0x\n",
			    ep->e_fw_build_string);
			printf("  Firmware revision %0x\n", ep->e_fw_build);
			printf("  (reserved word) %0x\n", ep->e_fw_resv);
			printf("  ASIC version %0x\n", ep->e_asic_version);
			printf("  TIB size %0x\n", ep->e_tibsize);
		}
	}

	/*
	 * Fixup tib size to be correct - on build 4 it is garbage
	 */
	if (sc->sc_version == RAY_ECFS_BUILD_4 && sc->sc_tibsize == 0x55)
		sc->sc_tibsize = sizeof(struct ray_tx_tib);

#if XXX_CLEARCCS_IN_INIT > 0
#else
	/* Set all ccs to be free */
	bzero(sc->sc_ccsinuse, sizeof(sc->sc_ccsinuse));
	ccs = RAY_CCS_ADDRESS(0);
	for (i = 0; i < RAY_CCS_LAST; ccs += RAY_CCS_SIZE, i++)
		RAY_CCS_FREE(sc, ccs);
#endif /* XXX_CLEARCCS_IN_INIT */

	/* Reset any pending interrupts */
	RAY_HCS_CLEAR_INTR(sc);

	/*
	 * Set the parameters that will survive stop/init
	 *
	 * Do not update these in ray_init's parameter setup
	 */
#if XXX
	see the ray_init section for stuff to move
#endif
	bzero(&sc->sc_d, sizeof(struct ray_nw_param));
	bzero(&sc->sc_c, sizeof(struct ray_nw_param));

	/*
	 * Initialise the network interface structure
	 */
	bcopy((char *)&ep->e_station_addr,
	    (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);
	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_name = "ray";
	ifp->if_unit = sc->unit;
	ifp->if_timer = 0;
#if XXX_MCAST
	ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
#else
	ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX);
#endif /* XXX_MCAST */
	ifp->if_hdrlen = sizeof(struct ieee80211_header) + 
	    sizeof(struct ether_header);
	ifp->if_baudrate = 1000000; /* Is this baud or bps ;-) */
	ifp->if_output = ether_output;
	ifp->if_start = ray_tx;
	ifp->if_ioctl = ray_ioctl;
	ifp->if_watchdog = ray_watchdog;
	ifp->if_init = ray_init_user;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	/*
	 * If this logical interface has already been attached,
	 * don't attach it again or chaos will ensue.
	 */
	sprintf(ifname, "ray%d", sc->unit);

	if (ifunit(ifname) == NULL) {
		callout_handle_init(&sc->com_timerh);
		callout_handle_init(&sc->reset_timerh);
		callout_handle_init(&sc->tx_timerh);
		TAILQ_INIT(&sc->sc_comq);
		if_attach(ifp);
		ether_ifattach(ifp);
#if NBPFILTER > 0
		bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif /* NBFFILTER */

#if XXX
		at_shutdown(ray_shutdown, sc, SHUTDOWN_POST_SYNC);
#endif /* XXX */
	}

	return (0);
}

/*
 * Network ioctl request.
 */
static int
ray_ioctl(register struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ray_softc *sc;
	struct ray_param_req pr;
	struct ray_stats_req sr;
	struct ifreq *ifr;
	int s, error, error2;

	sc = ifp->if_softc;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_IOCTL, "");
	RAY_MAP_CM(sc);

	if (sc->gone) {
	    RAY_PRINTF(sc, "unloaded");
	    ifp->if_flags &= ~IFF_RUNNING;
	    return (ENXIO);
	}

	ifr = (struct ifreq *)data;
	error = 0;
	error2 = 0;

	s = splimp();

	switch (command) {

	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "SIFADDR/GIFADDR/SIFMTU");
		error = ether_ioctl(ifp, command, data);
		break;

	case SIOCSIFFLAGS:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "SIFFLAGS");
		/*
		 * If the interface is marked up and stopped, then start
		 * it. If it is marked down and running, then stop it.
		 */
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				ray_init_user(sc);
			else
			    if (sc->sc_promisc !=
				!!(ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI)))
				    ray_promisc_user(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ray_stop(sc);
		}
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
		error = EINVAL;

	}

	splx(s);

	return (error);
}

/*
 * User land entry to network initialisation.
 *
 * An ioctl calls ray_init_user.
 * 
 * ray_init_user does a bit of house keeping before calling ray_download
 *
 * ray_init_download fills the startup parameter structure out and
 * sends it to the card.
 *
 * ray_init_sj tells the card to try and find an existing network or
 * start a new one.
 *
 * ray_init_sj_done checks a few parameters and we are ready to process packets
 *
 * the promiscuous and multi-cast modes are then set
 */
static void
ray_init_user(void *xsc)
{
	struct ray_softc *sc = (struct ray_softc *)xsc;
	struct ray_comq_entry *com[5];
	struct ifnet *ifp;
	int i, ncom;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

	if (sc->gone) {
		RAY_PRINTF(sc, "unloaded");
		return;
	}

	ifp = &sc->arpcom.ac_if;
	if ((ifp->if_flags & IFF_RUNNING))
		ray_stop(sc);

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
	 */
#if XXX
    see the ray_attach section for stuff to move
#endif
	sc->sc_d.np_upd_param = 0;
	bzero(sc->sc_d.np_bss_id, ETHER_ADDR_LEN);
	sc->sc_d.np_inited = 0;
	sc->sc_d.np_def_txrate = RAY_MIB_BASIC_RATE_SET_DEFAULT;
	sc->sc_d.np_encrypt = 0;

	sc->sc_d.np_ap_status = RAY_MIB_AP_STATUS_DEFAULT;
	sc->sc_d.np_net_type = RAY_MIB_NET_TYPE_DEFAULT;
	bzero(sc->sc_d.np_ssid, IEEE80211_NWID_LEN);
	strncpy(sc->sc_d.np_ssid, RAY_MIB_SSID_DEFAULT, IEEE80211_NWID_LEN);
	sc->sc_d.np_priv_start = RAY_MIB_PRIVACY_MUST_START_DEFAULT;
	sc->sc_d.np_priv_join = RAY_MIB_PRIVACY_CAN_JOIN_DEFAULT;
	sc->sc_promisc = !!(ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI));

	sc->sc_havenet = 0;
	sc->translation = SC_TRANSLATE_WEBGEAR;

#if XXX_CLEARCCS_IN_INIT > 0
	/* Set all ccs to be free */
	bzero(sc->sc_ccsinuse, sizeof(sc->sc_ccsinuse));
	ccs = RAY_CCS_ADDRESS(0);
	for (i = 0; i < RAY_CCS_LAST; ccs += RAY_CCS_SIZE, i++)
		RAY_CCS_FREE(sc, ccs);

	/* Clear any pending interrupts */
	RAY_HCS_CLEAR_INTR(sc);
#endif /* XXX_CLEARCCS_IN_INIT */

	/*
	 * We are now up and running. We are busy until network is joined.
	 */
	ifp->if_flags |= IFF_RUNNING | IFF_OACTIVE;

	/*
	 * Create the following runq entries:
	 *
	 *		download	- download the network to the card
	 *		sj		- find or start a BSS
	 *		assoc		- associate with a ESSID if needed
	 *		promisc		- force promiscuous mode update
	 *		mcast		- force multicast list
	 */
	ncom = 0;
	com[ncom++] = RAY_COM_MALLOC(ray_init_download, 0);
	com[ncom++] = RAY_COM_MALLOC(ray_init_sj, 0);
#if XXX_ASSOC
	if (sc->sc_d.np_net_type == RAY_MIB_NET_TYPE_INFRA)
		com[ncom++] = RAY_COM_MALLOC(ray_init_assoc, 0);
#endif /* XXX_ASSOC */
	com[ncom++] = RAY_COM_MALLOC(ray_promisc, 0);
#if XXX_MCAST
	com[ncom++] = RAY_COM_MALLOC(ray_mcast, 0);
#endif /* XXX_MCAST */

	ray_com_runq_arr(sc, com, ncom, "rayinit");

	/* XXX no error processing from anything yet! */

	for (i = 0; i < ncom; i++)
		FREE(com[i], M_RAYCOM);
}

/*
 * Runq entry for downloading start up structures to card
 */
static void
ray_init_download(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ray_mib_4 ray_mib_4_default;
	struct ray_mib_5 ray_mib_5_default;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

#define MIB4(m)		ray_mib_4_default.##m
#define MIB5(m)		ray_mib_5_default.##m
#define	PUT2(p, v) 	\
    do { (p)[0] = ((v >> 8) & 0xff); (p)[1] = (v & 0xff); } while(0)

	 /*
	  * Firmware version 4 defaults - see if_raymib.h for details
	  */
	 MIB4(mib_net_type)		= sc->sc_d.np_net_type;
	 MIB4(mib_ap_status)		= sc->sc_d.np_ap_status;
	 bcopy(sc->sc_d.np_ssid, MIB4(mib_ssid), IEEE80211_NWID_LEN);
	 MIB4(mib_scan_mode)		= RAY_MIB_SCAN_MODE_DEFAULT;
	 MIB4(mib_apm_mode)		= RAY_MIB_APM_MODE_DEFAULT;
	 bcopy(sc->sc_station_addr, MIB4(mib_mac_addr), ETHER_ADDR_LEN);
    PUT2(MIB4(mib_frag_thresh), 	  RAY_MIB_FRAG_THRESH_DEFAULT);
    PUT2(MIB4(mib_dwell_time),		  RAY_MIB_DWELL_TIME_V4);
    PUT2(MIB4(mib_beacon_period),	  RAY_MIB_BEACON_PERIOD_V4);
	 MIB4(mib_dtim_interval)	= RAY_MIB_DTIM_INTERVAL_DEFAULT;
	 MIB4(mib_max_retry)		= RAY_MIB_MAX_RETRY_DEFAULT;
	 MIB4(mib_ack_timo)		= RAY_MIB_ACK_TIMO_DEFAULT;
	 MIB4(mib_sifs)			= RAY_MIB_SIFS_DEFAULT;
	 MIB4(mib_difs)			= RAY_MIB_DIFS_DEFAULT;
	 MIB4(mib_pifs)			= RAY_MIB_PIFS_V4;
    PUT2(MIB4(mib_rts_thresh),		  RAY_MIB_RTS_THRESH_DEFAULT);
    PUT2(MIB4(mib_scan_dwell),		  RAY_MIB_SCAN_DWELL_V4);
    PUT2(MIB4(mib_scan_max_dwell),	  RAY_MIB_SCAN_MAX_DWELL_V4);
	 MIB4(mib_assoc_timo)		= RAY_MIB_ASSOC_TIMO_DEFAULT;
	 MIB4(mib_adhoc_scan_cycle)	= RAY_MIB_ADHOC_SCAN_CYCLE_DEFAULT;
	 MIB4(mib_infra_scan_cycle)	= RAY_MIB_INFRA_SCAN_CYCLE_DEFAULT;
	 MIB4(mib_infra_super_scan_cycle)
	 				= RAY_MIB_INFRA_SUPER_SCAN_CYCLE_DEFAULT;
	 MIB4(mib_promisc)		= RAY_MIB_PROMISC_DEFAULT;
    PUT2(MIB4(mib_uniq_word),		  RAY_MIB_UNIQ_WORD_DEFAULT);
	 MIB4(mib_slot_time)		= RAY_MIB_SLOT_TIME_V4;
	 MIB4(mib_roam_low_snr_thresh)	= RAY_MIB_ROAM_LOW_SNR_THRESH_DEFAULT;
	 MIB4(mib_low_snr_count)	= RAY_MIB_LOW_SNR_COUNT_DEFAULT;
	 MIB4(mib_infra_missed_beacon_count)
	 				= RAY_MIB_INFRA_MISSED_BEACON_COUNT_DEFAULT;
	 MIB4(mib_adhoc_missed_beacon_count)	
	 				= RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DEFAULT;
	 MIB4(mib_country_code)		= RAY_MIB_COUNTRY_CODE_DEFAULT;
	 MIB4(mib_hop_seq)		= RAY_MIB_HOP_SEQ_DEFAULT;
	 MIB4(mib_hop_seq_len)		= RAY_MIB_HOP_SEQ_LEN_V4;
	 MIB4(mib_cw_max)		= RAY_MIB_CW_MAX_V4;
	 MIB4(mib_cw_min)		= RAY_MIB_CW_MIN_V4;
	 MIB4(mib_noise_filter_gain)	= RAY_MIB_NOISE_FILTER_GAIN_DEFAULT;
	 MIB4(mib_noise_limit_offset)	= RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT;
	 MIB4(mib_rssi_thresh_offset)	= RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT;
	 MIB4(mib_busy_thresh_offset)	= RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT;
	 MIB4(mib_sync_thresh)		= RAY_MIB_SYNC_THRESH_DEFAULT;
	 MIB4(mib_test_mode)		= RAY_MIB_TEST_MODE_DEFAULT;
	 MIB4(mib_test_min_chan)	= RAY_MIB_TEST_MIN_CHAN_DEFAULT;
	 MIB4(mib_test_max_chan)	= RAY_MIB_TEST_MAX_CHAN_DEFAULT;

	 /*
	  * Firmware version 5 defaults - see if_raymib.h for details
	  */
	 MIB5(mib_net_type)		= sc->sc_d.np_net_type;
	 MIB4(mib_ap_status)		= sc->sc_d.np_ap_status;
	 bcopy(sc->sc_d.np_ssid, MIB5(mib_ssid), IEEE80211_NWID_LEN);
	 MIB5(mib_scan_mode)		= RAY_MIB_SCAN_MODE_DEFAULT;
	 MIB5(mib_apm_mode)		= RAY_MIB_APM_MODE_DEFAULT;
	 bcopy(sc->sc_station_addr, MIB5(mib_mac_addr), ETHER_ADDR_LEN);
    PUT2(MIB5(mib_frag_thresh), 	  RAY_MIB_FRAG_THRESH_DEFAULT);
    PUT2(MIB5(mib_dwell_time),		  RAY_MIB_DWELL_TIME_V5);
    PUT2(MIB5(mib_beacon_period),	  RAY_MIB_BEACON_PERIOD_V5);
	 MIB5(mib_dtim_interval)	= RAY_MIB_DTIM_INTERVAL_DEFAULT;
	 MIB5(mib_max_retry)		= RAY_MIB_MAX_RETRY_DEFAULT;
	 MIB5(mib_ack_timo)		= RAY_MIB_ACK_TIMO_DEFAULT;
	 MIB5(mib_sifs)			= RAY_MIB_SIFS_DEFAULT;
	 MIB5(mib_difs)			= RAY_MIB_DIFS_DEFAULT;
	 MIB5(mib_pifs)			= RAY_MIB_PIFS_V5;
    PUT2(MIB5(mib_rts_thresh),		  RAY_MIB_RTS_THRESH_DEFAULT);
    PUT2(MIB5(mib_scan_dwell),		  RAY_MIB_SCAN_DWELL_V5);
    PUT2(MIB5(mib_scan_max_dwell),	  RAY_MIB_SCAN_MAX_DWELL_V5);
	 MIB5(mib_assoc_timo)		= RAY_MIB_ASSOC_TIMO_DEFAULT;
	 MIB5(mib_adhoc_scan_cycle)	= RAY_MIB_ADHOC_SCAN_CYCLE_DEFAULT;
	 MIB5(mib_infra_scan_cycle)	= RAY_MIB_INFRA_SCAN_CYCLE_DEFAULT;
	 MIB5(mib_infra_super_scan_cycle)
	 				= RAY_MIB_INFRA_SUPER_SCAN_CYCLE_DEFAULT;
	 MIB5(mib_promisc)		= RAY_MIB_PROMISC_DEFAULT;
    PUT2(MIB5(mib_uniq_word),		  RAY_MIB_UNIQ_WORD_DEFAULT);
	 MIB5(mib_slot_time)		= RAY_MIB_SLOT_TIME_V5;
	 MIB5(mib_roam_low_snr_thresh)	= RAY_MIB_ROAM_LOW_SNR_THRESH_DEFAULT;
	 MIB5(mib_low_snr_count)	= RAY_MIB_LOW_SNR_COUNT_DEFAULT;
	 MIB5(mib_infra_missed_beacon_count)
	 				= RAY_MIB_INFRA_MISSED_BEACON_COUNT_DEFAULT;
	 MIB5(mib_adhoc_missed_beacon_count)
	 				= RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DEFAULT;
	 MIB5(mib_country_code)		= RAY_MIB_COUNTRY_CODE_DEFAULT;
	 MIB5(mib_hop_seq)		= RAY_MIB_HOP_SEQ_DEFAULT;
	 MIB5(mib_hop_seq_len)		= RAY_MIB_HOP_SEQ_LEN_V5;
    PUT2(MIB5(mib_cw_max),		  RAY_MIB_CW_MAX_V5);
    PUT2(MIB5(mib_cw_min),		  RAY_MIB_CW_MIN_V5);
	 MIB5(mib_noise_filter_gain)	= RAY_MIB_NOISE_FILTER_GAIN_DEFAULT;
	 MIB5(mib_noise_limit_offset)	= RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT;
	 MIB5(mib_rssi_thresh_offset)	= RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT;
	 MIB5(mib_busy_thresh_offset)	= RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT;
	 MIB5(mib_sync_thresh)		= RAY_MIB_SYNC_THRESH_DEFAULT;
	 MIB5(mib_test_mode)		= RAY_MIB_TEST_MODE_DEFAULT;
	 MIB5(mib_test_min_chan)	= RAY_MIB_TEST_MIN_CHAN_DEFAULT;
	 MIB5(mib_test_max_chan)	= RAY_MIB_TEST_MAX_CHAN_DEFAULT;
	 MIB5(mib_allow_probe_resp)	= RAY_MIB_ALLOW_PROBE_RESP_DEFAULT;
	 MIB5(mib_privacy_must_start)	= sc->sc_d.np_priv_start;
	 MIB5(mib_privacy_can_join)	= sc->sc_d.np_priv_join;
	 MIB5(mib_basic_rate_set[0])	= sc->sc_d.np_def_txrate;

	if (sc->sc_version == RAY_ECFS_BUILD_4)
		ray_write_region(sc, RAY_HOST_TO_ECF_BASE,
		    &ray_mib_4_default, sizeof(ray_mib_4_default));
	else
		ray_write_region(sc, RAY_HOST_TO_ECF_BASE,
		    &ray_mib_5_default, sizeof(ray_mib_5_default));

	(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_DOWNLOAD_PARAMS, 0);
	ray_com_ecf(sc, com);
}

/*
 * Download completion routine
 */
static void
ray_init_download_done(struct ray_softc *sc, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_COM_CHECK(sc, ccs);

	/* 
	 * Fake the current network parameter settings so start_join_net
	 * will not bother updating them to the card (we would need to
	 * zero these anyway, so we might as well copy).
	 */
	sc->sc_c.np_net_type = sc->sc_d.np_net_type;
	bcopy(sc->sc_d.np_ssid, sc->sc_c.np_ssid, IEEE80211_NWID_LEN);
	    
	ray_com_ecf_done(sc);
}

/*
 * Runq entry to starting or joining a network
 */
static void
ray_init_sj(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ray_net_params np;
	int update;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

	sc->sc_havenet = 0;
	if (sc->sc_d.np_net_type == RAY_MIB_NET_TYPE_ADHOC)
		(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_START_NET, 0);
	else
		(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_JOIN_NET, 0);

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
		ray_write_region(sc, RAY_HOST_TO_ECF_BASE, &np, sizeof(np));
		SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_net, c_upd_param, 1);
	} else
		SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_net, c_upd_param, 0);

	ray_com_ecf(sc, com);
}

/*
 * Complete start command or intermediate step in assoc command
 */
static void
ray_init_sj_done(struct ray_softc *sc, size_t ccs)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);
	RAY_COM_CHECK(sc, ccs);

	/*
	 * Read back network parameters that the ECF sets
	 */
	ray_read_region(sc, ccs, &sc->sc_c.p_1, sizeof(struct ray_cmd_net));

	/* adjust values for buggy build 4 */
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
		ray_read_region(sc, RAY_HOST_TO_ECF_BASE,
		    &sc->sc_c.p_2, sizeof(struct ray_net_params));
	}

	/*
	 * Hurrah! The network is now active.
	 *
	 * Clearing IFF_OACTIVE will ensure that the system will queue
	 * packets. Just before we return from the interrupt context
	 * we check to see if packets have been queued.
	 */
	if (SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd) == RAY_CMD_START_NET) {
		sc->sc_havenet = 1;
		ifp = &sc->arpcom.ac_if;
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	ray_com_ecf_done(sc);
}

/*
 * Runq entry to starting an association with an access point
 */
static void
ray_init_assoc(struct ray_softc *sc, struct ray_comq_entry *com)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

	(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_START_ASSOC, 0);
	ray_com_ecf(sc, com);
}

/*
 * Complete association
 */
static void
ray_init_assoc_done(struct ray_softc *sc, size_t ccs)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_COM_CHECK(sc, ccs);
	RAY_MAP_CM(sc);

	/*
	 * Hurrah! The network is now active.
	 *
	 * Clearing IFF_OACTIVE will ensure that the system will queue
	 * packets. Just before we return from the interrupt context
	 * we check to see if packets have been queued.
	 */
	ifp = &sc->arpcom.ac_if;
	sc->sc_havenet = 1;
	ifp->if_flags &= ~IFF_OACTIVE;

	ray_com_ecf_done(sc);
}

/*
 * Network stop.
 *
 * Assumes that a ray_init is used to restart the card.
 *
 */
static void
ray_stop(struct ray_softc *sc)
{
	struct ifnet *ifp;
	int s;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	if (sc->gone) {
		RAY_PRINTF(sc, "unloaded");
		return;
	}

	ifp = &sc->arpcom.ac_if;

	/*
	 * Clear out timers and sort out driver state
	 */
	RAY_DPRINTF(sc, RAY_DBG_STOP, "HCS_intr %d RCSI 0x%0x\n", 
	    RAY_HCS_INTR(sc), SRAM_READ_1(sc, RAY_SCB_RCSI));
	RAY_DPRINTF(sc, RAY_DBG_STOP, "ECF ready %d\n", RAY_ECF_READY(sc));

#if RAY_USE_CALLOUT_STOP
	callout_stop(sc->com_timerh);
	callout_stop(sc->reset_timerh);
#else
	untimeout(ray_com_ecf_timo, sc, sc->com_timerh);
	untimeout(ray_reset_timo, sc, sc->reset_timerh);
#endif /* RAY_USE_CALLOUT_STOP */
	untimeout(ray_tx_timo, sc, sc->tx_timerh);
	sc->sc_havenet = 0;
	sc->sc_rxoverflow = 0;
	sc->sc_rxcksum = 0;
	sc->sc_rxhcksum = 0;
	sc->sc_rxnoise = 0;

	/*
	 * Inhibit card - if we can't prevent reception then do not worry;
	 * stopping a NIC only guarantees no TX.
	 */
	s = splimp();
	/* XXX what does the SHUTDOWN command do? Or power saving in COR */
	splx(s);

	/*
	 * Mark as not running
	 */
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
}

/*
 * Reset the card
 *
 * I'm using the soft reset command in the COR register. I'm not sure
 * if the sequence is right but it does seem to do the right thing. A
 * nano second after reset is written the flashing light goes out, and
 * a few seconds after the default is written the main card light goes
 * out. We wait a while and then re-init the card.
 */
static void
ray_reset(struct ray_softc *sc)
{
#if XXX_RESET
	struct ifnet *ifp;
#endif /* XXX_RESET */

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

#if XXX_RESET
	ifp = &sc->arpcom.ac_if;
	if (ifp->if_flags & IFF_RUNNING)
		ray_stop(sc);

	RAY_PRINTF(sc, "resetting ECF");
	ray_attr_write((sc), RAY_COR, RAY_COR_RESET);
	ray_attr_write((sc), RAY_COR, RAY_COR_DEFAULT);
	sc->reset_timerh = timeout(ray_reset_timo, sc, RAY_RESET_TIMEOUT);
#else
	RAY_PRINTF(sc, "skip reset card");
#endif /* XXX_RESET */
}

/*
 * Finishing resetting and restarting the card
 */
static void
ray_reset_timo(void *xsc)
{
	struct ray_softc *sc = (struct ray_softc *)xsc;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	if (!RAY_ECF_READY(sc)) {
	    RAY_DPRINTF(sc, RAY_DBG_RECERR, "ECF busy, re-scheduling self");
	    sc->reset_timerh = timeout(ray_reset_timo, sc, RAY_RESET_TIMEOUT);
	    return;
	}

	RAY_HCS_CLEAR_INTR(sc);
	RAY_PRINTF(sc, "XXX need to restart ECF but not in sleepable context");
	RAY_PRINTF(sc, "XXX the user routines must restart as required");
}

static void
ray_watchdog(struct ifnet *ifp)
{
	struct ray_softc *sc;

	sc = ifp->if_softc;
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	if (sc->gone) {
		RAY_PRINTF(sc, "unloaded");
		return;
	}

	RAY_PRINTF(sc, "watchdog timeout");

/* XXX may need to have remedial action here
   for example
   	ray_reset
	    ray_stop
	    ...
	    ray_init

    do we only use on TX?
    	if so then we should clear OACTIVE etc.

*/
}

/*
 * Transmit packet handling
 */

/*
 * Start sending a packet.
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
 * wireless link on a P75. Earlier versions of this used to set
 * OACTIVE to add an extra layer of locking. It isn't really needed.
 *
 * Removing the OACTIVE gives much better performance. Here we
 * have this driver on a Libretto, the old driver (OACTIVE)
 * on a K6-233 and the Windows driver on a P100. FTPing 2048k
 * of zeros.
 *
 * Nonname box old+FreeBSD-3.4 (K6-233MHz) to
 *   Libretto 50CT new+FreeBSD-3.1 (75MHz Pentium)	110.77kB/s
 *   AST J30 Windows 95A (100MHz Pentium) 		109.40kB/s
 *
 * AST J30 Windows 95A (100MHz Pentium) to
 *   Libretto 50CT new+FreeBSD-3.1 (75MHz Pentium)	167.37kB/s
 *   Nonname box FreeBSD-3.4 (233MHz AMD K6)		161.82kB/s
 *
 * Libretto 50CT new+FreeBSD-3.1 (75MHz Pentium) to
 *   AST J30 Windows 95A (100MHz Pentium) 		167.37kB/s
 *   Nonname box FreeBSD-3.4 (233MHz AMD K6)		161.38kB/s
 *
 * Given that 160kB/s is saturating the 2Mb/s wireless link we
 * are about there.
 *
 * There is a little test in the code to see how many packets
 * could be chained together. For the FTP test this rarely showed
 * any and when it did, only two packets were on the queue.
 *
 * So, in short I'm happy that the added complexity of chaining TX
 * packets together isn't worth it for my machines.
 *
 * Flow is
 *		get a ccs
 *		build the packet
 *		interrupt the card to send the packet
 *		return
 *
 *		wait for interrupt telling us the packet has been sent
 *		get called by the interrupt routine if any packets left
 */
static void
ray_tx(struct ifnet *ifp)
{
	struct ray_softc *sc;
	struct mbuf *m0, *m;
	struct ether_header *eh;
	size_t ccs, bufp;
	int i, pktlen, len;
	u_int8_t status;

	sc = ifp->if_softc;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	/*
	 * Some simple checks first - some are overkill
	 */
	if (sc->gone) {
		RAY_PRINTF(sc, "unloaded");
		return;
	}
	if (!(ifp->if_flags & IFF_RUNNING)) {
		RAY_PRINTF(sc, "not running");
		return;
	}
	if (!sc->sc_havenet) {
		RAY_PRINTF(sc, "no network");
		return;
	}
	if (!RAY_ECF_READY(sc)) {
		/* Can't assume that the ECF is busy because of this driver */
		RAY_DPRINTF(sc, RAY_DBG_RECERR, "ECF busy, re-scheduling self");
		sc->tx_timerh = timeout(ray_tx_timo, sc, RAY_TX_TIMEOUT);
		return;
	} else
		untimeout(ray_tx_timo, sc, sc->tx_timerh);

	/*
	 * Find a free ccs; if none available wave good bye and exit.
	 *
	 * We find a ccs before we process the mbuf so that we are sure it
	 * is worthwhile processing the packet. All errors in the mbuf
	 * processing are either errors in the mbuf or gross configuration
	 * errors and the packet wouldn't get through anyway.
	 *
	 * Don't forget to clear the ccs on errors.
	 */
	i = RAY_CCS_TX_FIRST;
	do {
		status = SRAM_READ_FIELD_1(sc,
		    RAY_CCS_ADDRESS(i), ray_cmd, c_status);
		if (status == RAY_CCS_STATUS_FREE)
			break;
		i++;
	} while (i <= RAY_CCS_TX_LAST);
	if (i > RAY_CCS_TX_LAST) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}
	RAY_DPRINTF(sc, RAY_DBG_CCS, "using ccs 0x%02x", i);

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
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_apm_mode, 0); /* XXX */
	bufp += sizeof(struct ray_tx_phy_header);
    
	/*
	 * Get the mbuf and process it - we have to remember to free the
	 * ccs if there are any errors
	 */
#if XXX_IFQ_PEEK
	if (ifp->if_snd.ifq_len > 1)
		RAY_PRINTF(sc, "ifq_len %d", ifp->if_snd.ifq_len);
#endif /* XXX_IFQ_PEEK */
	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL) {
		RAY_CCS_FREE(sc, ccs);
		return;
	}
	eh = mtod(m0, struct ether_header *);

	for (pktlen = 0, m = m0; m != NULL; m = m->m_next) {
		pktlen += m->m_len;
	}
	if (pktlen > ETHER_MAX_LEN - ETHER_CRC_LEN) {
		RAY_DPRINTF(sc, RAY_DBG_RECERR, "mbuf too long %d", pktlen);
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		m_freem(m0);
		return;
	}

	/* XXX
	 * I would much prefer to have the complete 802.11 packet dropped to
	 * the bpf tap and then have a user land program parse the headers
	 * as needed. This way, tcpdump -w can be used to grab the raw data. If
	 * needed the 802.11 aware program can "translate" the .11 to ethernet
	 * for tcpdump -r.
	 */
#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp, m0);
#endif /* NBPFILTER */

	/*
	 * Translation - capability as described earlier
	 *
	 * Each case must write the 802.11 header using ray_tx_wrhdr,
	 * passing a pointer to the ethernet header in and getting a new
	 * tc buffer pointer. Next remove/modify/addto the 802.3 and 802.2
	 * headers as needed.
	 *
	 * We've pulled up the mbuf for you.
	 *
	 */
	if (m0->m_len < sizeof(struct ether_header))
		m = m_pullup(m, sizeof(struct ether_header));
	if (m0 == NULL) {
		RAY_DPRINTF(sc, RAY_DBG_RECERR, "could not pullup ether");
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		return;
	}
	switch (sc->translation) {

    	case SC_TRANSLATE_WEBGEAR:
		bufp = ray_tx_wrhdr(sc, eh, bufp);
		break;

	default:
		RAY_PRINTF(sc, "unknown translation type %d", sc->translation);
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		m_freem(m0);
		return;

	}
	if (m0 == NULL) {
		RAY_DPRINTF(sc, RAY_DBG_RECERR, "could not translate mbuf");
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		return;
	}

	/*
	 * Copy the mbuf to the buffer in common memory
	 *
	 * We panic and don't bother wrapping as ethernet packets are 1518
	 * bytes, we checked the mbuf earlier, and our TX buffers are 2048
	 * bytes. We don't have 530 bytes of headers etc. so something
	 * must be fubar.
	 */
	pktlen = sizeof(struct ieee80211_header);
	for (m = m0; m != NULL; m = m->m_next) {
		pktlen += m->m_len;
		if ((len = m->m_len) == 0)
			continue;
		if ((bufp + len) < RAY_TX_END)
			ray_write_region(sc, bufp, mtod(m, u_int8_t *), len);
		else 
			RAY_PANIC(sc, "tx buffer overflow");
		bufp += len;
	}
	RAY_MBUF_DUMP(sc, m0, "ray_tx");

	/*
	 * Fill in a few loose ends and kick the card to send the packet
	 */
	if (!RAY_ECF_READY(sc)) {
		/*
		 * XXX From NetBSD code:
		 *
		 * XXX If this can really happen perhaps we need to save
		 * XXX the chain and use it later.  I think this might
		 * XXX be a confused state though because we check above
		 * XXX and don't issue any commands between.
		 */
		RAY_PRINTF(sc, "ECF busy, dropping packet");
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		return;
	}
	SRAM_WRITE_FIELD_2(sc, ccs, ray_cmd_tx, c_len, pktlen);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_antenna,
	    ray_tx_best_antenna(sc, eh->ether_dhost));
	SRAM_WRITE_1(sc, RAY_SCB_CCSI, RAY_CCS_INDEX(ccs));
	RAY_ECF_START_CMD(sc);
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
	struct ifnet *ifp;
	int s;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;
	if (!(ifp->if_flags & IFF_OACTIVE) && (ifp->if_snd.ifq_head != NULL)) {
		s = splimp();
		ray_tx(ifp);
		splx(s);
	}
}

/*
 * Write an 802.11 header into the TX buffer and return the
 * adjusted buffer pointer.
 */
static size_t
ray_tx_wrhdr(struct ray_softc *sc, struct ether_header *eh, size_t bufp)
{
	struct ieee80211_header header;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	bzero(&header, sizeof(struct ieee80211_header));

	header.i_fc[0] = (IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA);
	if (sc->sc_c.np_net_type == RAY_MIB_NET_TYPE_ADHOC) {

		header.i_fc[1] = IEEE80211_FC1_STA_TO_STA;
		bcopy(eh->ether_dhost, header.i_addr1, ETHER_ADDR_LEN);
		bcopy(eh->ether_shost, header.i_addr2, ETHER_ADDR_LEN);
		bcopy(sc->sc_c.np_bss_id, header.i_addr3, ETHER_ADDR_LEN);

	} else {
		if (sc->sc_c.np_ap_status == RAY_MIB_AP_STATUS_TERMINAL) {
	    
			header.i_fc[1] = IEEE80211_FC1_STA_TO_AP;
			bcopy(sc->sc_c.np_bss_id, header.i_addr1,
			    ETHER_ADDR_LEN);
			bcopy(eh->ether_shost, header.i_addr2, ETHER_ADDR_LEN);
			bcopy(eh->ether_dhost, header.i_addr3, ETHER_ADDR_LEN);

		} else
			RAY_PANIC(sc, "can't be an AP yet");
	}

	ray_write_region(sc, bufp, (u_int8_t *)&header,
	    sizeof(struct ieee80211_header));

	return (bufp + sizeof(struct ieee80211_header));
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

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

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
    	/* This is a simple thresholding scheme which takes the mean
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
ray_tx_done(struct ray_softc *sc, size_t ccs)
{
	struct ifnet *ifp;
	char *ss[] = RAY_CCS_STATUS_STRINGS;
	u_int8_t status;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	status = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
	if (status != RAY_CCS_STATUS_COMPLETE) {
		RAY_PRINTF(sc, "tx completed but status is %s", ss[status]);
		ifp->if_oerrors++;
	}

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
	struct ieee80211_header *header;
	struct ether_header *eh;
	struct ifnet *ifp;
	struct mbuf *m0;
	size_t pktlen, fraglen, readlen, tmplen;
	size_t bufp, ebufp;
	u_int8_t *dst, *src;
	u_int8_t fc;
	u_int8_t siglev, antenna;
	u_int first, ni, i;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	RAY_DPRINTF(sc, RAY_DBG_CCS, "using rcs 0x%x", rcs);

	ifp = &sc->arpcom.ac_if;
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

	if ((pktlen > MCLBYTES) || (pktlen < sizeof(struct ieee80211_header))) {
		RAY_DPRINTF(sc, RAY_DBG_RECERR, "packet too big or too small");
		ifp->if_ierrors++;
		goto skip_read;
	}

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL) {
		RAY_DPRINTF(sc, RAY_DBG_RECERR, "MGETHDR failed");
		ifp->if_ierrors++;
		goto skip_read;
	}
	if (pktlen > MHLEN) {
		MCLGET(m0, M_DONTWAIT);
		if (!(m0->m_flags & M_EXT)) {
			RAY_DPRINTF(sc, RAY_DBG_RECERR, "MCLGET failed");
			ifp->if_ierrors++;
			m_freem(m0);
			m0 = NULL;
			goto skip_read;
		}
	}
	m0->m_pkthdr.rcvif = ifp;
	m0->m_pkthdr.len = pktlen;
	m0->m_len = pktlen;
	dst = mtod(m0, u_int8_t *);

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
		RAY_DPRINTF(sc, RAY_DBG_RX,
		    "frag index %d len %d bufp 0x%x ni %d\n",
		    i, fraglen, (int)bufp, ni);

		if (fraglen + readlen > pktlen) {
			RAY_DPRINTF(sc, RAY_DBG_RECERR,
			    "bad length current 0x%x pktlen 0x%x\n",
			    fraglen + readlen, pktlen);
			ifp->if_ierrors++;
			m_freem(m0);
			m0 = NULL;
			goto skip_read;
		}
		if ((i < RAY_RCS_FIRST) || (i > RAY_RCS_LAST)) {
			RAY_PRINTF(sc, "bad rcs index 0x%x", i);
			ifp->if_ierrors++;
			m_freem(m0);
			m0 = NULL;
			goto skip_read;
		}

		ebufp = bufp + fraglen;
		if (ebufp <= RAY_RX_END)
			ray_read_region(sc, bufp, dst, fraglen);
		else {
			ray_read_region(sc, bufp, dst,
			    (tmplen = RAY_RX_END - bufp));
			ray_read_region(sc, RAY_RX_BASE, dst + tmplen,
			    ebufp - RAY_RX_END);
		}
		dst += fraglen;
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

	RAY_MBUF_DUMP(sc, m0, "ray_rx");

	/*
	 * Check the 802.11 packet type and obtain the .11 src addresses.
	 *
	 * XXX CTL and MGT packets will have separate functions, DATA here
	 *
	 * XXX This needs some work for INFRA mode
	 */
	header = mtod(m0, struct ieee80211_header *);
	fc = header->i_fc[0];
	if ((fc & IEEE80211_FC0_VERSION_MASK) != IEEE80211_FC0_VERSION_0) {
		RAY_DPRINTF(sc, RAY_DBG_RECERR,
		    "header not version 0 fc 0x%x\n", fc);
		ifp->if_ierrors++;
		m_freem(m0);
		return;
	}
	switch (fc & IEEE80211_FC0_TYPE_MASK) {

	case IEEE80211_FC0_TYPE_MGT:
		RAY_PRINTF(sc, "unexpected MGT packet");
		ifp->if_ierrors++;
		m_freem(m0);
		return;

	case IEEE80211_FC0_TYPE_CTL:
		RAY_PRINTF(sc, "unexpected CTL packet");
		ifp->if_ierrors++;
		m_freem(m0);
		return;

	case IEEE80211_FC0_TYPE_DATA:
		RAY_DPRINTF(sc, RAY_DBG_RX, "got a DATA packet");
		break;

	default:
		RAY_PRINTF(sc, "unknown packet fc0 0x%x", fc);
		ifp->if_ierrors++;
		m_freem(m0);
		return;

	}
	fc = header->i_fc[1];
	src = header->i_addr2;
	switch (fc & IEEE80211_FC1_DS_MASK) {

	case IEEE80211_FC1_STA_TO_STA:
		RAY_DPRINTF(sc, RAY_DBG_RX, "packet from sta %6D",
		    src, ":");
		break;

	case IEEE80211_FC1_STA_TO_AP:
		RAY_DPRINTF(sc, RAY_DBG_RX, "packet from sta to ap %6D %6D",
		    src, ":", header->i_addr3, ":");
		ifp->if_ierrors++;
		m_freem(m0);
		break;

	case IEEE80211_FC1_AP_TO_STA:
		RAY_DPRINTF(RAY_DBG_RX, "packet from ap %6D",
		    src, ":");
		ifp->if_ierrors++;
		m_freem(m0);
		break;

	case IEEE80211_FC1_AP_TO_AP:
		RAY_DPRINTF(RAY_DBG_RX, "packet between aps %6D %6D",
		    src, ":", header->i_addr2, ":");
		ifp->if_ierrors++;
		m_freem(m0);
		return;

	default:
	    	src = NULL;
		RAY_PRINTF(sc, "unknown packet fc1 0x%x", fc);
		ifp->if_ierrors++;
		m_freem(m0);
		return;
	}

	/*
	 * Translation - capability as described earlier
	 *
	 * Each case must remove the 802.11 header and leave an 802.3
	 * header in the mbuf copy addresses as needed.
	 */
	switch (sc->translation) {

    	case SC_TRANSLATE_WEBGEAR:
		/* Nice and easy - just trim the 802.11 header */
		m_adj(m0, sizeof(struct ieee80211_header));
		break;

	default:
		RAY_PRINTF(sc, "unknown translation type %d", sc->translation);
		ifp->if_ierrors++;
		m_freem(m0);
		return;

	}

	/*
	 * Finally, do a bit of house keeping before sending the packet
	 * up the stack.
	 */
	ifp->if_ipackets++;
	ray_rx_update_cache(sc, src, siglev, antenna);
#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp, m0);
#endif /* NBPFILTER */
	eh = mtod(m0, struct ether_header *);
	m_adj(m0, sizeof(struct ether_header));
	ether_input(ifp, eh, m0);

	return;
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
static int
ray_intr(struct pccard_devinfo *dev_p)
{
	struct ray_softc *sc;
	struct ifnet *ifp;
	size_t ccs;
	u_int8_t cmd;
	int ccsi, count;

	sc = &ray_softc[dev_p->isahd.id_unit];

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	if (sc->gone) {
		RAY_PRINTF(sc, "unloaded");
		return (0);
	}

	/*
	 * Check that the interrupt was for us, if so get the rcs/ccs
	 * and vector on the command contained within it.
	 */
	if (!RAY_HCS_INTR(sc))
		count = 0;
	else {
		count = 1;
		ccsi = SRAM_READ_1(sc, RAY_SCB_RCSI);
		ccs = RAY_CCS_ADDRESS(ccsi);
		cmd = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd);
		if (ccsi <= RAY_CCS_LAST)
			ray_intr_ccs(sc, cmd, ccs);
		else if (ccsi <= RAY_RCS_LAST)
			ray_intr_rcs(sc, cmd, ccs);
		else
		    RAY_PRINTF(sc, "bad ccs index 0x%x\n", ccsi);
	}

	if (count)
		RAY_HCS_CLEAR_INTR(sc);

	RAY_DPRINTF(sc, RAY_DBG_RX, "interrupt %s handled", count?"was":"not");

	/* Send any packets lying around and update error counters */
	ifp = &sc->arpcom.ac_if;
	if (!(ifp->if_flags & IFF_OACTIVE) && (ifp->if_snd.ifq_head != NULL))
		ray_tx(ifp);

	if ((++sc->sc_checkcounters % 32) == 0)
		ray_intr_updt_errcntrs(sc);

	return (count);
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
ray_intr_ccs(struct ray_softc *sc, u_int8_t cmd, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	/* XXX replace this with a jump table? */
	switch (cmd) {

	case RAY_CMD_DOWNLOAD_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START_PARAMS");
		ray_init_download_done(sc, ccs);
		break;

	case RAY_CMD_UPDATE_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "UPDATE_PARAMS");
		ray_upparams_done(sc, ccs);
		break;

	case RAY_CMD_REPORT_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "REPORT_PARAMS");
		ray_repparams_done(sc, ccs);
		break;

	case RAY_CMD_UPDATE_MCAST:
		RAY_DPRINTF(sc, RAY_DBG_COM, "UPDATE_MCAST");
		ray_mcast_done(sc, ccs);
		break;

	case RAY_CMD_START_NET:
	case RAY_CMD_JOIN_NET:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START|JOIN_NET");
		ray_init_sj_done(sc, ccs);
		break;

	case RAY_CMD_TX_REQ:
		RAY_DPRINTF(sc, RAY_DBG_COM, "TX_REQ");
		ray_tx_done(sc, ccs);
		goto done;

	case RAY_CMD_START_ASSOC:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START_ASSOC");
		ray_init_assoc_done(sc, ccs);
		break;

	case RAY_CMD_UPDATE_APM:
		RAY_PRINTF(sc, "unexpected UPDATE_APM");
		break;

	case RAY_CMD_TEST_MEM:
		RAY_PRINTF(sc, "unexpected TEST_MEM");
		break;

	case RAY_CMD_SHUTDOWN:
		RAY_PRINTF(sc, "unexpected SHUTDOWN");
		break;

	case RAY_CMD_DUMP_MEM:
		RAY_PRINTF(sc, "unexpected DUMP_MEM");
		break;

	case RAY_CMD_START_TIMER:
		RAY_PRINTF(sc, "unexpected START_TIMER");
		break;

	default:
		RAY_PRINTF(sc, "unknown command 0x%x", cmd);
		break;
	}

	ray_ccs_free(sc, ccs);

done:
	/*
	 * See if needed things can be done now that a command has completed
	 */
	ray_com_runq(sc);
}

/*
 * Process ECF command request
 */
static void
ray_intr_rcs(struct ray_softc *sc, u_int8_t cmd, size_t rcs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	/* XXX replace this with a jump table? */
	switch (cmd) {

	case RAY_ECMD_RX_DONE:
		RAY_DPRINTF(sc, RAY_DBG_CCS, "RX_DONE");
		ray_rx(sc, rcs);
		break;

	case RAY_ECMD_REJOIN_DONE:
		RAY_DPRINTF(sc, RAY_DBG_CCS, "REJOIN_DONE");
		sc->sc_havenet = 1; /* XXX Should not be here but in function */
		break;

	case RAY_ECMD_ROAM_START:
		RAY_DPRINTF(sc, RAY_DBG_CCS, "ROAM_START");
		sc->sc_havenet = 0; /* XXX Should not be here but in function */
		break;

	case RAY_ECMD_JAPAN_CALL_SIGNAL:
		RAY_PRINTF(sc, "unexpected JAPAN_CALL_SIGNAL");
		break;

	default:
		RAY_PRINTF(sc, "unknown command 0x%x", cmd);
		break;
	}

	RAY_CCS_FREE(sc, rcs);
}

#if XXX_MCAST

/*
 * XXX First cut at this code - have not tried compiling it yet. V. confusing
 * XXX interactions between allmulti, promisc and mcast. Going to leave it
 * XXX for now.
 * XXX Don't like the code bloat to set promisc up - we use it here, ray_init,
 * XXX ray_promisc_user and ray_upparams_user...
 */

/*
 * User land entry to multicast list changes
 */
static int
ray_mcast_user(struct ray_softc *sc)
{
	struct ifnet *ifp;
	struct ray_comq_entry *com[2];
	int error, count;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	ifp = &sc->arpcom.ac_if;

	/*
	 * The multicast list is only 16 items long so use promiscuous
	 * mode if needed.
	 *
	 * We track this stuff even when not running.
	 */
	for (ifma = ifp->if_multiaddrs.lh_first, count = 0; ifma != NULL;
	    ifma = ifma->ifma_link.le_next, count++)
	if (count > 16)
		ifp->if_flags |= IFF_ALLMULTI;
	else if (ifp->if_flags & IFF_ALLMULTI)
		ifp->if_flags &= ~IFF_ALLMULTI;

	if (!(ifp->if_flags & IFF_RUNNING)) {
		return (0);
	}

	/*
	 * If we need to change the promiscuous mode then do so.
	 */
	if (sc->promisc != !!(ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI))) {
		MALLOC(com[0], struct ray_comq_entry *,
		    sizeof(struct ray_comq_entry), M_RAYCOM, M_WAITOK);
		RAY_COM_FUNCTION(com[0],ray_promisc);
		com[0]->c_flags = RAY_COM_FWOK; 
		com[0]->c_retval = 0;
		com[0]->c_ccs = NULL;
		com[0]->c_wakeup = com[1];
		ray_com_runq_add(sc, com[0]);
	} else
	    com[0] = NULL;

	/*
	 * If we need to set the mcast list then do so.
	 */
	if (!(ifp->if_flags & IFF_ALLMULTI))
		MALLOC(com[1], struct ray_comq_entry *,
		    sizeof(struct ray_comq_entry), M_RAYCOM, M_WAITOK);
		RAY_COM_FUNCTION(com[1], ray_mcast);
		com[0]->c_flags &= ~RAY_COM_FWOK; 
		com[1]->c_flags = RAY_COM_FWOK; 
		com[1]->c_retval = 0;
		com[1]->c_ccs = NULL;
		com[1]->c_wakeup = com[1];
		ray_com_runq_add(sc, com[1]);
	} else
	    com[1] = NULL;

	ray_com_runq(sc);
	RAY_DPRINTF(sc, RAY_DBG_COM, "sleeping");
	(void)tsleep(com[1], 0, "raymcast", 0);
	RAY_DPRINTF(sc, RAY_DBG_COM, "awakened");

	error = com->c_retval;
	if (com[0] != NULL)
	    FREE(com[0], M_RAYCOM);
	if (com[1] != NULL)
	    FREE(com[1], M_RAYCOM);
	return (error);
}

/*
 * Runq entry to setting the multicast filter list
 */
static void
ray_mcast(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	size_t bufp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_UPDATE_MCAST, 0);
	SRAM_WRITE_FIELD_1(sc, &com->c_ccs,
	    ray_cmd_update_mcast, c_nmcast, count);
	bufp = RAY_HOST_TO_ECF_BASE;
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
	    ifma = ifma->ifma_link.le_next) {
		ray_write_region(
		    sc,
		    bufp,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    ETHER_ADDR_LEN
		);
		bufp += ETHER_ADDR_LEN;
	}

	ray_com_ecf(sc, com);
}

/*
 * Complete the multicast filter list update
 */
static void
ray_mcast_done(struct ray_softc *sc, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_COM_CHECK(sc, ccs);

	ray_com_ecf_done(sc);
}
#else
static int ray_mcast_user(struct ray_softc *sc) {return (0);}
static void ray_mcast(struct ray_softc *sc, struct ray_comq_entry *com) {}
static void ray_mcast_done(struct ray_softc *sc, size_t ccs) {}
#endif /* XXX_MCAST */

/*
 * User land entry to promiscuous mode change
 */
static int
ray_promisc_user(struct ray_softc *sc)
{
	struct ray_comq_entry *com[1];
	int error, ncom, i;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	ncom = 0;
	com[ncom++] = RAY_COM_MALLOC(ray_promisc, RAY_COM_FWOK);

	ray_com_runq_arr(sc, com, ncom, "raypromisc");

	error = com[0]->c_retval;

	for (i = 0; i < ncom; i++)
		FREE(com[i], M_RAYCOM);
	
	return (error);
}

/*
 * Runq entry to set/reset promiscuous mode
 */
static void
ray_promisc(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_UPDATE_PARAMS, 0);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs,
	    ray_cmd_update, c_paramid, RAY_MIB_PROMISC);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_update, c_nparam, 1);
	SRAM_WRITE_1(sc, RAY_HOST_TO_ECF_BASE, 
	    !!(ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI)));

	ray_com_ecf(sc, com);
}

/*
 * User land entry to parameter reporting
 */
static int
ray_repparams_user(struct ray_softc *sc, struct ray_param_req *pr)
{
	struct ifnet *ifp;
	struct ray_comq_entry *com[1];
	int error, ncom, i;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	ifp = &sc->arpcom.ac_if;
	if (!(ifp->if_flags & IFF_RUNNING)) {
		pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
		return (EIO);
	}
	
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

	ray_com_runq_arr(sc, com, ncom, "rayrepparams");

	error = com[0]->c_retval;
	if (!error && pr->r_failcause)
		error = EINVAL;

	for (i = 0; i < ncom; i++)
		FREE(com[i], M_RAYCOM);

	return (error);
}

/*
 * Runq entry to read the required parameter
 */
static void
ray_repparams(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_REPORT_PARAMS, 0);

	SRAM_WRITE_FIELD_1(sc, com->c_ccs,
	    ray_cmd_report, c_paramid, com->c_pr->r_paramid);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_report, c_nparam, 1);

	ray_com_ecf(sc, com);
}

/*
 * Complete the parameter reporting
 */
static void
ray_repparams_done(struct ray_softc *sc, size_t ccs)
{
	struct ray_comq_entry *com;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);
	RAY_COM_CHECK(sc, ccs);

	com = TAILQ_FIRST(&sc->sc_comq);
	com->c_pr->r_failcause =
	    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_report, c_failcause);
	com->c_pr->r_len =
	    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_report, c_len);
	ray_read_region(sc, RAY_ECF_TO_HOST_BASE,
	    com->c_pr->r_data, com->c_pr->r_len);

	ray_com_ecf_done(sc);
}

/*
 * User land entry (and exit) to the error counters
 */
static int
ray_repstats_user(struct ray_softc *sc, struct ray_stats_req *sr)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	ifp = &sc->arpcom.ac_if;
	if (!(ifp->if_flags & IFF_RUNNING)) {
		return (EIO);
	}

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
 * invalid we have to re-sttart/join.
 */
static int
ray_upparams_user(struct ray_softc *sc, struct ray_param_req *pr)
{
	struct ifnet *ifp;
	struct ray_comq_entry *com[3];
	int i, todo, error, ncom;
#define RAY_UPP_SJ	0x1
#define RAY_UPP_PARAMS	0x2

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	ifp = &sc->arpcom.ac_if;
	if (!(ifp->if_flags & IFF_RUNNING)) {
		pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
		return (EIO); /* XXX Use this for other IFF_RUNNING checks */
	}

	/*
	 * Handle certain parameters specially
	 */
	todo = 0;
	pr->r_failcause = 0;
	if (pr->r_paramid > RAY_MIB_LASTUSER)
		return (EINVAL);
	if ((sc->sc_version == RAY_ECFS_BUILD_4) &&
	    !(mib_info[pr->r_paramid][0] & RAY_V4))
		return (EINVAL);
	if ((sc->sc_version == RAY_ECFS_BUILD_5) &&
	    !(mib_info[pr->r_paramid][0] & RAY_V5))
		return (EINVAL);
	switch (pr->r_paramid) {
	case RAY_MIB_NET_TYPE:		/* Updated via START_NET JOIN_NET  */
		if (sc->sc_c.np_net_type == *pr->r_data)
			return (0);
		sc->sc_d.np_net_type = *pr->r_data;
		todo |= RAY_UPP_SJ;
		break;

	case RAY_MIB_SSID:		/* Updated via START_NET JOIN_NET  */
		if (bcmp(sc->sc_c.np_ssid, pr->r_data, IEEE80211_NWID_LEN) == 0)
			return (0);
		bcopy(pr->r_data, sc->sc_d.np_ssid, IEEE80211_NWID_LEN);
		todo |= RAY_UPP_SJ;
		break;

	case RAY_MIB_PRIVACY_MUST_START:/* Updated via START_NET */
		if (sc->sc_c.np_net_type != RAY_MIB_NET_TYPE_ADHOC)
			return (EINVAL);
		if (sc->sc_c.np_priv_start == *pr->r_data)
			return (0);
		sc->sc_d.np_priv_start = *pr->r_data;
		todo |= RAY_UPP_SJ;
		break;

	case RAY_MIB_PRIVACY_CAN_JOIN:	/* Updated via START_NET JOIN_NET  */
		if (sc->sc_c.np_priv_join == *pr->r_data)
			return (0);
		sc->sc_d.np_priv_join = *pr->r_data;
		todo |= RAY_UPP_SJ;
		break;

	case RAY_MIB_BASIC_RATE_SET:
		sc->sc_d.np_def_txrate = *pr->r_data;
		todo |= RAY_UPP_PARAMS;
		break;

	case RAY_MIB_AP_STATUS:	/* Unsupported */
	case RAY_MIB_MAC_ADDR:	/* XXX Need interface up */
	case RAY_MIB_PROMISC:	/* BPF */
		return (EINVAL);
		break;

	default:
		todo |= RAY_UPP_PARAMS;
		todo |= RAY_UPP_SJ;
		break;
	}

	ncom = 0;
	if (todo & RAY_UPP_PARAMS) {
		com[ncom++] = RAY_COM_MALLOC(ray_upparams, 0);
		com[ncom-1]->c_pr = pr;
	}
	if ((todo & RAY_UPP_SJ) && (ifp->if_flags & IFF_RUNNING)) {
		com[ncom++] = RAY_COM_MALLOC(ray_init_sj, 0);
#if XXX_ASSOC
		if (sc->sc_d.np_net_type == RAY_MIB_NET_TYPE_INFRA)
			com[ncom++] = RAY_COM_MALLOC(ray_init_assoc, 0);
#endif /* XXX_ASSOC */
	}

	ray_com_runq_arr(sc, com, ncom, "rayupparams");

	error = com[0]->c_retval;
	if (!error && pr->r_failcause)
		error = EINVAL;
	/* XXX no error processing from ray_init_sj yet! */

	for (i = 0; i < ncom; i++)
		FREE(com[i], M_RAYCOM);

	return (error);
}

/*
 * Runq entry to update a parameter
 */
static void
ray_upparams(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_UPDATE_PARAMS, 0);

	SRAM_WRITE_FIELD_1(sc, com->c_ccs,
	    ray_cmd_update, c_paramid, com->c_pr->r_paramid);
	SRAM_WRITE_FIELD_1(sc, com->c_ccs, ray_cmd_update, c_nparam, 1);
	ray_write_region(sc, RAY_HOST_TO_ECF_BASE,
	    com->c_pr->r_data, com->c_pr->r_len);

	ray_com_ecf(sc, com);
}

/*
 * Complete the parameter update
 */
static void
ray_upparams_done(struct ray_softc *sc, size_t ccs)
{
	struct ray_comq_entry *com;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);
	RAY_COM_CHECK(sc, ccs);

	com = TAILQ_FIRST(&sc->sc_comq);

	switch (SRAM_READ_FIELD_1(sc, ccs, ray_cmd_update, c_paramid)) {

	case RAY_MIB_PROMISC:
		sc->sc_promisc = SRAM_READ_1(sc, RAY_HOST_TO_ECF_BASE);
		RAY_DPRINTF(sc, RAY_DBG_IOCTL,
		    "promisc value %d", sc->sc_promisc);
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
 * Malloc, set up a comq entry struct
 */
#if RAY_DEBUG & RAY_DBG_COM
static struct ray_comq_entry *
ray_com_malloc(ray_comqfn_t function, int flags, char *mesg)
#else
static struct ray_comq_entry *
ray_com_malloc(ray_comqfn_t function, int flags)
#endif /* RAY_DEBUG & RAY_DBG_COM */
{
	struct ray_comq_entry *com;

	MALLOC(com, struct ray_comq_entry *,
	    sizeof(struct ray_comq_entry), M_RAYCOM, M_WAITOK);

	com->c_function = function;
	com->c_flags = flags;
	com->c_retval = 0;
	com->c_ccs = NULL;
	com->c_wakeup = NULL;
	com->c_pr = NULL;
#if RAY_DEBUG & RAY_DBG_COM
	com->c_mesg = mesg;
#endif /* RAY_DEBUG & RAY_DBG_COM */

	return (com);
}

/*
 * Add an array of commands to the runq and then run them, waiting on
 * the last command
 */
static void
ray_com_runq_arr(struct ray_softc *sc, struct ray_comq_entry *com[], int ncom, char *wmesg)
{
	int i;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	for (i = 0; i < ncom; i++) {
		com[i]->c_wakeup = com[ncom-1];
		ray_com_runq_add(sc, com[i]);
	}
	com[ncom-1]->c_flags = RAY_COM_FWOK;

	ray_com_runq(sc);
	RAY_DPRINTF(sc, RAY_DBG_COM, "sleeping");
	(void)tsleep(com[ncom-1], 0, wmesg, 0);
	RAY_DPRINTF(sc, RAY_DBG_COM, "awakened");
}

/*
 * Add a command to the tail of the queue
 */
static void
ray_com_runq_add(struct ray_softc *sc, struct ray_comq_entry *com)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

	RAY_COM_DUMP(sc, com, "adding");
	TAILQ_INSERT_TAIL(&sc->sc_comq, com, c_chain);
}

/*
 * Run the command at the head of the queue (if not already running)
 */
static void
ray_com_runq(struct ray_softc *sc)
{
	struct ray_comq_entry *com;
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

	com = TAILQ_FIRST(&sc->sc_comq);
#if RAY_DEBUG & RAY_DBG_COM /* XXX this can go later */
	if (com == NULL) {
		RAY_DPRINTF(sc, RAY_DBG_COM, "empty command queue");
		return;
	}
	if (com->c_flags & RAY_COM_FRUNNING) {
		RAY_DPRINTF(sc, RAY_DBG_COM, "command already running");
		return;
	}
#else
	if ((com == NULL) || (com->c_flags & RAY_COM_FRUNNING))
		return;
#endif /* RAY_DEBUG & RAY_DBG_COM */

	/*
	 * XXX how can IFF_RUNNING be cleared
	 * XXX before this routine exits - check in ray_ioctl and the
	 * XXX network code itself. ray_stop should have prevented this
	 * XXX command from running?
	 *
	 * XXX also what about sc->sc_gone and sc->sc_havenet?
	 */
	ifp = &sc->arpcom.ac_if;
	if (!(ifp->if_flags & IFF_RUNNING))
		RAY_PANIC(sc, "not running");

	com->c_flags |= RAY_COM_FRUNNING;
	RAY_COM_DUMP(sc, com, "running");
	com->c_function(sc, com);
}

/*
 * Abort the execution of a run queue entry and wakeup the
 * user level caller.
 *
 * We do not remove the entry from the runq incase the caller want's to
 * retry and to prevent any other commands being run. The user level caller
 * must acknowledge the abort.
 */
static void
ray_com_runq_abort(struct ray_softc *sc, struct ray_comq_entry *com, int reason)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

#if RAY_DEBUG & RAY_DBG_COM
	if (com != TAILQ_FIRST(&sc->sc_comq))
		RAY_PANIC(sc, "com and head of queue");
#endif /* RAY_DEBUG & RAY_DBG_COM */
	RAY_COM_DUMP(sc, com, "aborting");
	com->c_retval = reason;

	wakeup(com->c_wakeup);
}

/*
 * Remove an aborted command and re-run the queue
 */
static void
ray_com_runq_clrabort(struct ray_softc *sc, struct ray_comq_entry *com)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

#if RAY_DEBUG & RAY_DBG_COM
	if (com != TAILQ_FIRST(&sc->sc_comq))
		RAY_PANIC(sc, "com and head of queue");
#endif /* RAY_DEBUG & RAY_DBG_COM */

	RAY_COM_DUMP(sc, com, "removing");
	TAILQ_REMOVE(&sc->sc_comq, com, c_chain);

	ray_com_runq(sc);
}

/*
 * Remove run command and wakeup caller.
 *
 * Minimal checks are done here as we ensure that the com and
 * command handler were matched up earlier.
 *
 * Remove the com from the comq, and wakeup the caller if it requested
 * to be woken. This is used for ensuring a sequence of commands
 * completes.
 */
static void
ray_com_runq_done(struct ray_softc *sc)
{
    	struct ray_comq_entry *com;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

	com = TAILQ_FIRST(&sc->sc_comq); /* XXX shall we do this as below */
	com->c_flags &= ~RAY_COM_FRUNNING;
	com->c_flags |= RAY_COM_FCOMPLETED;
	com->c_retval = 0;

	RAY_COM_DUMP(sc, com, "removing");
	TAILQ_REMOVE(&sc->sc_comq, com, c_chain);

	if (com->c_flags & RAY_COM_FWOK)
		wakeup(com->c_wakeup);

	/* XXX what about error on completion then? deal with when i fix
	 * XXX the status checking */
}

/*
 * Send a command to the ECF.
 */
static void
ray_com_ecf(struct ray_softc *sc, struct ray_comq_entry *com)
{
	u_int i;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");
	RAY_MAP_CM(sc);

#if RAY_DEBUG & RAY_DBG_COM
	if (com != TAILQ_FIRST(&sc->sc_comq))
		RAY_PANIC(sc, "com and head of queue");
#endif /* RAY_DEBUG & RAY_DBG_COM */

	/*
	 * XXX other drivers did this, but I think 
	 * XXX what we really want to do is just make sure we don't
	 * XXX get here or that spinning is ok
	 *
	 * XXX actually we probably want to call a timeout on
	 * XXX ourself here...
	 */
	i = 0;
	while (!RAY_ECF_READY(sc))
		if (++i > 50)
			RAY_PANIC(sc, "spun too long");
		else if (i == 1)
			RAY_PRINTF(sc, "spinning");

	RAY_COM_DUMP(sc, com, "sending");
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
	u_int8_t cmd;
	int s;

	s = splnet();

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");
	RAY_MAP_CM(sc);

	com = TAILQ_FIRST(&sc->sc_comq);
#if RAY_DEBUG & RAY_DBG_COM /* XXX get rid of this at some point or make it KASSERT */
	if (com == NULL)
		RAY_PANIC(sc, "no command queue");
#endif /* RAY_DEBUG & RAY_DBG_COM */

	cmd = SRAM_READ_FIELD_1(sc, com->c_ccs, ray_cmd, c_cmd);
	switch (SRAM_READ_FIELD_1(sc, com->c_ccs, ray_cmd, c_status)) {

	case RAY_CCS_STATUS_COMPLETE:
	case RAY_CCS_STATUS_FREE:			/* Buggy firmware */
		ray_intr_ccs(sc, cmd, com->c_ccs);
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
			ray_intr_ccs(sc, cmd, com->c_ccs);
		break;

	}

	splx(s);
}

/*
 * Called when interrupt handler for the command has done all it
 * needs to.
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
	RAY_MAP_CM(sc);

	com = TAILQ_FIRST(&sc->sc_comq);

	if (com == NULL)
		RAY_PANIC(sc, "no command queue");
	if (com->c_ccs != ccs)
		RAY_PANIC(sc, "ccs's don't match");
}
#endif /* RAY_DEBUG & RAY_DBG_COM */

/*
 * CCS allocator for commands
 */

/*
 * Obtain a ccs and fill easy bits in
 *
 * Returns 1 and in `ccsp' the bus offset of the free ccs. Will block
 * awaiting free ccs if needed, timo is passed to tsleep and will
 * return 0 if the timeout expired.
 */
static int
ray_ccs_alloc(struct ray_softc *sc, size_t *ccsp, u_int cmd, int timo)
{
	size_t ccs;
	u_int i;

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
			RAY_PANIC(sc, "out of CCS's");
		} else
			break;
	}

	sc->sc_ccsinuse[i] = 1;
	ccs = RAY_CCS_ADDRESS(i);
	RAY_DPRINTF(sc, RAY_DBG_CCS, "allocated 0x%02x", i);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_BUSY);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_cmd, cmd);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_link, RAY_CCS_LINK_NULL);

	*ccsp = ccs;
	return (1);
}

/*
 * Free up a ccs allocated via ray_ccs_alloc
 *
 * Return the old status. This routine is only used for ccs allocated via
 * ray_ccs_alloc (not tx, rx or ECF command requests).
 */
static u_int8_t
ray_ccs_free(struct ray_softc *sc, size_t ccs)
{
	u_int8_t status;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_CCS, "");
	RAY_MAP_CM(sc);

	status = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
	RAY_CCS_FREE(sc, ccs);
	sc->sc_ccsinuse[RAY_CCS_INDEX(ccs)] = 0;
	wakeup(ray_ccs_alloc);
	RAY_DPRINTF(sc, RAY_DBG_CCS, "freed 0x%02x", RAY_CCS_INDEX(ccs));

	return (status);
}

/*
 * Routines to read from/write to the attribute memory.
 *
 * Taken from if_xe.c.
 *
 * Until there is a real way of accessing the attribute memory from a driver
 * these have to stay.
 *
 * The hack to use the crdread/crdwrite device functions causes the attribute
 * memory to be remapped into the controller and looses the mapping of
 * the common memory.
 *
 * We cheat by using PIOCSMEM and assume that the common memory window
 * is in window 0 of the card structure.
 *
 * Also
 *	pccard/pcic.c/crdread does mark the unmapped window as inactive
 *	pccard/pccard.c/map_mem toggles the mapping of a window on
 *	successive calls
 *
 */

/*
 * Furtle around to get the initial map from pccardd
*/
#if (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP)
static void
ray_attr_getmap(struct ray_softc *sc)
{
	struct ucred uc;
	struct pcred pc;
	struct proc p;
	int result;

	RAY_DPRINTF(sc, RAY_DBG_CM, "");

	sc->md.window = 0;

	p.p_cred = &pc;
	p.p_cred->pc_ucred = &uc;
	p.p_cred->pc_ucred->cr_uid = 0;

	result = cdevsw[CARD_MAJOR]->d_ioctl(makedev(CARD_MAJOR, sc->slotnum),
	    PIOCGMEM, (caddr_t)&sc->md, 0, &p);
}

static void
ray_attr_mapcm(struct ray_softc *sc)
{
	struct ucred uc;
	struct pcred pc;
	struct proc p;

	RAY_DPRINTF(sc, RAY_DBG_CM, "");

	p.p_cred = &pc;
	p.p_cred->pc_ucred = &uc;
	p.p_cred->pc_ucred->cr_uid = 0;

	cdevsw[CARD_MAJOR]->d_ioctl(makedev(CARD_MAJOR, sc->slotnum),
	    PIOCSMEM, (caddr_t)&sc->md, 0, &p);
}
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

/******************************************************************************
 * XXX NOT KNF FROM HERE DOWN
 ******************************************************************************/

static int
ray_attr_write(struct ray_softc *sc, off_t offset, u_int8_t byte)
{
  struct iovec iov;
  struct uio uios;
  int err;

  iov.iov_base = &byte;
  iov.iov_len = sizeof(byte);

  uios.uio_iov = &iov;
  uios.uio_iovcnt = 1;
  uios.uio_offset = offset;
  uios.uio_resid = sizeof(byte);
  uios.uio_segflg = UIO_SYSSPACE;
  uios.uio_rw = UIO_WRITE;
  uios.uio_procp = 0;

  err = cdevsw[CARD_MAJOR]->d_write(makedev(CARD_MAJOR, sc->slotnum), &uios, 0);

#if RAY_NEED_CM_REMAPPING
  ray_attr_mapcm(sc);
#endif /* RAY_NEED_CM_REMAPPING */

  return (err);
}

static int
ray_attr_read(struct ray_softc *sc, off_t offset, u_int8_t *buf, int size)
{
  struct iovec iov;
  struct uio uios;
  int err;

  iov.iov_base = buf;
  iov.iov_len = size;

  uios.uio_iov = &iov;
  uios.uio_iovcnt = 1;
  uios.uio_offset = offset;
  uios.uio_resid = size;
  uios.uio_segflg = UIO_SYSSPACE;
  uios.uio_rw = UIO_READ;
  uios.uio_procp = 0;

  err =  cdevsw[CARD_MAJOR]->d_read(makedev(CARD_MAJOR, sc->slotnum), &uios, 0);

#if RAY_NEED_CM_REMAPPING
  ray_attr_mapcm(sc);
#endif /* RAY_NEED_CM_REMAPPING */

  return (err);
}

static u_int8_t
ray_read_reg(sc, reg)
    struct ray_softc	*sc;
    off_t		reg;
{
    u_int8_t		byte;

    ray_attr_read(sc, reg, &byte, 1);

    return (byte);
}

#if RAY_DEBUG & RAY_DBG_MBUF
static void
ray_dump_mbuf(sc, m, s)
    struct ray_softc	*sc;
    struct mbuf		*m;
    char		*s;
{
    u_int8_t		*d, *ed;
    u_int		i;
    char		p[17];

    RAY_PRINTF(sc, "%s mbuf dump:", s);
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
	printf("%s\n", p);
}
#endif /* RAY_DEBUG & RAY_DBG_MBUF */

#endif /* NRAY */