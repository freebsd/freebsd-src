/*	$NetBSD: awi_wicfg.c,v 1.3 2000/07/06 17:22:25 onoe Exp $	*/
/* $FreeBSD$ */

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * WaveLAN compatible configuration support routines for the awi driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#if defined(__FreeBSD__) && __FreeBSD_version >= 400000
#include <sys/bus.h>
#else
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#ifdef __FreeBSD__
#include <net/ethernet.h>
#include <net/if_arp.h>
#else
#include <net/if_ether.h>
#endif
#include <net/if_media.h>

#include <net80211/ieee80211.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#ifdef __NetBSD__
#include <dev/ic/am79c930reg.h>
#include <dev/ic/am79c930var.h>
#include <dev/ic/awireg.h>
#include <dev/ic/awivar.h>

#include <dev/pcmcia/if_wi_ieee.h>	/* XXX */
#endif
#ifdef __FreeBSD__
#include <dev/awi/am79c930reg.h>
#include <dev/awi/am79c930var.h>

#undef	_KERNEL		/* XXX */
#include <dev/wi/if_wavelan_ieee.h>	/* XXX */
#define	_KERNEL		/* XXX */
#include <dev/awi/awireg.h>
#include <dev/awi/awivar.h>
#endif

static int awi_cfgget(struct ifnet *ifp, u_long cmd, caddr_t data);
static int awi_cfgset(struct ifnet *ifp, u_long cmd, caddr_t data);

