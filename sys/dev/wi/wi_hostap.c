/*
 * Copyright (c) 2002
 *	Thomas Skibo <skibo@pacbell.net>.  All rights reserved.
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
 *	This product includes software developed by Thomas Skibo.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Thomas Skibo AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Thomas Skibo OR HIS DRINKING PALS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* This is experimental Host AP software for Prism 2 802.11b interfaces.
 *
 * Much of this is based upon the "Linux Host AP driver Host AP driver
 * for Intersil Prism2" by Jouni Malinen <jkm@ssh.com> or <jkmaline@cc.hut.fi>.
 */

#include <sys/param.h>
#include <sys/systm.h>
#if __FreeBSD_version >= 500033
#include <sys/endian.h>
#endif
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/bus_pio.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_ieee80211.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <dev/wi/if_wavelan_ieee.h>
#include <dev/wi/wi_hostap.h>
#include <dev/wi/if_wivar.h>
#include <dev/wi/if_wireg.h>

MALLOC_DEFINE(M_HAP_STA, "hostap_sta", "if_wi host AP mode station entry");

static void wihap_sta_timeout(void *v);
static struct wihap_sta_info *wihap_sta_alloc(struct wi_softc *sc,
    u_int8_t *addr);
static void wihap_sta_delete(struct wihap_sta_info *sta);
static struct wihap_sta_info *wihap_sta_find(struct wihap_info *whi,
    u_int8_t *addr);
static int wihap_sta_is_assoc(struct wihap_info *whi, u_int8_t addr[]);
static void wihap_auth_req(struct wi_softc *sc, struct wi_frame *rxfrm,
    caddr_t pkt, int len);
static void wihap_sta_deauth(struct wi_softc *sc, u_int8_t sta_addr[],
    u_int16_t reason);
static void wihap_deauth_req(struct wi_softc *sc, struct wi_frame *rxfrm,
    caddr_t pkt, int len);
static void wihap_assoc_req(struct wi_softc *sc, struct wi_frame *rxfrm,
    caddr_t pkt, int len);
static void wihap_sta_disassoc(struct wi_softc *sc, u_int8_t sta_addr[],
    u_int16_t reason);
static void wihap_disassoc_req(struct wi_softc *sc, struct wi_frame *rxfrm,
    caddr_t pkt, int len);

/*
 * Spl use in this driver.
 *
 * splnet is used everywhere here to block timeouts when we need to do
 * so.
 */

/*
 * take_hword()
 *
 *	Used for parsing management frames.  The pkt pointer and length
 *	variables are updated after the value is removed.
 */
static __inline u_int16_t
take_hword(caddr_t *ppkt, int *plen)
{
	u_int16_t s = le16toh(* (u_int16_t *) *ppkt);
	*ppkt += sizeof(u_int16_t);
	*plen -= sizeof(u_int16_t);
	return s;
}

/* take_tlv()
 *
 *	Parse out TLV element from a packet, check for underflow of packet
 *	or overflow of buffer, update pkt/len.
 */
static int
take_tlv(caddr_t *ppkt, int *plen, int id_expect, void *dst, int maxlen)
{
	u_int8_t id, len;

	if (*plen < 2)
		return -1;

	id = ((u_int8_t *)*ppkt)[0];
	len = ((u_int8_t *)*ppkt)[1];

	if (id != id_expect || *plen < len+2 || maxlen < len)
		return -1;

	bcopy(*ppkt + 2, dst, len);
	*plen -= 2 + len;
	*ppkt += 2 + len;

	return (len);
}

/* put_hword()
 *	Put half-word element into management frames.
 */
static __inline void
put_hword(caddr_t *ppkt, u_int16_t s)
{
	* (u_int16_t *) *ppkt = htole16(s);
	*ppkt += sizeof(u_int16_t);
}

/* put_tlv()
 *	Put TLV elements into management frames.
 */
static void
put_tlv(caddr_t *ppkt, u_int8_t id, void *src, u_int8_t len)
{
	(*ppkt)[0] = id;
	(*ppkt)[1] = len;
	bcopy(src, (*ppkt) + 2, len);
	*ppkt += 2 + len;
}

static int
put_rates(caddr_t *ppkt, u_int16_t rates)
{
	u_int8_t ratebuf[8];
	int len = 0;

	if (rates & WI_SUPPRATES_1M)
		ratebuf[len++] = 0x82;
	if (rates & WI_SUPPRATES_2M)
		ratebuf[len++] = 0x84;
	if (rates & WI_SUPPRATES_5M)
		ratebuf[len++] = 0x8b;
	if (rates & WI_SUPPRATES_11M)
		ratebuf[len++] = 0x96;

	put_tlv(ppkt, IEEE80211_ELEMID_RATES, ratebuf, len);
	return len;
}

/* wihap_init()
 *
 *	Initialize host AP data structures.  Called even if port type is
 *	not AP.
 */
