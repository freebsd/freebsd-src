/*-
 * Copyright (c) 2007-2009 Sam Leffler, Errno Consulting
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
 *
 * $FreeBSD$
 */
#ifndef _NET80211_IEEE80211_INPUT_H_
#define _NET80211_IEEE80211_INPUT_H_

/* Verify the existence and length of __elem or get out. */
#define IEEE80211_VERIFY_ELEMENT(__elem, __maxlen, _action) do {	\
	if ((__elem) == NULL) {						\
		IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,		\
		    wh, NULL, "%s", "no " #__elem );			\
		vap->iv_stats.is_rx_elem_missing++;			\
		_action;						\
	} else if ((__elem)[1] > (__maxlen)) {				\
		IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,		\
		    wh, NULL, "bad " #__elem " len %d", (__elem)[1]);	\
		vap->iv_stats.is_rx_elem_toobig++;			\
		_action;						\
	}								\
} while (0)

#define	IEEE80211_VERIFY_LENGTH(_len, _minlen, _action) do {		\
	if ((_len) < (_minlen)) {					\
		IEEE80211_DISCARD(vap, IEEE80211_MSG_ELEMID,		\
		    wh, NULL, "ie too short, got %d, expected %d",	\
		    (_len), (_minlen));					\
		vap->iv_stats.is_rx_elem_toosmall++;			\
		_action;						\
	}								\
} while (0)

#ifdef IEEE80211_DEBUG
void	ieee80211_ssid_mismatch(struct ieee80211vap *, const char *tag,
	uint8_t mac[IEEE80211_ADDR_LEN], uint8_t *ssid);

#define	IEEE80211_VERIFY_SSID(_ni, _ssid, _action) do {			\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {	\
		if (ieee80211_msg_input(vap))				\
			ieee80211_ssid_mismatch(vap, 			\
			    ieee80211_mgt_subtype_name[subtype >>	\
				IEEE80211_FC0_SUBTYPE_SHIFT],		\
				wh->i_addr2, _ssid);			\
		vap->iv_stats.is_rx_ssidmismatch++;			\
		_action;						\
	}								\
} while (0)
#else /* !IEEE80211_DEBUG */
#define	IEEE80211_VERIFY_SSID(_ni, _ssid, _action) do {			\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {	\
		vap->iv_stats.is_rx_ssidmismatch++;			\
		_action;						\
	}								\
} while (0)
#endif /* !IEEE80211_DEBUG */

/* unalligned little endian access */     
#define LE_READ_2(p)					\
	((uint16_t)					\
	 ((((const uint8_t *)(p))[0]      ) |		\
	  (((const uint8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)					\
	((uint32_t)					\
	 ((((const uint8_t *)(p))[0]      ) |		\
	  (((const uint8_t *)(p))[1] <<  8) |		\
	  (((const uint8_t *)(p))[2] << 16) |		\
	  (((const uint8_t *)(p))[3] << 24)))

static __inline int
iswpaoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}

static __inline int
iswmeoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI);
}

static __inline int
iswmeparam(const uint8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_PARAM_OUI_SUBTYPE;
}

static __inline int
iswmeinfo(const uint8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_INFO_OUI_SUBTYPE;
}

static __inline int
isatherosoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}

static __inline int
istdmaoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((TDMA_OUI_TYPE<<24)|TDMA_OUI);
}

static __inline int
ishtcapoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((BCM_OUI_HTCAP<<24)|BCM_OUI);
}

static __inline int
ishtinfooui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((BCM_OUI_HTINFO<<24)|BCM_OUI);
}

void	ieee80211_deliver_data(struct ieee80211vap *,
		struct ieee80211_node *, struct mbuf *);
struct mbuf *ieee80211_defrag(struct ieee80211_node *,
		struct mbuf *, int);
struct mbuf *ieee80211_realign(struct ieee80211vap *, struct mbuf *, size_t);
struct mbuf *ieee80211_decap(struct ieee80211vap *, struct mbuf *, int);
struct mbuf *ieee80211_decap1(struct mbuf *, int *);
int	ieee80211_setup_rates(struct ieee80211_node *ni,
		const uint8_t *rates, const uint8_t *xrates, int flags);
void ieee80211_send_error(struct ieee80211_node *,
		const uint8_t mac[IEEE80211_ADDR_LEN], int subtype, int arg);
int	ieee80211_alloc_challenge(struct ieee80211_node *);
int	ieee80211_parse_beacon(struct ieee80211_node *, struct mbuf *,
		struct ieee80211_scanparams *);
int	ieee80211_parse_action(struct ieee80211_node *, struct mbuf *);
#endif /* _NET80211_IEEE80211_INPUT_H_ */