int
awi_wicfg(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	int error;

	switch (cmd) {
	case SIOCGWAVELAN:
		error = awi_cfgget(ifp, cmd, data);
		break;
	case SIOCSWAVELAN:
#ifdef __FreeBSD__
#if __FreeBSD_version < 500028
		error = suser(curproc);
#else
		error = suser(curthread);
#endif
#else
		error = suser(curproc->p_ucred, &curproc->p_acflag);
#endif
		if (error)
			break;
		error = awi_cfgset(ifp, cmd, data);
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

static int
awi_cfgget(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	int i, error, keylen;
	char *p;
	struct awi_softc *sc = (struct awi_softc *)ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
        struct wi_ltv_keys *keys;
        struct wi_key *k;
	struct wi_req wreq;
#ifdef WICACHE
	struct wi_sigcache wsc;
	struct awi_bss *bp;
#endif /* WICACHE */

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
		memcpy(wreq.wi_val, sc->sc_banner, AWI_BANNER_LEN);
		wreq.wi_len = (AWI_BANNER_LEN + 1) / 2;
		break;
	case WI_RID_NODENAME:
		strcpy((char *)&wreq.wi_val[1], hostname);
		wreq.wi_val[0] = strlen(hostname);
		wreq.wi_len = (1 + wreq.wi_val[0] + 1) / 2;
		break;
	case WI_RID_OWN_SSID:
		p = sc->sc_ownssid;
		wreq.wi_val[0] = p[1];
		memcpy(&wreq.wi_val[1], p + 2, p[1]);
		wreq.wi_len = (1 + wreq.wi_val[0] + 1) / 2;
		break;
	case WI_RID_CURRENT_SSID:
		if (ifp->if_flags & IFF_RUNNING) {
			p = sc->sc_bss.essid;
			wreq.wi_val[0] = p[1];
			memcpy(&wreq.wi_val[1], p + 2, p[1]);
		} else {
			wreq.wi_val[0] = 0;
			wreq.wi_val[1] = '\0';
		}
		wreq.wi_len = (1 + wreq.wi_val[0] + 1) / 2;
		break;
	case WI_RID_DESIRED_SSID:
		p = sc->sc_mib_mac.aDesired_ESS_ID;
		wreq.wi_val[0] = p[1];
		memcpy(&wreq.wi_val[1], p + 2, p[1]);
		wreq.wi_len = (1 + wreq.wi_val[0] + 1) / 2;
		break;
	case WI_RID_CURRENT_BSSID:
		if (ifp->if_flags & IFF_RUNNING)
			memcpy(wreq.wi_val, sc->sc_bss.bssid, ETHER_ADDR_LEN);
		else
			memset(wreq.wi_val, 0, ETHER_ADDR_LEN);
		wreq.wi_len = ETHER_ADDR_LEN / 2;
		break;
	case WI_RID_CHANNEL_LIST:
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH) {
			wreq.wi_val[0] = sc->sc_scan_min;
			wreq.wi_val[1] = sc->sc_scan_max;
			wreq.wi_len = 2;
		} else {
			wreq.wi_val[0] = 0;
			for (i = sc->sc_scan_min; i <= sc->sc_scan_max; i++)
				wreq.wi_val[0] |= 1 << (i - 1);
			wreq.wi_len = 1;
		}
		break;
	case WI_RID_OWN_CHNL:
		wreq.wi_val[0] = sc->sc_ownch;
		wreq.wi_len = 1;
		break;
	case WI_RID_CURRENT_CHAN:
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH)
			wreq.wi_val[0] = sc->sc_bss.pattern;
		else
			wreq.wi_val[0] = sc->sc_bss.chanset;
		wreq.wi_len = 1;
		break;
	case WI_RID_COMMS_QUALITY:
		wreq.wi_val[0] = 0;			/* quality */
		wreq.wi_val[1] = sc->sc_bss.rssi;	/* signal */
		wreq.wi_val[2] = 0;			/* noise */
		wreq.wi_len = 3;
		break;
	case WI_RID_PROMISC:
		wreq.wi_val[0] = sc->sc_mib_mac.aPromiscuous_Enable;
		wreq.wi_len = 1;
		break;
	case WI_RID_PORTTYPE:
		if (sc->sc_mib_local.Network_Mode)
			wreq.wi_val[0] = 1;
		else if (!sc->sc_no_bssid)
			wreq.wi_val[0] = 2;
		else
			wreq.wi_val[0] = 3;
		wreq.wi_len = 1;
		break;
	case WI_RID_MAC_NODE:
		memcpy(wreq.wi_val, sc->sc_mib_addr.aMAC_Address,
		    ETHER_ADDR_LEN);
		wreq.wi_len = ETHER_ADDR_LEN / 2;
		break;
	case WI_RID_TX_RATE:
	case WI_RID_CUR_TX_RATE:
		wreq.wi_val[0] = sc->sc_tx_rate / 10;
		wreq.wi_len = 1;
		break;
	case WI_RID_RTS_THRESH:
		wreq.wi_val[0] = LE_READ_2(&sc->sc_mib_mac.aRTS_Threshold);
		wreq.wi_len = 1;
		break;
	case WI_RID_CREATE_IBSS:
		wreq.wi_val[0] = sc->sc_start_bss;
		wreq.wi_len = 1;
		break;
	case WI_RID_SYSTEM_SCALE:
		wreq.wi_val[0] = 1;	/* low density ... not supported */
		wreq.wi_len = 1;
		break;
	case WI_RID_PM_ENABLED:
		wreq.wi_val[0] = sc->sc_mib_local.Power_Saving_Mode_Dis ? 0 : 1;
		wreq.wi_len = 1;
		break;
	case WI_RID_MAX_SLEEP:
		wreq.wi_val[0] = 0;	/* not implemented */
		wreq.wi_len = 1;
		break;
	case WI_RID_WEP_AVAIL:
		wreq.wi_val[0] = 1;
		wreq.wi_len = 1;
		break;
	case WI_RID_ENCRYPTION:
		wreq.wi_val[0] = awi_wep_getalgo(sc);
		wreq.wi_len = 1;
		break;
	case WI_RID_TX_CRYPT_KEY:
		wreq.wi_val[0] = sc->sc_wep_defkid;
		wreq.wi_len = 1;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		keys = (struct wi_ltv_keys *)&wreq;
		/* do not show keys to non-root user */
#ifdef __FreeBSD__
#if __FreeBSD_version < 500028
		error = suser(curproc);
#else
		error = suser(curthread);
#endif
#else
		error = suser(curproc->p_ucred, &curproc->p_acflag);
#endif
		if (error) {
			memset(keys, 0, sizeof(*keys));
			error = 0;
			break;
		}
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			k = &keys->wi_keys[i];
			keylen = sizeof(k->wi_keydat);
			error = awi_wep_getkey(sc, i, k->wi_keydat, &keylen);
			if (error)
				break;
			k->wi_keylen = keylen;
		}
		wreq.wi_len = sizeof(*keys) / 2;
		break;
	case WI_RID_MAX_DATALEN:
		wreq.wi_val[0] = LE_READ_2(&sc->sc_mib_mac.aMax_Frame_Length);
		wreq.wi_len = 1;
		break;
	case WI_RID_IFACE_STATS:
		/* not implemented yet */
		wreq.wi_len = 0;
		break;
