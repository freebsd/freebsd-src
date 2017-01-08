/*-
 * Copyright (c) 2017 Adrian Chadd <adrian@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD$");
#endif

/*
 * IEEE 802.11ac-2013 protocol support.
 */

#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h> 
#include <sys/endian.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_action.h>
#include <net80211/ieee80211_input.h>
#include <net80211/ieee80211_vht.h>

/* define here, used throughout file */
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)
#define	SM(_v, _f)	(((_v) << _f##_S) & _f)

#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDWORD(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = ((v) >> 8) & 0xff;		\
	frm[2] = ((v) >> 16) & 0xff;		\
	frm[3] = ((v) >> 24) & 0xff;		\
	frm += 4;				\
} while (0)

/*
 * XXX TODO: handle WLAN_ACTION_VHT_OPMODE_NOTIF
 *
 * Look at mac80211/vht.c:ieee80211_vht_handle_opmode() for further details.
 */

static void
ieee80211_vht_init(void)
{
}

SYSINIT(wlan_vht, SI_SUB_DRIVERS, SI_ORDER_FIRST, ieee80211_vht_init, NULL);

void
ieee80211_vht_attach(struct ieee80211com *ic)
{
}

void
ieee80211_vht_detach(struct ieee80211com *ic)
{
}

void
ieee80211_vht_vattach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (! IEEE80211_CONF_VHT(ic))
		return;

	vap->iv_vhtcaps = ic->ic_vhtcaps;
	vap->iv_vhtextcaps = ic->ic_vhtextcaps;

	/* XXX assume VHT80 support; should really check vhtcaps */
	vap->iv_flags_vht =
	    IEEE80211_FVHT_VHT
	    | IEEE80211_FVHT_USEVHT40
	    | IEEE80211_FVHT_USEVHT80;
	/* XXX TODO: enable VHT80+80, VHT160 capabilities */

	memcpy(&vap->iv_vht_mcsinfo, &ic->ic_vht_mcsinfo,
	    sizeof(struct ieee80211_vht_mcs_info));
}

void
ieee80211_vht_vdetach(struct ieee80211vap *vap)
{
}

#if 0
static void
vht_announce(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
}
#endif

static int
vht_mcs_to_num(int m)
{

	switch (m) {
	case IEEE80211_VHT_MCS_SUPPORT_0_7:
		return (7);
	case IEEE80211_VHT_MCS_SUPPORT_0_8:
		return (8);
	case IEEE80211_VHT_MCS_SUPPORT_0_9:
		return (9);
	default:
		return (0);
	}
}

void
ieee80211_vht_announce(struct ieee80211com *ic)
{
	int i, tx, rx;

	if (! IEEE80211_CONF_VHT(ic))
		return;

	/* Channel width */
	ic_printf(ic, "[VHT] Channel Widths: 20MHz, 40MHz, 80MHz");
	if (ic->ic_vhtcaps & IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ)
		printf(" 80+80MHz");
	if (ic->ic_vhtcaps & IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_160MHZ)
		printf(" 160MHz");
	printf("\n");

	/* Features */
	ic_printf(ic, "[VHT] Features: %b\n", ic->ic_vhtcaps,
	    IEEE80211_VHTCAP_BITS);

	/* For now, just 5GHz VHT.  Worry about 2GHz VHT later */
	for (i = 0; i < 7; i++) {
		/* Each stream is 2 bits */
		tx = (ic->ic_vht_mcsinfo.tx_mcs_map >> (2*i)) & 0x3;
		rx = (ic->ic_vht_mcsinfo.rx_mcs_map >> (2*i)) & 0x3;
		if (tx == 3 && rx == 3)
			continue;
		ic_printf(ic, "[VHT] NSS %d: TX MCS 0..%d, RX MCS 0..%d\n",
		    i + 1,
		    vht_mcs_to_num(tx),
		    vht_mcs_to_num(rx));
	}
}

void
ieee80211_vht_node_init(struct ieee80211_node *ni)
{

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_11N, ni,
	    "%s: called", __func__);
	ni->ni_flags |= IEEE80211_NODE_VHT;
}

void
ieee80211_vht_node_cleanup(struct ieee80211_node *ni)
{

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_11N, ni,
	    "%s: called", __func__);
	ni->ni_flags &= ~IEEE80211_NODE_VHT;
	ni->ni_vhtcap = 0;
	bzero(&ni->ni_vht_mcsinfo, sizeof(struct ieee80211_vht_mcs_info));
}

