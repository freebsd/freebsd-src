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
 * $Id: if_ray.c,v 1.20 2000/04/21 15:01:49 dmlb Exp $
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
 *
 * ***unload needs to drain comq
 * ***unload checks in more routines
 * ***IFF_RUNNING checks are they really needed?
 * ***PCATCH tsleeps and have something that will clean the runq
 * havenet needs checking again
 * error handling of ECF command completions
 * proper setting of mib_hop_seq_len with country code for v4 firmware
 * _reset - check where needed
 * splimp or splnet?
 * faster TX routine
 * more translations
 * infrastructure mode - maybe need some of the old stuff for checking?
 * differeniate between parameters set in attach and init
 * spinning in ray_cmd_issue
 * make RAY_DEBUG a knob somehow - either sysctl or IFF_DEBUG
 * ray_update_params_done needs work
 * callout handles need rationalising. can probably remove sj_timerh
 * replace sc_comtimo with callout_pending() - see /sys/kern/kern_timeout.c
 *   unfortunately not in 3.2
 * fragmentation when rx level drops?
 * make RAY_DPRINTFN RAY_DPRINTF
 * rationalise CM mapping if needed - might mean moving a couple of things
 */

#define XXX		0
#define XXX_NETBSDTX	0
#define XXX_CLEARCCS_IN_INIT	0
#define XXX_ASSOCWORKING_AGAIN	0

/*
 * XXX build options - move to LINT
 */

/*
 * RAY_DEBUG settings
 *
 *	RECERR		Recoverable error's
 *	SUBR		Subroutine entry
 *	BOOTPARAM	Startup CM dump
 *	STARTJOIN	State transitions for start/join
 *	CCS		CCS info
 *	IOCTL		IOCTL calls
 *	NETPARAM	SSID when rejoining nets
 *	MBUF		MBUFs dumped
 *	RX		packet types reported
 *	CM		common memory re-mapping
 *	COM		new command sleep/wakeup
 */
#define RAY_DBG_RECERR		0x0001
#define RAY_DBG_SUBR		0x0002
#define RAY_DBG_BOOTPARAM	0x0004
#define RAY_DBG_STARTJOIN	0x0008
#define RAY_DBG_CCS		0x0010
#define RAY_DBG_IOCTL		0x0020
#define RAY_DBG_NETPARAM	0x0040
#define RAY_DBG_MBUF		0x0080
#define RAY_DBG_RX		0x0100
#define RAY_DBG_CM		0x0200
#define RAY_DBG_COM		0x0400

#ifndef RAY_DEBUG
#define RAY_DEBUG	(				\
 			   RAY_DBG_RECERR	|   	\
 			   RAY_DBG_SUBR		|    	\
			   RAY_DBG_BOOTPARAM	|	\
			   RAY_DBG_STARTJOIN	|	\
			   RAY_DBG_CCS		|   	\
                           RAY_DBG_IOCTL	|   	\
                        /* RAY_DBG_NETPARAM	| */	\
                        /* RAY_DBG_MBUF		| */	\
                        /* RAY_DBG_RX		| */	\
                        /* RAY_DBG_CM		| */	\
                           RAY_DBG_COM		|   	\
			0				\
			)
#endif

#define RAY_CCS_TIMEOUT		(hz/2)	/* Timeout for CCS commands */
#define	RAY_CHECK_SCHED_TIMEOUT	(hz)	/* Time to wait until command retry, should be > RAY_CCS_TIMEOUT */

#define RAY_NEED_CM_FIXUP	1	/* Needed until pccardd hacks for ed drivers are removed (pccardd forces 16bit memory and 0x4000 size) THIS IS A DANGEROUS THING TO USE IF YOU USE OTHER MEMORY MAPPED PCCARDS */

#define RAY_NEED_CM_REMAPPING	1	/* Needed until pccard maps more than one memory area */

#define RAY_RESET_TIMEOUT	(5*hz)	/* Timeout for resetting the card */

#define RAY_USE_CALLOUT_STOP	0	/* Set for kernels with callout_stop function - 3.3 and above */

#define RAY_SIMPLE_TX		1	/* Simple TX routine */
#define RAY_DECENT_TX		0	/* Decent TX routine - tbd */
/*
 * XXX build options - move to LINT
 */

/*
 * Debugging odds and odds
 */
#ifndef RAY_DEBUG
#define RAY_DEBUG 		0x0000
#endif /* RAY_DEBUG */

#if RAY_DEBUG > 0

/* XXX This macro assumes that common memory is mapped into kernel space and
 * XXX does not indirect through SRAM macros - it should
 */
#define RAY_DHEX8(p, l, mask) do { if (RAY_DEBUG & mask) {	\
    u_int8_t *i;						\
    for (i = p; i < (u_int8_t *)(p+l); i += 8)			\
    	printf("  0x%08lx %8D\n",				\
		(unsigned long)i, (unsigned char *)i, " ");	\
} } while (0)

#define RAY_DPRINTFN(mask, x) do { if (RAY_DEBUG & mask) {	\
    printf x ;							\
} } while (0)

#define RAY_DPRINTF(sc, mask, fmt, args...) do {if (RAY_DEBUG & mask) {	\
    printf("ray%d: %s(%d) " fmt "\n",					\
    	sc->unit, __FUNCTION__ , __LINE__ , ##args);			\
} } while (0)

#define RAY_DNET_DUMP(sc, s) do { if (RAY_DEBUG & RAY_DBG_NETPARAM) {	\
    printf("ray%d: Current network parameters%s\n", (sc)->unit, (s));	\
    printf("  bss_id %6D\n", (sc)->sc_c.np_bss_id, ":");		\
    printf("  inited 0x%02x\n", (sc)->sc_c.np_inited);			\
    printf("  def_txrate 0x%02x\n", (sc)->sc_c.np_def_txrate);		\
    printf("  encrypt 0x%02x\n", (sc)->sc_c.np_encrypt);		\
    printf("  net_type 0x%02x\n", (sc)->sc_c.np_net_type);		\
    printf("  ssid \"%.32s\"\n", (sc)->sc_c.np_ssid);			\
    printf("       %8D\n", (sc)->sc_c.np_ssid, " ");			\
    printf("       %8D\n", (sc)->sc_c.np_ssid+8, " ");			\
    printf("       %8D\n", (sc)->sc_c.np_ssid+16, " ");			\
    printf("       %8D\n", (sc)->sc_c.np_ssid+24, " ");			\
    printf("  priv_start 0x%02x\n", (sc)->sc_c.np_priv_start);		\
    printf("  priv_join 0x%02x\n", (sc)->sc_c.np_priv_join);		\
    printf("ray%d: Desired network parameters%s\n", (sc)->unit, (s));	\
    printf("  bss_id %6D\n", (sc)->sc_d.np_bss_id, ":");		\
    printf("  inited 0x%02x\n", (sc)->sc_d.np_inited);			\
    printf("  def_txrate 0x%02x\n", (sc)->sc_d.np_def_txrate);		\
    printf("  encrypt 0x%02x\n", (sc)->sc_d.np_encrypt);		\
    printf("  net_type 0x%02x\n", (sc)->sc_d.np_net_type);		\
    printf("  ssid \"%.32s\"\n", (sc)->sc_d.np_ssid);			\
    printf("       %8D\n", (sc)->sc_c.np_ssid, " ");			\
    printf("       %8D\n", (sc)->sc_c.np_ssid+8, " ");			\
    printf("       %8D\n", (sc)->sc_c.np_ssid+16, " ");			\
    printf("       %8D\n", (sc)->sc_c.np_ssid+24, " ");			\
    printf("  priv_start 0x%02x\n", (sc)->sc_d.np_priv_start);		\
    printf("  priv_join 0x%02x\n", (sc)->sc_d.np_priv_join);		\
} } while (0)

#define RAY_DCOM_DUMP(sc, com, s) do { if (RAY_DEBUG & RAY_DBG_COM) {	\
    printf("ray%d: %s(%d) %s com entry 0x%p\n",				\
        (sc)->unit, __FUNCTION__ , __LINE__ , (s) , (com));		\
    printf("  c_mesg %s\n", (com)->c_mesg);				\
    printf("  c_flags 0x%b\n", (com)->c_flags, RAY_COM_FLAGS_PRINTFB);	\
    printf("  c_retval 0x%x\n", (com)->c_retval);			\
    printf("  c_ccs 0x%0x index 0x%02x\n",				\
        com->c_ccs, RAY_CCS_INDEX((com)->c_ccs));			\
} } while (0)

#define RAY_DCOM_CHECK(sc, com) do { if (RAY_DEBUG & RAY_DBG_COM) {	\
    ray_com_ecf_check((sc), (com), __FUNCTION__ );			\
} } while (0)