#ifdef WICACHE
	case WI_RID_READ_CACHE:
		for (bp = TAILQ_FIRST(&sc->sc_scan), i = 0;
		    bp != NULL && i < MAXWICACHE;
		    bp = TAILQ_NEXT(bp, list), i++) {
			memcpy(wsc.macsrc, bp->esrc, ETHER_ADDR_LEN);
			/*XXX*/
			memcpy(&wsc.ipsrc, bp->bssid, sizeof(wsc.ipsrc));
			wsc.signal = bp->rssi;
			wsc.noise = 0;
			wsc.quality = 0;
			memcpy((caddr_t)wreq.wi_val + sizeof(wsc) * i,
			    &wsc, sizeof(wsc));
		}
		wreq.wi_len = sizeof(wsc) * i / 2;
		break;
#endif /* WICACHE */
	default:
		error = EINVAL;
		break;
	}
	if (error == 0) {
		wreq.wi_len++;
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
	}
	return error;
}

static int
awi_cfgset(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	int i, error, rate, oregion;
	u_int8_t *phy_rates;
	struct awi_softc *sc = (struct awi_softc *)ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
        struct wi_ltv_keys *keys;
        struct wi_key *k;
	struct wi_req wreq;

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	if (wreq.wi_len-- < 1)
		return EINVAL;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
	case WI_RID_NODENAME:
		error = EPERM;
		break;
	case WI_RID_OWN_SSID:
		if (wreq.wi_len < (1 + wreq.wi_val[0] + 1) / 2) {
			error = EINVAL;
			break;
		}
		if (wreq.wi_val[0] > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		memset(sc->sc_ownssid, 0, AWI_ESS_ID_SIZE);
		sc->sc_ownssid[0] = IEEE80211_ELEMID_SSID;
		sc->sc_ownssid[1] = wreq.wi_val[0];
		memcpy(&sc->sc_ownssid[2], &wreq.wi_val[1], wreq.wi_val[0]);
		if (!sc->sc_mib_local.Network_Mode &&
		    !sc->sc_no_bssid && sc->sc_start_bss)
			error = ENETRESET;
		break;
	case WI_RID_CURRENT_SSID:
		error = EPERM;
		break;
	case WI_RID_DESIRED_SSID:
		if (wreq.wi_len < (1 + wreq.wi_val[0] + 1) / 2) {
			error = EINVAL;
			break;
		}
		if (wreq.wi_val[0] > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		memset(sc->sc_mib_mac.aDesired_ESS_ID, 0, AWI_ESS_ID_SIZE);
		sc->sc_mib_mac.aDesired_ESS_ID[0] = IEEE80211_ELEMID_SSID;
		sc->sc_mib_mac.aDesired_ESS_ID[1] = wreq.wi_val[0];
		memcpy(&sc->sc_mib_mac.aDesired_ESS_ID[2], &wreq.wi_val[1],
		    wreq.wi_val[0]);
		if (sc->sc_mib_local.Network_Mode || !sc->sc_no_bssid)
			error = ENETRESET;
		break;
	case WI_RID_CURRENT_BSSID:
		error = EPERM;
		break;
	case WI_RID_CHANNEL_LIST:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		oregion = sc->sc_mib_phy.aCurrent_Reg_Domain;
		if (wreq.wi_val[0] == oregion)
			break;
		sc->sc_mib_phy.aCurrent_Reg_Domain = wreq.wi_val[0];
		error = awi_init_region(sc);
		if (error) {
			sc->sc_mib_phy.aCurrent_Reg_Domain = oregion;
			break;
		}
		error = ENETRESET;
		break;
	case WI_RID_OWN_CHNL:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		if (wreq.wi_val[0] < sc->sc_scan_min ||
		    wreq.wi_val[0] > sc->sc_scan_max) {
			error = EINVAL;
			break;
		}
		sc->sc_ownch = wreq.wi_val[0];
		if (!sc->sc_mib_local.Network_Mode)
			error = ENETRESET;
		break;
	case WI_RID_CURRENT_CHAN:
		error = EPERM;
		break;
	case WI_RID_COMMS_QUALITY:
		error = EPERM;
		break;
	case WI_RID_PROMISC:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		if (ifp->if_flags & IFF_PROMISC) {
			if (wreq.wi_val[0] == 0) {
				ifp->if_flags &= ~IFF_PROMISC;
				error = ENETRESET;
			}
		} else {
			if (wreq.wi_val[0] != 0) {
				ifp->if_flags |= IFF_PROMISC;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_PORTTYPE:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		switch (wreq.wi_val[0]) {
		case 1:
			sc->sc_mib_local.Network_Mode = 1;
			sc->sc_no_bssid = 0;
			error = ENETRESET;
			break;
		case 2:
			sc->sc_mib_local.Network_Mode = 0;
			sc->sc_no_bssid = 0;
			error = ENETRESET;
			break;
		case 3:
			if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH) {
				error = EINVAL;
				break;
			}
			sc->sc_mib_local.Network_Mode = 0;
			sc->sc_no_bssid = 1;
			error = ENETRESET;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case WI_RID_MAC_NODE:
		/* XXX: should be implemented? */
		error = EPERM;
		break;
	case WI_RID_TX_RATE:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		phy_rates = sc->sc_mib_phy.aSuprt_Data_Rates;
		switch (wreq.wi_val[0]) {
		case 1:
		case 2:
		case 5:
		case 11:
			rate = wreq.wi_val[0] * 10;
			if (rate == 50)
				rate += 5;	/*XXX*/
			break;
		case 3:
		case 6:
		case 7:
			/* auto rate */
			phy_rates = sc->sc_mib_phy.aSuprt_Data_Rates;
			rate = AWI_RATE_1MBIT;
			for (i = 0; i < phy_rates[1]; i++) {
				if (AWI_80211_RATE(phy_rates[2 + i]) > rate)
					rate = AWI_80211_RATE(phy_rates[2 + i]);
			}
			break;
		default:
			rate = 0;
			error = EINVAL;
			break;
		}
		if (error)
			break;
		for (i = 0; i < phy_rates[1]; i++) {
			if (rate == AWI_80211_RATE(phy_rates[2 + i]))
				break;
		}
		if (i == phy_rates[1]) {
			error = EINVAL;
			break;
		}
		sc->sc_tx_rate = rate;
		break;
	case WI_RID_CUR_TX_RATE:
		error = EPERM;
		break;
	case WI_RID_RTS_THRESH:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		LE_WRITE_2(&sc->sc_mib_mac.aRTS_Threshold, wreq.wi_val[0]);
		error = ENETRESET;
		break;
	case WI_RID_CREATE_IBSS:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		sc->sc_start_bss = wreq.wi_val[0] ? 1 : 0;
		error = ENETRESET;
		break;
	case WI_RID_SYSTEM_SCALE:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		if (wreq.wi_val[0] != 1)
			error = EINVAL;		/* not supported */
		break;
	case WI_RID_PM_ENABLED:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		if (wreq.wi_val[0] != 0)
			error = EINVAL;		/* not implemented */
		break;
	case WI_RID_MAX_SLEEP:
		error = EINVAL;		/* not implemented */
		break;
	case WI_RID_WEP_AVAIL:
		error = EPERM;
		break;
	case WI_RID_ENCRYPTION:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		error = awi_wep_setalgo(sc, wreq.wi_val[0]);
		if (error)
			break;
		error = ENETRESET;
		break;
	case WI_RID_TX_CRYPT_KEY:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		if (wreq.wi_val[0] >= IEEE80211_WEP_NKID) {
			error = EINVAL;
			break;
		}
		sc->sc_wep_defkid = wreq.wi_val[1];
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		if (wreq.wi_len != sizeof(*keys) / 2) {
			error = EINVAL;
			break;
		}
		keys = (struct wi_ltv_keys *)&wreq;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			k = &keys->wi_keys[i];
			error = awi_wep_setkey(sc, i, k->wi_keydat,
			    k->wi_keylen);
			if (error)
				break;
		}
		break;
	case WI_RID_MAX_DATALEN:
		if (wreq.wi_len != 1) {
			error = EINVAL;
			break;
		}
		if (wreq.wi_val[0] < 350 || wreq.wi_val[0] > 2304) {
			error = EINVAL;
			break;
		}
		LE_WRITE_2(&sc->sc_mib_mac.aMax_Frame_Length, wreq.wi_val[0]);
		break;
	case WI_RID_IFACE_STATS:
		error = EPERM;
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == ENETRESET) {
		if (sc->sc_enabled) {
			awi_stop(sc);
			error = awi_init(sc);
		} else
			error = 0;
	}
	return error;
}
