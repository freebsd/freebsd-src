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
 * $Id: if_ray.c,v 1.5 2000/03/03 17:07:42 dmlb Exp $
 *
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
 * assuming the card is an ed driver...).
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
 * _stop
 * _reset
 * havenet checking
 * unload
 * shutdown
 *
 * TX bpf
 * RX bpf
 * promisoius
 * multicast
 * ifp->if_hdr length
 *
 * apm
 *
 * more commands
 * ioctls - translation, BSS_ID, countrycode
 * faster TX routine
 * more translations
 * infrastructure mode
 * differeniate between parameters set in attach and init
 * start_join_done needs a restart in download_done
 * spinning in ray_issue_cmd
 *
 * command tracking - really needed? if not remove SCP_ stuff
 * 	will simplify ray_issue_cmd away
 */

#define XXX		0
#define XXX_TRACKING	0
#define XXX_INFRA	0
#define XXX_MCASTPROM	0

/*
 * XXX build options - move to LINT
 */

/*
 * RAY_DEBUG settings
 *
 *	2	Recoverable error's
 *	6	Subroutine entry
 *	11	Startup CM dump
 *	15	State transitions for start/join
 *	21	CCS info
 *	31	IOCTL calls
 *	51	MBUFs dumped/packet types reported
 */
#define RAY_DEBUG		6

#define RAY_CCS_TIMEOUT		(hz/2)	/* Timeout for CCS commands - only used for downloading startup parameters */

#define RAY_NEED_STARTJOIN_TIMO	0	/* Might be needed with build 4 */
#define RAY_SJ_TIMEOUT		(90*hz)	/* Timeout for failing STARTJOIN commands - only used with RAY_NEED_STARTJOIN_TIMO */

#define RAY_NEED_CM_FIXUP	1	/* Needed until pccardd hacks for ed drivers are removed (pccardd forces 16bit memory and 0x4000 size) THIS IS A DANGEROUS THING TO USE IF YOU USE OTHER MEMORY MAPPED PCCARDS */

#define RAY_NEED_CM_REMAPPING	1	/* Needed until pccard maps more than one memory area */

#define RAY_DUMP_CM_ON_GIFMEDIA	1	/* Dump some common memory when the SIOCGIFMEDIA ioctl is issued - a nasty hack for debugging and will be placed by an ioctl and control program */
/*
 * XXX build options - move to LINT
 */

/*
 * Debugging odds and odds
 */
#ifndef RAY_DEBUG
#define RAY_DEBUG 0
#endif /* RAY_DEBUG */

#if RAY_DEBUG > 0

/* XXX This macro assumes that common memory is mapped into kernel space and
 * XXX does not indirect through SRAM macros - it should
 */
#define RAY_DHEX8(p, l) do { if (RAY_DEBUG > 10) {		\
    u_int8_t *i;						\
    for (i = p; i < (u_int8_t *)(p+l); i += 8)			\
    	printf("  0x%08lx %8D\n",				\
		(unsigned long)i, (unsigned char *)i, " ");	\
} } while (0)

#define RAY_DPRINTFN(l, x) do { if (RAY_DEBUG > l) {		\
    printf x ;							\
} } while (0)

#define RAY_DNET_DUMP(sc, s) do { if (RAY_DEBUG) {			\
    printf("ray%d: Network parameters%s\n", (sc)->unit, (s));		\
    printf("  bss_id %6D\n", (sc)->sc_bss_id, ":");			\
    printf("  inited 0x%02x\n", (sc)->sc_inited);			\
    printf("  def_txrate 0x%02x\n", (sc)->sc_def_txrate);		\
    printf("  encrypt 0x%02x\n", (sc)->sc_encrypt);			\
    printf("  net_type 0x%02x\n", (sc)->sc_net_type);			\
    printf("  ssid \"%.32s\"\n", (sc)->sc_ssid);			\
    printf("  priv_start 0x%02x\n", (sc)->sc_priv_start);		\
    printf("  priv_join 0x%02x\n", (sc)->sc_priv_join);			\
} } while (0)

#else
#define RAY_DHEX8(p, l)
#define RAY_DPRINTFN(l,x)
#define RAY_DNET_DUMP(sc, s)
#endif /* RAY_DEBUG > 0 */

#if RAY_DEBUG > 50
#define RAY_DMBUF_DUMP(sc, m, s)	ray_dump_mbuf((sc), (m), (s))
#else
#define RAY_DMBUF_DUMP(sc, m, s)
#endif /* RAY_DEBUG > 10 */

#include "ray.h"
#include "card.h"
#include "apm.h"
#include "bpfilter.h"

#if NRAY > 0

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/ucred.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <i386/isa/if_ieee80211.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif /* NBPFILTER */

#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/if_rayreg.h>
#include <i386/isa/if_raymib.h>

#if NCARD > 0
#include <pccard/cardinfo.h>
#include <pccard/cis.h>
#include <pccard/driver.h>
#include <pccard/slot.h>
#endif /* NCARD */

#if NAPM > 0
#include <machine/apm_bios.h>
#endif /* NAPM */

/*
 * One of these structures per allocated device
 */
struct ray_softc {

    struct arpcom	arpcom;		/* Ethernet common 		*/
    struct ifmedia	ifmedia;	/* Ifnet common 		*/
    struct callout_handle \
    			timerh;		/* Handle for timer		*/
#if RAY_NEED_STARTJOIN_TIMO
    struct callout_handle \
    			sj_timerh;	/* Handle for start_join timer	*/
#endif /* RAY_NEED_STARTJOIN_TIMO */
    struct callout_handle \
    			start_timerh;	/* Handle for start timer	*/

    char		*card_type;	/* Card model name		*/
    char		*vendor;	/* Card manufacturer		*/

    int			unit;		/* Unit number			*/
    u_char		gone;		/* 1 = Card bailed out		*/
    caddr_t		maddr;		/* Shared RAM Address		*/
    int			flags;		/* Start up flags		*/

    int			translation;	/* Packet translation types	*/

#if (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP)
    int			slotnum;	/* Slot number			*/
    struct mem_desc	md;		/* Map info for common memory	*/
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

    struct ray_ecf_startup_v5 \
    			sc_ecf_startup; /* Startup info from card	*/

    u_int8_t		sc_ccsinuse[64];/* ccss' in use -- not for tx	*/
    size_t		sc_ccs;		/* ccs used by non-scheduled,	*/
    					/* non-overlapping procedures	*/

    struct ray_cmd_net	sc_cnet_1;	/* current network params from	*/
    struct ray_net_params sc_cnet_2;	/* starting/joining a network	*/

#if 0
    u_int8_t		sc_cnwid[IEEE80211_NWID_LEN];	/* Last nwid */
    u_int8_t		sc_dnwid[IEEE80211_NWID_LEN];	/* Desired nwid */
    u_int8_t		sc_omode;	/* Old operating mode SC_MODE_xx */
    u_int8_t		sc_mode;	/* Current operating mode SC_MODE_xx */
    u_int8_t		sc_countrycode;	/* Current country code */
    u_int8_t		sc_dcountrycode;/* Desired country code */
#endif
    int			sc_havenet;	/* true if we have aquired a network */
};
static struct ray_softc ray_softc[NRAY];

#define	sc_station_addr	sc_ecf_startup.e_station_addr
#define	sc_version	sc_ecf_startup.e_fw_build_string
#define	sc_tibsize	sc_ecf_startup.e_tibsize

#define sc_upd_param	sc_cnet_1.c_upd_param
#define	sc_bss_id	sc_cnet_1.c_bss_id
#define	sc_inited	sc_cnet_1.c_inited
#define	sc_def_txrate	sc_cnet_1.c_def_txrate
#define	sc_encrypt	sc_cnet_1.c_encrypt
#define sc_net_type	sc_cnet_2.p_net_type
#define sc_ssid		sc_cnet_2.p_ssid
#define sc_priv_start	sc_cnet_2.p_privacy_must_start
#define sc_priv_join	sc_cnet_2.p_privacy_can_join
/* Remember to add to the debug macro and ioctl*/

/* XXX might not need these */
/* Commands -- priority given to LSB */
#define	SCP_FIRST		0x0001
#define	SCP_UPDATESUBCMD	0x0001
#define	SCP_STARTASSOC		0x0002
#define	SCP_REPORTPARAMS	0x0004
#define	SCP_IFSTART		0x0008

/* Update sub commands -- issues are serialized priority to LSB */
#define	SCP_UPD_FIRST		0x0100
#define	SCP_UPD_STARTUP		0x0100
#define	SCP_UPD_STARTJOIN	0x0200
#define	SCP_UPD_PROMISC		0x0400
#define	SCP_UPD_MCAST		0x0800
#define	SCP_UPD_UPDATEPARAMS	0x1000
#define	SCP_UPD_SHIFT		8
#define	SCP_UPD_MASK		0xff00

/* These command (a subset of the update set) require timeout checking */
#define	SCP_TIMOCHECK_CMD_MASK	\
	(SCP_UPD_UPDATEPARAMS | SCP_UPD_STARTUP | SCP_UPD_MCAST | \
	SCP_UPD_PROMISC)
/*XXX might not need these */

/*
 * Translation types
 */
/* XXX maybe better as part of the if structure? */
#define SC_TRANSLATE_WEBGEAR	0

/*
 * PCMCIA driver definition
 */
static int       ray_pccard_init	__P((struct pccard_devinfo *dev_p));
static void      ray_pccard_unload	__P((struct pccard_devinfo *dev_p));
static int       ray_pccard_intr	__P((struct pccard_devinfo *dev_p));

PCCARD_MODULE(ray, ray_pccard_init, ray_pccard_unload, ray_pccard_intr, 0, net_imask);

/*
 * ISA driver definition
 */
static int       ray_probe		__P((struct isa_device *dev));
static int       ray_attach		__P((struct isa_device *dev));
struct isa_driver raydriver = {
    ray_probe,
    ray_attach,
    "ray",
    1
};

/*
 * Network driver definition
 */
static void      ray_start		__P((struct ifnet *ifp));
static int       ray_ioctl		__P((struct ifnet *ifp, u_long command, caddr_t data));
static void      ray_watchdog		__P((struct ifnet *ifp));
static void      ray_init		__P((void *xsc));
static void      ray_stop		__P((struct ray_softc *sc));

/*
 * Internal utilites
 */