void
wihap_init(struct wi_softc *sc)
{
	int i;
	struct wihap_info *whi = &sc->wi_hostap_info;

	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("wihap_init: sc=0x%x whi=0x%x\n", (int)sc, (int)whi);

	bzero(whi, sizeof(struct wihap_info));

	if (sc->wi_ptype != WI_PORTTYPE_HOSTAP)
		return;

	whi->apflags = WIHAPFL_ACTIVE;

	LIST_INIT(&whi->sta_list);
	for (i = 0; i < WI_STA_HASH_SIZE; i++)
		LIST_INIT(&whi->sta_hash[i]);

	whi->inactivity_time = WIHAP_DFLT_INACTIVITY_TIME;
}

/* wihap_sta_disassoc()
 *
 *	Send a disassociation frame to a specified station.
 */
static void
wihap_sta_disassoc(struct wi_softc *sc, u_int8_t sta_addr[], u_int16_t reason)
{
	struct wi_80211_hdr	*resp_hdr;
	caddr_t			pkt;

	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
 		printf("Sending disassoc to sta %6D\n", sta_addr, ":");

	/* Send disassoc packet. */
	resp_hdr = (struct wi_80211_hdr *) sc->wi_txbuf;
	bzero(resp_hdr, sizeof(struct wi_80211_hdr));
	resp_hdr->frame_ctl = WI_FTYPE_MGMT | WI_STYPE_MGMT_DISAS;
	pkt = sc->wi_txbuf + sizeof(struct wi_80211_hdr);

	bcopy(sta_addr, resp_hdr->addr1, ETHER_ADDR_LEN);
	bcopy(sc->arpcom.ac_enaddr, resp_hdr->addr2, ETHER_ADDR_LEN);
	bcopy(sc->arpcom.ac_enaddr, resp_hdr->addr3, ETHER_ADDR_LEN);

	put_hword(&pkt, reason);

	wi_mgmt_xmit(sc, sc->wi_txbuf, 2 + sizeof(struct wi_80211_hdr));
}

/* wihap_sta_deauth()
 *
 *	Send a deauthentication message to a specified station.
 */
static void
wihap_sta_deauth(struct wi_softc *sc, u_int8_t sta_addr[], u_int16_t reason)
{
	struct wi_80211_hdr	*resp_hdr;
	caddr_t			pkt;

	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("Sending deauth to sta %6D\n", sta_addr, ":");

	/* Send deauth packet. */
	resp_hdr = (struct wi_80211_hdr *) sc->wi_txbuf;
	bzero(resp_hdr, sizeof(struct wi_80211_hdr));
	resp_hdr->frame_ctl = htole16(WI_FTYPE_MGMT | WI_STYPE_MGMT_DEAUTH);
	pkt = sc->wi_txbuf + sizeof(struct wi_80211_hdr);

	bcopy(sta_addr, resp_hdr->addr1, ETHER_ADDR_LEN);
	bcopy(sc->arpcom.ac_enaddr, resp_hdr->addr2, ETHER_ADDR_LEN);
	bcopy(sc->arpcom.ac_enaddr, resp_hdr->addr3, ETHER_ADDR_LEN);

	put_hword(&pkt, reason);

	wi_mgmt_xmit(sc, sc->wi_txbuf, 2 + sizeof(struct wi_80211_hdr));
}

/* wihap_shutdown()
 *
 *	Disassociate all stations and free up data structures.
 */
void
wihap_shutdown(struct wi_softc *sc)
{
	struct wihap_info	*whi = &sc->wi_hostap_info;
	struct wihap_sta_info	*sta, *next;
	int s;

	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("wihap_shutdown: sc=0x%x whi=0x%x\n",
		    (int)sc, (int)whi);

	if (!(whi->apflags & WIHAPFL_ACTIVE))
		return;

	/* XXX: I read somewhere you can deauth all the stations with
	 * a single broadcast.  Maybe try that someday.
	 */

	s = splnet();
	sta = LIST_FIRST(&whi->sta_list);
	while (sta) {
		untimeout(wihap_sta_timeout, sta, sta->tmo);
		if (!sc->wi_gone) {
			/* Disassociate station. */
			if (sta->flags & WI_SIFLAGS_ASSOC)
				wihap_sta_disassoc(sc, sta->addr,
				    IEEE80211_REASON_ASSOC_LEAVE);
			/* Deauth station. */
			if (sta->flags & WI_SIFLAGS_AUTHEN)
				wihap_sta_deauth(sc, sta->addr,
				    IEEE80211_REASON_AUTH_LEAVE);
		}

		/* Delete the structure. */
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_shutdown: FREE(sta=0x%x)\n", (int)sta);
		next = LIST_NEXT(sta, list);
		FREE(sta, M_HAP_STA);
		sta = next;
	}

	whi->apflags = 0;
	splx(s);
}

/* sta_hash_func()
 * Hash function for finding stations from ethernet address.
 */
static __inline int
sta_hash_func(u_int8_t addr[])
{
	return ((addr[3] + addr[4] + addr[5]) % WI_STA_HASH_SIZE);
}

/* addr_cmp():  Maybe this is a faster way to compare addresses? */
static __inline int
addr_cmp(u_int8_t a[], u_int8_t b[])
{
	return (*(u_int16_t *)(a + 4) == *(u_int16_t *)(b + 4) &&
		*(u_int32_t *)(a    ) == *(u_int32_t *)(b));
}

