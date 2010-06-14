/*
 * hostapd / driver interface list
 * Copyright (c) 2007, Jouni Malinen <j@w1.fi>
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


#ifdef CONFIG_DRIVER_HOSTAP
extern struct wpa_driver_ops wpa_driver_hostap_ops; /* driver_hostap.c */
#endif /* CONFIG_DRIVER_HOSTAP */
#ifdef CONFIG_DRIVER_NL80211
extern struct wpa_driver_ops wpa_driver_nl80211_ops; /* driver_nl80211.c */
#endif /* CONFIG_DRIVER_NL80211 */
#ifdef CONFIG_DRIVER_PRISM54
extern struct wpa_driver_ops wpa_driver_prism54_ops; /* driver_prism54.c */
#endif /* CONFIG_DRIVER_PRISM54 */
#ifdef CONFIG_DRIVER_MADWIFI
extern struct wpa_driver_ops wpa_driver_madwifi_ops; /* driver_madwifi.c */
#endif /* CONFIG_DRIVER_MADWIFI */
#ifdef CONFIG_DRIVER_ATHEROS
extern struct wpa_driver_ops wpa_driver_atheros_ops; /* driver_atheros.c */
#endif /* CONFIG_DRIVER_ATHEROS */
#ifdef CONFIG_DRIVER_BSD
extern struct wpa_driver_ops wpa_driver_bsd_ops; /* driver_bsd.c */
#endif /* CONFIG_DRIVER_BSD */
#ifdef CONFIG_DRIVER_WIRED
extern struct wpa_driver_ops wpa_driver_wired_ops; /* driver_wired.c */
#endif /* CONFIG_DRIVER_WIRED */
#ifdef CONFIG_DRIVER_TEST
extern struct wpa_driver_ops wpa_driver_test_ops; /* driver_test.c */
#endif /* CONFIG_DRIVER_TEST */
#ifdef CONFIG_DRIVER_NONE
extern struct wpa_driver_ops wpa_driver_none_ops; /* driver_none.c */
#endif /* CONFIG_DRIVER_NONE */


struct wpa_driver_ops *hostapd_drivers[] =
{
#ifdef CONFIG_DRIVER_HOSTAP
	&wpa_driver_hostap_ops,
#endif /* CONFIG_DRIVER_HOSTAP */
#ifdef CONFIG_DRIVER_NL80211
	&wpa_driver_nl80211_ops,
#endif /* CONFIG_DRIVER_NL80211 */
#ifdef CONFIG_DRIVER_PRISM54
	&wpa_driver_prism54_ops,
#endif /* CONFIG_DRIVER_PRISM54 */
#ifdef CONFIG_DRIVER_MADWIFI
	&wpa_driver_madwifi_ops,
#endif /* CONFIG_DRIVER_MADWIFI */
#ifdef CONFIG_DRIVER_ATHEROS
	&wpa_driver_atheros_ops,
#endif /* CONFIG_DRIVER_ATHEROS */
#ifdef CONFIG_DRIVER_BSD
	&wpa_driver_bsd_ops,
#endif /* CONFIG_DRIVER_BSD */
#ifdef CONFIG_DRIVER_WIRED
	&wpa_driver_wired_ops,
#endif /* CONFIG_DRIVER_WIRED */
#ifdef CONFIG_DRIVER_TEST
	&wpa_driver_test_ops,
#endif /* CONFIG_DRIVER_TEST */
#ifdef CONFIG_DRIVER_NONE
	&wpa_driver_none_ops,
#endif /* CONFIG_DRIVER_NONE */
	NULL
};