#else
#define RAY_DHEX8(p, l, mask)
#define RAY_DPRINTFN(mask, x)
#define RAY_DPRINTF(sc, mask, fmt, args...)
#define RAY_DNET_DUMP(sc, s)
#define RAY_DCOM_DUMP(sc, com, s)
#define RAY_DCOM_CHECK(sc, com)
#endif /* RAY_DEBUG > 0 */
#define RAY_PANIC(sc, fmt, args...) do {			\
    panic("ray%d: %s(%d) " fmt "\n",				\
    	sc->unit, __FUNCTION__ , __LINE__ , ##args);		\
} while (0)

#if RAY_DEBUG & RAY_DBG_MBUF
#define RAY_DMBUF_DUMP(sc, m, s)	ray_dump_mbuf((sc), (m), (s))
#else
#define RAY_DMBUF_DUMP(sc, m, s)
#endif /* RAY_DEBUG & RAY_DBG_MBUF */

#include "ray.h"
#include "card.h"
#include "apm.h"
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

#include <i386/isa/if_ieee80211.h>
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
 * Sysctl knobs
 */
static int ray_debug = RAY_DEBUG;

SYSCTL_NODE(_hw, OID_AUTO, ray, CTLFLAG_RW, 0, "Raylink Driver");
SYSCTL_INT(_hw_ray, OID_AUTO, debug, CTLFLAG_RW, &ray_debug, RAY_DEBUG, "");

/*
 * Network parameters, used twice in sotfc to store what we want and what
 * we have.
 *
 * XXX promisc in here too?
 * XXX sc_station_addr in here too (for changing mac address)
 */
struct ray_nw_param {
    struct ray_cmd_net	p_1;
    u_int8_t		np_ap_status;
    struct ray_net_params \
    			p_2;
    u_int8_t		np_countrycode;
};
#define np_upd_param	p_1.c_upd_param
#define	np_bss_id	p_1.c_bss_id
#define	np_inited	p_1.c_inited
#define	np_def_txrate	p_1.c_def_txrate
#define	np_encrypt	p_1.c_encrypt
#define np_net_type	p_2.p_net_type
#define np_ssid		p_2.p_ssid
#define np_priv_start	p_2.p_privacy_must_start
#define np_priv_join	p_2.p_privacy_can_join

/*
 * One of these structures per allocated device
 */
struct ray_softc {

    struct arpcom	arpcom;		/* Ethernet common 		*/
    struct ifmedia	ifmedia;	/* Ifnet common 		*/
    struct callout_handle
    			reset_timerh;	/* Handle for reset timer	*/
    struct callout_handle
    			start_timerh;	/* Handle for start timer	*/
    struct callout_handle
    			com_timerh;	/* Handle for command timer	*/
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

    struct ray_ecf_startup_v5
    			sc_ecf_startup; /* Startup info from card	*/

    TAILQ_HEAD(ray_comq, ray_comq_entry) 
			sc_comq;	/* Command queue		*/

    struct ray_nw_param	sc_c;		/* current network params 	*/
    struct ray_nw_param sc_d;		/* desired network params	*/
    int			sc_havenet;	/* true if we have a network	*/
    int			sc_promisc;	/* current set value		*/
    u_int8_t		sc_ccsinuse[64];/* ccss' in use -- not for tx	*/

    int			sc_checkcounters;
    u_int64_t		sc_rxoverflow;	/* Number of rx overflows	*/
    u_int64_t		sc_rxcksum;	/* Number of checksum errors	*/
    u_int64_t		sc_rxhcksum;	/* Number of header checksum errors */
    u_int8_t		sc_rxnoise;	/* Average receiver level	*/
    struct ray_siglev	sc_siglevs[RAY_NSIGLEVRECS]; /* Antenna/levels	*/

    struct ray_param_req \
    			*sc_repreq;	/* used to return values	*/
    struct ray_param_req \
    			*sc_updreq;	/* to the user			*/
};
static struct ray_softc ray_softc[NRAY];

#define	sc_station_addr	sc_ecf_startup.e_station_addr
#define	sc_version	sc_ecf_startup.e_fw_build_string
#define	sc_tibsize	sc_ecf_startup.e_tibsize

/*
 * Command queue definitions
 */
MALLOC_DECLARE(M_RAYCOM);
MALLOC_DEFINE(M_RAYCOM, "raycom", "Raylink command queue entry");
struct ray_comq_entry {
	TAILQ_ENTRY(ray_comq_entry) c_chain;	/* Tail queue.		*/
	void		(*c_function)		/* Function to call */
			    __P((struct ray_softc *sc,
			    struct ray_comq_entry *com));
	int		c_flags;		/* Flags		*/
	u_int8_t	c_retval;		/* Return value		*/
	void		*c_wakeup;		/* Sleeping on this	*/
	size_t		c_ccs;			/* Control structure	*/
#if RAY_DEBUG & RAY_DBG_COM
	char		*c_mesg;
#endif /* RAY_DEBUG & RAY_DBG_COM */
};
#define RAY_COM_FWOK		0x0001		/* Wakeup on completion	*/
#define RAY_COM_FRUNNING	0x0002		/* This one running	*/
#define RAY_COM_FCOMPLETED	0x0004		/* This one completed	*/
#define RAY_COM_FLAGS_PRINTFB	\
	"\020"			\
	"\001WOK"		\
	"\002RUNNING"		\
	"\003COMPLETED"
#define RAY_COM_NEEDS_TIMO(cmd)			\
	(cmd == RAY_CMD_DOWNLOAD_PARAMS) ||	\
	(cmd == RAY_CMD_UPDATE_PARAMS) ||	\
	(cmd == RAY_CMD_UPDATE_MCAST)
#if RAY_DEBUG & RAY_DBG_COM
#define RAY_COM_FUNCTION(comp, function)	\
	(comp)->c_function = (function);	\
	(comp)->c_mesg = __STRING(function);
#else
#define RAY_COM_FUNCTION(comp, function)	\
	comp->c_function = function;
#endif /* RAY_DEBUG & RAY_DBG_COM */

/*
 * Translation types
 */
/* XXX maybe better as part of the if structure? */
#define SC_TRANSLATE_WEBGEAR	0

/*
 * Prototyping
 */
static int	ray_attach		__P((struct isa_device *dev));
static int	ray_ccs_alloc		__P((struct ray_softc *sc, size_t *ccsp, u_int cmd, int timo));
static void	ray_ccs_done		__P((struct ray_softc *sc, size_t ccs));
static u_int8_t	ray_ccs_free 		__P((struct ray_softc *sc, size_t ccs));
#if XXX_NETBSDTX
static void	ray_ccs_free_chain	__P((struct ray_softc *sc, u_int ni));
#endif /* XXX_NETBSDTX */
static void	ray_com_ecf		__P((struct ray_softc *sc, struct ray_comq_entry *com));
#if RAY_DEBUG & RAY_DBG_COM
static void	ray_com_ecf_check	__P((struct ray_softc *sc, size_t ccs, char *mesg));
#endif /* RAY_DEBUG & RAY_DBG_COM */
static void	ray_com_ecf_done	__P((struct ray_softc *sc));
static void	ray_com_ecf_timo	__P((void *xsc));
static void	ray_com_runq		__P((struct ray_softc *sc));
static void	ray_com_runq_add	__P((struct ray_softc *sc, struct ray_comq_entry *com));
static void	ray_com_runq_done	__P((struct ray_softc *sc));
static void	ray_download		__P((struct ray_softc *sc, struct ray_comq_entry *com));
static void	ray_download_done	__P((struct ray_softc *sc, size_t ccs));
#if RAY_DEBUG & RAY_DBG_MBUF
static void	ray_dump_mbuf		__P((struct ray_softc *sc, struct mbuf *m, char *s));
#endif /* RAY_DEBUG & RAY_DBG_MBUF */
static void	ray_init		__P((void *xsc));
static int	ray_ioctl		__P((struct ifnet *ifp, u_long command, caddr_t data));
static int	ray_intr		__P((struct pccard_devinfo *dev_p));
static void	ray_intr_updt_errcntrs	__P((struct ray_softc *sc));
static int	ray_pccard_init		__P((struct pccard_devinfo *dev_p));
static int	ray_pccard_intr		__P((struct pccard_devinfo *dev_p));
static void	ray_pccard_unload	__P((struct pccard_devinfo *dev_p));
static int	ray_probe		__P((struct isa_device *dev));
static void	ray_rcs_intr		__P((struct ray_softc *sc, size_t ccs));

