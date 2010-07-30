/*
 * hostapd / WMM (Wi-Fi Multimedia)
 * Copyright 2002-2003, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef WME_H
#define WME_H

/*
 * WMM Information Element (used in (Re)Association Request frames; may also be
 * used in Beacon frames)
 */
struct wmm_information_element {
	/* Element ID: 221 (0xdd); Length: 7 */
	/* required fields for WMM version 1 */
	u8 oui[3]; /* 00:50:f2 */
	u8 oui_type; /* 2 */
	u8 oui_subtype; /* 0 */
	u8 version; /* 1 for WMM version 1.0 */
	u8 qos_info; /* AP/STA specific QoS info */
};

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/endian.h>
#endif /* defined(__FreeBSD__) || defined(__NetBSD__) ||
	* defined(__DragonFly__) */


static inline u16 tsinfo(int tag1d, int contention_based, int direction)
{
	return (tag1d << 11) | (contention_based << 7) | (direction << 5) |
	  (tag1d << 1);
}


struct wme_information_element {
	/* required fields for WME version 1 */
	u8 oui[3];
	u8 oui_type;
	u8 oui_subtype;
	u8 version;
	u8 acInfo;

} __attribute__ ((packed));

#define WMM_AC_AIFSN_MASK 0x0f
#define WMM_AC_AIFNS_SHIFT 0
#define WMM_AC_ACM 0x10
#define WMM_AC_ACI_MASK 0x60
#define WMM_AC_ACI_SHIFT 5

#define WMM_AC_ECWMIN_MASK 0x0f
#define WMM_AC_ECWMIN_SHIFT 0
#define WMM_AC_ECWMAX_MASK 0xf0
#define WMM_AC_ECWMAX_SHIFT 4

struct wmm_ac_parameter {
	u8 aci_aifsn; /* AIFSN, ACM, ACI */
	u8 cw; /* ECWmin, ECWmax (CW = 2^ECW - 1) */
	le16 txop_limit;
} __attribute__ ((packed));

/*
 * WMM Parameter Element (used in Beacon, Probe Response, and (Re)Association
 * Response frmaes)
 */
struct wmm_parameter_element {
	/* Element ID: 221 (0xdd); Length: 24 */
	/* required fields for WMM version 1 */
	u8 oui[3]; /* 00:50:f2 */
	u8 oui_type; /* 2 */
	u8 oui_subtype; /* 1 */
	u8 version; /* 1 for WMM version 1.0 */
	u8 qos_info; /* AP/STA specif QoS info */
	u8 reserved; /* 0 */
	struct wmm_ac_parameter ac[4]; /* AC_BE, AC_BK, AC_VI, AC_VO */

} __attribute__ ((packed));

/* WMM TSPEC Element */
struct wmm_tspec_element {
	u8 eid; /* 221 = 0xdd */
	u8 length; /* 6 + 55 = 61 */
	u8 oui[3]; /* 00:50:f2 */
	u8 oui_type; /* 2 */
	u8 oui_subtype; /* 2 */
	u8 version; /* 1 */
	/* WMM TSPEC body (55 octets): */
	u8 ts_info[3];
	le16 nominal_msdu_size;
	le16 maximum_msdu_size;
	le32 minimum_service_interval;
	le32 maximum_service_interval;
	le32 inactivity_interval;
	le32 suspension_interval;
	le32 service_start_time;
	le32 minimum_data_rate;
	le32 mean_data_rate;
	le32 peak_data_rate;
	le32 maximum_burst_size;
	le32 delay_bound;
	le32 minimum_phy_rate;
	le16 surplus_bandwidth_allowance;
	le16 medium_time;
} __attribute__ ((packed));


/* Access Categories / ACI to AC coding */
enum {
	WMM_AC_BE = 0 /* Best Effort */,
	WMM_AC_BK = 1 /* Background */,
	WMM_AC_VI = 2 /* Video */,
	WMM_AC_VO = 3 /* Voice */
};

struct ieee80211_mgmt;

u8 * hostapd_eid_wmm(struct hostapd_data *hapd, u8 *eid);
int hostapd_eid_wmm_valid(struct hostapd_data *hapd, u8 *eid, size_t len);
int hostapd_wmm_sta_config(struct hostapd_data *hapd, struct sta_info *sta);
void hostapd_wmm_action(struct hostapd_data *hapd, struct ieee80211_mgmt *mgmt,
			size_t len);

#endif /* WME_H */