static void
wihap_sta_timeout(void *v)
{
	struct wihap_sta_info	*sta = v;
	struct wi_softc		*sc = sta->sc;
	struct wihap_info	*whi = &sc->wi_hostap_info;
	int	s;

	s = splnet();
	if (sta->flags & WI_SIFLAGS_ASSOC) {
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			device_printf(sc->dev, "inactivity disassoc: %6D\n",
			    sta->addr, ":");

		/* Disassoc station. */
		wihap_sta_disassoc(sc, sta->addr,
		    IEEE80211_REASON_ASSOC_EXPIRE);
		sta->flags &= ~WI_SIFLAGS_ASSOC;

		sta->tmo = timeout(wihap_sta_timeout, sta,
		    hz * whi->inactivity_time);

	} else if (sta->flags & WI_SIFLAGS_AUTHEN) {

		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			device_printf(sc->dev, "inactivity disassoc: %6D\n",
			    sta->addr, ":");

		/* Deauthenticate station. */
		wihap_sta_deauth(sc, sta->addr, IEEE80211_REASON_AUTH_EXPIRE);
		sta->flags &= ~WI_SIFLAGS_AUTHEN;

		/* Delete the station if it's not permanent. */
		if (!(sta->flags & WI_SIFLAGS_PERM))
			wihap_sta_delete(sta);
	}
	splx(s);
}

/* wihap_sta_delete()
 * Delete a single station and free up its data structure.
 */
static void
wihap_sta_delete(struct wihap_sta_info *sta)
{
	struct wi_softc		*sc = sta->sc;
	struct wihap_info	*whi = &sc->wi_hostap_info;
	int i = sta->asid - 0xc001;

	untimeout(wihap_sta_timeout, sta, sta->tmo);

	whi->asid_inuse_mask[i >> 4] &= ~(1UL << (i & 0xf));

	LIST_REMOVE(sta, list);
	LIST_REMOVE(sta, hash);
	if (sta->challenge)
		FREE(sta->challenge, M_TEMP);
	FREE(sta, M_HAP_STA);
	whi->n_stations--;
}

/* wihap_sta_alloc()
 *
 *	Create a new station data structure and put it in the list
 *	and hash table.
 */
static struct wihap_sta_info *
wihap_sta_alloc(struct wi_softc *sc, u_int8_t *addr)
{
	struct wihap_info	*whi = &sc->wi_hostap_info;
	struct wihap_sta_info	*sta;
	int i, hash = sta_hash_func(addr);

	/* Allocate structure. */
	MALLOC(sta, struct wihap_sta_info *, sizeof(struct wihap_sta_info),
	    M_HAP_STA, M_NOWAIT);
	if (sta == NULL)
		return(NULL);

	bzero(sta, sizeof(struct wihap_sta_info));

	/* Allocate an ASID. */
	i=hash<<4;
	while (whi->asid_inuse_mask[i >> 4] & (1UL << (i & 0xf)))
		i = (i == (WI_STA_HASH_SIZE << 4) - 1) ? 0 : (i + 1);
	whi->asid_inuse_mask[i >> 4] |= (1UL << (i & 0xf));
	sta->asid = 0xc001 + i;

	/* Insert in list and hash list. */
	LIST_INSERT_HEAD(&whi->sta_list, sta, list);
	LIST_INSERT_HEAD(&whi->sta_hash[hash], sta, hash);

	sta->sc = sc;
	whi->n_stations++;
	bcopy(addr, &sta->addr, ETHER_ADDR_LEN);

	return(sta);
}

/* wihap_sta_find()
 *
 *	Find station structure given address.
 */
static struct wihap_sta_info *
wihap_sta_find(struct wihap_info *whi, u_int8_t *addr)
{
	int i;
	struct wihap_sta_info *sta;

	i = sta_hash_func(addr);
	LIST_FOREACH(sta, &whi->sta_hash[i], hash)
		if (addr_cmp(addr,sta->addr))
			return sta;

	return (NULL);
}

static int
wihap_check_rates(struct wihap_sta_info *sta, u_int8_t rates[], int rates_len)
{
	struct wi_softc *sc = sta->sc;
	int	i;

	sta->rates = 0;
	sta->tx_max_rate = 0;
	for (i=0; i<rates_len; i++)
		switch (rates[i] & 0x7f) {
		case 0x02:
			sta->rates |= WI_SUPPRATES_1M;
			break;
		case 0x04:
			sta->rates |= WI_SUPPRATES_2M;
			if (sta->tx_max_rate<1)
				sta->tx_max_rate = 1;
			break;
		case 0x0b:
			sta->rates |= WI_SUPPRATES_5M;
			if (sta->tx_max_rate<2)
				sta->tx_max_rate = 2;
			break;
		case 0x16:
			sta->rates |= WI_SUPPRATES_11M;
			sta->tx_max_rate = 3;
			break;
		}

	sta->rates &= sc->wi_supprates;
	sta->tx_curr_rate = sta->tx_max_rate;

	return (sta->rates == 0 ? -1 : 0);
}


/* wihap_auth_req()
 *
 *	Handle incoming authentication request.  Only handle OPEN
 *	requests.
 */