static int	ray_alloc_ccs		__P((struct ray_softc *sc, size_t *ccsp, u_int cmd, u_int track));
static void	ray_ccs_done		__P((struct ray_softc *sc, size_t ccs));
static void	ray_download_params	__P((struct ray_softc *sc));
static void	ray_download_timo	__P((void *xsc));
static u_int8_t	ray_free_ccs 		__P((struct ray_softc *sc, size_t ccs));
static int	ray_issue_cmd		__P((struct ray_softc *sc, size_t ccs, u_int track));
static void	ray_rcs_intr		__P((struct ray_softc *sc, size_t ccs));
static void	ray_rx			__P((struct ray_softc *sc, size_t rcs));
static void	ray_start_done		__P((struct ray_softc *sc, size_t ccs, u_int8_t status));
static void	ray_start_timo		__P((void *xsc));
static size_t	ray_start_wrhdr		__P((struct ray_softc *sc, struct ether_header *eh, size_t bufp));
static void	ray_start_join_done	__P((struct ray_softc *sc, size_t ccs, u_int8_t status));
#if RAY_NEED_STARTJOIN_TIMO
static void	ray_start_join_timo	__P((void *xsc));
#endif /* RAY_NEED_STARTJOIN_TIMO */
#if RAY_DEBUG > 50
static void	ray_dump_mbuf		__P((struct ray_softc *sc, struct mbuf *m, char *s));
#endif /* RAY_DEBUG > 50 */

/*
 * Indirections for reading/writing shared memory - from NetBSD/if_ray.c
 */
#ifndef offsetof
#define offsetof(type, member) \
    ((size_t)(&((type *)0)->member))
#endif /* offsetof */

#define	SRAM_READ_1(sc, off) \
    (u_int8_t)*((sc)->maddr + (off))
/* ((u_int8_t)bus_space_read_1((sc)->sc_memt, (sc)->sc_memh, (off))) */

#define	SRAM_READ_FIELD_1(sc, off, s, f) \
    SRAM_READ_1(sc, (off) + offsetof(struct s, f))

#define	SRAM_READ_FIELD_2(sc, off, s, f)			\
    ((((u_int16_t)SRAM_READ_1(sc, (off) + offsetof(struct s, f)) << 8) \
    |(SRAM_READ_1(sc, (off) + 1 + offsetof(struct s, f)))))

#define	SRAM_READ_FIELD_N(sc, off, s, f, p, n)	\
    ray_read_region(sc, (off) + offsetof(struct s, f), (p), (n))

#define ray_read_region(sc, off, vp, n) \
    bcopy((sc)->maddr + (off), (vp), (n))

#define	SRAM_WRITE_1(sc, off, val)	\
    *((sc)->maddr + (off)) = (val)
/* bus_space_write_1((sc)->sc_memt, (sc)->sc_memh, (off), (val)) */

#define	SRAM_WRITE_FIELD_1(sc, off, s, f, v) 	\
    SRAM_WRITE_1(sc, (off) + offsetof(struct s, f), (v))

#define	SRAM_WRITE_FIELD_2(sc, off, s, f, v) do {	\
    SRAM_WRITE_1(sc, (off) + offsetof(struct s, f), (((v) >> 8 ) & 0xff)); \
    SRAM_WRITE_1(sc, (off) + 1 + offsetof(struct s, f), ((v) & 0xff)); \
} while (0)

#define	SRAM_WRITE_FIELD_N(sc, off, s, f, p, n)	\
    ray_write_region(sc, (off) + offsetof(struct s, f), (p), (n))

#define ray_write_region(sc, off, vp, n) \
    bcopy((vp), (sc)->maddr + (off), (n))

/*
 * Macro's
 */
#ifndef RAY_CCS_TIMEOUT
#define RAY_CCS_TIMEOUT		(hz / 2)
#endif
#ifndef RAY_START_TIMEOUT
#define RAY_START_TIMEOUT	(hz / 2)
#endif
#define RAY_CCS_FREE(sc, ccs) \
    SRAM_WRITE_FIELD_1((sc), (ccs), ray_cmd, c_status, RAY_CCS_STATUS_FREE)
#define RAY_ECF_READY(sc)	(!(ray_read_reg(sc, RAY_ECFIR) & RAY_ECFIR_IRQ))
#define	RAY_ECF_START_CMD(sc)	ray_attr_write((sc), RAY_ECFIR, RAY_ECFIR_IRQ)
#define	RAY_HCS_CLEAR_INTR(sc)	ray_attr_write((sc), RAY_HCSIR, 0)
#define RAY_HCS_INTR(sc)	(ray_read_reg(sc, RAY_HCSIR) & RAY_HCSIR_IRQ)

/*
 * XXX
 * As described in if_xe.c...
 *
 * Horrid stuff for accessing CIS tuples and remapping common memory...
 * XXX
 */
#define CARD_MAJOR		50
static int	ray_attr_write	__P((struct ray_softc *sc, off_t offset, u_int8_t byte));
static int	ray_attr_read	__P((struct ray_softc *sc, off_t offset, u_int8_t *buf, int size));
static u_int8_t	ray_read_reg	__P((struct ray_softc *sc, off_t reg));

#if (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP)
static void	ray_attr_getmap	__P((struct ray_softc *sc));
static void	ray_attr_cm	__P((struct ray_softc *sc));
#define	RAY_MAP_CM(sc)		ray_attr_cm(sc)
#else
#define RAY_MAP_CM(sc)
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

/*
 * PCCard initialise.
 */
static int
ray_pccard_init (dev_p)
    struct pccard_devinfo   *dev_p;
{
    struct ray_softc	*sc;
    int			doRemap;

    RAY_DPRINTFN(5, ("ray%d: PCCard probe\n", dev_p->isahd.id_unit));

    if (dev_p->isahd.id_unit >= NRAY)
	return(ENODEV);

    sc = &ray_softc[dev_p->isahd.id_unit];

#if (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP)
    sc->slotnum = dev_p->slt->slotnum;
    ray_attr_getmap(sc);
    RAY_DPRINTFN(1, ("ray%d: Memory window flags 0x%02x, start %p, size 0x%x, card address 0x%lx\n", sc->unit, sc->md.flags, sc->md.start, sc->md.size, sc->md.card));
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

#if RAY_NEED_CM_FIXUP
    doRemap = 0;
    if (sc->md.flags != MDF_ACTIVE) {
	printf("ray%d: Fixing up CM flags from 0x%x to 0x40\n",
		sc->unit, sc->md.flags);
	doRemap = 1;
	sc->md.flags = MDF_ACTIVE;
    }
    if (sc->md.size != 0xc000) {
	printf("ray%d: Fixing up CM size from 0x%x to 0xc000\n",
		sc->unit, sc->md.size);
	doRemap = 1;
	sc->md.size = 0xc000;
	dev_p->isahd.id_msize = sc->md.size;
    }
    if (sc->md.card != 0) {
	printf("ray%d: Fixing up CM card address from 0x%lx to 0x0\n",
		sc->unit, sc->md.card);
	doRemap = 1;
	sc->md.card = 0;
    }
    if (doRemap)
    	ray_attr_cm(sc);
#endif /* RAY_NEED_CM_FIXUP */

    sc->gone = 0;
    sc->unit = dev_p->isahd.id_unit;
    sc->maddr = dev_p->isahd.id_maddr;
    sc->flags = dev_p->isahd.id_flags;

    printf("ray%d: <Raylink/IEEE 802.11> maddr 0x%lx msize 0x%x irq %d flags 0x%x on isa (PC-Card slot %d)\n",
	sc->unit,
	(unsigned long)sc->maddr,
	dev_p->isahd.id_msize,
	ffs(dev_p->isahd.id_irq) - 1,
	sc->flags,
	sc->slotnum);

    if (ray_attach(&dev_p->isahd))
	return(ENXIO);

    return(0);
}

/*
 * PCCard unload.
 */
static void
ray_pccard_unload (dev_p)
    struct pccard_devinfo	*dev_p;
{
    struct ray_softc		*sc;
    struct ifnet		*ifp;

    RAY_DPRINTFN(5, ("ray%d: PCCard unload\n", dev_p->isahd.id_unit));

    sc = &ray_softc[dev_p->isahd.id_unit];

    if (sc->gone) {
	printf("ray%d: already unloaded\n", sc->unit);
	return;
    }

/* XXX shouldn't we call _stop? */
    /* Cleardown interface */
    ifp = &sc->arpcom.ac_if;
    ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
    if_down(ifp); /* XXX probably should be if_detach but I don't know if it works in 3.1 */

    /* Mark card as gone */
    sc->gone = 1;
    printf("ray%d: unloaded\n", sc->unit);

    return;
}

/*
 * PCCard interrupt.
 */
static int
ray_pccard_intr (dev_p)
    struct pccard_devinfo	*dev_p;
{
    struct ray_softc		*sc;
    struct ifnet		*ifp;
    int				ccsi, handled;

    RAY_DPRINTFN(5, ("ray%d: PCCard intr\n", dev_p->isahd.id_unit));

    sc = &ray_softc[dev_p->isahd.id_unit];
    ifp = &sc->arpcom.ac_if;
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before interrupt!\n", sc->unit);
	return(0);
    }

    /*
     * Check that the interrupt was for us, if so get the rcs/ccs and vector
     * on the command contained within it.
     */
    if (!RAY_HCS_INTR(sc)) {

	handled = 0;

    } else {

	handled = 1;
	ccsi = SRAM_READ_1(sc, RAY_SCB_RCSI);
	if (ccsi <= RAY_CCS_LAST)
	    ray_ccs_done(sc, RAY_CCS_ADDRESS(ccsi));
	else if (ccsi <= RAY_RCS_LAST)
	    ray_rcs_intr(sc, RAY_CCS_ADDRESS(ccsi));
	else
	    printf("ray%d: ray_intr bad ccs index %d\n", sc->unit, ccsi);

    }

    if (handled)
	RAY_HCS_CLEAR_INTR(sc);

    RAY_DPRINTFN(10, ("ray%d: interrupt %s handled\n",
	    sc->unit, handled?"was":"not"));

    /* Send any packets lying around */
    if (!(ifp->if_flags & IFF_OACTIVE) && (ifp->if_snd.ifq_head != NULL))
    	ray_start(ifp);

    return(handled);
}

/*
 * ISA probe routine.
 */
static int
ray_probe (dev_p)
    struct isa_device		*dev_p;
{

    RAY_DPRINTFN(5, ("ray%d: ISA probe\n", dev_p->id_unit));

    return(0);
}

/*
 * ISA/PCCard attach.
 */
