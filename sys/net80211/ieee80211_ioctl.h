/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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
#ifndef _NET80211_IEEE80211_IOCTL_H_
#define _NET80211_IEEE80211_IOCTL_H_

/*
 * IEEE 802.11 ioctls.
 */
#include <net80211/_ieee80211.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>

/*
 * Per/node (station) statistics.
 */
struct ieee80211_nodestats {
	uint32_t	ns_rx_data;		/* rx data frames */
	uint32_t	ns_rx_mgmt;		/* rx management frames */
	uint32_t	ns_rx_ctrl;		/* rx control frames */
	uint32_t	ns_rx_ucast;		/* rx unicast frames */
	uint32_t	ns_rx_mcast;		/* rx multi/broadcast frames */
	uint64_t	ns_rx_bytes;		/* rx data count (bytes) */
	uint64_t	ns_rx_beacons;		/* rx beacon frames */
	uint32_t	ns_rx_proberesp;	/* rx probe response frames */

	uint32_t	ns_rx_dup;		/* rx discard 'cuz dup */
	uint32_t	ns_rx_noprivacy;	/* rx w/ wep but privacy off */
	uint32_t	ns_rx_wepfail;		/* rx wep processing failed */
	uint32_t	ns_rx_demicfail;	/* rx demic failed */
	uint32_t	ns_rx_decap;		/* rx decapsulation failed */
	uint32_t	ns_rx_defrag;		/* rx defragmentation failed */
	uint32_t	ns_rx_disassoc;		/* rx disassociation */
	uint32_t	ns_rx_deauth;		/* rx deauthentication */
	uint32_t	ns_rx_action;		/* rx action */
	uint32_t	ns_rx_decryptcrc;	/* rx decrypt failed on crc */
	uint32_t	ns_rx_unauth;		/* rx on unauthorized port */
	uint32_t	ns_rx_unencrypted;	/* rx unecrypted w/ privacy */
	uint32_t	ns_rx_drop;		/* rx discard other reason */

	uint32_t	ns_tx_data;		/* tx data frames */
	uint32_t	ns_tx_mgmt;		/* tx management frames */
	uint32_t	ns_tx_ucast;		/* tx unicast frames */
	uint32_t	ns_tx_mcast;		/* tx multi/broadcast frames */
	uint64_t	ns_tx_bytes;		/* tx data count (bytes) */
	uint32_t	ns_tx_probereq;		/* tx probe request frames */

	uint32_t	ns_tx_novlantag;	/* tx discard 'cuz no tag */
	uint32_t	ns_tx_vlanmismatch;	/* tx discard 'cuz bad tag */

	uint32_t	ns_ps_discard;		/* ps discard 'cuz of age */

	/* MIB-related state */
	uint32_t	ns_tx_assoc;		/* [re]associations */
	uint32_t	ns_tx_assoc_fail;	/* [re]association failures */
	uint32_t	ns_tx_auth;		/* [re]authentications */
	uint32_t	ns_tx_auth_fail;	/* [re]authentication failures*/
	uint32_t	ns_tx_deauth;		/* deauthentications */
	uint32_t	ns_tx_deauth_code;	/* last deauth reason */
	uint32_t	ns_tx_disassoc;		/* disassociations */
	uint32_t	ns_tx_disassoc_code;	/* last disassociation reason */
};

/*
 * Summary statistics.
 */