static void	ray_report_params	__P((struct ray_softc *sc));
static void	ray_reset		__P((struct ray_softc *sc));
static void	ray_reset_timo		__P((void *xsc));
static void	ray_rx			__P((struct ray_softc *sc, size_t rcs));
static void	ray_rx_update_cache	__P((struct ray_softc *sc, u_int8_t *src, u_int8_t siglev, u_int8_t antenna));
static void	ray_sj			__P((struct ray_softc *sc, struct ray_comq_entry *com));
static void	ray_sj_done		__P((struct ray_softc *sc, size_t ccs));
static void	ray_start		__P((struct ifnet *ifp));
#if XXX_ASSOCWORKING_AGAIN
static void	ray_start_assoc		__P((struct ray_softc *sc));
static void	ray_start_assoc_done	__P((struct ray_softc *sc, size_t ccs, u_int8_t status));
#endif XXX_ASSOCWORKING_AGAIN
static u_int8_t ray_start_best_antenna	__P((struct ray_softc *sc, u_int8_t *dst));
static void	ray_start_done		__P((struct ray_softc *sc, size_t ccs, u_int8_t status));
static void	ray_start_timo		__P((void *xsc));
static size_t	ray_start_wrhdr		__P((struct ray_softc *sc, struct ether_header *eh, size_t bufp));
static void	ray_stop		__P((struct ray_softc *sc));
static void	ray_mcast		__P((struct ray_softc *sc, struct ray_comq_entry *com)); 
static void	ray_mcast_done		__P((struct ray_softc *sc, size_t ccs)); 
static int	ray_mcast_user		__P((struct ray_softc *sc)); 
static void	ray_update_params	__P((struct ray_softc *sc));
static void	ray_update_params_done	__P((struct ray_softc *sc, size_t ccs, u_int stat));
static void	ray_promisc		__P((struct ray_softc *sc, struct ray_comq_entry *com)); 
static void	ray_promisc_done	__P((struct ray_softc *sc, size_t ccs)); 
static int	ray_promisc_user	__P((struct ray_softc *sc)); 
static int	ray_user_update_params	__P((struct ray_softc *sc, struct ray_param_req *pr));
static int	ray_user_report_params	__P((struct ray_softc *sc, struct ray_param_req *pr));
static int	ray_user_report_stats	__P((struct ray_softc *sc, struct ray_stats_req *sr));
static void	ray_watchdog		__P((struct ifnet *ifp));

/*
 * PCMCIA driver definition
 */
PCCARD_MODULE(ray, ray_pccard_init, ray_pccard_unload, ray_pccard_intr, 0, net_imask);

/*
 * ISA driver definition
 */
struct isa_driver raydriver = {
    ray_probe,
    ray_attach,
    "ray",
    1
};

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
 * Macro's and constants
 */
#ifndef	RAY_CHECK_SCHED_TIMEOUT
#define	RAY_CHECK_SCHED_TIMEOUT	(hz)
#endif
#ifndef RAY_COM_TIMEOUT
#define RAY_COM_TIMEOUT		(hz / 2)
#endif
#ifndef RAY_RESET_TIMEOUT
#define RAY_RESET_TIMEOUT	(10 * hz)
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
 * As described in if_xe.c...
 *
 * Horrid stuff for accessing CIS tuples and remapping common memory...
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
ray_pccard_init(dev_p)
    struct pccard_devinfo   *dev_p;
{
    struct ray_softc	*sc;
    int			doRemap;

    RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: PCCard probe\n", dev_p->isahd.id_unit));

    if (dev_p->isahd.id_unit >= NRAY)
	return (ENODEV);

    sc = &ray_softc[dev_p->isahd.id_unit];

#if (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP)
    sc->slotnum = dev_p->slt->slotnum;
    ray_attr_getmap(sc);
    RAY_DPRINTFN(RAY_DBG_RECERR, ("ray%d: Memory window flags 0x%02x, start %p, size 0x%x, card address 0x%lx\n", sc->unit, sc->md.flags, sc->md.start, sc->md.size, sc->md.card));
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

#if RAY_NEED_CM_FIXUP
    doRemap = 0;
    if (sc->md.start == 0x0) {
	printf("ray%d: pccardd did not map CM - giving up\n", sc->unit);
	return (ENXIO);
    }
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

    printf("ray%d: <Raylink/IEEE 802.11> maddr %p msize 0x%x irq %d flags 0x%x on isa (PC-Card slot %d)\n",
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
ray_pccard_unload(dev_p)
    struct pccard_devinfo	*dev_p;
{
    struct ray_softc		*sc;
    struct ifnet		*ifp;

    RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_pccard_unload\n",
        dev_p->isahd.id_unit));

    sc = &ray_softc[dev_p->isahd.id_unit];
    ifp = &sc->arpcom.ac_if;

    if (sc->gone) {
	printf("ray%d: ray_pccard_unload unloaded!\n", sc->unit);
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
    untimeout(ray_start_timo, sc, sc->start_timerh);
    sc->sc_havenet = 0;

    /*
     * Mark as not running
     */
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
    printf("ray%d: ray_pccard_unload unloading complete\n", sc->unit);

    return;
}

/*
 * process an interrupt
 */
static int
ray_pccard_intr(dev_p)
    struct pccard_devinfo	*dev_p;
{
    return (ray_intr(dev_p));
}

/*
 * ISA probe routine.
 */
static int
ray_probe(dev_p)
    struct isa_device		*dev_p;
{

    RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ISA probe\n", dev_p->id_unit));

    return (0);
}

/*
 * ISA/PCCard attach.
 */
static int
ray_attach(dev_p)
    struct isa_device		*dev_p;
{
    struct ray_softc		*sc;
    struct ray_ecf_startup_v5	*ep;
    struct ifnet		*ifp;
    size_t			ccs;
    char			ifname[IFNAMSIZ];
    int				i;

    RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ISA/PCCard attach\n", dev_p->id_unit));

    sc = &ray_softc[dev_p->id_unit];
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: ray_attach unloaded!\n", sc->unit);
	return (1);
    }

    /*
     * Read startup results, check the card is okay and work out what
     * version we are using.
     */
    ep = &sc->sc_ecf_startup;
    ray_read_region(sc, RAY_ECF_TO_HOST_BASE, ep, sizeof(sc->sc_ecf_startup));
    if (ep->e_status != RAY_ECFS_CARD_OK) {
	printf("ray%d: card failed self test: status 0x%b\n", sc->unit,
	    ep->e_status, RAY_ECFS_PRINTFB);
	return (1);
    }
    if (sc->sc_version != RAY_ECFS_BUILD_4 &&
        sc->sc_version != RAY_ECFS_BUILD_5
       ) {
	printf("ray%d: unsupported firmware version 0x%0x\n", sc->unit,
	    ep->e_fw_build_string);
	return (1);
    }