static int
ray_attach (dev_p)
    struct isa_device		*dev_p;
{
    struct ray_softc		*sc;
    struct ray_ecf_startup_v5	*ep;
    struct ifnet		*ifp;
    char			ifname[IFNAMSIZ];

    RAY_DPRINTFN(5, ("ray%d: ISA/PCCard attach\n", dev_p->id_unit));

    sc = &ray_softc[dev_p->id_unit];
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before attach!\n", sc->unit);
	return(0);
    }

    /*
     * Read startup results, check the card is okay and work out what
     * version we are using.
     */
    ep = &sc->sc_ecf_startup;
    ray_read_region(sc, RAY_ECF_TO_HOST_BASE, ep, sizeof(sc->sc_ecf_startup));
    if (ep->e_status != RAY_ECFS_CARD_OK) {
	printf("ray%d: card failed self test: status 0x%b\n", sc->unit,
	    ep->e_status,
	    "\020"			/* print in hex */
	    "\001RESERVED0"
	    "\002PROC_SELF_TEST"
	    "\003PROG_MEM_CHECKSUM"
	    "\004DATA_MEM_TEST"
	    "\005RX_CALIBRATION"
	    "\006FW_VERSION_COMPAT"
	    "\007RERSERVED1"
	    "\008TEST_COMPLETE"
	);
	return(0);
    }
    if (sc->sc_version != RAY_ECFS_BUILD_4 &&
        sc->sc_version != RAY_ECFS_BUILD_5
       ) {
	printf("ray%d: unsupported firmware version 0x%0x\n", sc->unit,
	    ep->e_fw_build_string);
	return(0);
    }

    if (bootverbose || RAY_DEBUG) {
	printf("ray%d: Start Up Results\n", sc->unit);
	if (RAY_DEBUG > 10)
	    RAY_DHEX8((u_int8_t *)sc->maddr + RAY_ECF_TO_HOST_BASE, 0x40);
	if (sc->sc_version == RAY_ECFS_BUILD_4)
	    printf("  Firmware version 4\n");
	else
	    printf("  Firmware version 5\n");
	printf("  Status 0x%x\n", ep->e_status);
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
	    printf("  Firmware version %0x\n", ep->e_fw_build_string);
	    printf("  Firmware revision %0x\n", ep->e_fw_build);
	    printf("  (reserved word) %0x\n", ep->e_fw_resv);
	    printf("  ASIC version %0x\n", ep->e_asic_version);
	    printf("  TIB size %0x\n", ep->e_tibsize);
	}
    }

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
    ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX ); /* XXX - IFF_MULTICAST */
#if XXX
    ifp->if_hdr = ...; make this big enough to hold the .11 and .3 headers
#endif
    ifp->if_baudrate = 1000000; /* Is this baud or bps ;-) */

    ifp->if_output = ether_output;
    ifp->if_start = ray_start;
    ifp->if_ioctl = ray_ioctl;
    ifp->if_watchdog = ray_watchdog;
    ifp->if_init = ray_init;
    ifp->if_snd.ifq_maxlen = RAY_CCS_TX_LAST;

    /*
     * If this logical interface has already been attached,
     * don't attach it again or chaos will ensue.
     */
    sprintf(ifname, "ray%d", sc->unit);

    if (ifunit(ifname) == NULL) {
	callout_handle_init(&sc->timerh);
#if RAY_NEED_STARTJOIN_TIMO
	callout_handle_init(&sc->sj_timerh);
#endif /* RAY_NEED_STARTJOIN_TIMO */
	callout_handle_init(&sc->start_timerh);
	if_attach(ifp);
	ether_ifattach(ifp);
#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif /* NBFFILTER */

#if XXX
	this looks like a good idea
	at_shutdown(ray_shutdown, sc, SHUTDOWN_POST_SYNC);
#endif /* XXX */
    }

    return(0);
}

/*
 * Network start.
 *
 * Start sending a packet.
 *
 * We make two assumptions here:
 *  1) That the current priority is set to splimp _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) That the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
static void
ray_start (ifp)
    register struct ifnet	*ifp;
{
    struct ray_softc		*sc;
    struct mbuf			*m0, *m;
    struct ether_header		*eh;
    size_t			ccs, bufp;
    int				i, pktlen, len;
    u_int8_t			status;

    RAY_DPRINTFN(5, ("ray%d: Network start\n", ifp->if_unit));

    sc = ifp->if_softc;
    RAY_MAP_CM(sc);

    /*
     * Some simple checks first
     */
    if (sc->gone) {
	printf("ray%d: unloaded before start!\n", sc->unit);
	return;
    }
    if ((ifp->if_flags & IFF_RUNNING) == 0 || !sc->sc_havenet)
	return;
    if (!RAY_ECF_READY(sc)) {
	sc->start_timerh = timeout(ray_start_timo, sc, RAY_START_TIMEOUT);
	return;
    } else
	untimeout(ray_start_timo, sc, sc->start_timerh);

    /*
     * Simple one packet at a time TX routine - probably appaling performance
     * and we certainly chew CPU. However bing to windows boxes shows
     * a reliance on the far end too:
     *
     * Libretto 50CT (75MHz Pentium) with FreeBSD-3.1 to
     *   Nonname box Windows 95C (133MHz AMD 5x86) 		 996109bps
     *   AST J30 Windows 95A (100MHz Pentium) 			1307791bps
     *
     * Flow is
     *		get a ccs
     *		build the packet
     *		set IFF_OACTIVE
     *		interrupt the card to send the packet
     *		exit
     *
     *		wait for interrupt telling us the packet has been sent
     *		clear IFF_OACTIVE
     *		get called by the interrupt routine if any packets left
     */

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
	status = SRAM_READ_FIELD_1(sc, RAY_CCS_ADDRESS(i), ray_cmd, c_status);
	if (status == RAY_CCS_STATUS_FREE)
	    break;
	i++;
    } while (i <= RAY_CCS_TX_LAST);
    if (i > RAY_CCS_TX_LAST) {
	ifp->if_flags |= IFF_OACTIVE;
	return;
    }
    RAY_DPRINTFN(20, ("ray%d: ray_start using ccs 0x%02x\n", sc->unit, i));

    /*
     * Reserve and fill the ccs - must do the length later.
     *
     * Even though build 4 and build 5 have different fields all these
     * are common apart from tx_rate. This will be overwritten later if
     * needed.
     */
    ccs = RAY_CCS_ADDRESS(i);
    bufp = RAY_TX_BASE + i * RAY_TX_BUF_SIZE;
    bufp += sc->sc_tibsize;
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_status, RAY_CCS_STATUS_BUSY);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_cmd, RAY_CMD_TX_REQ);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_link, RAY_CCS_LINK_NULL);
    SRAM_WRITE_FIELD_2(sc, ccs, ray_cmd_tx, c_bufp, bufp);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_tx_rate, sc->sc_def_txrate);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_apm_mode, 0);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_antenna, 0);
    bufp += sizeof(struct ray_tx_phy_header);
    
    /*
     * Get the mbuf and process it - we have to remember to free the
     * ccs if there are any errors
     */
    IF_DEQUEUE(&ifp->if_snd, m0);
    if (m0 == NULL) {
	RAY_CCS_FREE(sc, ccs);
	return;
    }

    for (pktlen = 0, m = m0; m != NULL; m = m->m_next) {
	pktlen += m->m_len;
    }
    if (pktlen > ETHER_MAX_LEN - ETHER_CRC_LEN) {
	RAY_DPRINTFN(1, ("ray%d: mbuf too long %d\n", sc->unit, pktlen));
	RAY_CCS_FREE(sc, ccs);
	ifp->if_oerrors++;
	m_freem(m0);
	return;
    }

    /*
     * Translation - capability as described earlier
     *
     * Each case must write the 802.11 header using ray_start_wrhdr,
     * passing a pointer to the ethernet header in and getting a new
     * tc buffer pointer. Next remove/modify/addto the 802.3 and 802.2
     * headers as needed.
     *
     * We've pulled up the mbuf for you.
     */
    if (m0->m_len < sizeof(struct ether_header))
    	m = m_pullup(m, sizeof(struct ether_header));
    if (m0 == NULL) {
	RAY_DPRINTFN(1, ("ray%d: ray_start could not pullup ether\n", sc->unit));
	RAY_CCS_FREE(sc, ccs);
	ifp->if_oerrors++;
	return;
    }
    eh = mtod(m0, struct ether_header *);
    switch (sc->translation) {

    	case SC_TRANSLATE_WEBGEAR:
	    bufp = ray_start_wrhdr(sc, eh, bufp);
	    break;

	default:
	    printf("ray%d: ray_start unknown translation type 0x%x - why?\n",
	    		sc->unit, sc->translation);
	    RAY_CCS_FREE(sc, ccs);
	    ifp->if_oerrors++;
	    m0 = m_free(m0);
	    return;

    }
    if (m0 == NULL) {
	RAY_DPRINTFN(1, ("ray%d: ray_start could not translate mbuf\n", sc->unit));
	RAY_CCS_FREE(sc, ccs);
	ifp->if_oerrors++;
	return;
    }
    pktlen = sizeof(struct ieee80211_header);

    /*
     * Copy the mbuf to the buffer in common memory
     *
     * We panic and don't bother wrapping as ethernet packets are 1518
     * bytes, we checked the mbuf earlier, and our TX buffers are 2048
     * bytes. We don't have 530 bytes of headers etc. so something
     * must be fubar.
     */
    for (m = m0; m != NULL; m = m->m_next) {
	pktlen += m->m_len;
	if ((len = m->m_len) == 0)
	    continue;
	if ((bufp + len) < RAY_TX_END)
	    ray_write_region(sc, bufp, mtod(m, u_int8_t *), len);
	else 
	    panic("ray%d: ray_start tx buffer overflow\n", sc->unit);
	bufp += len;
    }
    RAY_DMBUF_DUMP(sc, m0, "ray_start");
    m_free(m0);

    /*
     * Fill in a few loose ends and kick the card to send the packet
     */
    if (!RAY_ECF_READY(sc)) {
	/*
	 * From NetBSD code:
	 *
	 * If this can really happen perhaps we need to save
	 * the chain and use it later.  I think this might
	 * be a confused state though because we check above
	 * and don't issue any commands between.
	 */
	printf("ray%d: ray_tx device busy\n", sc->unit);
	RAY_CCS_FREE(sc, ccs);
	ifp->if_oerrors++;
	return;
    }
    SRAM_WRITE_FIELD_2(sc, ccs, ray_cmd_tx, c_len, pktlen);
    SRAM_WRITE_1(sc, RAY_SCB_CCSI, ccs);
    ifp->if_opackets++;
    ifp->if_flags |= IFF_OACTIVE;
    RAY_ECF_START_CMD(sc);

