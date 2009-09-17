/*
 * WPA Supplicant / driver interface list
 * Copyright (c) 2004-2005, Jouni Malinen <j@w1.fi>
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

#include "includes.h"


#ifdef CONFIG_DRIVER_WEXT
extern struct wpa_driver_ops wpa_driver_wext_ops; /* driver_wext.c */
#endif /* CONFIG_DRIVER_WEXT */
#ifdef CONFIG_DRIVER_NL80211
extern struct wpa_driver_ops wpa_driver_nl80211_ops; /* driver_nl80211.c */
#endif /* CONFIG_DRIVER_NL80211 */
#ifdef CONFIG_DRIVER_HOSTAP
extern struct wpa_driver_ops wpa_driver_hostap_ops; /* driver_hostap.c */
#endif /* CONFIG_DRIVER_HOSTAP */
#ifdef CONFIG_DRIVER_PRISM54
extern struct wpa_driver_ops wpa_driver_prism54_ops; /* driver_prism54.c */
#endif /* CONFIG_DRIVER_PRISM54 */
#ifdef CONFIG_DRIVER_HERMES
extern struct wpa_driver_ops wpa_driver_hermes_ops; /* driver_hermes.c */
#endif /* CONFIG_DRIVER_HERMES */
#ifdef CONFIG_DRIVER_MADWIFI
extern struct wpa_driver_ops wpa_driver_madwifi_ops; /* driver_madwifi.c */
#endif /* CONFIG_DRIVER_MADWIFI */
#ifdef CONFIG_DRIVER_ATMEL
extern struct wpa_driver_ops wpa_driver_atmel_ops; /* driver_atmel.c */
#endif /* CONFIG_DRIVER_ATMEL */
#ifdef CONFIG_DRIVER_NDISWRAPPER
/* driver_ndiswrapper.c */
extern struct wpa_driver_ops wpa_driver_ndiswrapper_ops;
#endif /* CONFIG_DRIVER_NDISWRAPPER */
#ifdef CONFIG_DRIVER_BROADCOM
extern struct wpa_driver_ops wpa_driver_broadcom_ops; /* driver_broadcom.c */
#endif /* CONFIG_DRIVER_BROADCOM */
#ifdef CONFIG_DRIVER_IPW
extern struct wpa_driver_ops wpa_driver_ipw_ops; /* driver_ipw.c */
#endif /* CONFIG_DRIVER_IPW */
#ifdef CONFIG_DRIVER_BSD
extern struct wpa_driver_ops wpa_driver_bsd_ops; /* driver_bsd.c */
#endif /* CONFIG_DRIVER_BSD */
#ifdef CONFIG_DRIVER_NDIS
extern struct wpa_driver_ops wpa_driver_ndis_ops; /* driver_ndis.c */
#endif /* CONFIG_DRIVER_NDIS */
#ifdef CONFIG_DRIVER_WIRED
extern struct wpa_driver_ops wpa_driver_wired_ops; /* driver_wired.c */
#endif /* CONFIG_DRIVER_WIRED */
#ifdef CONFIG_DRIVER_TEST
extern struct wpa_driver_ops wpa_driver_test_ops; /* driver_test.c */
#endif /* CONFIG_DRIVER_TEST */
#ifdef CONFIG_DRIVER_RALINK
extern struct wpa_driver_ops wpa_driver_ralink_ops; /* driver_ralink.c */
#endif /* CONFIG_DRIVER_RALINK */
#ifdef CONFIG_DRIVER_OSX
extern struct wpa_driver_ops wpa_driver_osx_ops; /* driver_osx.m */
#endif /* CONFIG_DRIVER_OSX */
#ifdef CONFIG_DRIVER_PS3
extern struct wpa_driver_ops wpa_driver_ps3_ops; /* driver_ps3.c */
#endif /* CONFIG_DRIVER_PS3 */
#ifdef CONFIG_DRIVER_IPHONE
extern struct wpa_driver_ops wpa_driver_iphone_ops; /* driver_iphone.m */
#endif /* CONFIG_DRIVER_IPHONE */
#ifdef CONFIG_DRIVER_ROBOSWITCH
/* driver_roboswitch.c */
extern struct wpa_driver_ops wpa_driver_roboswitch_ops;
#endif /* CONFIG_DRIVER_ROBOSWITCH */


struct wpa_driver_ops *wpa_supplicant_drivers[] =
{
#ifdef CONFIG_DRIVER_WEXT
	&wpa_driver_wext_ops,
#endif /* CONFIG_DRIVER_WEXT */
#ifdef CONFIG_DRIVER_NL80211
	&wpa_driver_nl80211_ops,
#endif /* CONFIG_DRIVER_NL80211 */
#ifdef CONFIG_DRIVER_HOSTAP
	&wpa_driver_hostap_ops,
#endif /* CONFIG_DRIVER_HOSTAP */
#ifdef CONFIG_DRIVER_PRISM54
	&wpa_driver_prism54_ops,
#endif /* CONFIG_DRIVER_PRISM54 */
#ifdef CONFIG_DRIVER_HERMES
	&wpa_driver_hermes_ops,
#endif /* CONFIG_DRIVER_HERMES */
#ifdef CONFIG_DRIVER_MADWIFI
	&wpa_driver_madwifi_ops,
#endif /* CONFIG_DRIVER_MADWIFI */
#ifdef CONFIG_DRIVER_ATMEL
	&wpa_driver_atmel_ops,
#endif /* CONFIG_DRIVER_ATMEL */
#ifdef CONFIG_DRIVER_NDISWRAPPER
	&wpa_driver_ndiswrapper_ops,
#endif /* CONFIG_DRIVER_NDISWRAPPER */
#ifdef CONFIG_DRIVER_BROADCOM
	&wpa_driver_broadcom_ops,
#endif /* CONFIG_DRIVER_BROADCOM */
#ifdef CONFIG_DRIVER_IPW
	&wpa_driver_ipw_ops,
#endif /* CONFIG_DRIVER_IPW */
#ifdef CONFIG_DRIVER_BSD
	&wpa_driver_bsd_ops,
#endif /* CONFIG_DRIVER_BSD */
#ifdef CONFIG_DRIVER_NDIS
	&wpa_driver_ndis_ops,
#endif /* CONFIG_DRIVER_NDIS */
#ifdef CONFIG_DRIVER_WIRED
	&wpa_driver_wired_ops,
#endif /* CONFIG_DRIVER_WIRED */
#ifdef CONFIG_DRIVER_TEST
	&wpa_driver_test_ops,
#endif /* CONFIG_DRIVER_TEST */
#ifdef CONFIG_DRIVER_RALINK
	&wpa_driver_ralink_ops,
#endif /* CONFIG_DRIVER_RALINK */
#ifdef CONFIG_DRIVER_OSX
	&wpa_driver_osx_ops,
#endif /* CONFIG_DRIVER_OSX */
#ifdef CONFIG_DRIVER_PS3
	&wpa_driver_ps3_ops,
#endif /* CONFIG_DRIVER_PS3 */
#ifdef CONFIG_DRIVER_IPHONE
	&wpa_driver_iphone_ops,
#endif /* CONFIG_DRIVER_IPHONE */
#ifdef CONFIG_DRIVER_ROBOSWITCH
	&wpa_driver_roboswitch_ops,
#endif /* CONFIG_DRIVER_ROBOSWITCH */
	NULL
};