    if (bootverbose || (RAY_DEBUG & RAY_DBG_BOOTPARAM)) {
	printf("ray%d: Start Up Results\n", sc->unit);
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
	    printf("  Firmware version %0x\n", ep->e_fw_build_string);
	    printf("  Firmware revision %0x\n", ep->e_fw_build);
	    printf("  (reserved word) %0x\n", ep->e_fw_resv);
	    printf("  ASIC version %0x\n", ep->e_asic_version);
	    printf("  TIB size %0x\n", ep->e_tibsize);
	}
    }


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
    ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
    ifp->if_hdrlen = sizeof(struct ieee80211_header) + 
    	sizeof(struct ether_header);
    ifp->if_baudrate = 1000000; /* Is this baud or bps ;-) */

    ifp->if_output = ether_output;
    ifp->if_start = ray_start;
    ifp->if_ioctl = ray_ioctl;
    ifp->if_watchdog = ray_watchdog;
    ifp->if_init = ray_init;
    ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

    /*
     * If this logical interface has already been attached,
     * don't attach it again or chaos will ensue.
     */
    sprintf(ifname, "ray%d", sc->unit);

    if (ifunit(ifname) == NULL) {
	callout_handle_init(&sc->com_timerh);
	callout_handle_init(&sc->reset_timerh);
	callout_handle_init(&sc->start_timerh);
	TAILQ_INIT(&sc->sc_comq);
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
	    printf("ray%d: ray_ioctl unloaded!\n", sc->unit);
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
				ray_init(sc);
			else
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
		error = ray_user_update_params(sc, &pr);
		error2 = copyout(&pr, ifr->ifr_data, sizeof(pr));
		error = error2 ? error2 : error;
		break;

	case SIOCGRAYPARAM:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GRAYPARAM");
		if ((error = copyin(ifr->ifr_data, &pr, sizeof(pr))))
			break;
		error = ray_user_report_params(sc, &pr);
		error2 = copyout(&pr, ifr->ifr_data, sizeof(pr));
		error = error2 ? error2 : error;
		break;

	case SIOCGRAYSTATS:
		RAY_DPRINTF(sc, RAY_DBG_IOCTL, "GRAYSTATS");
		error = ray_user_report_stats(sc, &sr);
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
 *XXX change all this - it's wrong
 *
 * Start up flow is as follows.
 * The kernel calls ray_init when the interface is assigned an address.
 * 
 * ray_init does a bit of house keeping before calling ray_download.
 *
 * ray_download_params fills the startup parameter structure out and
 * sends it to the card. The download command simply completes, so we
 * use the timeout code in ray_check_ccs instead of spin locking. The
 * passes flow to the standard ccs handler and we eventually end up in
 * ray_download_done.
 *
 * ray_download_done tells the card to start an adhoc network or join
 * a managed network. This should complete via the interrupt
 * mechanism, but the NetBSD driver includes a timeout for some buggy
 * stuff somewhere - I've left the hooks in but don't use them. The
 * interrupt handler passes control to ray_sj_done - the ccs
 * is handled by the interrupt mechanism.
 *
 * Once ray_sj_done has checked the ccs and uploaded/updated
 * the network parameters we are ready to process packets. It is then
 * safe to call ray_start which is done by the interrupt handler.
 */
static void
ray_init(xsc)
    void			*xsc;
{
    struct ray_softc		*sc = xsc;
    struct ray_comq_entry	*com[4];
    struct ray_ecf_startup_v5	*ep;
    struct ifnet		*ifp;
    int i;

    RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: ray_init unloaded!\n", sc->unit);
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
     * All of the variables in these sets can be updated by the card or ioctls.
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
	    ep->e_status, RAY_ECFS_PRINTFB);
	return; /* XXX This doesn't mark the interface as down */
    }

    /*
     * Fixup tib size to be correct - on build 4 it is garbage
     */
    if (sc->sc_version == RAY_ECFS_BUILD_4 && sc->sc_tibsize == 0x55)
	sc->sc_tibsize = sizeof(struct ray_tx_tib);

    /*
     * We are now up and running. We are busy until network is joined.
     */
    ifp->if_flags |= IFF_RUNNING | IFF_OACTIVE;

    /*
     * Create the following runq entries:
     *
     *		download	- download the network definition to the card
     *		sj		- find or start a BSS
     *		mcast		- download multicast list
     *		promisc		- last in case mcast called it anyway
     */
    for (i = 0; i < 4; i++)
	    MALLOC(com[i], struct ray_comq_entry *,
		sizeof(struct ray_comq_entry), M_RAYCOM, M_WAITOK);

    RAY_COM_FUNCTION(com[0], ray_download);
    RAY_COM_FUNCTION(com[1], ray_sj);
    RAY_COM_FUNCTION(com[2], ray_mcast);
    RAY_COM_FUNCTION(com[3], ray_promisc);

    for (i = 0; i < 4; i++) {
	    com[i]->c_flags = 0;
	    com[i]->c_retval = 0;
	    com[i]->c_ccs = NULL;
	    com[i]->c_wakeup = com[3];
#if XXX
	    ray_com_runq_add(sc, com[i]);
#endif
    }
    ray_com_runq_add(sc, com[0]); /* XXX remove */
    ray_com_runq_add(sc, com[1]); /* XXX remove */

    com[1]->c_flags = RAY_COM_FWOK; /* XXX should be com[3] */

    ray_com_runq(sc);
    RAY_DPRINTF(sc, RAY_DBG_COM, "sleeping");
    (void)tsleep(com[3], 0, "rayinit", 0);
    RAY_DPRINTF(sc, RAY_DBG_COM, "awakened");

    for (i = 0; i < 4; i++)
	FREE(com[i], M_RAYCOM);
}

/*
 * Download start up structures to card.
 */
static void
ray_download(struct ray_softc *sc, struct ray_comq_entry *com)
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
 * Download completion routine.
 */
static void
ray_download_done(struct ray_softc *sc, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_DCOM_CHECK(sc, ccs);

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
 * Start or join a network
 */
static void
ray_sj(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ray_net_params np;
	struct ifnet *ifp;
	int update;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

	/* XXX do I need this anymore? how can IFF_RUNNING be cleared
	 * XXX before this routine exits - check in ray_ioctl and the
	 * network code itself.
	 */
	ifp = &sc->arpcom.ac_if;
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		RAY_PANIC(sc, "IFF_RUNNING == 0");
	}

	sc->sc_havenet = 0;
	if (sc->sc_d.np_net_type == RAY_MIB_NET_TYPE_ADHOC)
		(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_START_NET, 0);
	else
		(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_JOIN_NET, 0);

	update = 0;
	if (bcmp(sc->sc_c.np_ssid, sc->sc_d.np_ssid, IEEE80211_NWID_LEN))
		update++;
	if (sc->sc_c.np_net_type != sc->sc_d.np_net_type)
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
 * Complete start command or intermediate step in join command
 */
static void
ray_sj_done(struct ray_softc *sc, size_t ccs)
{
	struct ifnet *ifp;
	u_int8_t o_net_type;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_DCOM_CHECK(sc, ccs);
	RAY_MAP_CM(sc);

	/*
	 * Read back any network parameters the ECF changed
	 */
	ray_read_region(sc, ccs, &sc->sc_c.p_1, sizeof(struct ray_cmd_net));

	/* adjust values for buggy build 4 */
	if (sc->sc_c.np_def_txrate == 0x55)
		sc->sc_c.np_def_txrate = sc->sc_d.np_def_txrate;
	if (sc->sc_c.np_encrypt == 0x55)
		sc->sc_c.np_encrypt = sc->sc_d.np_encrypt;

	/* card is telling us to update the network parameters */
	if (sc->sc_c.np_upd_param) {
		RAY_DPRINTF(sc, RAY_DBG_STARTJOIN, "card updating parameters");
		o_net_type = sc->sc_c.np_net_type; /* XXX this may be wrong? */
		ray_read_region(sc, RAY_HOST_TO_ECF_BASE,
		    &sc->sc_c.p_2, sizeof(struct ray_net_params));
		if (sc->sc_c.np_net_type != o_net_type) {
			RAY_PANIC(sc, "card changing network type");
#if XXX
			restart ray_start_join sequence
			may need to split download_done for this
#endif
		}
	}
	RAY_DNET_DUMP(sc, " after start/join network completed.");

	/*
	 * Hurrah! The network is now active.
	 *
	 * Clearing IFF_OACTIVE will ensure that the system will queue
	 * packets. Just before we return from the interrupt context
	 * we check to see if packets have been queued.
	 */
	ifp = &sc->arpcom.ac_if;
#if XXX_ASSOCWORKING_AGAIN
	if (SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd) == RAY_CMD_JOIN_NET)
		ray_start_assoc(sc);
	else {
		sc->sc_havenet = 1;
		ifp->if_flags &= ~IFF_OACTIVE;
	}
#else
	sc->sc_havenet = 1;
	ifp->if_flags &= ~IFF_OACTIVE;
#endif XXX_ASSOCWORKING_AGAIN

	ray_com_ecf_done(sc);
}

#if XXX_ASSOCWORKING_AGAIN
/*XXX move this further down the code */
/*
 * Start an association with an access point
 */
static void
ray_start_assoc(struct ray_softc *sc)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	(void)ray_cmd_simple(sc, RAY_CMD_START_ASSOC, SCP_STARTASSOC);
}

/*
 * Complete association
 */
static void
ray_start_assoc_done(struct ray_softc *sc, size_t ccs)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_STARTJOIN, "");
	RAY_MAP_CM(sc);
	RAY_DCOM_CHECK(sc, ccs);

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
#endif XXX_ASSOCWORKING_AGAIN

/*
 * Network stop.
 *
 * Assumes that a ray_init is used to restart the card.
 *
 */
static void
ray_stop(sc)
    struct ray_softc	*sc;
{
    struct ifnet	*ifp;
    int			s;
    int scheduled, i;

    RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_stop\n", sc->unit));
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: ray_stop unloaded!\n", sc->unit);
	return;
    }

    ifp = &sc->arpcom.ac_if;

    /*
     * Clear out timers and sort out driver state
     */

     /*XXX splimp with care needed */
printf("ray%d: ray_stop hcs_intr %d rcsi 0x%0x\n", sc->unit,
    RAY_HCS_INTR(sc), SRAM_READ_1(sc, RAY_SCB_RCSI));
printf("ray%d: ray_stop ready %d\n", sc->unit, RAY_ECF_READY(sc));

    if (sc->sc_repreq) {
	sc->sc_repreq->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
	wakeup(ray_report_params);
    }
    if (sc->sc_updreq) {
	sc->sc_repreq->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
	wakeup(ray_update_params);
    }
#if RAY_USE_CALLOUT_STOP
    callout_stop(sc->com_timerh);
    callout_stop(sc->reset_timerh);
#else
    untimeout(ray_com_ecf_timo, sc, sc->com_timerh);
    untimeout(ray_reset_timo, sc, sc->reset_timerh);
#endif /* RAY_USE_CALLOUT_STOP */
    untimeout(ray_start_timo, sc, sc->start_timerh);
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

    return;
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
ray_reset(sc)
    struct ray_softc	*sc;
{
    struct ifnet	*ifp;

    RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_reset\n", sc->unit));
    RAY_MAP_CM(sc);

    printf("ray%d: ray_reset skip reset card\n", sc->unit);
    return;