#if XXX
#if NBPFILTER > 0
    /* XXX but the translation code dicks around with the mbuf structure sigh */
    /* XXX and I've freed the mbuf already */
    if (ifp->if_bpf)
	bpf_mtap(ifp, m0);
#endif
#endif /* XXX */

    return;
}
#if XXX
netbsd

driver uses a loop
    repeat
	get a ccs
	get a mbuf
	translate and send packet to shared ram
    until (no more ccs's) || (no more mbuf's)

    send ccs chain to card

    exit

Linux

driver is simple single shot packet (with a lot of spinlocks!)

general

the tx space is 0x7000 = 28kB, and TX  buffer size is 2048 so there
can be 14 requests at 2kB each

from this 2k we have to remove the TIB - whatever that is - for data


netbsd:
	we need to call _start after receiveing a packet to see
	if any packets were queued whilst in the interrupt

	there is a potential race in obtaining ccss for the tx, in that
	we might be in _start synchronously and then an rx interrupt
	occurs. the rx will call _start and steal tx ccs from underneath
	the interrupted entry.

	toptions
		dont call _start from rx interrupt

		find a safe way of locking

		find a better way of obtaining ccs using next free avilable?

		look at other drivers

		use tsleep/wakeup

		some form of ring to hold ccs

		free lsit

		rework calling

#endif

/*
 * TX completion routine.
 *
 * Clear ccs and network flags.
 */
static void
ray_start_done (sc, ccs, status)
    struct ray_softc	*sc;
    size_t		ccs;
    u_int8_t		status;
{
    struct ifnet	*ifp;
    char		*status_string[] = RAY_CCS_STATUS_STRINGS;

    RAY_DPRINTFN(5, ("ray%d: ray_start_done\n", sc->unit));
    RAY_MAP_CM(sc);

    ifp = &sc->arpcom.ac_if;

    if (status != RAY_CCS_STATUS_COMPLETE) {
    	printf("ray%d: ray_start tx completed but status is %s.\n",
		sc->unit, status_string[status]);
    	ifp->if_oerrors++;
    }

    RAY_CCS_FREE(sc, ccs);
    ifp->if_timer = 0;
    if (ifp->if_flags & IFF_OACTIVE)
	ifp->if_flags &= ~IFF_OACTIVE;

    return;
}

/*
 * Start timeout routine.
 *
 * Used when card was busy but we needed to send a packet.
 */
static void
ray_start_timo (xsc)
    void		*xsc;
{
    struct ray_softc	*sc = xsc;
    struct ifnet	*ifp;
    int			s;

    RAY_DPRINTFN(5, ("ray%d: ray_start_timo\n", sc->unit));
    RAY_MAP_CM(sc);

    ifp = &sc->arpcom.ac_if;

    if (!(ifp->if_flags & IFF_OACTIVE) && (ifp->if_snd.ifq_head != NULL)) {
	s = splimp();
	ray_start(ifp);
	splx(s);
    }

    return;
}

/*
 * Write an 802.11 header into the TX buffer and return the
 * adjusted buffer pointer.
 */
static size_t
ray_start_wrhdr (sc, eh, bufp)
    struct ray_softc		*sc;
    struct ether_header		*eh;
    size_t			bufp;
{
    struct ieee80211_header	header;

    RAY_DPRINTFN(5, ("ray%d: ray_start_wrhdr\n", sc->unit));
    RAY_MAP_CM(sc);

    bzero(&header, sizeof(struct ieee80211_header));

    header.i_fc[0] = (IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA);
    if (sc->sc_net_type == RAY_MIB_NET_TYPE_ADHOC) {

	header.i_fc[1] = IEEE80211_FC1_STA_TO_STA;
	bcopy(eh->ether_dhost, header.i_addr1, ETHER_ADDR_LEN);
	bcopy(eh->ether_shost, header.i_addr2, ETHER_ADDR_LEN);
	bcopy(sc->sc_bss_id, header.i_addr3, ETHER_ADDR_LEN);

    } else {
#if XXX_INFRA
	if (sc->sc_ap_status == RAY_MIB_AP_STATUS_TERMINAL) {
	    
	    header.i_fc[1] = IEEE80211_FC1_STA_TO_AP;
	    bcopy(sc->sc_bss_id, header.i_addr1, ETHER_ADDR_LEN);
	    bcopy(eh->ether_shost, header.i_addr2, ETHER_ADDR_LEN);
	    bcopy(eh->ether_dhost, header.i_addr3, ETHER_ADDR_LEN);

	} else
	    panic("ray%d: ray_start can't be an AP yet\n", sc-.unit);
#endif /* XXX_INFRA */
    }

    ray_write_region(sc, bufp, (u_int8_t *)&header,
	    sizeof(struct ieee80211_header));

    return(bufp + sizeof(struct ieee80211_header));
}

/*
 * Network ioctl request.
 */
static int
ray_ioctl (ifp, command, data)
    register struct ifnet	*ifp;
    u_long			command;
    caddr_t			data;
{
    struct ray_softc *sc;
    int s, error = 0;

    RAY_DPRINTFN(5, ("ray%d: Network ioctl\n", ifp->if_unit));

    sc = ifp->if_softc;
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before ioctl!\n", sc->unit);
	ifp->if_flags &= ~IFF_RUNNING;
	return(ENXIO);
    }

    s = splimp();

    switch (command) {

	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
	    RAY_DPRINTFN(30, ("ray%d: ioctl SIFADDR/GIFADDR/SIFMTU\n", sc->unit));
	    error = ether_ioctl(ifp, command, data);
	    break;

	case SIOCSIFFLAGS:
	    RAY_DPRINTFN(30, ("ray%d: for SIFFLAGS\n", sc->unit));
	    /*
	     * If the interface is marked up and stopped, then start
	     * it. If it is marked down and running, then stop it.
	     */
	    if (ifp->if_flags & IFF_UP) {
		if (!(ifp->if_flags & IFF_RUNNING))
		    ray_init(sc);
	    } else {
		if (ifp->if_flags & IFF_RUNNING)
		    ray_stop(sc);
	    }
	    /* DROP THROUGH */

#if XXX
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	    RAY_DPRINTFN(30, ("ray%d: ioctl called for ADDMULTI/DELMULTI\n, sc->unit"));
	    /*
	     * Multicast list has (maybe) changed; set the hardware filter
	     * accordingly. This also serves to deal with promiscuous mode
	     * if we have a BPF listener active.
	     */
	    ray_setmulti(sc);
#endif /* XXX */

	    error = 0;
	    break;

case SIOCGIFFLAGS:
    RAY_DPRINTFN(30, ("ray%d: ioctl called for GIFFLAGS\n", sc->unit));
    error = EINVAL;
    break;
case SIOCGIFMETRIC:
    RAY_DPRINTFN(30, ("ray%d: ioctl called for GIFMETRIC\n", sc->unit));
    error = EINVAL;
    break;
case SIOCGIFMTU:
    RAY_DPRINTFN(30, ("ray%d: ioctl called for GIFMTU\n", sc->unit));
    error = EINVAL;
    break;
case SIOCGIFPHYS:
    RAY_DPRINTFN(30, ("ray%d: ioctl called for GIFPYHS\n", sc->unit));
    error = EINVAL;
    break;
case SIOCSIFMEDIA:
    RAY_DPRINTFN(30, ("ray%d: ioctl called for SIFMEDIA\n", sc->unit));
    error = EINVAL;
    break;
case SIOCGIFMEDIA:
    RAY_DPRINTFN(30, ("ray%d: ioctl called for GIFMEDIA\n", sc->unit));
#if RAY_DUMP_CM_ON_GIFMEDIA
    RAY_DPRINTFN(10, ("ray%d: RAY_SCB\n", sc->unit));
    RAY_DHEX8((u_int8_t *)sc->maddr + RAY_SCB_BASE, 0x20);
    RAY_DPRINTFN(10, ("ray%d: RAY_STATUS\n", sc->unit));
    RAY_DNET_DUMP(sc, ".");
#endif /* RAY_DUMP_CM_ON_GIFMEDIA */
    error = EINVAL;
    break;

	default:
	    error = EINVAL;
    }

    splx(s);

    return(error);
}

static void
ray_watchdog (ifp)
    register struct ifnet	*ifp;
{
    struct ray_softc *sc;

    RAY_DPRINTFN(5, ("ray%d: Network watchdog\n", ifp->if_unit));

    sc = ifp->if_softc;
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before watchdog!\n", sc->unit);
	return;
    }

    printf("ray%d: watchdog timeout\n", sc->unit);

/* XXX may need to have remedial action here
   for example
   	ray_reset - may be useful elsewhere
		ray_stop
		...
		ray_init

    do we only use on TX?
    	if so then we should clear OACTIVE etc.

*/

    return;
}

/*
 * Network initialisation.
 *
 * Start up flow is as follows.
 * The kernel calls ray_init when the interface is assigned an address.
 * 
 * ray_init does a bit of house keeping before calling ray_download_params.
 *
 * ray_download_params fills the startup parameter structure out and
 * sends it to the card. The download command simply completes so we
 * use schedule a timeout function call to ray_download_timo instead
 * of spin locking. We pass the ccs in use via sc->sc_css.
 *
 * ray_download_timo checks the ccs for command completion and/or
 * errors. Then it tells the card to start an adhoc network or join a
 * managed network. This should complete via the interrupt mechanism,
 * but the NetBSD driver includes a timeout for some buggy stuff
 * somewhere - I've left the hooks in but don't use them. The interrupt
 * handler passes control to ray_start_join_done - the ccs is handled
 * by the interrupt mechanism.
 *
 * Once ray_start_join_done has checked the ccs and uploaded/updated
 * the network parameters we are ready to process packets. It is then
 * safe to call ray_start which is done by the interrupt handler.
 */
static void
ray_init (xsc)
    void			*xsc;
{
    struct ray_softc		*sc = xsc;
    struct ray_ecf_startup_v5	*ep;
    struct ifnet		*ifp;
    size_t			ccs;
    int				i;

    RAY_DPRINTFN(5, ("ray%d: Network init\n", sc->unit));
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before init!\n", sc->unit);
	return;
    }

    ifp = &sc->arpcom.ac_if;

    if ((ifp->if_flags & IFF_RUNNING))
	ray_stop(sc);

    /*
     * Reset instance variables
     *
     * The first set are network parameters that are fully initialised
     * when the card starts or joins the network.
     *
     * The second set are network parameters that are downloaded to
     * the card.
     *
     * The third set are driver parameters.
     *
     * All of the variables in these sets can be updated by the card or ioctls.
     */