/*
 * Parse an 802.11ac VHT operation IE.
 */
void
ieee80211_parse_vhtopmode(struct ieee80211_node *ni, const uint8_t *ie)
{
	/* vht operation */
	ni->ni_vht_chanwidth = ie[2];
	ni->ni_vht_chan1 = ie[3];
	ni->ni_vht_chan2 = ie[4];
	ni->ni_vht_basicmcs = le16dec(ie + 5);

#if 0
	printf("%s: chan1=%d, chan2=%d, chanwidth=%d, basicmcs=0x%04x\n",
	    __func__,
	    ni->ni_vht_chan1,
	    ni->ni_vht_chan2,
	    ni->ni_vht_chanwidth,
	    ni->ni_vht_basicmcs);
#endif
}

/*
 * Parse an 802.11ac VHT capability IE.
 */
void
ieee80211_parse_vhtcap(struct ieee80211_node *ni, const uint8_t *ie)
{

	/* vht capability */
	ni->ni_vhtcap = le32dec(ie + 2);

	/* suppmcs */
	ni->ni_vht_mcsinfo.rx_mcs_map = le16dec(ie + 6);
	ni->ni_vht_mcsinfo.rx_highest = le16dec(ie + 8);
	ni->ni_vht_mcsinfo.tx_mcs_map = le16dec(ie + 10);
	ni->ni_vht_mcsinfo.tx_highest = le16dec(ie + 12);
}

int
ieee80211_vht_updateparams(struct ieee80211_node *ni,
    const uint8_t *vhtcap_ie,
    const uint8_t *vhtop_ie)
{

	//printf("%s: called\n", __func__);

	ieee80211_parse_vhtcap(ni, vhtcap_ie);
	ieee80211_parse_vhtopmode(ni, vhtop_ie);
	return (0);
}

void
ieee80211_setup_vht_rates(struct ieee80211_node *ni,
    const uint8_t *vhtcap_ie,
    const uint8_t *vhtop_ie)
{

	//printf("%s: called\n", __func__);
	/* XXX TODO */
}

void
ieee80211_vht_timeout(struct ieee80211com *ic)
{
}

void
ieee80211_vht_node_join(struct ieee80211_node *ni)
{

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_11N, ni,
	    "%s: called", __func__);
}

void
ieee80211_vht_node_leave(struct ieee80211_node *ni)
{

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_11N, ni,
	    "%s: called", __func__);
}

uint8_t *
ieee80211_add_vhtcap(uint8_t *frm, struct ieee80211_node *ni)
{
	uint32_t cap;

	memset(frm, '\0', sizeof(struct ieee80211_ie_vhtcap));

	frm[0] = IEEE80211_ELEMID_VHT_CAP;
	frm[1] = sizeof(struct ieee80211_ie_vhtcap) - 2;
	frm += 2;

	/*
	 * For now, don't do any configuration.
	 * Just populate the node configuration.
	 * We can worry about making it configurable later.
	 */

	cap = ni->ni_vhtcap;

	/*
	 * XXX TODO: any capability changes required by
	 * configuration.
	 */

	/* 32-bit VHT capability */
	ADDWORD(frm, cap);

	/* suppmcs */
	ADDSHORT(frm, ni->ni_vht_mcsinfo.rx_mcs_map);
	ADDSHORT(frm, ni->ni_vht_mcsinfo.rx_highest);
	ADDSHORT(frm, ni->ni_vht_mcsinfo.tx_mcs_map);
	ADDSHORT(frm, ni->ni_vht_mcsinfo.tx_highest);

	return (frm);
}