    ifp = &sc->arpcom.ac_if;

    if (ifp->if_flags & IFF_RUNNING)
	printf("ray%d: *** ray_reset skip stop card\n", sc->unit);
    /* XXX ray_stop(sc); not always in a sleepable context? */

    printf("ray%d: resetting card\n", sc->unit);
    ray_attr_write((sc), RAY_COR, RAY_COR_RESET);
    ray_attr_write((sc), RAY_COR, RAY_COR_DEFAULT);
    sc->reset_timerh = timeout(ray_reset_timo, sc, RAY_RESET_TIMEOUT);

    return;
}

/*
 * Finishing resetting and restarting the card
 */
static void
ray_reset_timo(xsc)
    void		*xsc;
{
    struct ray_softc	*sc = xsc;

    RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_reset_timo\n", sc->unit));
    RAY_MAP_CM(sc);

    if (!RAY_ECF_READY(sc)) {
	RAY_DPRINTFN(RAY_DBG_RECERR,
	    ("ray%d: ray_reset_timo still busy, re-schedule\n", sc->unit));
	sc->reset_timerh = timeout(ray_reset_timo, sc, RAY_RESET_TIMEOUT);
	return;
    }

    RAY_HCS_CLEAR_INTR(sc);
    ray_init(sc);

    return;
}

static void
ray_watchdog(ifp)
    register struct ifnet	*ifp;
{
    struct ray_softc *sc;

    RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_watchdog\n", ifp->if_unit));

    sc = ifp->if_softc;
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: ray_watchdog unloaded!\n", sc->unit);
	return;
    }

    printf("ray%d: watchdog timeout\n", sc->unit);

/* XXX may need to have remedial action here
   for example
   	ray_reset
	    ray_stop
	    ...
	    ray_init

    do we only use on TX?
    	if so then we should clear OACTIVE etc.

*/

    return;
}
/******************************************************************************
 * XXX NOT KNF FROM HERE UP
 ******************************************************************************/

/*
 * Transmit packet handling
 */

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
ray_start(struct ifnet *ifp)
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
	 * Some simple checks first
	 */
	if (sc->gone) {
		printf("ray%d: ray_start unloaded!\n", sc->unit);
		return;
	}
	if ((ifp->if_flags & IFF_RUNNING) == 0 || !sc->sc_havenet)
		return;
	if (!RAY_ECF_READY(sc)) {
		RAY_DPRINTF(sc, RAY_DBG_RECERR, "busy, schedule a timeout");
		sc->start_timerh = timeout(ray_start_timo, sc,
		    RAY_START_TIMEOUT);
		return;
	} else
		untimeout(ray_start_timo, sc, sc->start_timerh);

	/*
	 * Simple one packet at a time TX routine - probably appaling performance
	 * and we certainly chew CPU. However bing to windows boxes shows
	 * a reliance on the far end too:
	 *
	 * 1500k default rate
	 *
	 * Libretto 50CT (75MHz Pentium) with FreeBSD-3.1 to
	 *   Nonname box Windows 95C (133MHz AMD 5x86) 		 996109bps
	 *   AST J30 Windows 95A (100MHz Pentium) 		1307791bps
	 *
	 * 2000k default rate
	 *
	 * Libretto 50CT (75MHz Pentium) with FreeBSD-3.1 to
	 *   Nonname box Windows 95C (133MHz AMD 5x86) 		1087049bps
	 *   AST J30 Windows 95A (100MHz Pentium) 		1307791bps
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
	 * Each case must write the 802.11 header using ray_start_wrhdr,
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
		bufp = ray_start_wrhdr(sc, eh, bufp);
		break;

	default:
		printf("ray%d: ray_start unknown translation type 0x%x",
	    		sc->unit, sc->translation);
		RAY_CCS_FREE(sc, ccs);
		ifp->if_oerrors++;
		m_freem(m0);
		m0 = NULL;
		return;

	}
	if (m0 == NULL) {
		RAY_DPRINTF(sc, RAY_DBG_RECERR, "could not translate mbuf");
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
			RAY_PANIC(sc, "tx buffer overflow");
		bufp += len;
	}
	RAY_DMBUF_DUMP(sc, m0, "ray_start");

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
	ifp->if_opackets++;
	ifp->if_flags |= IFF_OACTIVE;
	SRAM_WRITE_FIELD_2(sc, ccs, ray_cmd_tx, c_len, pktlen);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_antenna,
	    ray_start_best_antenna(sc, eh->ether_dhost));
	SRAM_WRITE_1(sc, RAY_SCB_CCSI, ccs);
	RAY_ECF_START_CMD(sc);
	m_freem(m0);
}
#if XXX_NETBSDTX
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
		is it just as simple as splimp() around the ccs search?

		dont call _start from rx interrupt

		find a safe way of locking

		find a better way of obtaining ccs using next free avilable?

		look at other drivers

		use tsleep/wakeup
		use asleep await *****

		some form of ring to hold ccs

		free lsit

		rework calling
#endif XXX_NETBSDTX

/*
 * Start timeout routine.
 *
 * Used when card was busy but we needed to send a packet.
 */
static void
ray_start_timo(void *xsc)
{
	struct ray_softc *sc = xsc;
	struct ifnet *ifp;
	int s;

	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_start_timo\n", sc->unit));
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_OACTIVE) && (ifp->if_snd.ifq_head != NULL)) {
		s = splimp();
		ray_start(ifp);
		splx(s);
	}
}

/*
 * Write an 802.11 header into the TX buffer and return the
 * adjusted buffer pointer.
 */
static size_t
ray_start_wrhdr(struct ray_softc *sc, struct ether_header *eh, size_t bufp)
{
	struct ieee80211_header header;

	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_start_wrhdr\n", sc->unit));
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
			printf("ray%d: ray_start can't be an AP yet\n",
			    sc->unit);
	}

	ray_write_region(sc, bufp, (u_int8_t *)&header,
	    sizeof(struct ieee80211_header));

	return (bufp + sizeof(struct ieee80211_header));
}

/*
 * Determine best antenna to use from rx level and antenna cache
 */