#if XXX
    see the ray_attach section for stuff to move
#endif
    sc->sc_upd_param = 0;
    bzero(sc->sc_bss_id, sizeof(sc->sc_bss_id));
    sc->sc_inited = 0;
    sc->sc_def_txrate = 0;
    sc->sc_encrypt = 0;

    sc->sc_net_type = RAY_MIB_NET_TYPE_DEFAULT;
    bzero(sc->sc_ssid, sizeof(sc->sc_ssid));
    strncpy(sc->sc_ssid, RAY_MIB_SSID_DEFAULT, RAY_MAXSSIDLEN);
    sc->sc_priv_start = RAY_MIB_PRIVACY_MUST_START_DEFAULT;
    sc->sc_priv_join = RAY_MIB_PRIVACY_CAN_JOIN_DEFAULT;
#if XXX_MCASTPROM
    sc->sc_promisc = !!(sc->sc_if.if_flags & (IFF_PROMISC|IFF_ALLMULTI));
#endif /* XXX_MCASTPROM */

    sc->sc_havenet = 0;
    sc->translation = SC_TRANSLATE_WEBGEAR;

    /* Set all ccs to be free */
    bzero(sc->sc_ccsinuse, sizeof(sc->sc_ccsinuse));
    sc->sc_ccs = RAY_CCS_LAST + 1;
    ccs = RAY_CCS_ADDRESS(0);
    for (i = 0; i < RAY_CCS_LAST; ccs += RAY_CCS_SIZE, i++)
	    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_FREE);

    /* Clear any pending interrupts */
    RAY_HCS_CLEAR_INTR(sc);

#if XXX
    Not sure why I really need this - maybe best to deal with
    this when resets are requested by me?
#endif /* XXX */
    /*
     * Get startup results - the card may have been reset
     */
    ep = &sc->sc_ecf_startup;
    ray_read_region(sc, RAY_ECF_TO_HOST_BASE, ep, sizeof(sc->sc_ecf_startup));
    if (ep->e_status != RAY_ECFS_CARD_OK) {
	printf("ray%d: card failed self test: status 0x%b\n", sc->unit,
	    ep->e_status,
	    "\020"			/* print in hex */
	    "\001RESERVED0"
	    "\002PROC_SELF_TEST"
	    "\003PROG_MEM_CHECKSUM"
	    "\004DATA_MEM_TEST"
	    "\005RX_CALIBRATION"
	    "\006FW_VERSION_COMPAT"
	    "\007RERSERVED1"
	    "\008TEST_COMPLETE"
	);
	return; /* XXX This doesn't mark the interface as down */
    }

    /*
     * Fixup tib size to be correct - on build 4 it is garbage
     */
    if (sc->sc_version == RAY_ECFS_BUILD_4 && sc->sc_tibsize == 0x55)
	sc->sc_tibsize = sizeof(struct ray_tx_tib);

    /*
     * We are now up and running. Next we have to download network
     * configuration into the card. We are busy until download is done.
     */
    ifp->if_flags |= IFF_RUNNING | IFF_OACTIVE;

    ray_download_params(sc);

    return;
}

/*
 * Network stop.
 */
static void
ray_stop (sc)
    struct ray_softc	*sc;
{
    struct ifnet	*ifp;

    RAY_DPRINTFN(5, ("ray%d: Network stop\n", sc->unit));
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before stop!\n", sc->unit);
	return;
    }

    ifp = &sc->arpcom.ac_if;

    /* XXX how do we clear ccs etc. properly */
    /* XXX how do we inhibit interrupts properly */
    /* XXX do these matter are we always restated with an _init? */

    untimeout(ray_download_timo, sc, sc->timerh);
#if RAY_NEED_STARTJOIN_TIMO
    untimeout(ray_start_join_timo, sc, sc->sj_timerh);
#endif /* RAY_NEED_STARTJOIN_TIMO */
    untimeout(ray_start_timo, sc, sc->start_timerh);

    /* Mark as not running */
    ifp->if_flags &= ~IFF_RUNNING;

    return;
}

/*
 * Process CCS command completion - called from ray_intr
 */
static void
ray_ccs_done (sc, ccs)
    struct ray_softc	*sc;
    size_t		ccs;
{
    u_int		cmd, status;
    
    RAY_DPRINTFN(5, ("ray%d: ray_ccs_done\n", sc->unit));
    RAY_MAP_CM(sc);

    cmd = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd);
    status = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
    RAY_DPRINTFN(20, ("ray%d: ccs idx %d ccs 0x%x cmd 0x%x status %d\n",
    		sc->unit, RAY_CCS_INDEX(ccs), ccs, cmd, status));

    switch (cmd) {
	case RAY_CMD_START_PARAMS:
	    printf("ray%d: ray_ccs_done got START_PARAMS - why?\n", sc->unit);
	    break;

	case RAY_CMD_UPDATE_PARAMS:
	    RAY_DPRINTFN(20, ("ray%d: ray_ccs_done got UPDATE_PARAMS\n",
		    sc->unit));
	    XXX;
	    break;

	case RAY_CMD_REPORT_PARAMS:
	    RAY_DPRINTFN(20, ("ray%d: ray_ccs_done got REPORT_PARAMS\n",
		    sc->unit));
	    XXX;
	    break;

	case RAY_CMD_UPDATE_MCAST:
	    RAY_DPRINTFN(20, ("ray%d: ray_ccs_done got UPDATE_MCAST\n",
		    sc->unit));
	    XXX;
	    break;

	case RAY_CMD_UPDATE_APM:
	    RAY_DPRINTFN(20, ("ray%d: ray_ccs_done got UPDATE_APM\n",
		    sc->unit));
	    XXX;
	    break;

	case RAY_CMD_START_NET:
	case RAY_CMD_JOIN_NET:
	    RAY_DPRINTFN(20, ("ray%d: ray_ccs_done got START|JOIN_NET\n",
		    sc->unit));
	    ray_start_join_done(sc, ccs, status);
	    break;

	case RAY_CMD_START_ASSOC:
	    RAY_DPRINTFN(20, ("ray%d: ray_ccs_done got START_ASSOC\n",
		    sc->unit));
	    sc->sc_havenet = 1; /* Should not be here but in function */
	    XXX;
	    break;

	case RAY_CMD_TX_REQ:
	    RAY_DPRINTFN(20, ("ray%d: ray_ccs_done got TX_REQ\n",
		    sc->unit));
	    ray_start_done(sc, ccs, status);
	    return;

	case RAY_CMD_TEST_MEM:
	    printf("ray%d: ray_ccs_done got TEST_MEM - why?\n", sc->unit);
	    break;

	case RAY_CMD_SHUTDOWN:
	    printf("ray%d: ray_ccs_done got SHUTDOWN - why?\n", sc->unit);
	    break;

	case RAY_CMD_DUMP_MEM:
	    printf("ray%d: ray_ccs_done got DUMP_MEM - why?\n", sc->unit);
	    break;

	case RAY_CMD_START_TIMER:
	    printf("ray%d: ray_ccs_done got START_TIMER - why?\n", sc->unit);
    	    break;

	default:
	    printf("ray%d: ray_ccs_done unknown command 0x%x\n", sc->unit, cmd);
	    break;
    }

    ray_free_ccs(sc, ccs);

    return;
}

/*
 * Process ECF command request - called from ray_intr
 */
static void
ray_rcs_intr (sc, rcs)
    struct ray_softc	*sc;
    size_t		rcs;
{
    struct ifnet	*ifp;
    u_int		cmd, status;
    
    RAY_DPRINTFN(5, ("ray%d: ray_rcs_intr\n", sc->unit));
    RAY_MAP_CM(sc);

    ifp = &sc->arpcom.ac_if;

    cmd = SRAM_READ_FIELD_1(sc, rcs, ray_cmd, c_cmd);
    status = SRAM_READ_FIELD_1(sc, rcs, ray_cmd, c_status);
    RAY_DPRINTFN(20, ("ray%d: rcs idx %d rcs 0x%x cmd 0x%x status %d\n",
    		sc->unit, RAY_CCS_INDEX(rcs), rcs, cmd, status));

    switch (cmd) {
	case RAY_ECMD_RX_DONE:
	    RAY_DPRINTFN(20, ("ray%d: ray_rcs_intr got RX_DONE\n", sc->unit));
	    ray_rx(sc, rcs);
	    break;

	case RAY_ECMD_REJOIN_DONE:
	    RAY_DPRINTFN(20, ("ray%d: ray_rcs_intr got UPDATE_PARAMS\n",
		    sc->unit));
	    XXX;
	    break;

	case RAY_ECMD_ROAM_START:
	    RAY_DPRINTFN(20, ("ray%d: ray_rcs_intr got ROAM_START\n",
		    sc->unit));
	    sc->sc_havenet = 0; /* Should not be here but in function */
	    XXX;
	    break;

	case RAY_ECMD_JAPAN_CALL_SIGNAL:
	    printf("ray%d: ray_rcs_intr got JAPAN_CALL_SIGNAL - why?\n",
		    sc->unit);
	    break;

	default:
	    printf("ray%d: ray_rcs_intr unknown command 0x%x\n", sc->unit, cmd);
	    break;
    }

    SRAM_WRITE_FIELD_1(sc, rcs, ray_cmd, c_status, RAY_CCS_STATUS_FREE);

    return;
}

/*
 * Receive a packet
 */