static void
wihap_auth_req(struct wi_softc *sc, struct wi_frame *rxfrm,
    caddr_t pkt, int len)
{
	struct wihap_info	*whi = &sc->wi_hostap_info;
	struct wihap_sta_info	*sta;

	u_int16_t		algo;
	u_int16_t		seq;
	u_int16_t		status;
	int			i, challenge_len;
	u_int32_t		challenge[32];

	struct wi_80211_hdr	*resp_hdr;

	if (len < 6)
		return;

	/* Break open packet. */
	algo = take_hword(&pkt, &len);
	seq = take_hword(&pkt, &len);
	status = take_hword(&pkt, &len);
	challenge_len = 0;
	if (len > 0 && (challenge_len = take_tlv(&pkt, &len,
	    IEEE80211_ELEMID_CHALLENGE, challenge, sizeof(challenge))) < 0)
		return;

	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("wihap_auth_req: station %6D algo=0x%x seq=0x%x\n",
		       rxfrm->wi_addr2, ":", algo, seq);

	/* Find or create station info. */
	sta = wihap_sta_find(whi, rxfrm->wi_addr2);
	if (sta == NULL) {

		/* Are we allowing new stations?
		 */
		if (whi->apflags & WIHAPFL_MAC_FILT) {
			status = IEEE80211_STATUS_OTHER; /* XXX */
			goto fail;
		}

		/* Check for too many stations.
		 */
		if (whi->n_stations >= WIHAP_MAX_STATIONS) {
			status = IEEE80211_STATUS_TOO_MANY_STATIONS;
			goto fail;
		}

		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_auth_req: new station\n");

		/* Create new station. */
		sta = wihap_sta_alloc(sc, rxfrm->wi_addr2);
		if (sta == NULL) {
			/* Out of memory! */
			status = IEEE80211_STATUS_TOO_MANY_STATIONS;
			goto fail;
		}
	}

	/* Note: it's okay to leave the station info structure around
	 * if the authen fails.  It'll be timed out eventually.
	 */
	switch (algo) {
	case IEEE80211_AUTH_ALG_OPEN:
		if (sc->wi_authmode != IEEE80211_AUTH_OPEN) {
			seq = 2;
			status = IEEE80211_STATUS_ALG;
			goto fail;
		}
		if (seq != 1) {
			seq = 2;
			status = IEEE80211_STATUS_SEQUENCE;
			goto fail;
		}
		challenge_len = 0;
		seq = 2;
		sta->flags |= WI_SIFLAGS_AUTHEN;
		break;
	case IEEE80211_AUTH_ALG_SHARED:
		if (sc->wi_authmode != IEEE80211_AUTH_SHARED) {
			seq = 2;
			status = IEEE80211_STATUS_ALG;
			goto fail;
		}
		switch (seq) {
		case 1:
			/* Create a challenge frame. */
			if (!sta->challenge) {
				MALLOC(sta->challenge, u_int32_t *, 128,
				       M_TEMP, M_NOWAIT);
				if (!sta->challenge)
					return;
			}
			for (i = 0; i < 32; i++)
				challenge[i] = sta->challenge[i] =
					arc4random();
			challenge_len = 128;
			seq = 2;
			break;
		case 3:
			if (challenge_len != 128 || !sta->challenge ||
			    !(le16toh(rxfrm->wi_frame_ctl) & WI_FCTL_WEP)) {
				status = IEEE80211_STATUS_CHALLENGE;
				goto fail;
			}
			challenge_len = 0;
			seq = 4;

			/* Check the challenge text.  (Was decrypted by
			 * the adapter.)
			 */
			for (i=0; i<32; i++)
				if (sta->challenge[i] != challenge[i]) {
					status = IEEE80211_STATUS_CHALLENGE;
					FREE(sta->challenge, M_TEMP);
					sta->challenge = NULL;
					goto fail;
				}

			sta->flags |= WI_SIFLAGS_AUTHEN;
			FREE(sta->challenge, M_TEMP);
			sta->challenge = NULL;
			break;
		default:
			seq = 2;
			status = IEEE80211_STATUS_SEQUENCE;
			goto fail;
		} /* switch (seq) */
		break;
	default:
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_auth_req: algorithm unsupported: 0x%x\n",
			       algo);
		status = IEEE80211_STATUS_ALG;
		goto fail;
	} /* switch (algo) */

	status = IEEE80211_STATUS_SUCCESS;

fail:
	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("wihap_auth_req: returns status=0x%x\n", status);

	/* Send response. */
	resp_hdr = (struct wi_80211_hdr *) sc->wi_txbuf;
	bzero(resp_hdr, sizeof(struct wi_80211_hdr));
	resp_hdr->frame_ctl = htole16(WI_FTYPE_MGMT | WI_STYPE_MGMT_AUTH);
	bcopy(rxfrm->wi_addr2, resp_hdr->addr1, ETHER_ADDR_LEN);
	bcopy(sc->arpcom.ac_enaddr, resp_hdr->addr2, ETHER_ADDR_LEN);
	bcopy(sc->arpcom.ac_enaddr, resp_hdr->addr3, ETHER_ADDR_LEN);
	pkt = &sc->wi_txbuf[sizeof(struct wi_80211_hdr)];
	put_hword(&pkt, algo);
	put_hword(&pkt, seq);
	put_hword(&pkt, status);
	if (challenge_len > 0)
		put_tlv(&pkt, IEEE80211_ELEMID_CHALLENGE,
		    challenge, challenge_len);
	wi_mgmt_xmit(sc, sc->wi_txbuf, (char *) pkt - (char *) sc->wi_txbuf);
}