static u_int8_t
ray_start_best_antenna(struct ray_softc *sc, u_int8_t *dst)
{
	struct ray_siglev *sl;
	int i;
	u_int8_t antenna;

	RAY_DPRINTFN(RAY_DBG_SUBR,
	    ("ray%d: ray_start_best_antenna\n", sc->unit));
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
ray_start_done(struct ray_softc *sc, size_t ccs, u_int8_t status)
{
	struct ifnet *ifp;
	char *status_string[] = RAY_CCS_STATUS_STRINGS;

	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_start_done\n", sc->unit));
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

	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_rx\n", sc->unit));
	RAY_MAP_CM(sc);

	RAY_DPRINTFN(RAY_DBG_CCS, ("ray%d: rcs chain - using rcs 0x%x\n",
	    sc->unit, rcs));

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
		RAY_DPRINTFN(RAY_DBG_RECERR,
		    ("ray%d: ray_rx packet is too big or too small\n",
		    sc->unit));
		ifp->if_ierrors++;
		goto skip_read;
	}

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL) {
		RAY_DPRINTFN(RAY_DBG_RECERR,
		    ("ray%d: ray_rx MGETHDR failed\n", sc->unit));
		ifp->if_ierrors++;
		goto skip_read;
	}
	if (pktlen > MHLEN) {
		MCLGET(m0, M_DONTWAIT);
		if ((m0->m_flags & M_EXT) == 0) {
			RAY_DPRINTFN(RAY_DBG_RECERR,
			    ("ray%d: ray_rx MCLGET failed\n", sc->unit));
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
		RAY_DPRINTFN(RAY_DBG_RX,
		    ("ray%d: ray_rx frag index %d len %d bufp 0x%x ni %d\n",
		    sc->unit, i, fraglen, (int)bufp, ni));

		if (fraglen + readlen > pktlen) {
			RAY_DPRINTFN(RAY_DBG_RECERR,
			    ("ray%d: ray_rx bad length current 0x%x pktlen 0x%x\n",
			    sc->unit, fraglen + readlen, pktlen));
			ifp->if_ierrors++;
			m_freem(m0);
			m0 = NULL;
			goto skip_read;
		}
		if ((i < RAY_RCS_FIRST) || (i > RAY_RCS_LAST)) {
			printf("ray%d: ray_rx bad rcs index 0x%x\n",
			    sc->unit, i);
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
		RAY_DPRINTFN(RAY_DBG_RECERR,
		    ("ray%d: header not version 0 fc 0x%x\n", sc->unit, fc));
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
		RAY_DPRINTFN(RAY_DBG_MBUF,
		    ("ray%d: ray_rx got a DATA packet\n", sc->unit));
		break;

	default:
		printf("ray%d: ray_rx got a unknown packet fc0 0x%x - why?\n",
		    sc->unit, fc);
		ifp->if_ierrors++;
		m_freem(m0);
		return;

	}
	fc = header->i_fc[1];
	src = header->i_addr2;
	switch (fc & IEEE80211_FC1_DS_MASK) {

	case IEEE80211_FC1_STA_TO_STA:
		RAY_DPRINTFN(RAY_DBG_RX,
		    ("ray%d: ray_rx packet from sta %6D\n",
		    sc->unit, src, ":"));
		break;

	case IEEE80211_FC1_STA_TO_AP:
		RAY_DPRINTFN(RAY_DBG_RX,
		    ("ray%d: ray_rx packet from sta to ap %6D %6D\n",
		    sc->unit, src, ":", header->i_addr3, ":"));
		ifp->if_ierrors++;
		m_freem(m0);
		break;

	case IEEE80211_FC1_AP_TO_STA:
		RAY_DPRINTFN(RAY_DBG_RX, ("ray%d: ray_rx packet from ap %6D\n",
		    sc->unit, src, ":"));
		ifp->if_ierrors++;
		m_freem(m0);
		break;

	case IEEE80211_FC1_AP_TO_AP:
		RAY_DPRINTFN(RAY_DBG_RX,
		    ("ray%d: ray_rx packet between aps %6D %6D\n",
		    sc->unit, src, ":", header->i_addr2, ":"));
		ifp->if_ierrors++;
		m_freem(m0);
		return;

	default:
	    	src = NULL;
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
	int i, mini;
	struct timeval mint;
	struct ray_siglev *sl;

	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_rx_update_cache\n", sc->unit));
	RAY_MAP_CM(sc);

	/* try to find host */
	for (i = 0; i < RAY_NSIGLEVRECS; i++) {
		sl = &sc->sc_siglevs[i];
		if (bcmp(sl->rsl_host, src, ETHER_ADDR_LEN) == 0)
			goto found;
	}
	/* not found, find oldest slot */
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
	int i, count;

	sc = &ray_softc[dev_p->isahd.id_unit];

	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_intr\n", sc->unit));
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	if (sc->gone) {
		printf("ray%d: ray_intr unloaded!\n", sc->unit);
		return (0);
	}

	if ((++sc->sc_checkcounters % 32) == 0)
		ray_intr_updt_errcntrs(sc);

	/*
	 * Check that the interrupt was for us, if so get the rcs/ccs
	 * and vector on the command contained within it.
	 */
	if (!RAY_HCS_INTR(sc))
		count = 0;
	else {
		count = 1;
		i = SRAM_READ_1(sc, RAY_SCB_RCSI);
		if (i <= RAY_CCS_LAST)
			ray_ccs_done(sc, RAY_CCS_ADDRESS(i));
		else if (i <= RAY_RCS_LAST)
			ray_rcs_intr(sc, RAY_CCS_ADDRESS(i));
		else
		    printf("ray%d: ray_intr bad ccs index %d\n", sc->unit, i);
	}

	if (count)
		RAY_HCS_CLEAR_INTR(sc);

	RAY_DPRINTFN(RAY_DBG_RX, ("ray%d: interrupt %s handled\n",
	    sc->unit, count?"was":"not"));

	/* Send any packets lying around */
	if (!(ifp->if_flags & IFF_OACTIVE) && (ifp->if_snd.ifq_head != NULL))
		ray_start(ifp);

	return (count);
}

/*
 * Read the error counters.
 *
 * The card implements the following protocol to keep the values from
 * being changed while read: It checks the `own' bit and if zero
 * writes the current internal counter value, it then sets the `own'
 * bit to 1. If the `own' bit was 1 it incremenets its internal
 * counter. The user thus reads the counter if the `own' bit is one
 * and then sets the own bit to 0.
 */
static void
ray_intr_updt_errcntrs(struct ray_softc *sc)
{
	size_t csc;

	RAY_DPRINTFN(RAY_DBG_SUBR,
	    ("ray%d: ray_intr_updt_errcntrs\n", sc->unit));
	RAY_MAP_CM(sc);

	/* try and update the error counters */
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
ray_ccs_done(struct ray_softc *sc, size_t ccs)
{
	struct ifnet *ifp;
	u_int cmd, status;
    
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	/* XXX don't really need stat here? */
	cmd = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd);
	status = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
	RAY_DPRINTF(sc, RAY_DBG_CCS,
	    "ccs index 0x%02x ccs addr 0x%02x cmd 0x%x status %d",
	    RAY_CCS_INDEX(ccs), ccs, cmd, status);

	switch (cmd) {

	case RAY_CMD_DOWNLOAD_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START_PARAMS");
		ray_download_done(sc, ccs);
		break;

	case RAY_CMD_UPDATE_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "UPDATE_PARAMS");
		ray_update_params_done(sc, ccs, status);
		break;

	case RAY_CMD_REPORT_PARAMS:
		RAY_DPRINTF(sc, RAY_DBG_COM, "REPORT_PARAMS");
		/* XXX proper function and don't forget to ecf_done */
		/* get the reported parameters */
		if (!sc->sc_repreq)
			break;
		sc->sc_repreq->r_failcause =
		    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_report, c_failcause);
		sc->sc_repreq->r_len =
		    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_report, c_len);
		ray_read_region(sc, RAY_ECF_TO_HOST_BASE, sc->sc_repreq->r_data,
		    sc->sc_repreq->r_len);
		sc->sc_repreq = 0;
		wakeup(ray_report_params);
		break;

	case RAY_CMD_UPDATE_MCAST:
		RAY_DPRINTF(sc, RAY_DBG_COM, "UPDATE_MCAST");
		ray_mcast_done(sc, ccs);
		break;

	case RAY_CMD_START_NET:
	case RAY_CMD_JOIN_NET:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START|JOIN_NET");
		ray_sj_done(sc, ccs);
		break;

	case RAY_CMD_TX_REQ:
		RAY_DPRINTF(sc, RAY_DBG_COM, "TX_REQ");
		ray_start_done(sc, ccs, status);
		goto done;

	case RAY_CMD_START_ASSOC:
		RAY_DPRINTF(sc, RAY_DBG_COM, "START_ASSOC");
#if XXX_ASSOCWORKING_AGAIN
		ray_start_assoc_done(sc, ccs);
#endif XXX_ASSOCWORKING_AGAIN
		break;

	case RAY_CMD_UPDATE_APM:
		printf("ray%d: ray_ccs_done got UPDATE_APM - why?\n", sc->unit);
		break;

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
		printf("ray%d: ray_ccs_done got START_TIMER - why?\n",
		    sc->unit);
		break;

	default:
		printf("ray%d: ray_ccs_done unknown command 0x%x\n",
		    sc->unit, cmd);
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
ray_rcs_intr(struct ray_softc *sc, size_t rcs)
{
	struct ifnet *ifp;
	u_int cmd, status;
    
	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_rcs_intr\n", sc->unit));
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	cmd = SRAM_READ_FIELD_1(sc, rcs, ray_cmd, c_cmd);
	status = SRAM_READ_FIELD_1(sc, rcs, ray_cmd, c_status);
	RAY_DPRINTFN(RAY_DBG_CCS,
	    ("ray%d: rcs idx %d rcs 0x%x cmd 0x%x status %d\n",
	    sc->unit, RAY_CCS_INDEX(rcs), rcs, cmd, status));

	switch (cmd) {

	case RAY_ECMD_RX_DONE:
		RAY_DPRINTFN(RAY_DBG_CCS, ("ray%d: ray_rcs_intr got RX_DONE\n",
		    sc->unit));
		ray_rx(sc, rcs);
		break;

	case RAY_ECMD_REJOIN_DONE:
		RAY_DPRINTFN(RAY_DBG_CCS, ("ray%d: ray_rcs_intr got REJOIN_DONE\n",
		    sc->unit));
		sc->sc_havenet = 1; /* Should not be here but in function */
		XXX;
		break;

	case RAY_ECMD_ROAM_START:
		RAY_DPRINTFN(RAY_DBG_CCS, ("ray%d: ray_rcs_intr got ROAM_START\n",
		    sc->unit));
		sc->sc_havenet = 0; /* Should not be here but in function */
		XXX;
		break;

	case RAY_ECMD_JAPAN_CALL_SIGNAL:
		printf("ray%d: ray_rcs_intr got JAPAN_CALL_SIGNAL - why?\n",
		    sc->unit);
		break;

	default:
		printf("ray%d: ray_rcs_intr unknown command 0x%x\n",
		    sc->unit, cmd);
		break;
	}

	RAY_CCS_FREE(sc, rcs);
}

/*
 * Functions based on CCS commands
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

	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		return (0);
	}

	/*
	 * If we need to change the promiscuous mode then do so.
	 */
	if (sc->promisc != !!(ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI))) {
		MALLOC(com[0], struct ray_comq_entry *,
		    sizeof(struct ray_comq_entry), M_RAYCOM, M_WAITOK);
		com[0]->c_function = ray_promisc;
		com[0]->c_flags = RAY_COM_FWOK; 
		com[0]->c_retval = 0;
		com[0]->c_ccs = NULL;
		com[0]->c_wakeup = com[1];
#if RAY_DEBUG > 0
		com[0]->c_mesg = "ray_promisc";
#endif /* RAY_DEBUG > 0 */
		ray_com_runq_add(sc, com[0]);
	} else
	    com[0] = NULL;

	/*
	 * If we need to set the mcast list then do so.
	 */
	if (!(ifp->if_flags & IFF_ALLMULTI))
		MALLOC(com[1], struct ray_comq_entry *,
		    sizeof(struct ray_comq_entry), M_RAYCOM, M_WAITOK);
		com[1]->c_function = ray_mcast;
		com[0]->c_flags &= ~RAY_COM_FWOK; 
		com[1]->c_flags = RAY_COM_FWOK; 
		com[1]->c_retval = 0;
		com[1]->c_ccs = NULL;
		com[1]->c_wakeup = com[1];
