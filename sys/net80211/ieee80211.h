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
#ifndef _NET80211_IEEE80211_H_
#define _NET80211_IEEE80211_H_

/*
 * 802.11 protocol definitions.
 */

#define	IEEE80211_ADDR_LEN	6		/* size of 802.11 address */
/* is 802.11 address multicast/broadcast? */
#define	IEEE80211_IS_MULTICAST(_a)	(*(_a) & 0x01)

/* IEEE 802.11 PLCP header */
struct ieee80211_plcp_hdr {
	u_int16_t	i_sfd;
	u_int8_t	i_signal;
	u_int8_t	i_service;
	u_int16_t	i_length;
	u_int16_t	i_crc;
} __attribute__((__packed__));

#define IEEE80211_PLCP_SFD      0xF3A0 
#define IEEE80211_PLCP_SERVICE  0x00

/*
 * generic definitions for IEEE 802.11 frames
 */
struct ieee80211_frame {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr3[IEEE80211_ADDR_LEN];
	u_int8_t	i_seq[2];
	/* possibly followed by addr4[IEEE80211_ADDR_LEN]; */
	/* see below */
} __attribute__((__packed__));

struct ieee80211_qosframe {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr3[IEEE80211_ADDR_LEN];
	u_int8_t	i_seq[2];
	u_int8_t	i_qos[2];
	/* possibly followed by addr4[IEEE80211_ADDR_LEN]; */
	/* see below */
} __attribute__((__packed__));

struct ieee80211_qoscntl {
	u_int8_t	i_qos[2];
};

struct ieee80211_frame_addr4 {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr3[IEEE80211_ADDR_LEN];
	u_int8_t	i_seq[2];
	u_int8_t	i_addr4[IEEE80211_ADDR_LEN];
} __attribute__((__packed__));


struct ieee80211_qosframe_addr4 {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr3[IEEE80211_ADDR_LEN];
	u_int8_t	i_seq[2];
	u_int8_t	i_addr4[IEEE80211_ADDR_LEN];
	u_int8_t	i_qos[2];
} __attribute__((__packed__));

/*
 * Management Notification Frame
 */
struct ieee80211_mnf {
	u_int8_t	mnf_category;
	u_int8_t	mnf_action;
	u_int8_t	mnf_dialog;
	u_int8_t	mnf_status;
} __attribute__((__packed__));
#define	MNF_SETUP_REQ	0
#define	MNF_SETUP_RESP	1
#define	MNF_TEARDOWN	2

/*
 * WME/802.11e Tspec Element
 */
struct ieee80211_wme_tspec {
	u_int8_t	ts_id;
	u_int8_t	ts_len;
	u_int8_t	ts_oui[3];
	u_int8_t	ts_oui_type;
	u_int8_t	ts_oui_subtype;
	u_int8_t	ts_version;
	u_int8_t	ts_tsinfo[3];
	u_int8_t	ts_nom_msdu[2];
	u_int8_t	ts_max_msdu[2];
	u_int8_t	ts_min_svc[4];
	u_int8_t	ts_max_svc[4];
	u_int8_t	ts_inactv_intv[4];
	u_int8_t	ts_susp_intv[4];
	u_int8_t	ts_start_svc[4];
	u_int8_t	ts_min_rate[4];
	u_int8_t	ts_mean_rate[4];
	u_int8_t	ts_max_burst[4];
	u_int8_t	ts_min_phy[4];
	u_int8_t	ts_peak_rate[4];
	u_int8_t	ts_delay[4];
	u_int8_t	ts_surplus[2];
	u_int8_t	ts_medium_time[2];
} __attribute__((__packed__));

#define	IEEE80211_FC0_VERSION_MASK		0x03
#define	IEEE80211_FC0_VERSION_SHIFT		0
#define	IEEE80211_FC0_VERSION_0			0x00
#define	IEEE80211_FC0_TYPE_MASK			0x0c
#define	IEEE80211_FC0_TYPE_SHIFT		2
#define	IEEE80211_FC0_TYPE_MGT			0x00
#define	IEEE80211_FC0_TYPE_CTL			0x04
#define	IEEE80211_FC0_TYPE_DATA			0x08