static void
ray_rx (sc, rcs)
    struct ray_softc		*sc;
    size_t			rcs;
{
    struct ieee80211_header	*header;
    struct ether_header		*eh;
    struct ifnet		*ifp;
    struct mbuf			*m0;
    size_t			pktlen, fraglen, readlen, tmplen;
    size_t			bufp, ebufp;
    u_int8_t			*dst, *src;
    u_int8_t			fc;
    u_int			first, ni, i;

    RAY_DPRINTFN(5, ("ray%d: ray_rx\n", sc->unit));
    RAY_MAP_CM(sc);

    RAY_DPRINTFN(20, ("ray%d: rcs chain - using rcs 0x%x\n", sc->unit, rcs));

    ifp = &sc->arpcom.ac_if;
    m0 = NULL;
    readlen = 0;

    /*
     * Get first part of packet and the length. Do some sanity checks
     * and get a mbuf.
     */
    first = RAY_CCS_INDEX(rcs);
    pktlen = SRAM_READ_FIELD_2(sc, rcs, ray_cmd_rx, c_pktlen);

    if ((pktlen > MCLBYTES) || (pktlen < sizeof(struct ieee80211_header))) {
	RAY_DPRINTFN(1, ("ray%d: ray_rx packet is too big or too small\n",
	    sc->unit));
	ifp->if_ierrors++;
	goto skip_read;
    }

    MGETHDR(m0, M_DONTWAIT, MT_DATA);
    if (m0 == NULL) {
	RAY_DPRINTFN(1, ("ray%d: ray_rx MGETHDR failed\n", sc->unit));
	ifp->if_ierrors++;
	goto skip_read;
    }
    if (pktlen > MHLEN) {
	MCLGET(m0, M_DONTWAIT);
	if ((m0->m_flags & M_EXT) == 0) {
	    RAY_DPRINTFN(1, ("ray%d: ray_rx MCLGET failed\n", sc->unit));
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
	RAY_DPRINTFN(50, ("ray%d: ray_rx frag index %d len %d bufp 0x%x ni %d\n",
		sc->unit, i, fraglen, (int)bufp, ni));

	if (fraglen + readlen > pktlen) {
	    RAY_DPRINTFN(1, ("ray%d: ray_rx bad length current 0x%x pktlen 0x%x\n",
		    sc->unit, fraglen + readlen, pktlen));
	    ifp->if_ierrors++;
	    m_freem(m0);
	    m0 = NULL;
	    goto skip_read;
	}
	if ((i < RAY_RCS_FIRST) || (i > RAY_RCS_LAST)) {
	    printf("ray%d: ray_rx bad rcs index 0x%x\n", sc->unit, i);
	    ifp->if_ierrors++;
	    m_freem(m0);
	    m0 = NULL;
	    goto skip_read;
	}

	ebufp = bufp + fraglen;
	if (ebufp <= RAY_RX_END)
	    ray_read_region(sc, bufp, dst, fraglen);
	else {
	    ray_read_region(sc, bufp, dst, (tmplen = RAY_RX_END - bufp));
	    ray_read_region(sc, RAY_RX_BASE, dst + tmplen, ebufp - RAY_RX_END);
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
	SRAM_WRITE_FIELD_1(sc, rcs, ray_cmd, c_status, RAY_CCS_STATUS_FREE);
    }

    if (m0 == NULL)
   	return;

    RAY_DMBUF_DUMP(sc, m0, "ray_rx");

    /*
     * Check the 802.11 packet type and obtain the .11 src addresses.
     *
     * XXX CTL and MGT packets will have separate functions, DATA with here
     *
     * XXX This needs some work for INFRA mode
     */
    header = mtod(m0, struct ieee80211_header *);
    fc = header->i_fc[0];
    if ((fc & IEEE80211_FC0_VERSION_MASK) != IEEE80211_FC0_VERSION_0) {
	RAY_DPRINTFN(1, ("ray%d: header not version 0 fc 0x%x\n", sc->unit, fc));
	ifp->if_ierrors++;
	m_freem(m0);
	return;
    }
    switch (fc & IEEE80211_FC0_TYPE_MASK) {

	case IEEE80211_FC0_TYPE_MGT:
	    printf("ray%d: ray_rx got a MGT packet - why?\n", sc->unit);
	    ifp->if_ierrors++;
	    m_freem(m0);
	    return;

	case IEEE80211_FC0_TYPE_CTL:
	    printf("ray%d: ray_rx got a CTL packet - why?\n", sc->unit);
	    ifp->if_ierrors++;
	    m_freem(m0);
	    return;

	case IEEE80211_FC0_TYPE_DATA:
	    RAY_DPRINTFN(50, ("ray%d: ray_rx got a DATA packet\n", sc->unit));
	    break;

	default:
	    printf("ray%d: ray_rx got a unknown packet fc0 0x%x - why?\n",
	    		sc->unit, fc);
	    ifp->if_ierrors++;
	    m_freem(m0);
	    return;

    }
    fc = header->i_fc[1];
    switch (fc & IEEE80211_FC1_DS_MASK) {

	case IEEE80211_FC1_STA_TO_STA:
	    src = header->i_addr2;
	    RAY_DPRINTFN(50, ("ray%d: ray_rx packet from sta %6D\n",
		    sc->unit, src, ":"));
	    break;

	case IEEE80211_FC1_STA_TO_AP:
	    RAY_DPRINTFN(1, ("ray%d: ray_rx packet from sta %6D to ap %6D\n",
		    sc->unit,
		    header->i_addr2, ":", header->i_addr3, ":"));
	    ifp->if_ierrors++;
	    m_freem(m0);
	    break;

	case IEEE80211_FC1_AP_TO_STA:
	    RAY_DPRINTFN(1, ("ray%d: ray_rx packet from ap %6D\n",
		    sc->unit,
		    header->i_addr3, ":"));
	    ifp->if_ierrors++;
	    m_freem(m0);
	    break;

	case IEEE80211_FC1_AP_TO_AP:
	    RAY_DPRINTFN(1, ("ray%d: ray_rx saw packet between aps %6D %6D\n",
		    sc->unit,
		    header->i_addr1, ":", header->i_addr2, ":"));
	    ifp->if_ierrors++;
	    m_freem(m0);
	    return;

	default:
	    printf("ray%d: ray_rx packet type unknown fc1 0x%x - why?\n",
		    sc->unit, fc);
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
	    printf("ray%d: ray_rx unknown translation type 0x%x - why?\n",
	    		sc->unit, sc->translation);
	    ifp->if_ierrors++;
	    m_freem(m0);
	    return;

    }

    /*
     * Finally, do a bit of house keeping before sending the packet
     * up the stack.
     */
    ifp->if_ipackets++;
#if NBPFILTER > 0
    if (ifp->if_bpf)
	bpf_mtap(ifp, m0);
#endif /* NBPFILTER */
#if XXX
if_wi.c - might be needed if we hear our own broadcasts in promiscuous mode
but will not be if we dont see them
	if ((ifp->if_flags & IFF_PROMISC) &&
	    (bcmp(eh->ether_shost, sc->arpcom.ac_enaddr, ETHER_ADDR_LEN) &&
	    (eh->ether_dhost[0] & 1) == 0)
	) {
	    m_freem(m0);
	    return;
	}
#endif /* XXX */
    eh = mtod(m0, struct ether_header *);
    m_adj(m0, sizeof(struct ether_header));
    ether_input(ifp, eh, m0);

    return;
}

/*
 * Download start up structures to card.
 *
 * Part of ray_init, download, startjoin control flow.
 */
static void
ray_download_params (sc)
    struct ray_softc	*sc;
{
    struct ray_mib_4	ray_mib_4_default;
    struct ray_mib_5	ray_mib_5_default;

    RAY_DPRINTFN(5, ("ray%d: Downloading startup parameters\n", sc->unit));
    RAY_MAP_CM(sc);

#if XXX_TRACKING
    ray_cmd_cancel(sc, SCP_UPD_STARTUP);
#endif /* XXX_TRACKING */

#define MIB4(m)		ray_mib_4_default.##m
#define MIB5(m)		ray_mib_5_default.##m
#define	PUT2(p, v) 	\
    do { (p)[0] = ((v >> 8) & 0xff); (p)[1] = (v & 0xff); } while(0)

     /*
      * Firmware version 4 defaults - see if_raymib.h for details
      */
     MIB4(mib_net_type)			= sc->sc_net_type;
     MIB4(mib_ap_status)		= RAY_MIB_AP_STATUS_DEFAULT;
     strncpy(MIB4(mib_ssid), sc->sc_ssid, RAY_MAXSSIDLEN);
     MIB4(mib_scan_mode)		= RAY_MIB_SCAN_MODE_DEFAULT;
     MIB4(mib_apm_mode)			= RAY_MIB_APM_MODE_DEFAULT;
     bcopy(sc->sc_station_addr, MIB4(mib_mac_addr), ETHER_ADDR_LEN);
PUT2(MIB4(mib_frag_thresh), 		  RAY_MIB_FRAG_THRESH_DEFAULT);
PUT2(MIB4(mib_dwell_time),		  RAY_MIB_DWELL_TIME_V4);
PUT2(MIB4(mib_beacon_period),		  RAY_MIB_BEACON_PERIOD_V4);
     MIB4(mib_dtim_interval)		= RAY_MIB_DTIM_INTERVAL_DEFAULT;
     MIB4(mib_max_retry)		= RAY_MIB_MAX_RETRY_DEFAULT;
     MIB4(mib_ack_timo)			= RAY_MIB_ACK_TIMO_DEFAULT;
     MIB4(mib_sifs)			= RAY_MIB_SIFS_DEFAULT;
     MIB4(mib_difs)			= RAY_MIB_DIFS_DEFAULT;
     MIB4(mib_pifs)			= RAY_MIB_PIFS_V4;
PUT2(MIB4(mib_rts_thresh),		  RAY_MIB_RTS_THRESH_DEFAULT);
PUT2(MIB4(mib_scan_dwell),		  RAY_MIB_SCAN_DWELL_V4);
PUT2(MIB4(mib_scan_max_dwell),		  RAY_MIB_SCAN_MAX_DWELL_V4);
     MIB4(mib_assoc_timo)		= RAY_MIB_ASSOC_TIMO_DEFAULT;
     MIB4(mib_adhoc_scan_cycle)		= RAY_MIB_ADHOC_SCAN_CYCLE_DEFAULT;
     MIB4(mib_infra_scan_cycle)		= RAY_MIB_INFRA_SCAN_CYCLE_DEFAULT;
     MIB4(mib_infra_super_scan_cycle)	= RAY_MIB_INFRA_SUPER_SCAN_CYCLE_DEFAULT;
     MIB4(mib_promisc)			= RAY_MIB_PROMISC_DEFAULT;
PUT2(MIB4(mib_uniq_word),		  RAY_MIB_UNIQ_WORD_DEFAULT);
     MIB4(mib_slot_time)		= RAY_MIB_SLOT_TIME_V4;
     MIB4(mib_roam_low_snr_thresh)	= RAY_MIB_ROAM_LOW_SNR_THRESH_DEFAULT;
     MIB4(mib_low_snr_count)		= RAY_MIB_LOW_SNR_COUNT_DEFAULT;
     MIB4(mib_infra_missed_beacon_count)= RAY_MIB_INFRA_MISSED_BEACON_COUNT_DEFAULT;
     MIB4(mib_adhoc_missed_beacon_count)= RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DEFAULT;
     MIB4(mib_country_code)		= RAY_MIB_COUNTRY_CODE_DEFAULT;
     MIB4(mib_hop_seq)			= RAY_MIB_HOP_SEQ_DEFAULT;
     MIB4(mib_hop_seq_len)		= RAY_MIB_HOP_SEQ_LEN_V4;
     MIB4(mib_cw_max)			= RAY_MIB_CW_MAX_V4;
     MIB4(mib_cw_min)			= RAY_MIB_CW_MIN_V4;
     MIB4(mib_noise_filter_gain)	= RAY_MIB_NOISE_FILTER_GAIN_DEFAULT;
     MIB4(mib_noise_limit_offset)	= RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT;
     MIB4(mib_rssi_thresh_offset)	= RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT;
     MIB4(mib_busy_thresh_offset)	= RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT;
     MIB4(mib_sync_thresh)		= RAY_MIB_SYNC_THRESH_DEFAULT;
     MIB4(mib_test_mode)		= RAY_MIB_TEST_MODE_DEFAULT;
     MIB4(mib_test_min_chan)		= RAY_MIB_TEST_MIN_CHAN_DEFAULT;
     MIB4(mib_test_max_chan)		= RAY_MIB_TEST_MAX_CHAN_DEFAULT;

     /*
      * Firmware version 5 defaults - see if_raymib.h for details
      */
     MIB5(mib_net_type)			= sc->sc_net_type;
     MIB5(mib_ap_status)		= RAY_MIB_AP_STATUS_DEFAULT;
     strncpy(MIB5(mib_ssid), sc->sc_ssid, RAY_MAXSSIDLEN);
     MIB5(mib_scan_mode)		= RAY_MIB_SCAN_MODE_DEFAULT;
     MIB5(mib_apm_mode)			= RAY_MIB_APM_MODE_DEFAULT;
     bcopy(sc->sc_station_addr, MIB5(mib_mac_addr), ETHER_ADDR_LEN);
PUT2(MIB5(mib_frag_thresh), 		  RAY_MIB_FRAG_THRESH_DEFAULT);
PUT2(MIB5(mib_dwell_time),		  RAY_MIB_DWELL_TIME_V5);
PUT2(MIB5(mib_beacon_period),		  RAY_MIB_BEACON_PERIOD_V5);
     MIB5(mib_dtim_interval)		= RAY_MIB_DTIM_INTERVAL_DEFAULT;
     MIB5(mib_max_retry)		= RAY_MIB_MAX_RETRY_DEFAULT;
     MIB5(mib_ack_timo)			= RAY_MIB_ACK_TIMO_DEFAULT;
     MIB5(mib_sifs)			= RAY_MIB_SIFS_DEFAULT;
     MIB5(mib_difs)			= RAY_MIB_DIFS_DEFAULT;
     MIB5(mib_pifs)			= RAY_MIB_PIFS_V5;
PUT2(MIB5(mib_rts_thresh),		  RAY_MIB_RTS_THRESH_DEFAULT);
PUT2(MIB5(mib_scan_dwell),		  RAY_MIB_SCAN_DWELL_V5);
PUT2(MIB5(mib_scan_max_dwell),		  RAY_MIB_SCAN_MAX_DWELL_V5);
     MIB5(mib_assoc_timo)		= RAY_MIB_ASSOC_TIMO_DEFAULT;
     MIB5(mib_adhoc_scan_cycle)		= RAY_MIB_ADHOC_SCAN_CYCLE_DEFAULT;
     MIB5(mib_infra_scan_cycle)		= RAY_MIB_INFRA_SCAN_CYCLE_DEFAULT;
     MIB5(mib_infra_super_scan_cycle)	= RAY_MIB_INFRA_SUPER_SCAN_CYCLE_DEFAULT;
     MIB5(mib_promisc)			= RAY_MIB_PROMISC_DEFAULT;
PUT2(MIB5(mib_uniq_word),		  RAY_MIB_UNIQ_WORD_DEFAULT);
     MIB5(mib_slot_time)		= RAY_MIB_SLOT_TIME_V5;
     MIB5(mib_roam_low_snr_thresh)	= RAY_MIB_ROAM_LOW_SNR_THRESH_DEFAULT;
     MIB5(mib_low_snr_count)		= RAY_MIB_LOW_SNR_COUNT_DEFAULT;
     MIB5(mib_infra_missed_beacon_count)= RAY_MIB_INFRA_MISSED_BEACON_COUNT_DEFAULT;
     MIB5(mib_adhoc_missed_beacon_count)= RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DEFAULT;
     MIB5(mib_country_code)		= RAY_MIB_COUNTRY_CODE_DEFAULT;
     MIB5(mib_hop_seq)			= RAY_MIB_HOP_SEQ_DEFAULT;
     MIB5(mib_hop_seq_len)		= RAY_MIB_HOP_SEQ_LEN_V5;
PUT2(MIB5(mib_cw_max),			  RAY_MIB_CW_MAX_V5);
PUT2(MIB5(mib_cw_min),			  RAY_MIB_CW_MIN_V5);
     MIB5(mib_noise_filter_gain)	= RAY_MIB_NOISE_FILTER_GAIN_DEFAULT;
     MIB5(mib_noise_limit_offset)	= RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT;
     MIB5(mib_rssi_thresh_offset)	= RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT;
     MIB5(mib_busy_thresh_offset)	= RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT;
     MIB5(mib_sync_thresh)		= RAY_MIB_SYNC_THRESH_DEFAULT;
     MIB5(mib_test_mode)		= RAY_MIB_TEST_MODE_DEFAULT;
     MIB5(mib_test_min_chan)		= RAY_MIB_TEST_MIN_CHAN_DEFAULT;
     MIB5(mib_test_max_chan)		= RAY_MIB_TEST_MAX_CHAN_DEFAULT;
     MIB5(mib_allow_probe_resp)		= RAY_MIB_ALLOW_PROBE_RESP_DEFAULT;
     MIB5(mib_privacy_must_start)	= RAY_MIB_PRIVACY_MUST_START_DEFAULT;
     MIB5(mib_privacy_can_join)		= sc->sc_priv_start;
     MIB5(mib_basic_rate_set[0])	= sc->sc_priv_join;

    /* XXX I don't really want to panic here but we must ensure that the
     * XXX card is idle - maybe stop it and reset it? */
    if (!RAY_ECF_READY(sc))
    	panic("ray%d: ray_download_params something is already happening\n",
		sc->unit);

    if (sc->sc_version == RAY_ECFS_BUILD_4)
	ray_write_region(sc, RAY_HOST_TO_ECF_BASE,
			 &ray_mib_4_default, sizeof(ray_mib_4_default));
    else
	ray_write_region(sc, RAY_HOST_TO_ECF_BASE,
			 &ray_mib_5_default, sizeof(ray_mib_5_default));

    /*
     * Get a free command ccs and issue the command - there is nothing
     * to fill in for a START_PARAMS command. The start parameters
     * command just gets serviced, so we use a timeout to complete the
     * sequence.
     */
    if (!ray_alloc_ccs(sc, &sc->sc_ccs, RAY_CMD_START_PARAMS, SCP_UPD_STARTUP))
    	panic("ray%d: ray_download_params can't get a CCS\n", sc->unit);

    if (!ray_issue_cmd(sc, sc->sc_ccs, SCP_UPD_STARTUP))
    	panic("ray%d: ray_download_params can't issue command\n", sc->unit);

    sc->timerh = timeout(ray_download_timo, sc, RAY_CCS_TIMEOUT);

    RAY_DPRINTFN(15, ("ray%d: Download now awaiting timeout\n", sc->unit));

    return;
}

/*
 * Download timeout routine.
 *
 * Part of ray_init, download, start_join control flow.
 */
static void
ray_download_timo (xsc)
    void		*xsc;
{
    struct ray_softc	*sc = xsc;
    size_t		ccs;
    u_int8_t		status, cmd;

    RAY_DPRINTFN(5, ("ray%d: ray_download_timo\n", sc->unit));
    RAY_MAP_CM(sc);

    status = SRAM_READ_FIELD_1(sc, sc->sc_ccs, ray_cmd, c_status);
    cmd = SRAM_READ_FIELD_1(sc, sc->sc_ccs, ray_cmd, c_cmd);
    RAY_DPRINTFN(20, ("ray%d: check rayidx %d ccs 0x%x cmd 0x%x status %d\n",
    		sc->unit, RAY_CCS_INDEX(sc->sc_ccs), sc->sc_ccs, cmd, status));
    if ((cmd != RAY_CMD_START_PARAMS) || (status != RAY_CCS_STATUS_FREE))
    	printf("ray%d: Download ccs odd cmd = 0x%02x, status = 0x%02x",
		sc->unit, cmd, status);
	/*XXX so what do we do? reset or retry? */

    /*
     * If the card is still busy, re-schedule ourself
     */
    if (status == RAY_CCS_STATUS_BUSY) {
	RAY_DPRINTFN(1, ("ray%d: ray_download_timo still busy, re-schedule\n",
		sc->unit));
	sc->timerh = timeout(ray_download_timo, sc, RAY_CCS_TIMEOUT);
	return;
    }

    /* Clear the ccs */
    (void)ray_free_ccs(sc, sc->sc_ccs);
    sc->sc_ccs = RAY_CCS_LAST + 1;

    /*
     * Grab a ccs and don't bother updating the network parameters.
     * Issue the start/join command and we get interrupted back.
     */
    if (sc->sc_net_type == RAY_MIB_NET_TYPE_ADHOC)
	    cmd = RAY_CMD_START_NET;
    else
	    cmd = RAY_CMD_JOIN_NET;

    if (!ray_alloc_ccs(sc, &ccs, cmd, SCP_UPD_STARTJOIN))
    	panic("ray%d: ray_download_timo can't get a CCS to start/join net\n",
		sc->unit);

    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_net, c_upd_param, 0);

    if (!ray_issue_cmd(sc, ccs, SCP_UPD_STARTJOIN))
    	panic("ray%d: ray_download_timo can't issue start/join\n", sc->unit);

#if RAY_NEED_STARTJOIN_TIMO
    sc->sj_timerh = timeout(ray_start_join_timo, sc, RAY_CCS_TIMEOUT);
#endif /* RAY_NEED_STARTJOIN_TIMO */

    RAY_DPRINTFN(15, ("ray%d: Start-join awaiting interrupt/timeout\n",
	    sc->unit));

    return;
}

/*
 * Complete start or join command.
 *
 * Part of ray_init, download, start_join control flow.
 */
static void
ray_start_join_done (sc, ccs, status)
    struct ray_softc	*sc;
    size_t		ccs;
    u_int8_t		status;
{
    struct ifnet	*ifp;
    u_int8_t		o_net_type;

    RAY_DPRINTFN(5, ("ray%d: ray_start_join_done\n", sc->unit));
    RAY_MAP_CM(sc);

    ifp = &sc->arpcom.ac_if;

#if RAY_NEED_STARTJOIN_TIMO
    untimeout(ray_start_join_timo, sc, sc->sj_timerh);
#endif /* RAY_NEED_STARTJOIN_TIMO */

#if XXX_TRACKING
    ray_cmd_done(sc, SCP_UPD_STARTJOIN);
#endif /* XXX_TRACKING */

    switch (status) {

    	case RAY_CCS_STATUS_FREE:
    	case RAY_CCS_STATUS_BUSY:
	    printf("ray%d: ray_start_join_done status is FREE/BUSY - why?\n",
	    		sc->unit);
	    break;

    	case RAY_CCS_STATUS_COMPLETE:
	    break;

    	case RAY_CCS_STATUS_FAIL:
	    printf("ray%d: ray_start_join_done status is FAIL - why?\n",
	    		sc->unit);
	    sc->sc_havenet = 0;
#if XXX
	    if we fired the start command we successfully set the card up
	    so just restart ray_start_join sequence and dont reset the card
	    may need to split download_done for this
#endif
	    break;

	default:
	    printf("ray%d: ray_start_join_done unknown status 0x%x\n",
	    		sc->unit, status);
	    break;
    }

    if (status != RAY_CCS_STATUS_COMPLETE)
	return;

    /*
     * If the command completed correctly, get a few network parameters
     * from the ccs and active the network.
     */
    ray_read_region(sc, ccs, &sc->sc_cnet_1, sizeof(struct ray_cmd_net));

    /* adjust values for buggy build 4 */
    if (sc->sc_def_txrate == 0x55)
	    sc->sc_def_txrate = RAY_MIB_BASIC_RATE_SET_1500K;
    if (sc->sc_encrypt == 0x55)
	    sc->sc_encrypt = 0;

    /* card is telling us to update the network parameters */
    if (sc->sc_upd_param) {
	RAY_DPRINTFN(1, ("ray%d: sj_done card updating parameters - why?\n",
		sc->unit));
	o_net_type = sc->sc_net_type;
	ray_read_region(sc, RAY_HOST_TO_ECF_BASE,
		&sc->sc_cnet_2, sizeof(struct ray_net_params));
	if (sc->sc_net_type != o_net_type) {
	    printf("ray%d: sj_done card changing network type - why?\n",
	    	sc->unit);
#if XXX
	    restart ray_start_join sequence
	    may need to split download_timo for this
#endif
	}
    }
    RAY_DNET_DUMP(sc, " after start/join network completed.");

    /*
     * Hurrah! The network is now active.
     *
     * Clearing IFF_OACTIVE will ensure that the system will queue packets.
     * Just before we return from the interrupt context we check to
     * see if packets have been queued.
     */
#if XXX_MCASTPROM
    ray_cmd_schedule(sc, SCP_UPD_MCAST|SCP_UPD_PROMISC);
#endif /* XXX_MCASTPROM */
    if (SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd) == RAY_CMD_JOIN_NET)
#if XXX_INFRA
	ray_start_assoc(sc);
#else
	RAY_DPRINTFN(1, ("wanted to join a NET!\n"));
#endif /* XXX_INFRA */
    else {
	sc->sc_havenet = 1;
        ifp->if_flags &= ~IFF_OACTIVE;
    }

    return;
}

