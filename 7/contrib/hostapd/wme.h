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

#ifdef __linux__
#include <endian.h>
#endif /* __linux__ */

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/endian.h>
#endif /* defined(__FreeBSD__) || defined(__NetBSD__) ||
	* defined(__DragonFly__) */

#define WME_OUI_TYPE 2
#define WME_OUI_SUBTYPE_INFORMATION_ELEMENT 0
#define WME_OUI_SUBTYPE_PARAMETER_ELEMENT 1
#define WME_OUI_SUBTYPE_TSPEC_ELEMENT 2
#define WME_VERSION 1

#define WME_ACTION_CATEGORY 17
#define WME_ACTION_CODE_SETUP_REQUEST 0
#define WME_ACTION_CODE_SETUP_RESPONSE 1
#define WME_ACTION_CODE_TEARDOWN 2

#define WME_SETUP_RESPONSE_STATUS_ADMISSION_ACCEPTED 0
#define WME_SETUP_RESPONSE_STATUS_INVALID_PARAMETERS 1
#define WME_SETUP_RESPONSE_STATUS_REFUSED 3

#define WME_TSPEC_DIRECTION_UPLINK 0
#define WME_TSPEC_DIRECTION_DOWNLINK 1
#define WME_TSPEC_DIRECTION_BI_DIRECTIONAL 3

extern inline u16 tsinfo(int tag1d, int contention_based, int direction)
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

struct wme_ac_parameter {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	/* byte 1 */
	u8 	aifsn:4,
		acm:1,
	 	aci:2,
	 	reserved:1;

	/* byte 2 */
	u8 	eCWmin:4,
	 	eCWmax:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	/* byte 1 */
	u8 	reserved:1,
	 	aci:2,
	 	acm:1,
	 	aifsn:4;

	/* byte 2 */
	u8 	eCWmax:4,
	 	eCWmin:4;
#else
#error	"Please fix <endian.h>"
#endif

	/* bytes 3 & 4 */
	u16 txopLimit;
} __attribute__ ((packed));

struct wme_parameter_element {
	/* required fields for WME version 1 */
	u8 oui[3];
	u8 oui_type;
	u8 oui_subtype;
	u8 version;
	u8 acInfo;
	u8 reserved;
	struct wme_ac_parameter ac[4];

} __attribute__ ((packed));

struct wme_tspec_info_element {
	u8 eid;
	u8 length;
	u8 oui[3];
	u8 oui_type;
	u8 oui_subtype;
	u8 version;
	u16 ts_info;
	u16 nominal_msdu_size;
	u16 maximum_msdu_size;
	u32 minimum_service_interval;
	u32 maximum_service_interval;
	u32 inactivity_interval;
	u32 start_time;
	u32 minimum_data_rate;
	u32 mean_data_rate;
	u32 maximum_burst_size;
	u32 minimum_phy_rate;
	u32 peak_data_rate;
	u32 delay_bound;
	u16 surplus_bandwidth_allowance;
	u16 medium_time;
} __attribute__ ((packed));


/* Access Categories */
enum {
	WME_AC_BK = 1,
	WME_AC_BE = 0,
	WME_AC_VI = 2,
	WME_AC_VO = 3
};


u8 * hostapd_eid_wme(struct hostapd_data *hapd, u8 *eid);
int hostapd_eid_wme_valid(struct hostapd_data *hapd, u8 *eid, size_t len);
int hostapd_wme_sta_config(struct hostapd_data *hapd, struct sta_info *sta);
void hostapd_wme_action(struct hostapd_data *hapd, struct ieee80211_mgmt *mgmt,
			size_t len);

#endif /* WME_H */