#define	IEEE80211_FC0_SUBTYPE_MASK		0xf0
#define	IEEE80211_FC0_SUBTYPE_SHIFT		4
/* for TYPE_MGT */
#define	IEEE80211_FC0_SUBTYPE_ASSOC_REQ		0x00
#define	IEEE80211_FC0_SUBTYPE_ASSOC_RESP	0x10
#define	IEEE80211_FC0_SUBTYPE_REASSOC_REQ	0x20
#define	IEEE80211_FC0_SUBTYPE_REASSOC_RESP	0x30
#define	IEEE80211_FC0_SUBTYPE_PROBE_REQ		0x40
#define	IEEE80211_FC0_SUBTYPE_PROBE_RESP	0x50
#define	IEEE80211_FC0_SUBTYPE_BEACON		0x80
#define	IEEE80211_FC0_SUBTYPE_ATIM		0x90
#define	IEEE80211_FC0_SUBTYPE_DISASSOC		0xa0
#define	IEEE80211_FC0_SUBTYPE_AUTH		0xb0
#define	IEEE80211_FC0_SUBTYPE_DEAUTH		0xc0
/* for TYPE_CTL */
#define	IEEE80211_FC0_SUBTYPE_PS_POLL		0xa0
#define	IEEE80211_FC0_SUBTYPE_RTS		0xb0
#define	IEEE80211_FC0_SUBTYPE_CTS		0xc0
#define	IEEE80211_FC0_SUBTYPE_ACK		0xd0
#define	IEEE80211_FC0_SUBTYPE_CF_END		0xe0
#define	IEEE80211_FC0_SUBTYPE_CF_END_ACK	0xf0
/* for TYPE_DATA (bit combination) */
#define	IEEE80211_FC0_SUBTYPE_DATA		0x00
#define	IEEE80211_FC0_SUBTYPE_CF_ACK		0x10
#define	IEEE80211_FC0_SUBTYPE_CF_POLL		0x20
#define	IEEE80211_FC0_SUBTYPE_CF_ACPL		0x30
#define	IEEE80211_FC0_SUBTYPE_NODATA		0x40
#define	IEEE80211_FC0_SUBTYPE_CFACK		0x50
#define	IEEE80211_FC0_SUBTYPE_CFPOLL		0x60
#define	IEEE80211_FC0_SUBTYPE_CF_ACK_CF_ACK	0x70
#define	IEEE80211_FC0_SUBTYPE_QOS		0x80

#define	IEEE80211_FC1_DIR_MASK			0x03
#define	IEEE80211_FC1_DIR_NODS			0x00	/* STA->STA */
#define	IEEE80211_FC1_DIR_TODS			0x01	/* STA->AP  */
#define	IEEE80211_FC1_DIR_FROMDS		0x02	/* AP ->STA */
#define	IEEE80211_FC1_DIR_DSTODS		0x03	/* AP ->AP  */

#define	IEEE80211_FC1_MORE_FRAG			0x04
#define	IEEE80211_FC1_RETRY			0x08
#define	IEEE80211_FC1_PWR_MGT			0x10
#define	IEEE80211_FC1_MORE_DATA			0x20
#define	IEEE80211_FC1_WEP			0x40
#define	IEEE80211_FC1_ORDER			0x80

#define	IEEE80211_SEQ_FRAG_MASK			0x000f
#define	IEEE80211_SEQ_FRAG_SHIFT		0
#define	IEEE80211_SEQ_SEQ_MASK			0xfff0
#define	IEEE80211_SEQ_SEQ_SHIFT			4

#define	IEEE80211_NWID_LEN			32

#define	IEEE80211_QOS_TXOP			0x00ff
/* bit 8 is reserved */
#define	IEEE80211_QOS_ACKPOLICY			0x0600
#define	IEEE80211_QOS_ESOP			0x0800
#define	IEEE80211_QOS_TID			0xf000

/*
 * Control frames.
 */
struct ieee80211_frame_min {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[IEEE80211_ADDR_LEN];
	u_int8_t	i_addr2[IEEE80211_ADDR_LEN];
	/* FCS */
} __attribute__((__packed__));