static uint8_t
ieee80211_vht_get_chwidth_ie(struct ieee80211_channel *c)
{

	/*
	 * XXX TODO: look at the node configuration as
	 * well?
	 */

	if (IEEE80211_IS_CHAN_VHT160(c)) {
		return IEEE80211_VHT_CHANWIDTH_160MHZ;
	}
	if (IEEE80211_IS_CHAN_VHT80_80(c)) {
		return IEEE80211_VHT_CHANWIDTH_80P80MHZ;
	}
	if (IEEE80211_IS_CHAN_VHT80(c)) {
		return IEEE80211_VHT_CHANWIDTH_80MHZ;
	}
	if (IEEE80211_IS_CHAN_VHT40(c)) {
		return IEEE80211_VHT_CHANWIDTH_USE_HT;
	}
	if (IEEE80211_IS_CHAN_VHT20(c)) {
		return IEEE80211_VHT_CHANWIDTH_USE_HT;
	}

	/* We shouldn't get here */
	printf("%s: called on a non-VHT channel (freq=%d, flags=0x%08x\n",
	    __func__,
	    (int) c->ic_freq,
	    c->ic_flags);
	return IEEE80211_VHT_CHANWIDTH_USE_HT;
}

/*
 * Note: this just uses the current channel information;
 * it doesn't use the node info after parsing.
 */
uint8_t *
ieee80211_add_vhtinfo(uint8_t *frm, struct ieee80211_node *ni)
{
	memset(frm, '\0', sizeof(struct ieee80211_ie_vht_operation));

	frm[0] = IEEE80211_ELEMID_VHT_OPMODE;
	frm[1] = sizeof(struct ieee80211_ie_vht_operation) - 2;
	frm += 2;

	/*
	 * XXX if it's a station, then see if we have a node
	 * channel or ANYC.  If it's ANYC then assume we're
	 * scanning, and announce our capabilities.
	 *
	 * This should set the "20/40/80/160MHz wide config";
	 * the 80/80 or 160MHz wide config is done in VHTCAP.
	 *
	 * Other modes - just limit it to the channel.
	 */

	/* 8-bit chanwidth */
	*frm++ = ieee80211_vht_get_chwidth_ie(ni->ni_chan);

	/* 8-bit freq1 */
	*frm++ = ni->ni_chan->ic_vht_ch_freq1;

	/* 8-bit freq2 */
	*frm++ = ni->ni_chan->ic_vht_ch_freq1;

	/* 16-bit basic MCS set - just MCS0..7 for NSS=1 for now */
	ADDSHORT(frm, 0xfffc);

	return (frm);
}

void
ieee80211_vht_update_cap(struct ieee80211_node *ni, const uint8_t *vhtcap_ie,
    const uint8_t *vhtop_ie)
{

	ieee80211_parse_vhtcap(ni, vhtcap_ie);
	ieee80211_parse_vhtopmode(ni, vhtop_ie);
}

static struct ieee80211_channel *
findvhtchan(struct ieee80211com *ic, struct ieee80211_channel *c, int vhtflags)
{

	return (ieee80211_find_channel(ic, c->ic_freq,
	    (c->ic_flags & ~IEEE80211_CHAN_VHT) | vhtflags));
}

/*
 * Handle channel promotion to VHT, similar to ieee80211_ht_adjust_channel().
 */
struct ieee80211_channel *
ieee80211_vht_adjust_channel(struct ieee80211com *ic,
    struct ieee80211_channel *chan, int flags)
{
	struct ieee80211_channel *c;

	/* First case - handle channel demotion - if VHT isn't set */
	if ((flags & IEEE80211_FVHT_VHT) == 0) {
#if 0
		printf("%s: demoting channel %d/0x%08x\n", __func__,
		    chan->ic_ieee, chan->ic_flags);
#endif
		c = ieee80211_find_channel(ic, chan->ic_freq,
		    chan->ic_flags & ~IEEE80211_CHAN_VHT);
		if (c == NULL)
			c = chan;
#if 0
		printf("%s: .. to %d/0x%08x\n", __func__,
		    c->ic_ieee, c->ic_flags);
#endif
		return (c);
	}

	/*
	 * We can upgrade to VHT - attempt to do so
	 *
	 * Note: we don't clear the HT flags, these are the hints
	 * for HT40U/HT40D when selecting VHT40 or larger channels.
	 */
	/* Start with VHT80 */
	c = NULL;
	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT160))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT80);

	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT80P80))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT80_80);

	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT80))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT80);

	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT40))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT40U);
	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT40))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT40D);
	/*
	 * If we get here, VHT20 is always possible because we checked
	 * for IEEE80211_FVHT_VHT above.
	 */
	if (c == NULL)
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT20);

	if (c != NULL)
		chan = c;

#if 0
	printf("%s: selected %d/0x%08x\n", __func__, c->ic_ieee, c->ic_flags);
#endif
	return (chan);
}