#if RAY_DEBUG > 0
		com[1]->c_mesg = "ray_mcast";
#endif /* RAY_DEBUG > 0 */
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
 * Set the multicast filter list
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
	RAY_DCOM_CHECK(sc, ccs);

	ray_com_ecf_done(sc);
}

#if 0
/*
 * User land entry to promiscuous mode changes
 */
static int
ray_promisc_user(struct ray_softc *sc)
{
	struct ifnet *ifp;
	struct ray_comq_entry *com;
	int error;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return (0);
	if (sc->promisc != !!(ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI)))
		return (0);

	MALLOC(com, struct ray_comq_entry *, sizeof(struct ray_comq_entry),
	    M_RAYCOM, M_WAITOK);
	com->c_function = ray_promisc;
	com->c_flags = RAY_COM_FWOK;
	com->c_retval = 0;
	com->c_ccs = NULL;
	com->c_wakeup = com;
#if RAY_DEBUG > 0
	com->c_mesg = "ray_promisc";
#endif /* RAY_DEBUG > 0 */
	ray_com_runq_add(sc, com);

	ray_com_runq(sc);
	RAY_DPRINTF(sc, RAY_DBG_COM, "sleeping");
	(void)tsleep(com[3], 0, "raypromisc", 0);
	RAY_DPRINTF(sc, RAY_DBG_COM, "awakened");

	error = com->c_retval;
	FREE(com, M_RAYCOM);
	return (error);
}

/*
 * Set/reset promiscuous mode
 */
static void
ray_promisc(struct ray_softc *sc, struct ray_comq_entry *com)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	(void)ray_ccs_alloc(sc, &com->c_ccs, RAY_CMD_UPDATE_PARAMS, 0);
	SRAM_WRITE_FIELD_1(sc, &com->c_ccs,
	    ray_cmd_update, c_paramid, RAY_MIB_PROMISC);
	SRAM_WRITE_FIELD_1(sc, &com->c_ccs, ray_cmd_update, c_nparam, 1);
	SRAM_WRITE_1(sc, RAY_HOST_TO_ECF_BASE, 
	    !!(ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI)));

	ray_com_ecf(sc, com);
}

/*
 * Complete the promiscuous mode update
 */
static void
ray_promisc_done(struct ray_softc *sc, size_t ccs)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");
	RAY_DCOM_CHECK(sc, ccs);

	ray_com_ecf_done(sc);
}

/*
 * issue a report params
 *
 * expected to be called in sleapable context -- intended for user stuff
 */
static int
ray_user_report_params(struct ray_softc *sc, struct ray_param_req *pr)
{
	struct ifnet *ifp;
	int mib_sizes[] = RAY_MIB_SIZES;
	int rv;

	RAY_DPRINTFN(RAY_DBG_SUBR,
	    ("ray%d: ray_user_report_params\n", sc->unit));
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
		return (EIO);
	}
	
	/* test for illegal values or immediate responses */
	if (pr->r_paramid > RAY_MIB_LASTUSER) {
	    	switch (pr->r_paramid) {

		case  RAY_MIB_VERSION:
			if (sc->sc_version == RAY_ECFS_BUILD_4)
			    *pr->r_data = 4;
			else
			    *pr->r_data = 5;
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
		pr->r_len = mib_sizes[pr->r_paramid];
		return (0);
	}

	/* wait to be able to issue the command */
	rv = 0;
	while (ray_cmd_is_running(sc, SCP_REPORTPARAMS)
	    || ray_cmd_is_scheduled(sc, SCP_REPORTPARAMS)) {
		rv = tsleep(ray_report_params, 0|PCATCH, "cmd in use", 0);
		if (rv)
			return (rv);
		if ((ifp->if_flags & IFF_RUNNING) == 0) {
			pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
			return (EIO);
		}
	}

	pr->r_failcause = RAY_FAILCAUSE_WAITING;
	sc->sc_repreq = pr;
	ray_cmd_schedule(sc, SCP_REPORTPARAMS);
	ray_cmd_check_scheduled(sc);

	while (pr->r_failcause == RAY_FAILCAUSE_WAITING)
		(void)tsleep(ray_report_params, 0, "waiting cmd", 0);
	wakeup(ray_report_params);

	return (0);
}

/*
 * report a parameter
 */
static void
ray_report_params(struct ray_softc *sc)
{
	struct ifnet *ifp;
	size_t ccs;

	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_report_params\n", sc->unit));
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	if (!sc->sc_repreq)
		return;

	/* do the issue check before equality check */
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
	else if (!ray_ccs_alloc(sc, &ccs, RAY_CMD_REPORT_PARAMS, 0))
		return;

	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_report, c_paramid,
	    sc->sc_repreq->r_paramid);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_report, c_nparam, 1);

}

/*
 * Return the error counters
 */
static int
ray_user_report_stats(struct ray_softc *sc, struct ray_stats_req *sr)
{
	struct ifnet *ifp;

	RAY_DPRINTF(sc, RAY_DBG_SUBR, "");

	ifp = &sc->arpcom.ac_if;
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		return (EIO);
	}

	sr->rxoverflow = sc->sc_rxoverflow;
	sr->rxcksum = sc->sc_rxcksum;
	sr->rxhcksum = sc->sc_rxhcksum;
	sr->rxnoise = sc->sc_rxnoise;

	return (0);
}

/*
 * issue a update params
 *
 * expected to be called in sleepable context -- intended for user stuff
 */
static int
ray_user_update_params(struct ray_softc *sc, struct ray_param_req *pr)
{
	struct ifnet *ifp;
	int rv;

	RAY_DPRINTFN(RAY_DBG_SUBR,
	    ("ray%d: ray_user_update_params\n", sc->unit));
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
		return (EIO);
	}

	if (pr->r_paramid > RAY_MIB_MAX) {
		return (EINVAL);
	}

	/*
	 * Handle certain parameters specially
	 */
	switch (pr->r_paramid) {
	case RAY_MIB_NET_TYPE:
		if (sc->sc_c.np_net_type == *pr->r_data)
			return (0);
		sc->sc_d.np_net_type = *pr->r_data;
		if (ifp->if_flags & IFF_RUNNING)
			ray_sj_net(sc);
		return (0);

	case RAY_MIB_SSID:
		if (bcmp(sc->sc_c.np_ssid, pr->r_data, IEEE80211_NWID_LEN) == 0)
			return (0);
		bcopy(pr->r_data, sc->sc_d.np_ssid, IEEE80211_NWID_LEN);
		if (ifp->if_flags & IFF_RUNNING)
			ray_sj_net(sc);
		return (0);

	case RAY_MIB_BASIC_RATE_SET:
		sc->sc_d.np_def_txrate = *pr->r_data;
		break;

	case RAY_MIB_AP_STATUS:	/* Unsupported */
	case RAY_MIB_MAC_ADDR:	/* XXX Need interface up */
	case RAY_MIB_PROMISC:	/* BPF */
		return (EINVAL);
		break;

	default:
		break;
	}

	if (pr->r_paramid > RAY_MIB_LASTUSER) {
		return (EINVAL);
	}

	/* wait to be able to issue the command */
	rv = 0;
	while (ray_cmd_is_running(sc, SCP_UPD_UPDATEPARAMS) ||
	    ray_cmd_is_scheduled(sc, SCP_UPD_UPDATEPARAMS)) {
		rv = tsleep(ray_update_params, 0|PCATCH, "cmd in use", 0);
		if (rv)
			return (rv);
		if ((ifp->if_flags & IFF_RUNNING) == 0) {
			pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
			return (EIO);
		}
	}

	pr->r_failcause = RAY_FAILCAUSE_WAITING;
	sc->sc_updreq = pr;
	ray_cmd_schedule(sc, SCP_UPD_UPDATEPARAMS);
	ray_cmd_check_scheduled(sc);

	while (pr->r_failcause == RAY_FAILCAUSE_WAITING)
		(void)tsleep(ray_update_params, 0, "waiting cmd", 0);
	wakeup(ray_update_params);

	return (0);
}

