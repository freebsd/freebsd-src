/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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
#ifndef _NET80211_IEEE80211_IOCTL_H_
#define _NET80211_IEEE80211_IOCTL_H_

/*
 * IEEE 802.11 ioctls.
 */

struct ieee80211_stats {
	u_int32_t	is_rx_badversion;	/* rx frame with bad version */
	u_int32_t	is_rx_tooshort;		/* rx frame too short */
	u_int32_t	is_rx_wrongbss;		/* rx from wrong bssid */
	u_int32_t	is_rx_dup;		/* rx discard 'cuz dup */
	u_int32_t	is_rx_wrongdir;		/* rx w/ wrong direction */
	u_int32_t	is_rx_mcastecho;	/* rx discard 'cuz mcast echo */
	u_int32_t	is_rx_notassoc;		/* rx discard 'cuz sta !assoc */
	u_int32_t	is_rx_nowep;		/* rx w/ wep but wep !config */
	u_int32_t	is_rx_wepfail;		/* rx wep processing failed */
	u_int32_t	is_rx_decap;		/* rx decapsulation failed */
	u_int32_t	is_rx_mgtdiscard;	/* rx discard mgt frames */
	u_int32_t	is_rx_ctl;		/* rx discard ctrl frames */
	u_int32_t	is_rx_rstoobig;		/* rx rate set truncated */
	u_int32_t	is_rx_elem_missing;	/* rx required element missing*/
	u_int32_t	is_rx_elem_toobig;	/* rx element too big */
	u_int32_t	is_rx_elem_toosmall;	/* rx element too small */
	u_int32_t	is_rx_elem_unknown;	/* rx element unknown */
	u_int32_t	is_rx_badchan;		/* rx frame w/ invalid chan */
	u_int32_t	is_rx_chanmismatch;	/* rx frame chan mismatch */
	u_int32_t	is_rx_nodealloc;	/* rx frame dropped */
	u_int32_t	is_rx_ssidmismatch;	/* rx frame ssid mismatch  */
	u_int32_t	is_rx_auth_unsupported;	/* rx w/ unsupported auth alg */
	u_int32_t	is_rx_auth_fail;	/* rx sta auth failure */
	u_int32_t	is_rx_assoc_bss;	/* rx assoc from wrong bssid */
	u_int32_t	is_rx_assoc_notauth;	/* rx assoc w/o auth */
	u_int32_t	is_rx_assoc_capmismatch;/* rx assoc w/ cap mismatch */
	u_int32_t	is_rx_assoc_norate;	/* rx assoc w/ no rate match */
	u_int32_t	is_rx_deauth;		/* rx deauthentication */
	u_int32_t	is_rx_disassoc;		/* rx disassociation */
	u_int32_t	is_rx_badsubtype;	/* rx frame w/ unknown subtype*/
	u_int32_t	is_rx_nombuf;		/* rx failed for lack of mbuf */
	u_int32_t	is_rx_decryptcrc;	/* rx decrypt failed on crc */
	u_int32_t	is_rx_ahdemo_mgt;	/* rx discard ahdemo mgt frame*/
	u_int32_t	is_rx_bad_auth;		/* rx bad auth request */
	u_int32_t	is_tx_nombuf;		/* tx failed for lack of mbuf */
	u_int32_t	is_tx_nonode;		/* tx failed for no node */
	u_int32_t	is_tx_unknownmgt;	/* tx of unknown mgt frame */
	u_int32_t	is_scan_active;		/* active scans started */
	u_int32_t	is_scan_passive;	/* passive scans started */
	u_int32_t	is_node_timeout;	/* nodes timed out inactivity */
	u_int32_t	is_crypto_nomem;	/* no memory for crypto ctx */
};

#ifdef __FreeBSD__
/*
 * FreeBSD-style ioctls.
 */
/* the first member must be matched with struct ifreq */
struct ieee80211req {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	u_int16_t	i_type;			/* req type */
	int16_t		i_val;			/* Index or simple value */
	int16_t		i_len;			/* Index or simple value */
	void		*i_data;		/* Extra data */
};
#define	SIOCS80211		 _IOW('i', 234, struct ieee80211req)
#define	SIOCG80211		_IOWR('i', 235, struct ieee80211req)

#define IEEE80211_IOC_SSID		1
#define IEEE80211_IOC_NUMSSIDS		2
#define IEEE80211_IOC_WEP		3
#define 	IEEE80211_WEP_NOSUP	-1
#define 	IEEE80211_WEP_OFF	0
#define 	IEEE80211_WEP_ON	1
#define 	IEEE80211_WEP_MIXED	2
#define IEEE80211_IOC_WEPKEY		4
#define IEEE80211_IOC_NUMWEPKEYS	5
#define IEEE80211_IOC_WEPTXKEY		6
#define IEEE80211_IOC_AUTHMODE		7
#define IEEE80211_IOC_STATIONNAME	8
#define IEEE80211_IOC_CHANNEL		9
#define IEEE80211_IOC_POWERSAVE		10
#define 	IEEE80211_POWERSAVE_NOSUP	-1
#define 	IEEE80211_POWERSAVE_OFF		0
#define 	IEEE80211_POWERSAVE_CAM		1
#define 	IEEE80211_POWERSAVE_PSP		2
#define 	IEEE80211_POWERSAVE_PSP_CAM	3
#define 	IEEE80211_POWERSAVE_ON		IEEE80211_POWERSAVE_CAM
#define IEEE80211_IOC_POWERSAVESLEEP	11
#define	IEEE80211_IOC_RTSTHRESHOLD	12
#define IEEE80211_IOC_PROTMODE		13
#define 	IEEE80211_PROTMODE_OFF		0
#define 	IEEE80211_PROTMODE_CTS		1
#define 	IEEE80211_PROTMODE_RTSCTS	2
#define IEEE80211_IOC_TXPOWER		14

#ifndef IEEE80211_CHAN_ANY
#define	IEEE80211_CHAN_ANY	0xffff		/* token for ``any channel'' */
#endif

#define	SIOCG80211STATS		_IOWR('i', 236, struct ifreq)
#endif /* __FreeBSD__ */

#endif /* _NET80211_IEEE80211_IOCTL_H_ */