/* wihap_assoc_req()
 *
 *	Handle incoming association and reassociation requests.
 */
static void
wihap_assoc_req(struct wi_softc *sc, struct wi_frame *rxfrm,
		caddr_t pkt, int len)
{
	struct wihap_info	*whi = &sc->wi_hostap_info;
	struct wihap_sta_info	*sta;
	struct wi_80211_hdr	*resp_hdr;
	u_int16_t		capinfo;
	u_int16_t		lstintvl;
	u_int8_t		rates[8];
	int			ssid_len, rates_len;
	char			ssid[33];
	u_int16_t		status;
	u_int16_t		asid = 0;

	if (len < 8)
		return;

	/* Pull out request parameters. */
	capinfo = take_hword(&pkt, &len);
	lstintvl = take_hword(&pkt, &len);

	if ((rxfrm->wi_frame_ctl & htole16(WI_FCTL_STYPE)) ==
	    htole16(WI_STYPE_MGMT_REASREQ)) {
		if (len < 6)
			return;
		/* Eat the MAC address of the current AP */
		take_hword(&pkt, &len);
		take_hword(&pkt, &len);
		take_hword(&pkt, &len);
	}

	if ((ssid_len = take_tlv(&pkt, &len, IEEE80211_ELEMID_SSID,
	    ssid, sizeof(ssid) - 1))<0)
		return;
	ssid[ssid_len] = '\0';
	if ((rates_len = take_tlv(&pkt, &len, IEEE80211_ELEMID_RATES,
	    rates, sizeof(rates)))<0)
		return;

	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("wihap_assoc_req: from station %6D\n",
		    rxfrm->wi_addr2, ":");

	/* If SSID doesn't match, simply drop. */
	if (strcmp(sc->wi_net_name, ssid) != 0) {
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_assoc_req: bad ssid: '%s' != '%s'\n",
			    ssid, sc->wi_net_name);
		return;
	}

	/* Is this station authenticated yet? */
	sta = wihap_sta_find(whi, rxfrm->wi_addr2);
	if (sta == NULL || !(sta->flags & WI_SIFLAGS_AUTHEN)) {
		wihap_sta_deauth(sc, rxfrm->wi_addr2,
		    IEEE80211_REASON_NOT_AUTHED);
		return;
	}

	/* Check supported rates against ours. */
	if (wihap_check_rates(sta, rates, rates_len) < 0) {
		status = IEEE80211_STATUS_RATES;
		goto fail;
	}

	/* Check capinfo.
	 * Check for ESS, not IBSS.
	 * Check WEP/PRIVACY flags match.
	 * Refuse stations requesting to be put on CF-polling list.
	 */
	sta->capinfo = capinfo;
	status = IEEE80211_STATUS_CAPINFO;
	if ((capinfo & (IEEE80211_CAPINFO_ESS | IEEE80211_CAPINFO_IBSS)) !=
	    IEEE80211_CAPINFO_ESS) {
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_assoc_req: capinfo mismatch: "
			    "client using IBSS mode\n");
		goto fail;

	}
	if ((sc->wi_use_wep && !(capinfo & IEEE80211_CAPINFO_PRIVACY)) ||
	    (!sc->wi_use_wep && (capinfo & IEEE80211_CAPINFO_PRIVACY))) {
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_assoc_req: capinfo mismatch: client "
			    "%susing WEP\n", sc->wi_use_wep ? "not " : "");
		goto fail;
	}
	if ((capinfo & (IEEE80211_CAPINFO_CF_POLLABLE |
	    IEEE80211_CAPINFO_CF_POLLREQ)) == IEEE80211_CAPINFO_CF_POLLABLE) {
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_assoc_req: capinfo mismatch: "
			    "client requested CF polling\n");
		goto fail;
	}

	/* Use ASID is allocated by whi_sta_alloc(). */
	asid = sta->asid;

	if (sta->flags & WI_SIFLAGS_ASSOC) {
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_assoc_req: already assoc'ed?\n");
	}

	sta->flags |= WI_SIFLAGS_ASSOC;
	sta->inactivity_timer = whi->inactivity_time;
	status = IEEE80211_STATUS_SUCCESS;

fail:
	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("wihap_assoc_req: returns status=0x%x\n", status);

	/* Send response. */
	resp_hdr = (struct wi_80211_hdr *) sc->wi_txbuf;
	bzero(resp_hdr, sizeof(struct wi_80211_hdr));
	resp_hdr->frame_ctl = htole16(WI_FTYPE_MGMT | WI_STYPE_MGMT_ASRESP);
	pkt = sc->wi_txbuf + sizeof(struct wi_80211_hdr);

	bcopy(rxfrm->wi_addr2, resp_hdr->addr1, ETHER_ADDR_LEN);
	bcopy(sc->arpcom.ac_enaddr, resp_hdr->addr2, ETHER_ADDR_LEN);
	bcopy(sc->arpcom.ac_enaddr, resp_hdr->addr3, ETHER_ADDR_LEN);

	put_hword(&pkt, capinfo);
	put_hword(&pkt, status);
	put_hword(&pkt, asid);
	rates_len = put_rates(&pkt, sc->wi_supprates);

	wi_mgmt_xmit(sc, sc->wi_txbuf,
	    8 + rates_len + sizeof(struct wi_80211_hdr));
}