struct ieee80211_frame_rts {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_ra[IEEE80211_ADDR_LEN];
	u_int8_t	i_ta[IEEE80211_ADDR_LEN];
	/* FCS */
} __attribute__((__packed__));

struct ieee80211_frame_cts {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_ra[IEEE80211_ADDR_LEN];
	/* FCS */
} __attribute__((__packed__));

struct ieee80211_frame_ack {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_ra[IEEE80211_ADDR_LEN];
	/* FCS */
} __attribute__((__packed__));

struct ieee80211_frame_pspoll {
	u_int8_t	i_fc[2];
	u_int8_t	i_aid[2];
	u_int8_t	i_bssid[IEEE80211_ADDR_LEN];
	u_int8_t	i_ta[IEEE80211_ADDR_LEN];
	/* FCS */
} __attribute__((__packed__));

struct ieee80211_frame_cfend {		/* NB: also CF-End+CF-Ack */
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];	/* should be zero */
	u_int8_t	i_ra[IEEE80211_ADDR_LEN];
	u_int8_t	i_bssid[IEEE80211_ADDR_LEN];
	/* FCS */
} __attribute__((__packed__));

/*
 * BEACON management packets
 *
 *	octet timestamp[8]
 *	octet beacon interval[2]
 *	octet capability information[2]
 *	information element
 *		octet elemid
 *		octet length
 *		octet information[length]
 */

typedef uint8_t *ieee80211_mgt_beacon_t;

#define	IEEE80211_BEACON_INTERVAL(beacon) \
	((beacon)[8] | ((beacon)[9] << 8))
#define	IEEE80211_BEACON_CAPABILITY(beacon) \
	((beacon)[10] | ((beacon)[11] << 8))

#define	IEEE80211_CAPINFO_ESS			0x0001
#define	IEEE80211_CAPINFO_IBSS			0x0002
#define	IEEE80211_CAPINFO_CF_POLLABLE		0x0004
#define	IEEE80211_CAPINFO_CF_POLLREQ		0x0008
#define	IEEE80211_CAPINFO_PRIVACY		0x0010
#define	IEEE80211_CAPINFO_SHORT_PREAMBLE	0x0020
#define	IEEE80211_CAPINFO_PBCC			0x0040
#define	IEEE80211_CAPINFO_CHNL_AGILITY		0x0080
/* bits 8-9 are reserved */
#define	IEEE80211_CAPINFO_SHORT_SLOTTIME	0x0400
#define	IEEE80211_CAPINFO_RSN			0x0800
/* bit 12 is reserved */
#define	IEEE80211_CAPINFO_DSSSOFDM		0x2000
/* bits 14-15 are reserved */

/*
 * 802.11i/WPA information element (maximally sized).
 */
struct ieee80211_ie_wpa {
	u_int8_t	wpa_oui[3];	/* 0x00, 0x50, 0xf2 */
	u_int8_t	wpa_type;	/* OUI type */
	u_int16_t	wpa_version;	/* spec revision */
	u_int32_t	wpa_mcipher[1];	/* multicast/group key cipher */
	u_int16_t	wpa_uciphercnt;	/* # pairwise key ciphers */
	u_int32_t	wpa_uciphers[8];/* ciphers */
	u_int16_t	wpa_authselcnt;	/* authentication selector cnt*/
	u_int32_t	wpa_authsels[8];/* selectors */
} __attribute__((__packed__));

/*
 * Management information elements
 */
struct ieee80211_information {
	char	ssid[IEEE80211_NWID_LEN+1];
	struct rates {
		u_int8_t	*p;
	} rates;
	struct fh {
		u_int16_t	dwell;
		u_int8_t	set;
		u_int8_t	pattern;
		u_int8_t	index;
	} fh;
	struct ds {
		u_int8_t	channel;
	} ds;
	struct cf {
		u_int8_t	count;
		u_int8_t	period;
		u_int8_t	maxdur[2];
		u_int8_t	dur[2];
	} cf;
	struct tim {
		u_int8_t	count;
		u_int8_t	period;
		u_int8_t	bitctl;
		/* u_int8_t	pvt[251]; The driver needs to use this. */
	} tim;
	struct ibss {
		u_int16_t	atim;
	} ibss;
	struct challenge {
		u_int8_t	*p;
		u_int8_t	len;
	} challenge;
	struct erp {
		u_int8_t	flags;
	} erp;
	struct country {
		u_int8_t	cc[3];		/* ISO CC+(I)ndoor/(O)utdoor */
		struct {
			u_int8_t schan;		/* starting channel */
			u_int8_t nchan;		/* number channels */
			u_int8_t maxtxpwr;	
		} band[4];			/* up to 4 sub bands */
	} country;
	struct ath {
		u_int8_t	flags;
	} ath;
	struct ieee80211_ie_wpa	wpa;
};