struct ieee80211_stats {
	uint32_t	is_rx_badversion;	/* rx frame with bad version */
	uint32_t	is_rx_tooshort;		/* rx frame too short */
	uint32_t	is_rx_wrongbss;		/* rx from wrong bssid */
	uint32_t	is_rx_dup;		/* rx discard 'cuz dup */
	uint32_t	is_rx_wrongdir;		/* rx w/ wrong direction */
	uint32_t	is_rx_mcastecho;	/* rx discard 'cuz mcast echo */
	uint32_t	is_rx_notassoc;		/* rx discard 'cuz sta !assoc */
	uint32_t	is_rx_noprivacy;	/* rx w/ wep but privacy off */
	uint32_t	is_rx_unencrypted;	/* rx w/o wep and privacy on */
	uint32_t	is_rx_wepfail;		/* rx wep processing failed */
	uint32_t	is_rx_decap;		/* rx decapsulation failed */
	uint32_t	is_rx_mgtdiscard;	/* rx discard mgt frames */
	uint32_t	is_rx_ctl;		/* rx discard ctrl frames */
	uint32_t	is_rx_beacon;		/* rx beacon frames */
	uint32_t	is_rx_rstoobig;		/* rx rate set truncated */
	uint32_t	is_rx_elem_missing;	/* rx required element missing*/
	uint32_t	is_rx_elem_toobig;	/* rx element too big */
	uint32_t	is_rx_elem_toosmall;	/* rx element too small */
	uint32_t	is_rx_elem_unknown;	/* rx element unknown */
	uint32_t	is_rx_badchan;		/* rx frame w/ invalid chan */
	uint32_t	is_rx_chanmismatch;	/* rx frame chan mismatch */
	uint32_t	is_rx_nodealloc;	/* rx frame dropped */
	uint32_t	is_rx_ssidmismatch;	/* rx frame ssid mismatch  */
	uint32_t	is_rx_auth_unsupported;	/* rx w/ unsupported auth alg */
	uint32_t	is_rx_auth_fail;	/* rx sta auth failure */
	uint32_t	is_rx_auth_countermeasures;/* rx auth discard 'cuz CM */
	uint32_t	is_rx_assoc_bss;	/* rx assoc from wrong bssid */
	uint32_t	is_rx_assoc_notauth;	/* rx assoc w/o auth */
	uint32_t	is_rx_assoc_capmismatch;/* rx assoc w/ cap mismatch */
	uint32_t	is_rx_assoc_norate;	/* rx assoc w/ no rate match */
	uint32_t	is_rx_assoc_badwpaie;	/* rx assoc w/ bad WPA IE */
	uint32_t	is_rx_deauth;		/* rx deauthentication */
	uint32_t	is_rx_disassoc;		/* rx disassociation */
	uint32_t	is_rx_badsubtype;	/* rx frame w/ unknown subtype*/
	uint32_t	is_rx_nobuf;		/* rx failed for lack of buf */
	uint32_t	is_rx_decryptcrc;	/* rx decrypt failed on crc */
	uint32_t	is_rx_ahdemo_mgt;	/* rx discard ahdemo mgt frame*/
	uint32_t	is_rx_bad_auth;		/* rx bad auth request */
	uint32_t	is_rx_unauth;		/* rx on unauthorized port */
	uint32_t	is_rx_badkeyid;		/* rx w/ incorrect keyid */
	uint32_t	is_rx_ccmpreplay;	/* rx seq# violation (CCMP) */
	uint32_t	is_rx_ccmpformat;	/* rx format bad (CCMP) */
	uint32_t	is_rx_ccmpmic;		/* rx MIC check failed (CCMP) */
	uint32_t	is_rx_tkipreplay;	/* rx seq# violation (TKIP) */
	uint32_t	is_rx_tkipformat;	/* rx format bad (TKIP) */
	uint32_t	is_rx_tkipmic;		/* rx MIC check failed (TKIP) */
	uint32_t	is_rx_tkipicv;		/* rx ICV check failed (TKIP) */
	uint32_t	is_rx_badcipher;	/* rx failed 'cuz key type */
	uint32_t	is_rx_nocipherctx;	/* rx failed 'cuz key !setup */
	uint32_t	is_rx_acl;		/* rx discard 'cuz acl policy */
	uint32_t	is_tx_nobuf;		/* tx failed for lack of buf */
	uint32_t	is_tx_nonode;		/* tx failed for no node */
	uint32_t	is_tx_unknownmgt;	/* tx of unknown mgt frame */
	uint32_t	is_tx_badcipher;	/* tx failed 'cuz key type */
	uint32_t	is_tx_nodefkey;		/* tx failed 'cuz no defkey */
	uint32_t	is_tx_noheadroom;	/* tx failed 'cuz no space */
	uint32_t	is_tx_fragframes;	/* tx frames fragmented */
	uint32_t	is_tx_frags;		/* tx fragments created */
	uint32_t	is_scan_active;		/* active scans started */
	uint32_t	is_scan_passive;	/* passive scans started */
	uint32_t	is_node_timeout;	/* nodes timed out inactivity */
	uint32_t	is_crypto_nomem;	/* no memory for crypto ctx */
	uint32_t	is_crypto_tkip;		/* tkip crypto done in s/w */
	uint32_t	is_crypto_tkipenmic;	/* tkip en-MIC done in s/w */
	uint32_t	is_crypto_tkipdemic;	/* tkip de-MIC done in s/w */
	uint32_t	is_crypto_tkipcm;	/* tkip counter measures */
	uint32_t	is_crypto_ccmp;		/* ccmp crypto done in s/w */
	uint32_t	is_crypto_wep;		/* wep crypto done in s/w */
	uint32_t	is_crypto_setkey_cipher;/* cipher rejected key */
	uint32_t	is_crypto_setkey_nokey;	/* no key index for setkey */
	uint32_t	is_crypto_delkey;	/* driver key delete failed */
	uint32_t	is_crypto_badcipher;	/* unknown cipher */
	uint32_t	is_crypto_nocipher;	/* cipher not available */
	uint32_t	is_crypto_attachfail;	/* cipher attach failed */
	uint32_t	is_crypto_swfallback;	/* cipher fallback to s/w */
	uint32_t	is_crypto_keyfail;	/* driver key alloc failed */
	uint32_t	is_crypto_enmicfail;	/* en-MIC failed */
	uint32_t	is_ibss_capmismatch;	/* merge failed-cap mismatch */
	uint32_t	is_ibss_norate;		/* merge failed-rate mismatch */
	uint32_t	is_ps_unassoc;		/* ps-poll for unassoc. sta */
	uint32_t	is_ps_badaid;		/* ps-poll w/ incorrect aid */
	uint32_t	is_ps_qempty;		/* ps-poll w/ nothing to send */
	uint32_t	is_ff_badhdr;		/* fast frame rx'd w/ bad hdr */
	uint32_t	is_ff_tooshort;		/* fast frame rx decap error */
	uint32_t	is_ff_split;		/* fast frame rx split error */
	uint32_t	is_ff_decap;		/* fast frames decap'd */
	uint32_t	is_ff_encap;		/* fast frames encap'd for tx */
	uint32_t	is_rx_badbintval;	/* rx frame w/ bogus bintval */
	uint32_t	is_rx_demicfail;	/* rx demic failed */
	uint32_t	is_rx_defrag;		/* rx defragmentation failed */
	uint32_t	is_rx_mgmt;		/* rx management frames */
	uint32_t	is_rx_action;		/* rx action mgt frames */
	uint32_t	is_amsdu_tooshort;	/* A-MSDU rx decap error */
	uint32_t	is_amsdu_split;		/* A-MSDU rx split error */
	uint32_t	is_amsdu_decap;		/* A-MSDU decap'd */
	uint32_t	is_amsdu_encap;		/* A-MSDU encap'd for tx */
	uint32_t	is_ampdu_bar_bad;	/* A-MPDU BAR out of window */
	uint32_t	is_ampdu_bar_oow;	/* A-MPDU BAR before ADDBA */
	uint32_t	is_ampdu_bar_move;	/* A-MPDU BAR moved window */
	uint32_t	is_ampdu_bar_rx;	/* A-MPDU BAR frames handled */
	uint32_t	is_ampdu_rx_flush;	/* A-MPDU frames flushed */
	uint32_t	is_ampdu_rx_oor;	/* A-MPDU frames out-of-order */
	uint32_t	is_ampdu_rx_copy;	/* A-MPDU frames copied down */
	uint32_t	is_ampdu_rx_drop;	/* A-MPDU frames dropped */
	uint32_t	is_tx_badstate;		/* tx discard state != RUN */
	uint32_t	is_tx_notassoc;		/* tx failed, sta not assoc */
	uint32_t	is_tx_classify;		/* tx classification failed */
	uint32_t	is_ht_assoc_nohtcap;	/* non-HT sta rejected */
	uint32_t	is_ht_assoc_downgrade;	/* HT sta forced to legacy */
	uint32_t	is_ht_assoc_norate;	/* HT assoc w/ rate mismatch */
	uint32_t	is_ampdu_rx_age;	/* A-MPDU sent up 'cuz of age */
	uint32_t	is_ampdu_rx_move;	/* A-MPDU MSDU moved window */
	uint32_t	is_addba_reject;	/* ADDBA reject 'cuz disabled */
	uint32_t	is_addba_norequest;	/* ADDBA response w/o ADDBA */
	uint32_t	is_addba_badtoken;	/* ADDBA response w/ wrong
						   dialogtoken */
	uint32_t	is_ampdu_stop;		/* A-MPDU stream stopped */
	uint32_t	is_ampdu_stop_failed;	/* A-MPDU stream not running */
	uint32_t	is_ampdu_rx_reorder;	/* A-MPDU held for rx reorder */
	uint32_t	is_spare[16];
};