/* wihap_deauth_req()
 *
 *	Handle deauthentication requests.  Delete the station.
 */
static void
wihap_deauth_req(struct wi_softc *sc, struct wi_frame *rxfrm,
		 caddr_t pkt, int len)
{
	struct wihap_info	*whi = &sc->wi_hostap_info;
	struct wihap_sta_info	*sta;
	u_int16_t		reason;

	if (len<2)
		return;

	reason = take_hword(&pkt, &len);

	sta = wihap_sta_find(whi, rxfrm->wi_addr2);
	if (sta == NULL) {
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_deauth_req: unknown station: %6D\n",
			    rxfrm->wi_addr2, ":");
	}
	else
		wihap_sta_delete(sta);
}

/* wihap_disassoc_req()
 *
 *	Handle disassociation requests.  Just reset the assoc flag.
 *	We'll free up the station resources when we get a deauth
 *	request or when it times out.
 */
static void
wihap_disassoc_req(struct wi_softc *sc, struct wi_frame *rxfrm,
    caddr_t pkt, int len)
{
	struct wihap_info	*whi = &sc->wi_hostap_info;
	struct wihap_sta_info	*sta;
	u_int16_t		reason;

	if (len < 2)
		return;

	reason = take_hword(&pkt, &len);

	sta = wihap_sta_find(whi, rxfrm->wi_addr2);
	if (sta == NULL) {
		if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
			printf("wihap_disassoc_req: unknown station: %6D\n",
			    rxfrm->wi_addr2, ":");
	}
	else if (!(sta->flags & WI_SIFLAGS_AUTHEN)) {
		/*
		 * If station is not authenticated, send deauthentication
		 * frame.
		 */
		wihap_sta_deauth(sc, rxfrm->wi_addr2,
		    IEEE80211_REASON_NOT_AUTHED);
		return;
	}
	else
		sta->flags &= ~WI_SIFLAGS_ASSOC;
}

/* wihap_debug_frame_type()
 *
 * Print out frame type.  Used in early debugging.
 */
static __inline void
wihap_debug_frame_type(struct wi_frame *rxfrm)
{
	printf("wihap_mgmt_input: len=%d ", le16toh(rxfrm->wi_dat_len));

	if ((rxfrm->wi_frame_ctl & htole16(WI_FCTL_FTYPE)) ==
	    htole16(WI_FTYPE_MGMT)) {

		printf("MGMT: ");

		switch (le16toh(rxfrm->wi_frame_ctl) & WI_FCTL_STYPE) {
		case WI_STYPE_MGMT_ASREQ:
			printf("assoc req: \n");
			break;
		case WI_STYPE_MGMT_ASRESP:
			printf("assoc resp: \n");
			break;
		case WI_STYPE_MGMT_REASREQ:
			printf("reassoc req: \n");
			break;
		case WI_STYPE_MGMT_REASRESP:
			printf("reassoc resp: \n");
			break;
		case WI_STYPE_MGMT_PROBEREQ:
			printf("probe req: \n");
			break;
		case WI_STYPE_MGMT_PROBERESP:
			printf("probe resp: \n");
			break;
		case WI_STYPE_MGMT_BEACON:
			printf("beacon: \n");
			break;
		case WI_STYPE_MGMT_ATIM:
			printf("ann traf ind \n");
			break;
		case WI_STYPE_MGMT_DISAS:
			printf("disassociation: \n");
			break;
		case WI_STYPE_MGMT_AUTH:
			printf("auth: \n");
			break;
		case WI_STYPE_MGMT_DEAUTH:
			printf("deauth: \n");
			break;
		default:
			printf("unknown (stype=0x%x)\n",
			    le16toh(rxfrm->wi_frame_ctl) & WI_FCTL_STYPE);
		}

	}
	else {
		printf("ftype=0x%x (ctl=0x%x)\n",
		    le16toh(rxfrm->wi_frame_ctl) & WI_FCTL_FTYPE,
		    le16toh(rxfrm->wi_frame_ctl));
	}
}

/* wihap_mgmt_input:
 *
 *	Called for each management frame received in host ap mode.
 *	wihap_mgmt_input() is expected to free the mbuf.
 */