enum {
	IEEE80211_ELEMID_SSID			= 0,
	IEEE80211_ELEMID_RATES			= 1,
	IEEE80211_ELEMID_FHPARMS		= 2,
	IEEE80211_ELEMID_DSPARMS		= 3,
	IEEE80211_ELEMID_CFPARMS		= 4,
	IEEE80211_ELEMID_TIM			= 5,
	IEEE80211_ELEMID_IBSSPARMS		= 6,
	IEEE80211_ELEMID_COUNTRY		= 7,
	IEEE80211_ELEMID_CHALLENGE		= 16,
	/* 17-31 reserved for challenge text extension */
	IEEE80211_ELEMID_ERP			= 42,
	IEEE80211_ELEMID_XRATES			= 50,
	IEEE80211_ELEMID_TPC			= 150,
	IEEE80211_ELEMID_CCKM			= 156,
	IEEE80211_ELEMID_VENDOR			= 221,	/* vendor private */
};

#define IEEE80211_CHALLENGE_LEN			128

#define	IEEE80211_RATE_BASIC			0x80
#define	IEEE80211_RATE_VAL			0x7f

/* EPR information element flags */
#define	IEEE80211_ERP_NON_ERP_PRESENT		0x01
#define	IEEE80211_ERP_USE_PROTECTION		0x02
#define	IEEE80211_ERP_BARKER_MODE		0x04

/* Atheros private advanced capabilities info */
#define	ATHEROS_CAP_TURBO_PRIME			0x01
#define	ATHEROS_CAP_COMPRESSION			0x02
#define	ATHEROS_CAP_FAST_FRAME			0x04
/* bits 3-6 reserved */
#define	ATHEROS_CAP_BOOST			0x80

#define	ATH_OUI			0x7f0300		/* Atheros OUI */
#define	ATH_OUI_TYPE		0x01
#define	ATH_OUI_VERSION		0x01

#define	WPA_OUI			0xf25000
#define	WPA_OUI_TYPE		0x01
#define	WPA_OUI_VERSION		1		/* current supported version */

#define	WPA_CSE_NULL		0x00
#define	WPA_CSE_WEP40		0x01
#define	WPA_CSE_TKIP		0x02
#define	WPA_CSE_WRAP		0x03		/* WPA2/802.11i */
#define	WPA_CSE_CCMP		0x04
#define	WPA_CSE_WEP104		0x05

#define	WPA_ASE_NONE		0x00
#define	WPA_ASE_8021X_UNSPEC	0x01
#define	WPA_ASE_8021X_PSK	0x02

/*
 * AUTH management packets
 *
 *	octet algo[2]
 *	octet seq[2]
 *	octet status[2]
 *	octet chal.id
 *	octet chal.length
 *	octet chal.text[253]
 */

typedef u_int8_t *ieee80211_mgt_auth_t;

#define	IEEE80211_AUTH_ALGORITHM(auth) \
	((auth)[0] | ((auth)[1] << 8))
#define	IEEE80211_AUTH_TRANSACTION(auth) \
	((auth)[2] | ((auth)[3] << 8))
#define	IEEE80211_AUTH_STATUS(auth) \
	((auth)[4] | ((auth)[5] << 8))

#define	IEEE80211_AUTH_ALG_OPEN			0x0000
#define	IEEE80211_AUTH_ALG_SHARED		0x0001
#define	IEEE80211_AUTH_ALG_LEAP			0x0080

enum {
	IEEE80211_AUTH_OPEN_REQUEST		= 1,
	IEEE80211_AUTH_OPEN_RESPONSE		= 2,
};