#if RAY_NEED_STARTJOIN_TIMO
/*
 * Back stop catcher for start_join command. The NetBSD driver
 * suggests that they need it to catch a bug in the firmware or the
 * parameters they use - they are not sure. I'll just panic as I seem
 * to get interrupts back fine and I have version 4 firmware.
 */
static void
ray_start_join_timo (xsc)
    void		*xsc;
{
    struct ray_softc	*sc = xsc;

    RAY_DPRINTFN(5, ("ray%d: ray_start_join_timo\n", sc->unit));
    RAY_MAP_CM(sc);

    panic("ray%d: ray-start_join_timo occured\n", sc->unit);

    return;
}
#endif /* RAY_NEED_STARTJOIN_TIMO */

/*
 * Obtain a free ccs buffer.
 *
 * Returns 1 and in `ccsp' the bus offset of the free ccs 
 * or 0 if none are free
 *
 * If `track' is not zero, handles tracking this command
 * possibly indicating a callback is needed and setting a timeout
 * also if ECF isn't ready we terminate earlier to avoid overhead.
 *
 * This routine is only used for commands
 */
static int
ray_alloc_ccs (sc, ccsp, cmd, track)
    struct	ray_softc *sc;
    size_t	*ccsp;
    u_int	cmd, track;
{
    size_t	ccs;
    u_int	i;

    RAY_DPRINTFN(5, ("ray%d: ray_alloc_ccs\n", sc->unit));
    RAY_MAP_CM(sc);

#if XXX_TRACKING
	/* for tracked commands, if not ready just set pending */
	if (track && !RAY_ECF_READY(sc)) {
		ray_cmd_schedule(sc, track);
		return(0);
	}
#endif /* XXX_TRACKING */

    for (i = RAY_CCS_CMD_FIRST; i <= RAY_CCS_CMD_LAST; i++) {
	/* probe here to make the card go */
	(void)SRAM_READ_FIELD_1(sc, RAY_CCS_ADDRESS(i), ray_cmd, c_status);
	if (!sc->sc_ccsinuse[i])
	    break;
    }
    if (i > RAY_CCS_CMD_LAST) {
#if XXX_TRACKING
	if (track)
		ray_cmd_schedule(sc, track);
#endif /* XXX_TRACKING */
	return(0);
    }
    sc->sc_ccsinuse[i] = 1;
    ccs = RAY_CCS_ADDRESS(i);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_BUSY);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_cmd, cmd);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_link, RAY_CCS_LINK_NULL);

    *ccsp = ccs;
    return(1);
}

