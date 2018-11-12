/*
 * Control interface for shared AP commands
 * Copyright (c) 2004-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef CTRL_IFACE_AP_H
#define CTRL_IFACE_AP_H

int hostapd_ctrl_iface_sta_first(struct hostapd_data *hapd,
				 char *buf, size_t buflen);
int hostapd_ctrl_iface_sta(struct hostapd_data *hapd, const char *txtaddr,
			   char *buf, size_t buflen);
int hostapd_ctrl_iface_sta_next(struct hostapd_data *hapd, const char *txtaddr,
				char *buf, size_t buflen);
int hostapd_ctrl_iface_deauthenticate(struct hostapd_data *hapd,
				      const char *txtaddr);
int hostapd_ctrl_iface_disassociate(struct hostapd_data *hapd,
				    const char *txtaddr);
int hostapd_ctrl_iface_status(struct hostapd_data *hapd, char *buf,
			      size_t buflen);
int hostapd_parse_csa_settings(const char *pos,
			       struct csa_settings *settings);
int hostapd_ctrl_iface_stop_ap(struct hostapd_data *hapd);

#endif /* CTRL_IFACE_AP_H */