enum {
	IEEE80211_AUTH_SHARED_REQUEST		= 1,
	IEEE80211_AUTH_SHARED_CHALLENGE		= 2,
	IEEE80211_AUTH_SHARED_RESPONSE		= 3,
	IEEE80211_AUTH_SHARED_PASS		= 4,
};

/*
 * Reason codes
 *
 * Unlisted codes are reserved
 */

enum {
	IEEE80211_REASON_UNSPECIFIED		= 1,
	IEEE80211_REASON_AUTH_EXPIRE		= 2,
	IEEE80211_REASON_AUTH_LEAVE		= 3,
	IEEE80211_REASON_ASSOC_EXPIRE		= 4,
	IEEE80211_REASON_ASSOC_TOOMANY		= 5,
	IEEE80211_REASON_NOT_AUTHED		= 6,
	IEEE80211_REASON_NOT_ASSOCED		= 7,
	IEEE80211_REASON_ASSOC_LEAVE		= 8,
	IEEE80211_REASON_ASSOC_NOT_AUTHED	= 9,

	IEEE80211_REASON_RSN_REQUIRED		= 11,
	IEEE80211_REASON_RSN_INCONSISTENT	= 12,
	IEEE80211_REASON_IE_INVALID		= 13,
	IEEE80211_REASON_MIC_FAILURE		= 14,

	IEEE80211_STATUS_SUCCESS		= 0,
	IEEE80211_STATUS_UNSPECIFIED		= 1,
	IEEE80211_STATUS_CAPINFO		= 10,
	IEEE80211_STATUS_NOT_ASSOCED		= 11,
	IEEE80211_STATUS_OTHER			= 12,
	IEEE80211_STATUS_ALG			= 13,
	IEEE80211_STATUS_SEQUENCE		= 14,
	IEEE80211_STATUS_CHALLENGE		= 15,
	IEEE80211_STATUS_TIMEOUT		= 16,
	IEEE80211_STATUS_TOOMANY		= 17,
	IEEE80211_STATUS_BASIC_RATE		= 18,
	IEEE80211_STATUS_SP_REQUIRED		= 19,
	IEEE80211_STATUS_PBCC_REQUIRED		= 20,
	IEEE80211_STATUS_CA_REQUIRED		= 21,
	IEEE80211_STATUS_TOO_MANY_STATIONS	= 22,
	IEEE80211_STATUS_RATES			= 23,
	IEEE80211_STATUS_SHORTSLOT_REQUIRED	= 25,
	IEEE80211_STATUS_DSSSOFDM_REQUIRED	= 26,
};

#define	IEEE80211_WEP_KEYLEN			5	/* 40bit */
#define	IEEE80211_WEP_IVLEN			3	/* 24bit */
#define	IEEE80211_WEP_KIDLEN			1	/* 1 octet */
#define	IEEE80211_WEP_CRCLEN			4	/* CRC-32 */
#define	IEEE80211_WEP_NKID			4	/* number of key ids */

#define	IEEE80211_CRC_LEN			4

/*
 * Maximum acceptable MTU is:
 *	IEEE80211_MAX_LEN - WEP overhead - CRC -
 *		QoS overhead - RSN/WPA overhead
 * Min is arbitrarily chosen > IEEE80211_MIN_LEN.  The default
 * mtu is Ethernet-compatible; it's set by ether_ifattach.
 */
#define	IEEE80211_MTU_MAX			2290
#define	IEEE80211_MTU_MIN			32

#define	IEEE80211_MAX_LEN			(2300 + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))
#define	IEEE80211_MIN_LEN \
	(sizeof(struct ieee80211_frame_min) + IEEE80211_CRC_LEN)

/* 
 * RTS frame length parameters.  The default is specified in
 * the 802.11 spec.  The max may be wrong for jumbo frames.
 */
#define	IEEE80211_RTS_DEFAULT			512
#define	IEEE80211_RTS_MIN			1
#define	IEEE80211_RTS_MAX			IEEE80211_MAX_LEN

enum {
	IEEE80211_AUTH_NONE	= 0,
	IEEE80211_AUTH_OPEN	= 1,
	IEEE80211_AUTH_SHARED	= 2,
};

#endif /* _NET80211_IEEE80211_H_ */