/*
 * Free up a ccs/cmd and return the old status.
 * This routine is only used for commands.
 */
static u_int8_t
ray_free_ccs (sc, ccs)
    struct	ray_softc *sc;
    size_t	ccs;
{
    u_int8_t	status;

    RAY_DPRINTFN(5, ("ray%d: ray_free_ccs\n", sc->unit));
    RAY_MAP_CM(sc);

    status = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
    RAY_CCS_FREE(sc, ccs);
    if (ccs <= RAY_CCS_ADDRESS(RAY_CCS_LAST))
	    sc->sc_ccsinuse[RAY_CCS_INDEX(ccs)] = 0;
    RAY_DPRINTFN(20, ("ray%d: ray_free_ccs freed 0x%02x\n",
	    sc->unit, RAY_CCS_INDEX(ccs)));

    return(status);
}

/*
 * Issue a command by writing the mailbox and tickling the card.
 * Only used for commands not transmitted packets.
 */
static int
ray_issue_cmd(sc, ccs, track)
    struct	ray_softc *sc;
    size_t	ccs;
    u_int	track;
{
    u_int	i;

    RAY_DPRINTFN(5, ("ray%d: ray_cmd_issue\n", sc->unit));
    RAY_MAP_CM(sc);

    /*
     * XXX other drivers did this, but I think 
     * what we really want to do is just make sure we don't
     * get here or that spinning is ok
     */
    i = 0;
    while (!RAY_ECF_READY(sc))
	if (++i > 50) {
	    printf("\n");
	    (void)ray_free_ccs(sc, ccs);
#if XXX_TRACKING
	    if (track)
		ray_cmd_schedule(sc, track);
#endif /* XXX_TRACKING */
	    return(0);
	} else if (i == 1)
	    printf("ray%d: ray_issue_cmd spinning", sc->unit);
	else
	    printf(".");

    SRAM_WRITE_1(sc, RAY_SCB_CCSI, RAY_CCS_INDEX(ccs));
    RAY_ECF_START_CMD(sc);
#if XXX_TRACKING
    ray_cmd_ran(sc, track);
#endif /* XXX_TRACKING */

    return(1);
}

#if RAY_DEBUG > 50
static void
ray_dump_mbuf(sc, m, s)
    struct ray_softc	*sc;
    struct mbuf		*m;
    char		*s;
{
    u_int8_t		*d, *ed;
    u_int		i;
    char		p[17];

    printf("ray%d: %s mbuf dump:", sc->unit, s);
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
#endif /* RAY_DEBUG > 50 */

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
#if (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP)
static void
ray_attr_getmap (struct ray_softc *sc)
{
    struct ucred	uc;
    struct pcred	pc;
    struct proc		p;
    int			result;

    RAY_DPRINTFN(5, ("ray%d: attempting to get map for common memory\n",
	    sc->unit));

    sc->md.window = 0;

    p.p_cred = &pc;
    p.p_cred->pc_ucred = &uc;
    p.p_cred->pc_ucred->cr_uid = 0;

    result = cdevsw[CARD_MAJOR]->d_ioctl(makedev(CARD_MAJOR, sc->slotnum), PIOCGMEM, (caddr_t)&sc->md, 0, &p);

    return;
}

static void
ray_attr_cm (struct ray_softc *sc)
{
    struct ucred uc;
    struct pcred pc;
    struct proc p;

    RAY_DPRINTFN(100, ("ray%d: attempting to remap common memory\n", sc->unit));

    p.p_cred = &pc;
    p.p_cred->pc_ucred = &uc;
    p.p_cred->pc_ucred->cr_uid = 0;

    cdevsw[CARD_MAJOR]->d_ioctl(makedev(CARD_MAJOR, sc->slotnum), PIOCSMEM, (caddr_t)&sc->md, 0, &p);

    return;
}
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

static int
ray_attr_write (struct ray_softc *sc, off_t offset, u_int8_t byte)
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
  ray_attr_cm(sc);
#endif /* RAY_NEED_CM_REMAPPING */

  return(err);
}

static int
ray_attr_read (struct ray_softc *sc, off_t offset, u_int8_t *buf, int size)
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
  ray_attr_cm(sc);
#endif /* RAY_NEED_CM_REMAPPING */

  return(err);
}

static u_int8_t
ray_read_reg (sc, reg)
    struct ray_softc	*sc;
    off_t		reg;
{
    u_int8_t		byte;

    ray_attr_read(sc, reg, &byte, 1);

    return(byte);
}

#endif /* NRAY */