/*
 * update the parameter based on what the user passed in
 */
static void
ray_update_params(struct ray_softc *sc)
{
	struct ifnet *ifp;
	size_t ccs;

	RAY_DPRINTFN(RAY_DBG_SUBR, ("ray%d: ray_update_params\n", sc->unit));
	RAY_MAP_CM(sc);

	ifp = &sc->arpcom.ac_if;

	ray_cmd_cancel(sc, SCP_UPD_UPDATEPARAMS);
	if (!sc->sc_updreq) {
		/* XXX do we need to wakeup here? */
		return;
	}

	/* do the issue check before equality check */
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
	else if (ray_cmd_is_running(sc, SCP_UPDATESUBCMD)) {
		ray_cmd_schedule(sc, SCP_UPD_UPDATEPARAMS);
		return;
	} else if (!ray_ccs_alloc(sc, &ccs, RAY_CMD_UPDATE_PARAMS, 0))
		return;

	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_update, c_paramid,
	    sc->sc_updreq->r_paramid);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_update, c_nparam, 1);
	ray_write_region(sc, RAY_HOST_TO_ECF_BASE, sc->sc_updreq->r_data,
	    sc->sc_updreq->r_len);

	(void)ray_cmd_issue(sc, ccs, SCP_UPD_UPDATEPARAMS);
}

/*
 * an update params command has completed lookup which command and
 * the status
 *
 * XXX this isn't finished yet, we need to grok the command used
 */
static void
ray_update_params_done(struct ray_softc *sc, size_t ccs, u_int stat)
{
	RAY_DPRINTFN(RAY_DBG_SUBR,
	    ("ray%d: ray_update_params_done\n", sc->unit));
	RAY_MAP_CM(sc);

	/* this will get more complex as we add commands */
	if (stat == RAY_CCS_STATUS_FAIL) {
		printf("ray%d: failed to update a promisc\n", sc->unit);
		/* XXX should probably reset */
		/* rcmd = ray_reset; */
	}

	if (sc->sc_running & SCP_UPD_PROMISC) {
		ray_cmd_done(sc, SCP_UPD_PROMISC);
		sc->sc_promisc = SRAM_READ_1(sc, RAY_HOST_TO_ECF_BASE);
		RAY_DPRINTFN(RAY_DBG_IOCTL,
		    ("ray%d: new promisc value %d\n", sc->unit,
		    sc->sc_promisc));
	} else if (sc->sc_updreq) {
		ray_cmd_done(sc, SCP_UPD_UPDATEPARAMS);
		/* get the update parameter */
		sc->sc_updreq->r_failcause =
		    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_update, c_failcause);
		sc->sc_updreq = 0;
		wakeup(ray_update_params);
		ray_sj_net(sc);
	}
}

#else
static void ray_update_params(struct ray_softc *sc) {}
static void ray_update_params_done(struct ray_softc *sc, size_t ccs, u_int stat) {}

static int ray_mcast_user(struct ray_softc *sc) {return (0);}
static void ray_mcast(struct ray_softc *sc, struct ray_comq_entry *com) {}
static void ray_mcast_done(struct ray_softc *sc, size_t ccs) {}
static int ray_promisc_user(struct ray_softc *sc) {return (0);}
static void ray_promisc(struct ray_softc *sc, struct ray_comq_entry *com) {}
static void ray_promisc_done(struct ray_softc *sc, size_t ccs) {}


static int ray_user_update_params(struct ray_softc *sc, struct ray_param_req *pr) {return (0);}
static int ray_user_report_params(struct ray_softc *sc, struct ray_param_req *pr) {return (0);}
#endif

/*
 * Command queuing and execution
 *
 * XXX
 * Set up a command queue.  To submit a command, you do this:
 *
 *      s = splnet()
 *      put_cmd_on_queue(sc, cmd)
 *      start_command_on_queue(sc)
 *      tsleep(com, 0, "raycmd", 0)
 *      splx(s)
 *      handle_completed_command(cmd)
 *
 * The start_command_on_queue() function looks like this:
 *
 *      if (device_ready_for_command(sc) && queue_not_empty(sc))
 *              running_cmd = pop_command_from_queue(sc)
 *              submit_command(running_cmd)
 *
 *
 * In your interrupt handler you do:
 *
 *      if (interrupt_is_completed_command(sc))
 *              wakeup(running_cmd)
 *              running_cmd = NULL;
 *              start_command_on_queue(sc)
 */

/*
 * Add a command to the tail of the queue
 */
static void
ray_com_runq_add(struct ray_softc *sc, struct ray_comq_entry *com)
{
	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");

	RAY_DCOM_DUMP(sc, com, "adding");
	TAILQ_INSERT_TAIL(&sc->sc_comq, com, c_chain);
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

	com->c_flags |= RAY_COM_FRUNNING;
	RAY_DCOM_DUMP(sc, com, "running");
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
	RAY_DCOM_DUMP(sc, com, "aborting");
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

	RAY_DCOM_DUMP(sc, com, "removing");
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

	RAY_DCOM_DUMP(sc, com, "removing");
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
	 * what we really want to do is just make sure we don't
	 * get here or that spinning is ok
	 *
	 * XXX actually we probably want to call a timeout on
	 * XXX ourself here...
	 */
	i = 0;
	while (!RAY_ECF_READY(sc))
		if (++i > 50) {
			printf("\n");
			RAY_PANIC(sc, "spun too long");
		} else if (i == 1)
			printf("ray%d: ray_com_issue spinning", sc->unit);
		else
			printf(".");

	RAY_DCOM_DUMP(sc, com, "");
	SRAM_WRITE_1(sc, RAY_SCB_CCSI, RAY_CCS_INDEX(com->c_ccs));
	RAY_ECF_START_CMD(sc);

	if (RAY_COM_NEEDS_TIMO(
	    SRAM_READ_FIELD_1(sc, com->c_ccs, ray_cmd, c_cmd)
	    )) {
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
	struct ray_softc *sc = xsc;
    	struct ray_comq_entry *com;
	u_int8_t status;
	int s;

	s = splnet();

	RAY_DPRINTF(sc, RAY_DBG_SUBR | RAY_DBG_COM, "");
	RAY_MAP_CM(sc);

	com = TAILQ_FIRST(&sc->sc_comq);
#if RAY_DEBUG & RAY_DBG_COM /* XXX get rid of this at some point or make it KASSERT */
	if (com == NULL)
		RAY_PANIC(sc, "no command queue");
#endif /* RAY_DEBUG & RAY_DBG_COM */

	status = SRAM_READ_FIELD_1(sc, com->c_ccs, ray_cmd, c_status);
	RAY_DPRINTF(sc, RAY_DBG_COM, "ccs 0x%02x status %d",
	    RAY_CCS_INDEX(com->c_ccs), status);
	
	switch (status) {

	case RAY_CCS_STATUS_COMPLETE:
	case RAY_CCS_STATUS_FREE:			/* Buggy firmware */
		ray_ccs_done(sc, com->c_ccs);
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
			ray_ccs_done(sc, com->c_ccs);
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

/******************************************************************************
 * XXX NOT KNF FROM HERE DOWN
 ******************************************************************************/

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
ray_attr_getmap(struct ray_softc *sc)
{
    struct ucred	uc;
    struct pcred	pc;
    struct proc		p;
    int			result;

    RAY_DPRINTFN(RAY_DBG_SUBR,
        ("ray%d: attempting to get map for common memory\n", sc->unit));

    sc->md.window = 0;

    p.p_cred = &pc;
    p.p_cred->pc_ucred = &uc;
    p.p_cred->pc_ucred->cr_uid = 0;

    result = cdevsw[CARD_MAJOR]->d_ioctl(makedev(CARD_MAJOR, sc->slotnum), PIOCGMEM, (caddr_t)&sc->md, 0, &p);

    return;
}

static void
ray_attr_cm(struct ray_softc *sc)
{
    struct ucred uc;
    struct pcred pc;
    struct proc p;

    RAY_DPRINTFN(RAY_DBG_CM,
        ("ray%d: attempting to remap common memory\n", sc->unit));

    p.p_cred = &pc;
    p.p_cred->pc_ucred = &uc;
    p.p_cred->pc_ucred->cr_uid = 0;

    cdevsw[CARD_MAJOR]->d_ioctl(makedev(CARD_MAJOR, sc->slotnum), PIOCSMEM, (caddr_t)&sc->md, 0, &p);

    return;
}
#endif /* (RAY_NEED_CM_REMAPPING | RAY_NEED_CM_FIXUP) */

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
  ray_attr_cm(sc);
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
  ray_attr_cm(sc);
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
#endif /* RAY_DEBUG & RAY_DBG_MBUF */

#endif /* NRAY */