void
wihap_mgmt_input(struct wi_softc *sc, struct wi_frame *rxfrm, struct mbuf *m)
{
	caddr_t	pkt;
	int	s, len;

	if (sc->arpcom.ac_if.if_flags & IFF_DEBUG)
		wihap_debug_frame_type(rxfrm);

	pkt = mtod(m, caddr_t) + WI_802_11_OFFSET_RAW;
	len = m->m_len - WI_802_11_OFFSET_RAW;

	if ((rxfrm->wi_frame_ctl & htole16(WI_FCTL_FTYPE)) ==
	    htole16(WI_FTYPE_MGMT)) {

		/* any of the following will mess w/ the station list */
		s = splnet();
		switch (le16toh(rxfrm->wi_frame_ctl) & WI_FCTL_STYPE) {
		case WI_STYPE_MGMT_ASREQ:
			wihap_assoc_req(sc, rxfrm, pkt, len);
			break;
		case WI_STYPE_MGMT_ASRESP:
			break;
		case WI_STYPE_MGMT_REASREQ:
			wihap_assoc_req(sc, rxfrm, pkt, len);
			break;
		case WI_STYPE_MGMT_REASRESP:
			break;
		case WI_STYPE_MGMT_PROBEREQ:
			break;
		case WI_STYPE_MGMT_PROBERESP:
			break;
		case WI_STYPE_MGMT_BEACON:
			break;
		case WI_STYPE_MGMT_ATIM:
			break;
		case WI_STYPE_MGMT_DISAS:
			wihap_disassoc_req(sc, rxfrm, pkt, len);
			break;
		case WI_STYPE_MGMT_AUTH:
			wihap_auth_req(sc, rxfrm, pkt, len);
			break;
		case WI_STYPE_MGMT_DEAUTH:
			wihap_deauth_req(sc, rxfrm, pkt, len);
			break;
		}
		splx(s);
	}

	m_freem(m);
}

/* wihap_sta_is_assoc()
 *
 *	Determine if a station is assoc'ed.  Update its activity
 *	counter as a side-effect.
 */
static int
wihap_sta_is_assoc(struct wihap_info *whi, u_int8_t addr[])
{
	struct wihap_sta_info *sta;
	int retval, s;

	s = splnet();
	retval = 0;
	sta = wihap_sta_find(whi, addr);
	if (sta != NULL && (sta->flags & WI_SIFLAGS_ASSOC)) {
		/* Keep it active. */
		untimeout(wihap_sta_timeout, sta, sta->tmo);
		sta->tmo = timeout(wihap_sta_timeout, sta,
		    hz * whi->inactivity_time);
		retval = 1;
	}
	splx(s);
	return (retval);
}

/* wihap_check_tx()
 *
 *	Determine if a station is assoc'ed, get its tx rate, and update
 *	its activity.
 */
int
wihap_check_tx(struct wihap_info *whi, u_int8_t addr[], u_int8_t *txrate)
{
	struct wihap_sta_info *sta;
	static u_int8_t txratetable[] = { 10, 20, 55, 110 };
	int s;

	if (addr[0] & 0x01) {
		*txrate = 0; /* XXX: multicast rate? */
		return(1);
	}
	s = splnet();
	sta = wihap_sta_find(whi, addr);
	if (sta != NULL && (sta->flags & WI_SIFLAGS_ASSOC)) {
		/* Keep it active. */
		untimeout(wihap_sta_timeout, sta, sta->tmo);
		sta->tmo = timeout(wihap_sta_timeout, sta,
		    hz * whi->inactivity_time);
		*txrate = txratetable[ sta->tx_curr_rate ];
		splx(s);
		return(1);
	}
	splx(s);

	return(0);
}

/*
 * wihap_data_input()
 *
 *	Handle all data input on interface when in Host AP mode.
 *	Some packets are destined for this machine, others are
 *	repeated to other stations.
 *
 *	If wihap_data_input() returns a non-zero, it has processed
 *	the packet and will free the mbuf.
 */
int
wihap_data_input(struct wi_softc *sc, struct wi_frame *rxfrm, struct mbuf *m)
{
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct wihap_info	*whi = &sc->wi_hostap_info;
	struct wihap_sta_info	*sta;
	int			mcast, s;

	/* TODS flag must be set. */
	if (!(rxfrm->wi_frame_ctl & htole16(WI_FCTL_TODS))) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("wihap_data_input: no TODS src=%6D\n",
			    rxfrm->wi_addr2, ":");
		m_freem(m);
		return(1);
	}

	/* Check BSSID. (Is this necessary?) */
	if (!addr_cmp(rxfrm->wi_addr1, sc->arpcom.ac_enaddr)) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("wihap_data_input: incorrect bss: %6D\n",
				rxfrm->wi_addr1, ":");
		m_freem(m);
		return (1);
	}

	s = splnet();

	/* Find source station. */
	sta = wihap_sta_find(whi, rxfrm->wi_addr2);

	/* Source station must be associated. */
	if (sta == NULL || !(sta->flags & WI_SIFLAGS_ASSOC)) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("wihap_data_input: dropping unassoc src %6D\n",
			    rxfrm->wi_addr2, ":");
 		wihap_sta_disassoc(sc, rxfrm->wi_addr2,
 		    IEEE80211_REASON_ASSOC_LEAVE);
		splx(s);
		m_freem(m);
		return(1);
	}

	untimeout(wihap_sta_timeout, sta, sta->tmo);
	sta->tmo = timeout(wihap_sta_timeout, sta,
	    hz * whi->inactivity_time);
	sta->sig_info = le16toh(rxfrm->wi_q_info);

	splx(s);

	/* Repeat this packet to BSS? */
	mcast = (rxfrm->wi_addr3[0] & 0x01) != 0;
	if (mcast || wihap_sta_is_assoc(whi, rxfrm->wi_addr3)) {

		/* If it's multicast, make a copy.
		 */
		if (mcast) {
			m = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
			if (m == NULL)
				return(0);
			m->m_flags |= M_MCAST; /* XXX */
		}

		/* Queue up for repeating.
		 */
		IF_HANDOFF(&ifp->if_snd, m, ifp);
		return (!mcast);
	}

	return(0);
}