/*
 * Max size of optional information elements.  We artificially
 * constrain this; it's limited only by the max frame size (and
 * the max parameter size of the wireless extensions).
 */
#define	IEEE80211_MAX_OPT_IE	256

/*
 * WPA/RSN get/set key request.  Specify the key/cipher
 * type and whether the key is to be used for sending and/or
 * receiving.  The key index should be set only when working
 * with global keys (use IEEE80211_KEYIX_NONE for ``no index'').
 * Otherwise a unicast/pairwise key is specified by the bssid
 * (on a station) or mac address (on an ap).  They key length
 * must include any MIC key data; otherwise it should be no
 * more than IEEE80211_KEYBUF_SIZE.
 */
struct ieee80211req_key {
	uint8_t		ik_type;	/* key/cipher type */
	uint8_t		ik_pad;
	uint16_t	ik_keyix;	/* key index */
	uint8_t		ik_keylen;	/* key length in bytes */
	uint8_t		ik_flags;
/* NB: IEEE80211_KEY_XMIT and IEEE80211_KEY_RECV defined elsewhere */
#define	IEEE80211_KEY_DEFAULT	0x80	/* default xmit key */
	uint8_t		ik_macaddr[IEEE80211_ADDR_LEN];
	uint64_t	ik_keyrsc;	/* key receive sequence counter */
	uint64_t	ik_keytsc;	/* key transmit sequence counter */
	uint8_t		ik_keydata[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
};

/*
 * Delete a key either by index or address.  Set the index
 * to IEEE80211_KEYIX_NONE when deleting a unicast key.
 */
struct ieee80211req_del_key {
	uint8_t		idk_keyix;	/* key index */
	uint8_t		idk_macaddr[IEEE80211_ADDR_LEN];
};

/*
 * MLME state manipulation request.  IEEE80211_MLME_ASSOC
 * only makes sense when operating as a station.  The other
 * requests can be used when operating as a station or an
 * ap (to effect a station).
 */
struct ieee80211req_mlme {
	uint8_t		im_op;		/* operation to perform */
#define	IEEE80211_MLME_ASSOC		1	/* associate station */
#define	IEEE80211_MLME_DISASSOC		2	/* disassociate station */
#define	IEEE80211_MLME_DEAUTH		3	/* deauthenticate station */
#define	IEEE80211_MLME_AUTHORIZE	4	/* authorize station */
#define	IEEE80211_MLME_UNAUTHORIZE	5	/* unauthorize station */
	uint8_t		im_ssid_len;	/* length of optional ssid */
	uint16_t	im_reason;	/* 802.11 reason code */
	uint8_t		im_macaddr[IEEE80211_ADDR_LEN];
	uint8_t		im_ssid[IEEE80211_NWID_LEN];
};

/* 
 * MAC ACL operations.
 */
enum {
	IEEE80211_MACCMD_POLICY_OPEN	= 0,	/* set policy: no ACL's */
	IEEE80211_MACCMD_POLICY_ALLOW	= 1,	/* set policy: allow traffic */
	IEEE80211_MACCMD_POLICY_DENY	= 2,	/* set policy: deny traffic */
	IEEE80211_MACCMD_FLUSH		= 3,	/* flush ACL database */
	IEEE80211_MACCMD_DETACH		= 4,	/* detach ACL policy */
	IEEE80211_MACCMD_POLICY		= 5,	/* get ACL policy */
	IEEE80211_MACCMD_LIST		= 6,	/* get ACL database */
};

struct ieee80211req_maclist {
	uint8_t		ml_macaddr[IEEE80211_ADDR_LEN];
};

/*
 * Set the active channel list.  Note this list is
 * intersected with the available channel list in
 * calculating the set of channels actually used in
 * scanning.
 */
struct ieee80211req_chanlist {
	uint8_t		ic_channels[IEEE80211_CHAN_BYTES];
};

/*
 * Get the active channel list info.
 */
struct ieee80211req_chaninfo {
	u_int	ic_nchans;
	struct ieee80211_channel ic_chans[IEEE80211_CHAN_MAX];
};

/*
 * Retrieve the WPA/RSN information element for an associated station.
 */
struct ieee80211req_wpaie {	/* old version w/ only one ie */
	uint8_t		wpa_macaddr[IEEE80211_ADDR_LEN];
	uint8_t		wpa_ie[IEEE80211_MAX_OPT_IE];
};
struct ieee80211req_wpaie2 {
	uint8_t		wpa_macaddr[IEEE80211_ADDR_LEN];
	uint8_t		wpa_ie[IEEE80211_MAX_OPT_IE];
	uint8_t		rsn_ie[IEEE80211_MAX_OPT_IE];
};

/*
 * Retrieve per-node statistics.
 */
struct ieee80211req_sta_stats {
	union {
		/* NB: explicitly force 64-bit alignment */
		uint8_t		macaddr[IEEE80211_ADDR_LEN];
		uint64_t	pad;
	} is_u;
	struct ieee80211_nodestats is_stats;
};

/*
 * Station information block; the mac address is used
 * to retrieve other data like stats, unicast key, etc.
 */
struct ieee80211req_sta_info {
	uint16_t	isi_len;		/* length (mult of 4) */
	uint16_t	isi_ie_off;		/* offset to IE data */
	uint16_t	isi_ie_len;		/* IE length */
	uint16_t	isi_freq;		/* MHz */
	uint32_t	isi_flags;		/* channel flags */
	uint16_t	isi_state;		/* state flags */
	uint8_t		isi_authmode;		/* authentication algorithm */
	int8_t		isi_rssi;		/* receive signal strength */
	int8_t		isi_noise;		/* noise floor */
	uint8_t		isi_capinfo;		/* capabilities */
	uint8_t		isi_erp;		/* ERP element */
	uint8_t		isi_macaddr[IEEE80211_ADDR_LEN];
	uint8_t		isi_nrates;
						/* negotiated rates */
	uint8_t		isi_rates[IEEE80211_RATE_MAXSIZE];
	uint8_t		isi_txrate;		/* index to isi_rates[] */
	uint16_t	isi_associd;		/* assoc response */
	uint16_t	isi_txpower;		/* current tx power */
	uint16_t	isi_vlan;		/* vlan tag */
	/* NB: [IEEE80211_NONQOS_TID] holds seq#'s for non-QoS stations */
	uint16_t	isi_txseqs[IEEE80211_TID_SIZE];/* tx seq #/TID */
	uint16_t	isi_rxseqs[IEEE80211_TID_SIZE];/* rx seq#/TID */
	uint16_t	isi_inact;		/* inactivity timer */
	/* XXX frag state? */
	/* variable length IE data */
};

/*
 * Retrieve per-station information; to retrieve all
 * specify a mac address of ff:ff:ff:ff:ff:ff.
 */
struct ieee80211req_sta_req {
	union {
		/* NB: explicitly force 64-bit alignment */
		uint8_t		macaddr[IEEE80211_ADDR_LEN];
		uint64_t	pad;
	} is_u;
	struct ieee80211req_sta_info info[1];	/* variable length */
};

/*
 * Get/set per-station tx power cap.
 */
struct ieee80211req_sta_txpow {
	uint8_t		it_macaddr[IEEE80211_ADDR_LEN];
	uint8_t		it_txpow;
};

/*
 * WME parameters are set and return using i_val and i_len.
 * i_val holds the value itself.  i_len specifies the AC
 * and, as appropriate, then high bit specifies whether the
 * operation is to be applied to the BSS or ourself.
 */
#define	IEEE80211_WMEPARAM_SELF	0x0000		/* parameter applies to self */
#define	IEEE80211_WMEPARAM_BSS	0x8000		/* parameter applies to BSS */
#define	IEEE80211_WMEPARAM_VAL	0x7fff		/* parameter value */

#ifdef __FreeBSD__
/*
 * FreeBSD-style ioctls.
 */
/* the first member must be matched with struct ifreq */
struct ieee80211req {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	uint16_t	i_type;			/* req type */
	int16_t		i_val;			/* Index or simple value */
	int16_t		i_len;			/* Index or simple value */
	void		*i_data;		/* Extra data */
};
#define	SIOCS80211		 _IOW('i', 234, struct ieee80211req)
#define	SIOCG80211		_IOWR('i', 235, struct ieee80211req)
#define	SIOCG80211STATS		_IOWR('i', 236, struct ifreq)

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
#define	IEEE80211_IOC_TXPOWER		14	/* global tx power limit */
#define	IEEE80211_IOC_BSSID		15
#define	IEEE80211_IOC_ROAMING		16	/* roaming mode */
#define	IEEE80211_IOC_PRIVACY		17	/* privacy invoked */
#define	IEEE80211_IOC_DROPUNENCRYPTED	18	/* discard unencrypted frames */
#define	IEEE80211_IOC_WPAKEY		19
#define	IEEE80211_IOC_DELKEY		20
#define	IEEE80211_IOC_MLME		21
#define	IEEE80211_IOC_OPTIE		22	/* optional info. element */
#define	IEEE80211_IOC_SCAN_REQ		23
/* 24 was IEEE80211_IOC_SCAN_RESULTS */
#define	IEEE80211_IOC_COUNTERMEASURES	25	/* WPA/TKIP countermeasures */
#define	IEEE80211_IOC_WPA		26	/* WPA mode (0,1,2) */
#define	IEEE80211_IOC_CHANLIST		27	/* channel list */
#define	IEEE80211_IOC_WME		28	/* WME mode (on, off) */
#define	IEEE80211_IOC_HIDESSID		29	/* hide SSID mode (on, off) */
#define	IEEE80211_IOC_APBRIDGE		30	/* AP inter-sta bridging */
#define	IEEE80211_IOC_MCASTCIPHER	31	/* multicast/default cipher */
#define	IEEE80211_IOC_MCASTKEYLEN	32	/* multicast key length */
#define	IEEE80211_IOC_UCASTCIPHERS	33	/* unicast cipher suites */
#define	IEEE80211_IOC_UCASTCIPHER	34	/* unicast cipher */
#define	IEEE80211_IOC_UCASTKEYLEN	35	/* unicast key length */
#define	IEEE80211_IOC_DRIVER_CAPS	36	/* driver capabilities */
#define	IEEE80211_IOC_KEYMGTALGS	37	/* key management algorithms */
#define	IEEE80211_IOC_RSNCAPS		38	/* RSN capabilities */
#define	IEEE80211_IOC_WPAIE		39	/* WPA information element */
#define	IEEE80211_IOC_STA_STATS		40	/* per-station statistics */
#define	IEEE80211_IOC_MACCMD		41	/* MAC ACL operation */
#define	IEEE80211_IOC_CHANINFO		42	/* channel info list */
#define	IEEE80211_IOC_TXPOWMAX		43	/* max tx power for channel */
#define	IEEE80211_IOC_STA_TXPOW		44	/* per-station tx power limit */
/* 45 was IEEE80211_IOC_STA_INFO */
#define	IEEE80211_IOC_WME_CWMIN		46	/* WME: ECWmin */
#define	IEEE80211_IOC_WME_CWMAX		47	/* WME: ECWmax */
#define	IEEE80211_IOC_WME_AIFS		48	/* WME: AIFSN */
#define	IEEE80211_IOC_WME_TXOPLIMIT	49	/* WME: txops limit */
#define	IEEE80211_IOC_WME_ACM		50	/* WME: ACM (bss only) */
#define	IEEE80211_IOC_WME_ACKPOLICY	51	/* WME: ACK policy (!bss only)*/
#define	IEEE80211_IOC_DTIM_PERIOD	52	/* DTIM period (beacons) */
#define	IEEE80211_IOC_BEACON_INTERVAL	53	/* beacon interval (ms) */
#define	IEEE80211_IOC_ADDMAC		54	/* add sta to MAC ACL table */
#define	IEEE80211_IOC_DELMAC		55	/* del sta from MAC ACL table */
#define	IEEE80211_IOC_PUREG		56	/* pure 11g (no 11b stations) */
#define	IEEE80211_IOC_FF		57	/* ATH fast frames (on, off) */
#define	IEEE80211_IOC_TURBOP		58	/* ATH turbo' (on, off) */
#define	IEEE80211_IOC_BGSCAN		59	/* bg scanning (on, off) */
#define	IEEE80211_IOC_BGSCAN_IDLE	60	/* bg scan idle threshold */
#define	IEEE80211_IOC_BGSCAN_INTERVAL	61	/* bg scan interval */
#define	IEEE80211_IOC_SCANVALID		65	/* scan cache valid threshold */
#define	IEEE80211_IOC_ROAM_RSSI_11A	66	/* rssi threshold in 11a */
#define	IEEE80211_IOC_ROAM_RSSI_11B	67	/* rssi threshold in 11b */
#define	IEEE80211_IOC_ROAM_RSSI_11G	68	/* rssi threshold in 11g */
#define	IEEE80211_IOC_ROAM_RATE_11A	69	/* tx rate threshold in 11a */
#define	IEEE80211_IOC_ROAM_RATE_11B	70	/* tx rate threshold in 11b */
#define	IEEE80211_IOC_ROAM_RATE_11G	71	/* tx rate threshold in 11g */
#define	IEEE80211_IOC_MCAST_RATE	72	/* tx rate for mcast frames */
#define	IEEE80211_IOC_FRAGTHRESHOLD	73	/* tx fragmentation threshold */
#define	IEEE80211_IOC_BURST		75	/* packet bursting */
#define	IEEE80211_IOC_SCAN_RESULTS	76	/* get scan results */
#define	IEEE80211_IOC_BMISSTHRESHOLD	77	/* beacon miss threshold */
#define	IEEE80211_IOC_STA_INFO		78	/* station/neighbor info */
#define	IEEE80211_IOC_WPAIE2		79	/* WPA+RSN info elements */
#define	IEEE80211_IOC_CURCHAN		80	/* current channel */
#define	IEEE80211_IOC_SHORTGI		81	/* 802.11n half GI */
#define	IEEE80211_IOC_AMPDU		82	/* 802.11n A-MPDU (on, off) */
#define	IEEE80211_IOC_AMPDU_LIMIT	83	/* A-MPDU length limit */
#define	IEEE80211_IOC_AMPDU_DENSITY	84	/* A-MPDU density */
#define	IEEE80211_IOC_AMSDU		85	/* 802.11n A-MSDU (on, off) */
#define	IEEE80211_IOC_AMSDU_LIMIT	86	/* A-MSDU length limit */
#define	IEEE80211_IOC_PUREN		87	/* pure 11n (no legacy sta's) */
#define	IEEE80211_IOC_DOTH		88	/* 802.11h (on, off) */
#define	IEEE80211_IOC_REGDOMAIN		89	/* regulatory domain */
#define	IEEE80211_IOC_COUNTRYCODE	90	/* ISO country code */
#define	IEEE80211_IOC_LOCATION		91	/* indoor/outdoor/anywhere */
#define	IEEE80211_IOC_HTCOMPAT		92	/* support pre-D1.10 HT ie's */
#define	IEEE80211_IOC_INACTIVITY	94	/* sta inactivity handling */
#define IEEE80211_IOC_HTPROTMODE	102	/* HT protection (off, rts) */
#define	IEEE80211_IOC_HTCONF		105	/* HT config (off, HT20, HT40)*/

/*
 * Scan result data returned for IEEE80211_IOC_SCAN_RESULTS.
 * Each result is a fixed size structure followed by a variable
 * length SSID and one or more variable length information elements.
 * The size of each variable length item is found in the fixed
 * size structure and the entire length of the record is specified
 * in isr_len.  Result records are rounded to a multiple of 4 bytes.
 */
struct ieee80211req_scan_result {
	uint16_t	isr_len;		/* length (mult of 4) */
	uint16_t	isr_ie_off;		/* offset to IE data */
	uint16_t	isr_ie_len;		/* IE length */
	uint16_t	isr_freq;		/* MHz */
	uint16_t	isr_flags;		/* channel flags */
	int8_t		isr_noise;
	int8_t		isr_rssi;
	uint8_t		isr_intval;		/* beacon interval */
	uint8_t		isr_capinfo;		/* capabilities */
	uint8_t		isr_erp;		/* ERP element */
	uint8_t		isr_bssid[IEEE80211_ADDR_LEN];
	uint8_t		isr_nrates;
	uint8_t		isr_rates[IEEE80211_RATE_MAXSIZE];
	uint8_t		isr_ssid_len;		/* SSID length */
	/* variable length SSID followed by IE data */
};

struct ieee80211_clone_params {
	char	icp_parent[IFNAMSIZ];		/* parent device */
	int	icp_opmode;			/* operating mode */
};
#endif /* __FreeBSD__ */

#endif /* _NET80211_IEEE80211_IOCTL_H_ */