/* wihap_ioctl()
 *
 *	Handle Host AP specific ioctls.  Called from wi_ioctl().
 */
int
wihap_ioctl(struct wi_softc *sc, u_long command, caddr_t data)
{
	struct ifreq		*ifr = (struct ifreq *) data;
	struct wihap_info	*whi = &sc->wi_hostap_info;
	struct wihap_sta_info	*sta;
	struct hostap_getall	reqall;
	struct hostap_sta	reqsta;
	struct hostap_sta	stabuf;
	int			s, error = 0, n, flag;
#if __FreeBSD_version >= 500000
	struct thread		*td = curthread;
#else
	struct proc		*td = curproc;		/* Little white lie */
#endif

	if (!(sc->arpcom.ac_if.if_flags & IFF_RUNNING))
		return ENODEV;

	switch (command) {
	case SIOCHOSTAP_DEL:
		if ((error = suser(td)))
			break;
		if ((error = copyin(ifr->ifr_data, &reqsta, sizeof(reqsta))))
			break;
		s = splnet();
		sta = wihap_sta_find(whi, reqsta.addr);
		if (sta == NULL)
			error = ENOENT;
		else {
			/* Disassociate station. */
			if (sta->flags & WI_SIFLAGS_ASSOC)
				wihap_sta_disassoc(sc, sta->addr,
				    IEEE80211_REASON_ASSOC_LEAVE);
			/* Deauth station. */
			if (sta->flags & WI_SIFLAGS_AUTHEN)
				wihap_sta_deauth(sc, sta->addr,
				    IEEE80211_REASON_AUTH_LEAVE);

			wihap_sta_delete(sta);
		}
		splx(s);
		break;

	case SIOCHOSTAP_GET:
		if ((error = copyin(ifr->ifr_data, &reqsta, sizeof(reqsta))))
			break;
		s = splnet();
		sta = wihap_sta_find(whi, reqsta.addr);
		if (sta == NULL) {
			error = ENOENT;
			splx(s);
		} else {
			reqsta.flags = sta->flags;
			reqsta.asid = sta->asid;
			reqsta.capinfo = sta->capinfo;
			reqsta.sig_info = sta->sig_info;
			reqsta.rates = sta->rates;
			splx(s);
			error = copyout(&reqsta, ifr->ifr_data,
			    sizeof(reqsta));
		}
		break;

	case SIOCHOSTAP_ADD:
		if ((error = suser(td)))
			break;
		if ((error = copyin(ifr->ifr_data, &reqsta, sizeof(reqsta))))
			break;
		s = splnet();
		sta = wihap_sta_find(whi, reqsta.addr);
		if (sta != NULL) {
			error = EEXIST;
			splx(s);
			break;
		}
		if (whi->n_stations >= WIHAP_MAX_STATIONS) {
			error = ENOSPC;
			splx(s);
			break;
		}
		sta = wihap_sta_alloc(sc, reqsta.addr);
		sta->flags = reqsta.flags;
		sta->tmo = timeout(wihap_sta_timeout, sta,
		    hz * whi->inactivity_time);
		splx(s);
		break;

	case SIOCHOSTAP_SFLAGS:
		if ((error = suser(td)))
			break;
		if ((error = copyin(ifr->ifr_data, &flag, sizeof(int))))
			break;

		whi->apflags = (whi->apflags & WIHAPFL_CANTCHANGE) |
		    (flag & ~WIHAPFL_CANTCHANGE);
		break;

	case SIOCHOSTAP_GFLAGS:
		flag = (int) whi->apflags;
		error = copyout(&flag, ifr->ifr_data, sizeof(int));
		break;

	case SIOCHOSTAP_GETALL:
		if ((error = copyin(ifr->ifr_data, &reqall, sizeof(reqall))))
			break;

		reqall.nstations = whi->n_stations;
		n = 0;
		s = splnet();
		sta = LIST_FIRST(&whi->sta_list);
		while (sta && reqall.size >= n+sizeof(struct hostap_sta)) {

			bcopy(sta->addr, stabuf.addr, ETHER_ADDR_LEN);
			stabuf.asid = sta->asid;
			stabuf.flags = sta->flags;
			stabuf.capinfo = sta->capinfo;
			stabuf.sig_info = sta->sig_info;
			stabuf.rates = sta->rates;

			error = copyout(&stabuf, (caddr_t) reqall.addr + n,
			    sizeof(struct hostap_sta));
			if (error)
				break;

			sta = LIST_NEXT(sta, list);
			n += sizeof(struct hostap_sta);
		}
		splx(s);

		if (!error)
			error = copyout(&reqall, ifr->ifr_data,
			    sizeof(reqall));
		break;
	default:
		printf("wihap_ioctl: i shouldn't get other ioctls!\n");
		error = EINVAL;
	}

	return(error);
}
